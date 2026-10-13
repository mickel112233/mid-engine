# AI Agent Integration

Mid Engine exposes a local, provider-neutral tool API. Any AI agent that can make HTTP requests and exchange JSON can discover the available tools and operate on the current scene. An LLM is not bundled: the built-in command box is a deterministic demonstration adapter, not a claim of autonomous language understanding.

## Connection

Start Mid Engine with `npm start`. The default server is `http://127.0.0.1:4173`.

- `GET /openapi.json` — OpenAPI 3.1 contract
- `GET /api/health` — readiness and version
- `GET /api/scene` — current scene and asset registry
- `GET /api/tools` — tool manifest and safety metadata
- `GET /api/events?limit=20` — recent activity
- `POST /api/tools/execute` — invoke a tool
- `POST /api/agent/command` — use the simple built-in command interpreter
- `POST /api/save` — validate and persist the scene

Example tool request:

```http
POST /api/tools/execute
Content-Type: application/json

{"name":"add_entity","args":{"name":"Hero","type":"cube"}}
```

Example response shape:

```json
{
  "ok": true,
  "entity": { "id": "generated-id", "name": "Hero", "type": "cube" },
  "scene": { "version": 1, "entities": [] },
  "event": { "type": "entity.created", "message": "Created cube: Hero" }
}
```

The response above is illustrative; actual IDs and timestamps are generated at runtime.

## Tool manifest

| Tool | Purpose | Destructive |
|---|---|---|
| `get_scene` | Read scene and entities | No |
| `add_entity` | Create empty, cube, camera, or light entity | No |
| `update_entity` | Update name, enabled state, or transform | No |
| `delete_entity` | Delete one entity | **Yes — explicit confirmation required** |
| `list_assets` | Read asset registry | No |
| `get_events` | Read recent activity | No |

To delete, the caller must supply `confirm: "delete:<entity-id>"`. A production agent UI should obtain that confirmation from the user; an agent must not silently invent consent.

## MCP-compatible clients

A dependency-free MCP stdio adapter is included at `server/mcp-stdio.mjs`. Configure an MCP client to launch `node` with the absolute path to that file; an example configuration is in `docs/mcp-config.example.json`. The adapter supports `initialize`, `ping`, `tools/list`, and `tools/call`, and delegates calls to the same validated tool registry as the HTTP API. Its standard output is reserved for JSON-RPC messages. It is a first adapter and should be exercised against the target MCP client before relying on it in production.

## Recommended agent loop

1. Discover tools via `GET /api/tools`.
2. Read current state with `get_scene`.
3. Propose a small, structured operation.
4. Execute the tool and inspect the returned scene and event.
5. Verify the intended change before reporting success.
6. Ask the user before destructive actions or broad project changes.

## Security boundary

- The server binds to `127.0.0.1` by default.
- The HTTP API exposes only named tools; it does not expose arbitrary shell execution or arbitrary file reads/writes.
- Scene mutations are schema-validated and saved under `project/scene.json`.
- Requests with a foreign Origin are rejected.
- Request bodies are limited to 64 KiB.
- Destructive deletion requires an explicit matching confirmation.

This is an early local-development boundary, not a hardened multi-user security system. Do not set `MID_ENGINE_HOST=0.0.0.0` or expose the port to a LAN or the internet. Before adding MCP or remote agents, implement authentication, per-tool capability grants, stronger origin/CSRF protections, approval records, rate limits, and audit retention. Treat agent-generated content as untrusted input.

## MCP direction

The editor now includes a basic WebGL viewport for cube-like scene entities with orbit and zoom controls; it is an early renderer, not yet a full asset/material renderer. The current tool definitions are intentionally transport-neutral and can be wrapped in an MCP server adapter. The adapter should map each manifest entry to a typed MCP tool, preserve the same validation and approval checks, and return structured content plus an error code. Keep the engine core independent from any specific model provider.
