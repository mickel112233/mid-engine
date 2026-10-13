import test from "node:test";
import assert from "node:assert/strict";
import { createScene } from "../engine/scene.js";
import { executeTool, interpretCommand, TOOL_DEFINITIONS } from "../server/agent-tools.mjs";

function makeContext() {
  let scene = createScene();
  const events = [];
  return {
    getScene: () => scene,
    setScene: (next) => { scene = next; },
    getAssets: () => [],
    getEvents: () => events,
    addEvent: (type, message, data) => { const event = { type, message, data }; events.push(event); return event; }
  };
}
test("publishes discoverable tool definitions", () => {
  assert.ok(TOOL_DEFINITIONS.some((tool) => tool.name === "get_scene"));
  assert.ok(TOOL_DEFINITIONS.find((tool) => tool.name === "delete_entity").requiresApproval);
});
test("agent can add and inspect an entity", () => {
  const context = makeContext();
  const created = executeTool("add_entity", { type: "cube", name: "Agent Cube" }, context);
  assert.equal(created.entity.name, "Agent Cube");
  assert.equal(executeTool("get_scene", {}, context).scene.entities.length, 1);
});
test("delete requires an exact explicit confirmation", () => {
  const context = makeContext();
  const created = executeTool("add_entity", { type: "cube" }, context);
  assert.throws(() => executeTool("delete_entity", { id: created.entity.id }, context), { code: "approval_required" });
  const deleted = executeTool("delete_entity", { id: created.entity.id, confirm: `delete:${created.entity.id}` }, context);
  assert.equal(deleted.scene.entities.length, 0);
});
test("command interpreter supports add cube and camera", () => {
  const context = makeContext();
  assert.match(interpretCommand("Add a cube entity", context).message, /Created Cube/);
  assert.match(interpretCommand("Create a camera entity", context).message, /Created Camera/);
  assert.equal(context.getScene().entities.length, 2);
});
test("unsupported natural language fails explicitly rather than pretending success", () => {
  assert.throws(() => interpretCommand("make a beautiful forest with mountains", makeContext()), { code: "unsupported_command" });
});
