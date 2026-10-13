import test from "node:test";
import assert from "node:assert/strict";
import { createMidEngineServer } from "../server/server.mjs";

test("local HTTP server exposes health, tools, and OpenAPI contract", async () => {
  const server = createMidEngineServer();
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address();
  const base = `http://127.0.0.1:${address.port}`;
  try {
    const health = await fetch(`${base}/api/health`);
    assert.equal(health.status, 200);
    assert.equal((await health.json()).ok, true);

    const tools = await fetch(`${base}/api/tools`);
    assert.equal(tools.status, 200);
    const manifest = await tools.json();
    assert.ok(manifest.tools.some((tool) => tool.name === "add_entity"));
    assert.ok(manifest.tools.some((tool) => tool.name === "delete_entity"));

    const spec = await fetch(`${base}/openapi.json`);
    assert.equal(spec.status, 200);
    assert.equal((await spec.json()).openapi, "3.1.0");

    const ui = await fetch(base);
    assert.equal(ui.status, 200);
    assert.match(await ui.text(), /Mid Engine/);
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
});

test("local API rejects a foreign Origin header", async () => {
  const server = createMidEngineServer();
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address();
  try {
    const response = await fetch(`http://127.0.0.1:${address.port}/api/tools`, {
      headers: { Origin: `http://127.0.0.1.evil:${address.port}` }
    });
    assert.equal(response.status, 403);
    assert.equal((await response.json()).error, "origin_not_allowed");
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
});
