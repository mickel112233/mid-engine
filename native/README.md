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


## Expanded systems (experimental, build-tested on CI where noted)

- `mid/editor_session.hpp`: constrained axis/plane transform edits, scene-snapshot undo/redo, and an isolated play-mode copy.
- `mid/physics.hpp`: spatial-hash sphere broadphase, static plane/axis-aligned box colliders, and spring-damper distance joints. This is not a full rigid-body solver; angular dynamics, convex collision, hinge/fixed joints, and robust manifold solving remain future work.
- `mid/animation.hpp`: hierarchical quaternion world-pose evaluation, quaternion pose blending, CPU vertex skinning, keyframe position/rotation sampling, and CCD IK. Rotation-limit-aware IK and a full animation graph remain future work.
- `mid/image_decoder.hpp`: bounds-checked uncompressed 24/32-bit BMP decoding to RGBA8. PNG/JPEG/HDR decoding and GPU texture binding are not yet provided by this decoder.
- `mid/model_importer.hpp`: optional Assimp-backed mesh/material extraction for formats enabled in the installed Assimp build, including glTF/GLB and FBX. Install Assimp development files and reconfigure CMake to enable it; embedded texture extraction and complete node/animation import are still incomplete.
- `mid/lua_runtime.hpp`: optional Lua script instances with start/update/stop callbacks, bounded memory and instruction counts, a restricted global environment, scene mutation functions, log capture, and line breakpoints. Lua is enabled only when CMake finds its development package; this is not a process-level security boundary or a full step/resume debugger.
- The WebGL editor viewport supports ANGLE instanced draws where available, optional depth-texture directional shadows, hemisphere ambient lighting, and asynchronous GPU timer-query profiling. These are viewport features; they do not yet constitute a native C++ GPU backend.

For a reproducible verification, use the current GitHub Actions run linked from the repository's Actions tab. Do not assume every optional dependency is present on every platform.

- `mid/opengl33_device.hpp`: a concrete native OpenGL 3.3 core renderer with host-loaded GL dispatch, GPU mesh/material registries, RGBA8 texture upload/binding, Cook-Torrance-style shading, and instanced draw batches. The host must create a current GL context and supply a complete function table. The browser viewport's shadow-map path is separate; native shadow maps and GPU timer-query profiling are not yet wired into this backend.
