import { createWebGLRenderer } from "/engine/webgl-renderer.js";
const $ = (selector) => document.querySelector(selector);
let renderer = null;
try { renderer = createWebGLRenderer($("#scene-canvas")); }
catch (error) { console.error(error); $("#connection-state").textContent = "WEBGL UNAVAILABLE"; }
let scene = null;
let selectedId = null;
let assets = [];
let runtimeScene = null;
let isPlaying = false;
let playFrameHandle = null;
let lastPlayFrameAt = 0;
const runtimeVelocities = new Map();
const undoStack = [];
const redoStack = [];
const clone = (value) => structuredClone(value);
function updateHistoryButtons() {
  $("#undo-button").disabled = isPlaying || undoStack.length === 0;
  $("#redo-button").disabled = isPlaying || redoStack.length === 0;
}
function recordHistory(before, after) {
  if (!before || !after || JSON.stringify(before) === JSON.stringify(after)) return;
  undoStack.push(before);
  if (undoStack.length > 100) undoStack.shift();
  redoStack.length = 0;
  updateHistoryButtons();
}
async function restoreHistory(from, to, label) {
  if (isPlaying || !from.length || !scene) return;
  const snapshot = from.pop(), current = clone(scene);
  try {
    const result = await api("/api/scene/restore", { method: "POST", body: JSON.stringify({ scene: snapshot }) });
    to.push(current); scene = result.scene;
    if (selectedId && !scene.entities.some((entity) => entity.id === selectedId)) selectedId = null;
    render(); updateHistoryButtons(); log(label + " completed.");
  } catch (error) { from.push(snapshot); showResponse(label + " failed: " + error.message); }
}
function undo() { return restoreHistory(undoStack, redoStack, "Undo"); }
function redo() { return restoreHistory(redoStack, undoStack, "Redo"); }

