import test from "node:test";
import assert from "node:assert/strict";
import { addEntity, createEntity, createScene, removeEntity, updateEntity, validateScene } from "../engine/scene.js";

test("creates a valid empty scene", () => {
  const scene = createScene();
  assert.equal(scene.entities.length, 0);
  assert.equal(validateScene(scene), true);
});
test("adds entity with safe default transforms", () => {
  const { scene, entity } = addEntity(createScene(), { id: "cube-1", name: "Cube", type: "cube" });
  assert.equal(scene.entities.length, 1);
  assert.deepEqual(entity.transform.position, [0, 0, 0]);
  assert.deepEqual(entity.transform.scale, [1, 1, 1]);
});
test("rejects duplicate ids", () => {
  const scene = createScene({ entities: [createEntity({ id: "same" })] });
  assert.throws(() => addEntity(scene, { id: "same" }), /already exists/);
});
test("updates only allowed properties and validates transforms", () => {
  const { scene, entity } = addEntity(createScene(), { id: "e1", name: "Before" });
  const next = updateEntity(scene, entity.id, { name: "After", transform: { position: [1, 2, 3] }, injected: true });
  assert.equal(next.entities[0].name, "After");
  assert.deepEqual(next.entities[0].transform.position, [1, 2, 3]);
  assert.equal(next.entities[0].injected, undefined);
});
test("rejects duplicate ids and malformed transforms in serialized scenes", () => {
  assert.throws(() => validateScene({ entities: [{ id: "x", name: "X", transform: {} }] }), /invalid transform/);
  assert.throws(() => validateScene({ entities: [{ id: "x", name: "X", transform: { position: [0,0,0], rotation: [0,0,0], scale: [1,1,1] } }, { id: "x", name: "Y", transform: { position: [0,0,0], rotation: [0,0,0], scale: [1,1,1] } }] }), /Duplicate entity id/);
});
test("removes known entities and rejects unknown ids", () => {
  const { scene, entity } = addEntity(createScene(), { id: "gone" });
  assert.equal(removeEntity(scene, entity.id).entities.length, 0);
  assert.throws(() => removeEntity(scene, "missing"), /not found/);
});
