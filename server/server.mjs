import { createServer } from "node:http";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, extname, join, normalize } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { createScene, validateScene } from "../engine/scene.js";
import { executeTool, interpretCommand, TOOL_DEFINITIONS } from "./agent-tools.mjs";

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const PROJECT_DIR = join(ROOT, "project");
const SCENE_FILE = join(PROJECT_DIR, "scene.json");
const HOST = process.env.MID_ENGINE_HOST || "127.0.0.1";
const PORT = Number(process.env.PORT || 4173);
const MAX_BODY_BYTES = 64 * 1024;
const STATIC_FILES = new Map([
  ["/", ["index.html", "text/html; charset=utf-8"]],
  ["/index.html", ["index.html", "text/html; charset=utf-8"]],
  ["/styles.css", ["styles.css", "text/css; charset=utf-8"]],
  ["/app.js", ["app.js", "text/javascript; charset=utf-8"]]
]);

mkdirSync(PROJECT_DIR, { recursive: true });
let scene = loadScene();
let assets = [];
const events = [];
function loadScene() {
  if (!existsSync(SCENE_FILE)) {
    const initial = createScene();
    writeFileSync(SCENE_FILE, JSON.stringify(initial, null, 2) + "\n");
    return initial;
  }
  try {
    const parsed = JSON.parse(readFileSync(SCENE_FILE, "utf8"));
    validateScene(parsed);
    return parsed;
  } catch (error) {
    console.error("Could not load project/scene.json:", error.message);
    return createScene();
  }
}
function saveScene() {
  validateScene(scene);
  writeFileSync(SCENE_FILE, JSON.stringify(scene, null, 2) + "\n", "utf8");
}
function addEvent(type, message, data = {}) {
  const event = { id: crypto.randomUUID(), type, message, data, at: new Date().toISOString() };
  events.unshift(event);
  if (events.length > 200) events.length = 200;
  return event;
}
const context = {
  getScene: () => scene,
  setScene: (next) => { validateScene(next); scene = next; saveScene(); },
  getAssets: () => assets,
  getEvents: (limit = 50) => events.slice(0, Math.max(1, Math.min(100, limit))),
  addEvent
};

function send(response, status, body, headers = {}) {
  response.writeHead(status, { "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store", "X-Content-Type-Options": "nosniff", ...headers });
  response.end(JSON.stringify(body));
}
async function readJson(request) {
  let body = "";
  for await (const chunk of request) {
    body += chunk;
    if (Buffer.byteLength(body) > MAX_BODY_BYTES) throw Object.assign(new Error("Request body is too large."), { status: 413 });
  }
  if (!body.trim()) return {};
  try { return JSON.parse(body); }
  catch { throw Object.assign(new Error("Request body must be valid JSON."), { status: 400 }); }
}
function serveStatic(pathname, response) {
  const entry = STATIC_FILES.get(pathname);
  if (!entry) return false;
  const [relativePath, contentType] = entry;
  const fullPath = normalize(join(ROOT, relativePath));
  if (!fullPath.startsWith(ROOT)) return false;
  const body = readFileSync(fullPath);
  response.writeHead(200, { "Content-Type": contentType, "Cache-Control": "no-cache", "X-Content-Type-Options": "nosniff", "Content-Security-Policy": "default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self'; base-uri 'none'; frame-ancestors 'none'" });
  response.end(body);
  return true;
}

export function createMidEngineServer() {
  return createServer(async (request, response) => {
    const origin = request.headers.origin;
    if (origin) {
      const host = request.headers.host || "";
      if (!origin.startsWith(`http://${host}`) && !origin.startsWith(`https://${host}`)) {
        send(response, 403, { error: "origin_not_allowed", message: "Cross-origin requests are not allowed." });
        return;
      }
    }
    const url = new URL(request.url || "/", `http://${request.headers.host || "127.0.0.1"}`);
    try {
      if (request.method === "GET" && url.pathname === "/api/health") return send(response, 200, { ok: true, name: "Mid Engine", version: "0.1.0", host: HOST });
      if (request.method === "GET" && url.pathname === "/api/scene") return send(response, 200, { scene, assets });
      if (request.method === "GET" && url.pathname === "/api/tools") return send(response, 200, { tools: TOOL_DEFINITIONS, safety: { loopbackOnly: HOST === "127.0.0.1" || HOST === "localhost", destructiveOperationsRequireExplicitConfirmation: true } });
      if (request.method === "GET" && url.pathname === "/api/events") return send(response, 200, { events: context.getEvents(Number(url.searchParams.get("limit")) || 50) });
      if (request.method === "POST" && url.pathname === "/api/save") {
        saveScene();
        const event = addEvent("project.saved", "Scene saved to project/scene.json.");
        return send(response, 200, { ok: true, message: "Scene saved to project/scene.json.", event });
      }
      if (request.method === "POST" && url.pathname === "/api/tools/execute") {
        const body = await readJson(request);
        if (typeof body.name !== "string" || !body.name) return send(response, 400, { error: "invalid_tool", message: "A tool name is required." });
        const result = executeTool(body.name, body.args && typeof body.args === "object" ? body.args : {}, context);
        return send(response, 200, { ok: true, ...result });
      }
      if (request.method === "POST" && url.pathname === "/api/agent/command") {
        const body = await readJson(request);
        const result = interpretCommand(body.command, context);
        return send(response, 200, { ok: true, ...result });
      }
      if (request.method === "GET" && serveStatic(url.pathname, response)) return;
      send(response, 404, { error: "not_found", message: "Route not found." });
    } catch (error) {
      const status = error.status || (error.code === "approval_required" ? 403 : error.code === "unknown_tool" || error.code === "unsupported_command" ? 400 : 422);
      send(response, status, { error: error.code || "request_failed", message: error.message || "Request failed." });
    }
  });
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const server = createMidEngineServer();
  server.listen(PORT, HOST, () => {
    console.log(`Mid Engine 0.1.0 listening at http://${HOST}:${PORT}`);
    console.log("Agent API: GET /api/tools · POST /api/tools/execute");
    console.log(`Project scene: ${SCENE_FILE}`);
  });
  for (const signal of ["SIGINT", "SIGTERM"]) process.on(signal, () => server.close(() => process.exit(0)));
}
