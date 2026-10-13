import { addEntity, removeEntity, updateEntity, validateScene } from "../engine/scene.js";

export const TOOL_DEFINITIONS = [
  { name: "get_scene", description: "Read the current scene and entity list.", input: {}, requiresApproval: false },
  { name: "add_entity", description: "Create an entity of type empty, cube, camera, or light.", input: { name: "string?", type: "string?" }, requiresApproval: false },
  { name: "update_entity", description: "Update an entity name, enabled state, or transform.", input: { id: "string", patch: "object" }, requiresApproval: false },
  { name: "delete_entity", description: "Delete an entity. Requires a matching explicit confirmation string.", input: { id: "string", confirm: "delete:<id>" }, requiresApproval: true },
  { name: "list_assets", description: "List registered project assets.", input: {}, requiresApproval: false },
  { name: "get_events", description: "Read recent engine activity events.", input: { limit: "number?" }, requiresApproval: false }
];

export function executeTool(name, args, context) {
  const { getScene, setScene, getAssets, getEvents, addEvent } = context;
  const scene = getScene();
  if (name === "get_scene") return { scene, assets: getAssets() };
  if (name === "list_assets") return { assets: getAssets() };
  if (name === "get_events") return { events: getEvents(Number(args.limit) || 50) };
  if (name === "add_entity") {
    const type = ["empty", "cube", "camera", "light"].includes(args.type) ? args.type : "empty";
    const result = addEntity(scene, { name: typeof args.name === "string" ? args.name : defaultName(type, scene), type });
    setScene(result.scene);
    const event = addEvent("entity.created", `Created ${result.entity.type}: ${result.entity.name}`, { entityId: result.entity.id });
    return { scene: result.scene, entity: result.entity, event };
  }
  if (name === "update_entity") {
    if (typeof args.id !== "string" || !args.patch || typeof args.patch !== "object" || Array.isArray(args.patch)) throw new TypeError("update_entity needs an id and a patch object.");
    const next = updateEntity(scene, args.id, args.patch);
    setScene(next);
    const event = addEvent("entity.updated", `Updated entity ${args.id}`, { entityId: args.id });
    return { scene: next, entity: next.entities.find((entity) => entity.id === args.id), event };
  }
  if (name === "delete_entity") {
    if (typeof args.id !== "string") throw new TypeError("delete_entity needs an entity id.");
    if (args.confirm !== `delete:${args.id}`) {
      const error = new Error("Explicit approval required. Ask the user to confirm deletion, then pass confirm: delete:<entity-id>.");
      error.code = "approval_required";
      throw error;
    }
    const next = removeEntity(scene, args.id);
    setScene(next);
    const event = addEvent("entity.deleted", `Deleted entity ${args.id}`, { entityId: args.id });
    return { scene: next, event };
  }
  throw Object.assign(new Error(`Unknown tool: ${name}`), { code: "unknown_tool" });
}

export function interpretCommand(command, context) {
  if (typeof command !== "string" || !command.trim()) throw new TypeError("A non-empty command is required.");
  const text = command.trim();
  const lower = text.toLowerCase();
  let type = null;
  if (/\b(cube|box)\b/.test(lower)) type = "cube";
  else if (/\b(camera)\b/.test(lower)) type = "camera";
  else if (/\b(light|lamp)\b/.test(lower)) type = "light";
  else if (/\b(empty|entity|object)\b/.test(lower)) type = "empty";
  if (/\b(add|create|make|insert)\b/.test(lower) && type) {
    const nameMatch = text.match(/\b(?:named|called)\s+["']?([^"']+)["']?$/i);
    const name = nameMatch?.[1]?.trim() || undefined;
    const result = executeTool("add_entity", { type, name }, context);
    return { ...result, message: `Created ${result.entity.name} (${type}).` };
  }
  const rename = text.match(/\brename\s+(.+?)\s+to\s+(.+)$/i);
  if (rename) {
    const scene = context.getScene();
    const targetName = rename[1].trim().replace(/^["']|["']$/g, "");
    const entity = scene.entities.find((item) => item.id === targetName || item.name.toLowerCase() === targetName.toLowerCase());
    if (!entity) throw new Error(`Could not find an entity named “${targetName}”. Select an entity by id or exact name.`);
    const result = executeTool("update_entity", { id: entity.id, patch: { name: rename[2].trim().replace(/^["']|["']$/g, "") } }, context);
    return { ...result, message: `Renamed entity to ${result.entity.name}.` };
  }
  throw Object.assign(new Error("This command is not supported by the built-in command interpreter yet. Compatible AI agents can call the documented scene tools directly."), { code: "unsupported_command" });
}

function defaultName(type, scene) {
  const base = ({ cube: "Cube", camera: "Camera", light: "Light", empty: "Entity" })[type] || "Entity";
  let number = 1;
  while (scene.entities.some((entity) => entity.name === (number === 1 ? base : `${base} ${number}`))) number++;
  return number === 1 ? base : `${base} ${number}`;
}
