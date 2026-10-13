import { createInterface } from "node:readline";
import { context } from "./server.mjs";
import { executeTool, TOOL_DEFINITIONS } from "./agent-tools.mjs";

const schemas = {
  get_scene: { type: "object", properties: {}, additionalProperties: false },
  add_entity: {
    type: "object",
    properties: {
      name: { type: "string", description: "Optional display name for the entity." },
      type: { type: "string", enum: ["empty", "cube", "camera", "light"], description: "Entity type." }
    },
    additionalProperties: false
  },
  update_entity: {
    type: "object",
    required: ["id", "patch"],
    properties: {
      id: { type: "string", description: "Exact entity ID from get_scene." },
      patch: {
        type: "object",
        properties: {
          name: { type: "string" },
          enabled: { type: "boolean" },
          transform: {
            type: "object",
            properties: {
              position: { type: "array", items: { type: "number" }, minItems: 3, maxItems: 3 },
              rotation: { type: "array", items: { type: "number" }, minItems: 3, maxItems: 3 },
              scale: { type: "array", items: { type: "number" }, minItems: 3, maxItems: 3 }
            },
            additionalProperties: false
          }
        },
        additionalProperties: false
      }
    },
    additionalProperties: false
  },
  delete_entity: {
    type: "object",
    required: ["id", "confirm"],
    properties: {
      id: { type: "string", description: "Exact entity ID from get_scene." },
      confirm: { type: "string", description: "Must exactly equal delete:<entity-id>, after the user explicitly approves deletion." }
    },
    additionalProperties: false
  },
  list_assets: { type: "object", properties: {}, additionalProperties: false },
  get_events: { type: "object", properties: { limit: { type: "integer", minimum: 1, maximum: 100 } }, additionalProperties: false }
};

function send(message) {
  process.stdout.write(JSON.stringify(message) + "\n");
}
function handle(message) {
  if (!message || typeof message !== "object" || Array.isArray(message)) return;
  const id = message.id;
  const isRequest = id !== undefined;
  const respond = (result) => { if (isRequest) send({ jsonrpc: "2.0", id, result }); };
  const fail = (code, text) => { if (isRequest) send({ jsonrpc: "2.0", id, error: { code, message: text } }); };
  try {
    if (message.jsonrpc !== "2.0" || typeof message.method !== "string") return fail(-32600, "Invalid JSON-RPC request.");
    if (message.method === "notifications/initialized" || message.method === "notifications/cancelled") return;
    if (message.method === "initialize") {
      return respond({
        protocolVersion: message.params?.protocolVersion || "2024-11-05",
        capabilities: { tools: {} },
        serverInfo: { name: "mid-engine", version: "0.1.0" },
        instructions: "Read the current scene before editing. Verify tool results. Ask the user before destructive actions; delete_entity requires exact explicit confirmation."
      });
    }
    if (message.method === "ping") return respond({});
    if (message.method === "tools/list") {
      return respond({
        tools: TOOL_DEFINITIONS.map((tool) => ({
          name: tool.name,
          description: tool.description,
          inputSchema: schemas[tool.name] || { type: "object", properties: {} },
          annotations: { readOnlyHint: ["get_scene", "list_assets", "get_events"].includes(tool.name), destructiveHint: tool.name === "delete_entity", openWorldHint: false }
        }))
      });
    }
    if (message.method === "tools/call") {
      const name = message.params?.name;
      if (typeof name !== "string" || !TOOL_DEFINITIONS.some((tool) => tool.name === name)) {
        return respond({ content: [{ type: "text", text: `Unknown tool: ${String(name)}` }], isError: true });
      }
      const result = executeTool(name, message.params?.arguments || {}, context);
      return respond({ content: [{ type: "text", text: JSON.stringify(result) }], structuredContent: result, isError: false });
    }
    return fail(-32601, `Method not found: ${message.method}`);
  } catch (error) {
    if (message.method === "tools/call" && isRequest) {
      return respond({ content: [{ type: "text", text: JSON.stringify({ error: error.code || "tool_failed", message: error.message }) }], isError: true });
    }
    return fail(-32603, error.message || "Internal error.");
  }
}

const input = createInterface({ input: process.stdin, crlfDelay: Infinity, terminal: false });
input.on("line", (line) => {
  if (!line.trim()) return;
  try { handle(JSON.parse(line)); }
  catch { send({ jsonrpc: "2.0", id: null, error: { code: -32700, message: "Parse error." } }); }
});
