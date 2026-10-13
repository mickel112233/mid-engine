# Mid Engine

**Mid Engine** is an agent-friendly game-engine foundation: a local-first editor, a serializable scene model, and a permission-aware HTTP tool API for compatible AI agents.

> This repository is an early engine foundation, not yet a replacement for Unreal Engine. It now contains an additive native C++20 scene core alongside the browser editor. Advanced rendering, physics, animation, scripting, packaging, and a native desktop shell remain staged work.

## Requirements

- Node.js 20 or newer for the existing editor/API
- CMake 3.24+ and a C++20 compiler for the native core
- No third-party runtime dependency is required for the current native scene slice

## Run

```bash
npm start
```

Open [http://127.0.0.1:4173](http://127.0.0.1:4173). The server binds to loopback by default and creates `project/scene.json` on first launch.

Run checks:

```bash
npm test
npm run check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## First milestone

- Editor layout with scene tree, interactive WebGL viewport, inspector, asset list, and agent command panel
- Basic 3D cube geometry, perspective camera, grid, mouse orbit, and wheel zoom
- WebGL preview directional diffuse lighting with per-face normals and reduced redundant geometry uploads
- Native renderer planning APIs for mesh/material batching, frustum culling, LOD selection, profiling, and deferred resource destruction
- Async asset cache with LRU memory budget, explicit load failure/eviction states, and a dependency-free Wavefront OBJ importer
- Native scene-to-renderer bridge, fixed-step sphere physics, keyframe animation, CCD IK, and a bounded data-only script VM
- Persistent scene/entity model with transforms and JSON serialization
- Local HTTP API and OpenAPI contract for AI agents to inspect the project and perform allowlisted scene operations
- Dependency-free MCP stdio adapter for compatible agent clients
- Optional approval gate for operations marked destructive
- Event log and tool result envelopes for agent verification
- Node's built-in test runner; no third-party runtime dependencies

## Native C++ core

The native engine foundation is built with C++20 and CMake. The native slices provide stable entity IDs, transforms, a cycle-safe parent/child hierarchy, deterministic serialization, render planning/resource lifetime contracts, async asset caching, editor scene snapshots, simple rigid-body physics, keyframe animation/CCD IK, a bounded data-only script VM, and regression tests. The native renderer remains backend-neutral; a production GPU backend and complete PBR pipeline are still future work. See [native/README.md](native/README.md). GitHub Actions builds and tests this target alongside the existing JavaScript checks.

## Agent integration

Read [docs/AGENTS.md](docs/AGENTS.md) for the tool contract and safety model. An MCP-compatible client can launch `node server/mcp-stdio.mjs`; see [docs/mcp-config.example.json](docs/mcp-config.example.json). The HTTP OpenAPI contract is served at `/openapi.json`. The API is intended for local trusted integrations. Do not expose this server to a network or the public internet without adding authentication, origin controls, and a hardened permission model.

## Roadmap

1. **Foundation (in progress):** project persistence, editor shell, scene/entity operations, agent tool API, native C++20 scene core, and tests.
2. **Rendering:** native render-device abstraction, GPU backend, materials, lighting, meshes, asset import, camera entities, and scalable rendering paths.
3. **Interaction:** input mapping, gizmos, selection, undo/redo, play mode.
4. **Runtime:** scripting sandbox, deterministic update loop, physics integration.
5. **AI workflows:** MCP adapter, provider-agnostic agent bridge, task plans, checkpoints, and artifact verification.
6. **Desktop:** package the editor in a native desktop shell; keep the web preview usable independently.
7. **Production:** project migration, plugin API, profiling, builds, export targets, security review.

Contributions should preserve the no-surprise rule: tool calls return structured results, filesystem access stays inside the project root, and risky actions require explicit approval.
