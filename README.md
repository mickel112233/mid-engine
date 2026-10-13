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
- WebGL preview metallic-roughness Cook-Torrance lighting, hemisphere ambient light, optional directional depth-texture shadows, ANGLE GPU instancing, CPU/GPU timer profiling, per-face normals, and reduced redundant geometry uploads
- Native renderer planning APIs for mesh/material batching, frustum culling, LOD selection, profiling, and deferred resource destruction
- Async asset cache with LRU memory budget and explicit load failure/eviction states; Wavefront OBJ/MTL import; BMP and optional PNG texture decoding; optional Assimp glTF/GLB/FBX mesh/material/node and position/rotation animation import with embedded-image RGBA8 decoding
- Native scene-to-renderer bridge and editor frame loop connected to the OpenGL device, transform gizmo operations with undo/redo and isolated play-mode copy, spatial-hash sphere broadphase with plane/box/capsule contacts, spring distance and rope constraints, hierarchical quaternion pose evaluation, timed Animator cross-fades/debug snapshots, CPU skinning, constrained CCD IK, atomic bounded command VM, optional sandboxed Lua lifecycle runtime, and CPU GGX PBR reference shading
- Persistent scene/entity model with transforms and JSON serialization
- Local HTTP API and OpenAPI contract for AI agents to inspect the project and perform allowlisted scene operations
- Dependency-free MCP stdio adapter for compatible agent clients
- Optional approval gate for operations marked destructive
- Event log and tool result envelopes for agent verification
- Node's built-in test runner; no third-party runtime dependencies

## Native C++ core

The native engine foundation is built with C++20 and CMake. The native slices provide stable entity IDs, transforms, a cycle-safe parent/child hierarchy, deterministic serialization, render planning/resource lifetime contracts, async asset caching, editor scene snapshots, simple rigid-body physics, keyframe animation/CCD IK, a bounded data-only script VM, and regression tests. The browser viewport now uses a concrete WebGL preview renderer with optional shadow maps, instancing, and GPU timing. The native C++ core now includes an OpenGL 3.3 render device with mesh/material registration, RGBA8 texture upload/binding, Cook-Torrance-style shading, and GPU-instanced batches; the host application must supply a current GL context and loaded function table. The native OpenGL device also has optional directional depth-map shadows and timer-query profiling when the host exposes those GL functions. The optional SDL2 desktop host creates a resizable OpenGL 3.3 window, supports viewport picking, mouse camera orbit, axis-constrained translation, resize-aware rendering, play mode, and undo/redo. `decodeImportedTexture()` converts embedded Assimp texture payloads to upload-ready RGBA8 data. CMake install rules and CI-generated ZIP SDK packages are available for both CI platforms. Production-grade glTF/FBX scene/animation import, a full angular rigid-body/joint solver, hardware/display-specific rendering verification, and a step/resume script debugger remain future work. Optional Assimp and Lua support require their development packages at configure time. See [native/README.md](native/README.md). GitHub Actions builds and tests this target alongside the existing JavaScript checks.

## Agent integration

Read [docs/AGENTS.md](docs/AGENTS.md) for the tool contract and safety model. An MCP-compatible client can launch `node server/mcp-stdio.mjs`; see [docs/mcp-config.example.json](docs/mcp-config.example.json). The HTTP OpenAPI contract is served at `/openapi.json`. The API is intended for local trusted integrations. Do not expose this server to a network or the public internet without adding authentication, origin controls, and a hardened permission model.

## Roadmap

1. **Foundation (in progress):** project persistence, scene/entity operations, agent tool API, native C++20 core, and regression tests.
2. **Rendering (in progress):** native OpenGL 3.3 device, GPU instancing, material texture binding, PBR shading, optional directional shadow maps, and GPU timing are implemented; production import/color-space coverage and backend integration with a desktop host remain.
3. **Interaction (in progress):** native axis/plane transform operations, snapshot undo/redo, native viewport picking/mouse orbit, browser on-object translation controls, browser undo/redo controls, and an isolated play preview are implemented; the native host's transform control is axis-constrained mouse dragging rather than a fully rendered in-viewport 3D gizmo.
4. **Runtime (in progress):** optional sandboxed Lua start/update/stop callbacks, instruction/memory budgets, line breakpoints, sphere/plane/box/capsule physics, distance/rope constraints, skeletal pose blending/skinning, and constrained CCD IK are implemented; a full deterministic gameplay runtime, production rigid-body solver, animation graph, and step/resume debugger remain.
5. **AI workflows:** MCP adapter, allowlisted tools, task plans, checkpoints, and artifact verification.
6. **Desktop:** native SDL2/OpenGL window/context host and native SDK packaging are available; finish full visual gizmo overlays, platform-specific UX polish, and end-to-end display testing.
7. **Production:** project migration, plugin API, broad platform tests, export targets, and security review.

Contributions should preserve the no-surprise rule: tool calls return structured results, filesystem access stays inside the project root, and risky actions require explicit approval.
