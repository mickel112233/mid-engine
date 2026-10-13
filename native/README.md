# Mid Engine Native Core

This directory starts the native C++20 engine alongside the existing JavaScript/WebGL editor. It is intentionally additive: the current editor and agent API remain usable while native systems grow.

## Build

Requires CMake 3.24+ and a C++20 compiler (Visual Studio 2022, Clang, or GCC).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Current native slice

- Stable, monotonically assigned entity IDs.
- Entity names, active state, transforms, parent/child hierarchy.
- Cycle-safe reparenting and explicit missing-entity errors.
- Finite-number validation for transform values.
- Deterministic JSON serialization with string escaping.
- Dependency-free C++20 build and regression tests.

## Engineering direction

Keep core runtime systems independent of browser APIs. Future modules should build on this library in separate, testable targets: math, world/scene, platform, render abstraction, asset pipeline, physics, animation, scripting, and editor bridge. A production renderer and Unreal-scale feature parity require substantial additional implementation and hardware validation; this initial slice does not claim those capabilities yet.


## Native subsystems added

- `mid/rendering.hpp`: validated mesh/material/camera data, conservative frustum testing, transparent/opaque render planning, mesh/material batching (instancing candidates), distance-based LOD selection, planning timings, a backend-neutral render-device contract, and frame-fence-based deferred GPU resource destruction.
- `mid/asset_manager.hpp`: asynchronous file loading, normalized-path deduplication, failure states, snapshots, and a configurable least-recently-used resident-memory budget.
- `mid/obj_importer.hpp`: dependency-free Wavefront OBJ parsing with polygon triangulation, relative indices, UVs/normals, generated normals, and calculated bounds.
- `mid/editor_bridge.hpp`: converts the authoritative native `Scene` plus per-entity render components into an immutable renderer snapshot, resolving parent transforms.
- `mid/physics.hpp`: fixed-step sphere rigid-body integration, force accumulation, basic contacts, restitution, and bounded catch-up steps.
- `mid/animation.hpp`: keyframe position sampling and a CCD inverse-kinematics solver.
- `mid/script_runtime.hpp`: a data-only, instruction-budgeted command VM that can mutate scene transforms/active state without arbitrary native calls, filesystem access, or unbounded instruction loops.

These are production-oriented interfaces and testable foundations, not a complete GPU backend or a claim of feature parity with Unreal. In particular, the renderer still needs a concrete graphics backend, robust broadphase (BVH/spatial hash), full PBR/shadow/IBL pipeline, production image/material decoders, skeleton-local transform propagation, and a full scripting language integration.


Additional native modules: `mid/pbr.hpp` provides a CPU Cook-Torrance/GGX shading reference, and `mid/mtl_importer.hpp` imports Wavefront MTL diffuse colors, opacity, shininess-derived roughness, and base-color texture references. The physics world now uses a spatial-hash broadphase with candidate/test counters and supports static plane colliders. Animation clips sample both position and Euler rotation tracks; script execution stages scene changes and commits only when the program completes successfully.
