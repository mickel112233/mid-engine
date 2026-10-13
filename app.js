import { createWebGLRenderer } from "/engine/webgl-renderer.js";
const $ = (selector) => document.querySelector(selector);
let renderer = null;
try { renderer = createWebGLRenderer($("#scene-canvas")); }
catch (error) { console.error(error); $("#connection-state").textContent = "WEBGL UNAVAILABLE"; }
let scene = null;
let selectedId = null;
let assets = [];

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
  const stats = renderer?.render(scene, selectedId);
  if (stats) $("#render-stats").textContent = `${stats.visible}/${stats.submitted} visible · ${stats.drawCalls} draw calls · ${stats.cpuMilliseconds.toFixed(2)} ms CPU`;
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
    section.append(row); $("#inspector-content").append(section);
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
}
async function refresh() {
  const result = await api("/api/scene");
  scene = result.scene; assets = result.assets || [];
  if (selectedId && !scene.entities.some((entity) => entity.id === selectedId)) selectedId = null;
  render();
}
async function runTool(name, args = {}) {
  const result = await api("/api/tools/execute", { method: "POST", body: JSON.stringify({ name, args }) });
  if (result.scene) scene = result.scene;
  if (result.assets) assets = result.assets;
  if (result.entity) selectedId = result.entity.id;
  if (result.event?.message) log(result.event.message);
  render();
  return result;
}
function showResponse(message) { $("#agent-response").textContent = message; }
async function submitCommand(command) {
  showResponse("Working…");
  try {
    const result = await api("/api/agent/command", { method: "POST", body: JSON.stringify({ command }) });
    if (result.scene) scene = result.scene;
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
$("#play-button").addEventListener("click", () => { const playing = $("#play-button").textContent.includes("Play"); $("#play-button").textContent = playing ? "■ Stop preview" : "▶ Play preview"; $("#play-button").classList.toggle("primary", !playing); log(playing ? "Preview mode started (editor preview only)." : "Preview mode stopped."); });
$("#clear-log").addEventListener("click", () => $("#activity-log").replaceChildren());
$("#agent-tools").addEventListener("click", async () => { try { const result = await api("/api/tools"); showResponse("Available agent tools:\n" + result.tools.map((tool) => `• ${tool.name}: ${tool.description}`).join("\n")); } catch (e) { showResponse(e.message); } });
(async () => {
  try { await refresh(); $("#connection-state").textContent = "LOCAL API CONNECTED"; $("#connection-state").style.color = "#a8f06a"; log("Connected to local engine API."); }
  catch (error) { $("#connection-state").textContent = "API OFFLINE"; showResponse(error.message); log(`Connection error: ${error.message}`); }
})();
