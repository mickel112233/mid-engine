export const SCENE_VERSION = 1;

export function createEntity({ id = crypto.randomUUID(), name = "Entity", type = "empty", transform = {} } = {}) {
  return {
    id,
    name: String(name).trim() || "Entity",
    type: String(type || "empty"),
    enabled: true,
    transform: {
      position: vector3(transform.position, [0, 0, 0]),
      rotation: vector3(transform.rotation, [0, 0, 0]),
      scale: vector3(transform.scale, [1, 1, 1])
    }
  };
}

function vector3(value, fallback) {
  if (!Array.isArray(value) || value.length !== 3) return [...fallback];
  return value.map((n, i) => Number.isFinite(Number(n)) ? Number(n) : fallback[i]);
}

export function createScene({ id = "main-scene", name = "Main Scene", entities = [] } = {}) {
  return {
    version: SCENE_VERSION,
    id: String(id),
    name: String(name),
    entities: entities.map((entity) => createEntity(entity)),
    updatedAt: new Date().toISOString()
  };
}

export function validateScene(input) {
  if (!input || typeof input !== "object" || Array.isArray(input)) throw new TypeError("Scene must be an object.");
  if (!Array.isArray(input.entities)) throw new TypeError("Scene entities must be an array.");
  const ids = new Set();
  for (const entity of input.entities) {
    if (!entity || typeof entity.id !== "string" || !entity.id.trim()) throw new TypeError("Every entity needs a non-empty string id.");
    if (ids.has(entity.id)) throw new TypeError(`Duplicate entity id: ${entity.id}`);
    ids.add(entity.id);
    if (typeof entity.name !== "string" || !entity.name.trim()) throw new TypeError(`Entity ${entity.id} needs a name.`);
    for (const key of ["position", "rotation", "scale"]) {
      const value = entity.transform?.[key];
      if (!Array.isArray(value) || value.length !== 3 || !value.every(Number.isFinite)) {
        throw new TypeError(`Entity ${entity.id} has an invalid transform.`);
      }
    }
  }
  return true;
}

export function addEntity(scene, options = {}) {
  const entity = createEntity(options);
  if (scene.entities.some((item) => item.id === entity.id)) throw new Error(`Entity id already exists: ${entity.id}`);
  const next = { ...scene, entities: [...scene.entities, entity], updatedAt: new Date().toISOString() };
  validateScene(next);
  return { scene: next, entity };
}

export function updateEntity(scene, id, patch = {}) {
  const existing = scene.entities.find((entity) => entity.id === id);
  if (!existing) throw new Error(`Entity not found: ${id}`);
  const allowed = {};
  if (typeof patch.name === "string") allowed.name = patch.name.trim() || existing.name;
  if (typeof patch.enabled === "boolean") allowed.enabled = patch.enabled;
  if (patch.transform && typeof patch.transform === "object") {
    allowed.transform = { ...existing.transform };
    for (const key of ["position", "rotation", "scale"]) {
      if (patch.transform[key] !== undefined) allowed.transform[key] = vector3(patch.transform[key], existing.transform[key]);
    }
  }
  const next = {
    ...scene,
    entities: scene.entities.map((entity) => entity.id === id ? { ...entity, ...allowed } : entity),
    updatedAt: new Date().toISOString()
  };
  validateScene(next);
  return next;
}

export function removeEntity(scene, id) {
  if (!scene.entities.some((entity) => entity.id === id)) throw new Error(`Entity not found: ${id}`);
  return { ...scene, entities: scene.entities.filter((entity) => entity.id !== id), updatedAt: new Date().toISOString() };
}
