# Mid Engine

**Mid Engine** is an agent-friendly game-engine foundation: a local-first editor, a serializable scene model, and a permission-aware HTTP tool API for compatible AI agents.

> This repository is an early foundation, not yet a replacement for Unreal Engine. The editor and scene tools are the first working milestone; advanced 3D rendering, physics, scripting, packaging, and a native desktop shell are staged roadmap items.

## Requirements

- Node.js 20 or newer
- No runtime npm dependencies

## Run

```bash
npm start
```

Open [http://127.0.0.1:4173](http://127.0.0.1:4173). The server binds to loopback by default and creates `project/scene.json` on first launch.

Run checks:

```bash
npm test
npm run check
```

## First milestone

- Editor layout with scene tree, interactive WebGL viewport, inspector, asset list, and agent command panel
- Basic 3D cube geometry, perspective camera, grid, mouse orbit, and wheel zoom
- Persistent scene/entity model with transforms and JSON serialization
- Local HTTP API and OpenAPI contract for AI agents to inspect the project and perform allowlisted scene operations
- Dependency-free MCP stdio adapter for compatible agent clients
- Optional approval gate for operations marked destructive
- Event log and tool result envelopes for agent verification
- Node's built-in test runner; no third-party runtime dependencies

## Agent integration

Read [docs/AGENTS.md](docs/AGENTS.md) for the tool contract and safety model. An MCP-compatible client can launch `node server/mcp-stdio.mjs`; see [docs/mcp-config.example.json](docs/mcp-config.example.json). The HTTP OpenAPI contract is served at `/openapi.json`. The API is intended for local trusted integrations. Do not expose this server to a network or the public internet without adding authentication, origin controls, and a hardened permission model.

## Roadmap

1. **Foundation (current):** project persistence, editor shell, scene/entity operations, agent tool API, tests.
2. **Rendering:** expand the WebGL foundation into materials, lighting, meshes, asset import, camera entities, and WebGPU options.
3. **Interaction:** input mapping, gizmos, selection, undo/redo, play mode.
4. **Runtime:** scripting sandbox, deterministic update loop, physics integration.
5. **AI workflows:** MCP adapter, provider-agnostic agent bridge, task plans, checkpoints, and artifact verification.
6. **Desktop:** package the editor in a native desktop shell; keep the web preview usable independently.
7. **Production:** project migration, plugin API, profiling, builds, export targets, security review.

Contributions should preserve the no-surprise rule: tool calls return structured results, filesystem access stays inside the project root, and risky actions require explicit approval.