async function api(path, options = {}) {
  const response = await fetch(path, { headers: { "Content-Type": "application/json", ...(options.headers || {}) }, ...options });
  const payload = await response.json().catch(() => ({ error: "The server returned an unreadable response." }));
  if (!response.ok) throw new Error(payload.message || payload.error || `Request failed (${response.status})`);
  return payload;
}
function log(message) {
  const line = document.createElement("div");
  line.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
  $("#activity-log").prepend(line);
}
function currentEntity() { return scene?.entities.find((entity) => entity.id === selectedId) || null; }
$("#scene-canvas").addEventListener("mid-engine-pick", (event) => {
  if (isPlaying) return;
  selectedId = event.detail?.entityId || null;
  render();
});
$("#viewport-gizmo").querySelectorAll("button[data-axis]").forEach((button) => {
  button.addEventListener("click", () => nudgeSelectedAxis(button.dataset.axis, Number(button.dataset.delta)));
});
function renderTransformGizmo(visibleScene) {
  const gizmo = $("#viewport-gizmo"), entity = currentEntity();
  const projected = !isPlaying && entity ? renderer?.project(entity) : null;
  gizmo.classList.toggle("hidden", !projected);
  if (!projected) return;
  gizmo.style.left = projected.x + "px";
  gizmo.style.top = projected.y + "px";
  for (const button of gizmo.querySelectorAll("button")) button.disabled = isPlaying;
}
function render() {
  if (!scene) return;
  $("#project-name").textContent = scene.name;
  $("#scene-summary").textContent = `${scene.entities.length} entities`;
  $("#footer-entities").textContent = `${scene.entities.length} entities`;
  $("#scene-tree").replaceChildren();
  for (const entity of scene.entities) {
    const row = document.createElement("div");
    row.className = `tree-item${entity.id === selectedId ? " selected" : ""}`;
    row.setAttribute("role", "treeitem");
    row.setAttribute("aria-selected", String(entity.id === selectedId));
    const glyph = entity.type === "camera" ? "◉" : entity.type === "light" ? "☼" : entity.type === "cube" ? "◇" : "◈";
    const icon = document.createElement("span"); icon.className = "tree-icon"; icon.textContent = glyph;
    const name = document.createElement("span"); name.className = "tree-name"; name.textContent = entity.name;
    const type = document.createElement("span"); type.className = "tree-type"; type.textContent = entity.type.toUpperCase();
    row.append(icon, name, type);
    row.addEventListener("click", () => { selectedId = entity.id; render(); });
    $("#scene-tree").append(row);
  }
  $("#viewport-empty").classList.toggle("hidden", scene.entities.length > 0);
  window.__midEngineLastScene = scene;
  window.__midEngineSelectedId = selectedId;
  const visibleScene = runtimeScene || scene;
  const stats = renderer?.render(visibleScene, selectedId);
  if (stats) {
    const gpu = Number.isFinite(stats.gpuMilliseconds) ? " · " + stats.gpuMilliseconds.toFixed(2) + " ms GPU" : "";
    $("#render-stats").textContent = stats.visible + "/" + stats.submitted + " visible · " + stats.culled + " culled · " + stats.drawCalls + " draws · " + stats.cpuMilliseconds.toFixed(2) + " ms CPU" + gpu;
  }
  renderTransformGizmo(visibleScene);
  renderInspector();
  $("#asset-count").textContent = `${assets.length} ITEMS`;
  $("#asset-list").replaceChildren();
  if (!assets.length) {
    const empty = document.createElement("div"); empty.className = "asset-item muted"; empty.textContent = "No assets yet"; $("#asset-list").append(empty);
  } else for (const asset of assets) {
    const row = document.createElement("div"); row.className = "asset-item";
    const icon = document.createElement("span"); icon.className = "asset-icon"; icon.textContent = "◇";
    const label = document.createElement("span"); label.textContent = asset.name;
    row.append(icon, label); $("#asset-list").append(row);
  }
}
function renderInspector() {
  const entity = currentEntity();
  $("#inspector-content").replaceChildren();
  $("#selected-name").textContent = entity?.name || "No selection";
  $("#selected-type").textContent = entity?.type?.toUpperCase() || "—";
  if (!entity) {
    const empty = document.createElement("div"); empty.className = "inspector-empty"; empty.textContent = "Select an entity in the hierarchy to edit its properties."; $("#inspector-content").append(empty); return;
  }
  const nameLabel = document.createElement("label"); nameLabel.className = "field-label"; nameLabel.textContent = "Name";
  const nameInput = document.createElement("input"); nameInput.className = "text-field"; nameInput.value = entity.name; nameInput.maxLength = 80;
  nameInput.addEventListener("change", async () => { try { await runTool("update_entity", { id: entity.id, patch: { name: nameInput.value } }); } catch (error) { showResponse(error.message); } });
  $("#inspector-content").append(nameLabel, nameInput);
  for (const [key, title] of [["position","Position"],["rotation","Rotation"],["scale","Scale"]]) {
    const section = document.createElement("section"); section.className = "property-section";
    const heading = document.createElement("div"); heading.className = "property-heading"; heading.textContent = title; section.append(heading);
    const row = document.createElement("div"); row.className = "vector-row";
    entity.transform[key].forEach((value, index) => {
      const input = document.createElement("input"); input.className = "vector-input"; input.type = "number"; input.step = "0.1"; input.value = String(value); input.setAttribute("aria-label", `${title} ${["X","Y","Z"][index]}`);
      input.addEventListener("change", async () => {
        const next = [...entity.transform[key]]; next[index] = Number(input.value);
        try { await runTool("update_entity", { id: entity.id, patch: { transform: { [key]: next } } }); } catch (error) { showResponse(error.message); }
      });
      row.append(input);
    });
    section.append(row);
    if (key === "position") {
      const gizmo = document.createElement("div"); gizmo.className = "gizmo-controls"; gizmo.setAttribute("aria-label", "Transform gizmo controls");
      for (const axis of ["X", "Y", "Z"]) for (const delta of [-0.5, 0.5]) {
        const button = document.createElement("button"); button.type = "button"; button.className = "gizmo-button axis-" + axis.toLowerCase();
        button.textContent = (delta < 0 ? "−" : "+") + axis; button.title = "Move " + axis + " by " + delta;
        button.disabled = isPlaying; button.addEventListener("click", () => nudgeSelectedAxis(axis, delta)); gizmo.append(button);
      }
      section.append(gizmo);
    }
    $("#inspector-content").append(section);
  }
  const toggle = document.createElement("label"); toggle.className = "toggle-row"; toggle.textContent = "Enabled";
  const checkbox = document.createElement("input"); checkbox.type = "checkbox"; checkbox.checked = entity.enabled;
  checkbox.addEventListener("change", async () => { try { await runTool("update_entity", { id: entity.id, patch: { enabled: checkbox.checked } }); } catch (error) { showResponse(error.message); } });
  toggle.append(checkbox); $("#inspector-content").append(toggle);
  const remove = document.createElement("button"); remove.className = "danger-button"; remove.textContent = "Delete entity…";
  remove.addEventListener("click", async () => {
    if (!confirm(`Delete “${entity.name}”? This cannot be undone.`)) return;
    try { await runTool("delete_entity", { id: entity.id, confirm: `delete:${entity.id}` }); selectedId = null; }
    catch (error) { showResponse(error.message); }
  });
  $("#inspector-content").append(remove);
  if (isPlaying) for (const control of $("#inspector-content").querySelectorAll("input,button")) control.disabled = true;
}
async function refresh() {
  const result = await api("/api/scene");
  scene = result.scene; assets = result.assets || [];
  if (selectedId && !scene.entities.some((entity) => entity.id === selectedId)) selectedId = null;
  render();
}
async function runTool(name, args = {}) {
  const before = scene ? clone(scene) : null;
  const result = await api("/api/tools/execute", { method: "POST", body: JSON.stringify({ name, args }) });
  if (result.scene) { recordHistory(before, result.scene); scene = result.scene; }
  if (result.assets) assets = result.assets;
  if (result.entity) selectedId = result.entity.id;
  if (result.event?.message) log(result.event.message);
  render();
  return result;
}
function showResponse(message) { $("#agent-response").textContent = message; }
async function submitCommand(command) {
  showResponse("Working…");
  const before = scene ? clone(scene) : null;
  try {
    const result = await api("/api/agent/command", { method: "POST", body: JSON.stringify({ command }) });
    if (result.scene) { recordHistory(before, result.scene); scene = result.scene; }
    if (result.entity) selectedId = result.entity.id;
    showResponse(result.message || "Task completed.");
    log(result.message || "Agent command completed.");
    render();
  } catch (error) { showResponse(`Could not complete task: ${error.message}`); log(`Agent error: ${error.message}`); }
}
$("#agent-form").addEventListener("submit", async (event) => {
  event.preventDefault(); const command = $("#agent-prompt").value.trim(); if (!command) return;
  $("#agent-prompt").value = ""; await submitCommand(command);
});
document.querySelectorAll("[data-command]").forEach((button) => button.addEventListener("click", () => submitCommand(button.dataset.command)));
$("#add-entity").addEventListener("click", () => submitCommand("Add an empty entity"));
$("#empty-add").addEventListener("click", () => submitCommand("Add a cube entity"));
$("#refresh-scene").addEventListener("click", () => refresh().then(() => log("Scene refreshed.")).catch((e) => showResponse(e.message)));
$("#save-button").addEventListener("click", async () => { try { const result = await api("/api/save", { method: "POST", body: "{}" }); log(result.message); showResponse(result.message); } catch (e) { showResponse(e.message); } });
function nudgeSelectedAxis(axis, delta) {
  const entity = currentEntity(); if (!entity || isPlaying) return;
  const position = [...entity.transform.position]; position[["X","Y","Z"].indexOf(axis)] += delta;
  runTool("update_entity", { id: entity.id, patch: { transform: { position } } }).catch((error) => showResponse(error.message));
}
function stopPlayMode() {
  isPlaying = false; runtimeScene = null; runtimeVelocities.clear();
  if (playFrameHandle !== null) cancelAnimationFrame(playFrameHandle);
  playFrameHandle = null; $("#play-button").textContent = "▶ Play preview"; $("#play-button").classList.add("primary");
  $("#connection-state").textContent = "LOCAL API CONNECTED"; render(); updateHistoryButtons(); log("Play mode stopped; authoring scene preserved.");
}
function playFrame(timestamp) {
  if (!isPlaying || !runtimeScene) return;
  const dt = Math.min(0.05, Math.max(0, (timestamp - lastPlayFrameAt) / 1000)); lastPlayFrameAt = timestamp;
  for (const entity of runtimeScene.entities) {
    if (entity.type !== "cube" || entity.enabled === false) continue;
    const p = entity.transform.position, s = entity.transform.scale;
    const floor = Math.max(0.1, Math.max(Math.abs(s[0]), Math.abs(s[1]), Math.abs(s[2])));
    let velocity = runtimeVelocities.get(entity.id) || 0;
    if (p[1] <= floor && velocity <= 0) { p[1] = floor; velocity = 0; }
    else {
      velocity -= 9.81 * dt; p[1] += velocity * dt;
      if (p[1] < floor) { p[1] = floor; velocity = velocity < -0.25 ? -velocity * 0.12 : 0; }
    }
    runtimeVelocities.set(entity.id, velocity);
  }
  window.__midEngineLastScene = runtimeScene;
  const stats = renderer?.render(runtimeScene, selectedId);
  if (stats) {
    const gpu = Number.isFinite(stats.gpuMilliseconds) ? " · " + stats.gpuMilliseconds.toFixed(2) + " ms GPU" : "";
    $("#render-stats").textContent = stats.visible + "/" + stats.submitted + " visible · " + stats.culled + " culled · " + stats.drawCalls + " draws · " + stats.cpuMilliseconds.toFixed(2) + " ms CPU" + gpu;
  }
  playFrameHandle = requestAnimationFrame(playFrame);
}
$("#play-button").addEventListener("click", () => {
  if (isPlaying) { stopPlayMode(); return; }
  if (!scene) return;
  runtimeScene = clone(scene); runtimeVelocities.clear(); isPlaying = true; lastPlayFrameAt = performance.now();
  $("#play-button").textContent = "■ Stop preview"; $("#play-button").classList.remove("primary");
  $("#connection-state").textContent = "PLAY MODE"; render(); updateHistoryButtons();
  log("Play mode started from an isolated runtime scene. Cube entities use the preview gravity/floor simulation.");
  playFrameHandle = requestAnimationFrame(playFrame);
});
$("#undo-button").addEventListener("click", () => undo());
$("#redo-button").addEventListener("click", () => redo());
document.addEventListener("keydown", (event) => {
  if (!(event.ctrlKey || event.metaKey) || event.altKey) return;
  const tag = document.activeElement?.tagName?.toLowerCase();
  if (tag === "input" || tag === "textarea" || document.activeElement?.isContentEditable) return;
  if (event.key.toLowerCase() === "z" && !event.shiftKey) { event.preventDefault(); undo(); }
  else if (event.key.toLowerCase() === "y" || (event.key.toLowerCase() === "z" && event.shiftKey)) { event.preventDefault(); redo(); }
});
$("#clear-log").addEventListener("click", () => $("#activity-log").replaceChildren());
$("#agent-tools").addEventListener("click", async () => { try { const result = await api("/api/tools"); showResponse("Available agent tools:\n" + result.tools.map((tool) => `• ${tool.name}: ${tool.description}`).join("\n")); } catch (e) { showResponse(e.message); } });
(async () => {
  try { await refresh(); $("#connection-state").textContent = "LOCAL API CONNECTED"; $("#connection-state").style.color = "#a8f06a"; log("Connected to local engine API."); }
  catch (error) { $("#connection-state").textContent = "API OFFLINE"; showResponse(error.message); log(`Connection error: ${error.message}`); }
  updateHistoryButtons();
})();
