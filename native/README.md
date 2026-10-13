# Mid Engine Native Core

This directory starts the native C++20 engine alongside the existing JavaScript/WebGL editor. It is intentionally additive: the current editor and agent API remain usable while native systems grow.

## Build

Requires CMake 3.24+ and a C++20 compiler (Visual Studio 2022, Clang, or GCC).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Launch the native desktop starter

When CMake finds SDL2 development files, it also builds `mid_engine_desktop`. Run it from the build directory (Windows: `build/Release/mid_engine_desktop.exe`; single-configuration generators: `./build/mid_engine_desktop`). It opens a resizable OpenGL 3.3 window with a lit starter cube and the native editor controls. A desktop display and a working OpenGL 3.3 driver are required; CI verifies compilation/tests but does not assert real hardware rendering.

Controls: left-drag to orbit, click an object to select it, WASD/QE to move the camera, F5 to toggle isolated play mode, Ctrl+Z/Ctrl+Y to undo/redo, and G to enable axis-constrained translation (X/Y/Z select the axis). The native axis drag is intentionally minimal and is not a rendered 3D gizmo.

### Animation and physics debugging

`animation::Animator` exposes sampled local channels, world poses, active clip/time, and cross-fade progress via `debugSnapshot()`. It now supports `pause()`, `resume()`, and deterministic `step(deltaSeconds)` while paused, which can drive an editor step/resume panel. The physics rigid body has angular velocity, torque, angular damping, and integrated orientation; inertia is currently approximated as a solid sphere, and the contact solver still does not implement full angular contact impulses, friction, or general-purpose joint constraints. These are foundational APIs, not a production rigid-body solver or visual animation graph editor.

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
- `mid/physics.hpp`: spatial-hash sphere broadphase, six-pass sequential contact resolution, static plane/axis-aligned box/capsule colliders, spring-damper distance joints, and unilateral rope constraints. This is still not a full rigid-body solver; angular dynamics, convex collision, hinge/fixed joints, and robust manifold solving remain future work.
- `mid/animation.hpp`: hierarchical quaternion world-pose evaluation, quaternion pose blending, CPU vertex skinning, keyframe position/rotation/scale sampling, animation-debug snapshots (clip/time/local channels/world pose), and CCD IK with per-joint Euler limits and maximum angular steps. A full animation graph and quaternion-native constrained IK remain future work.
- `mid/image_decoder.hpp`: bounds-checked BMP decoding, optional libpng PNG decoding, optional libjpeg JPEG decoding, and a built-in Radiance RGBE HDR decoder that preserves linear floating-point RGBA values. The generic RGBA8 dispatcher tone-maps HDR for preview; HDR-aware render paths should consume `decodeHdr()` directly.
- `mid/model_importer.hpp`: optional Assimp-backed mesh/material/node-hierarchy extraction and position/rotation/scale animation-channel import for formats enabled in the installed Assimp build, including glTF/GLB and FBX. Joint scale is sampled and propagated through the native animation pose. Install Assimp development files and reconfigure CMake to enable it; embedded textures are now extracted into indexed imported-asset records (compressed payloads are preserved; uncompressed texels are converted to RGBA8). Material texture references remain available for asset registration. The regression suite includes a checked-in glTF fixture with an embedded PNG data URI; broader GLB/FBX asset coverage remains desirable.
- `mid/lua_runtime.hpp`: optional Lua script instances with start/update/stop callbacks, bounded memory and instruction counts, a restricted global environment, scene mutation functions, log capture, and line breakpoints. Lua is enabled only when CMake finds its development package; this is not a process-level security boundary or a full step/resume debugger.
- The WebGL editor viewport supports ANGLE instanced draws where available, optional depth-texture directional shadows, hemisphere ambient lighting, and asynchronous GPU timer-query profiling. These are viewport features; they do not yet constitute a native C++ GPU backend.

For a reproducible verification, use the current GitHub Actions run linked from the repository's Actions tab. Do not assume every optional dependency is present on every platform.

- `mid/opengl33_device.hpp`: a concrete native OpenGL 3.3 core renderer with host-loaded GL dispatch, GPU mesh/material registries, RGBA8 texture upload/binding, Cook-Torrance-style shading, and instanced draw batches. The host must create a current GL context and supply a complete function table. Optional native directional depth-map shadows and GPU elapsed-time queries activate when the host provides the corresponding GL entry points.

- `mid/native_editor_runtime.hpp`: selects the editor or play-mode scene, resolves hierarchical world matrices, builds a render plan, submits it through the native OpenGL device, and returns render/GPU timing stats. The optional SDL2 desktop host now owns window/context creation, viewport updates, basic keyboard/mouse input, and mesh/material/texture registration.

## Optional native desktop host

When SDL2 development files are available, CMake builds `mid/native_desktop_host.hpp` and the SDL2 host into `MidEngine::Core` (`MID_ENGINE_WITH_SDL2=1`). The host creates a resizable SDL window with an OpenGL 3.3 core context, loads the renderer's GL dispatch, keeps the viewport and camera aspect ratio synchronized on resize, handles close/play-mode/undo-redo/navigation input, and exposes mesh/material/texture registration helpers.

Minimal integration shape:

```cpp
mid::editor::EditorSession session;
mid::editor::SceneRenderBridge bridge;
std::unordered_map<mid::render::ResourceId, mid::render::Material> materials;
mid::platform::NativeDesktopHost host(session, bridge, materials);
const auto entity = session.editScene().createEntity("Triangle");
// Register a validated MeshData and Material with host.registerRenderable(...).
while (host.pumpEvents()) {
    host.frame(1.0F / 60.0F);
}
```

The native host supports viewport ray picking, mouse camera orbit, resize/DPI-aware pointer mapping, axis-constrained translation, F5 isolated play mode, and Ctrl+Z/Ctrl+Y undo/redo. It still does not draw a fully rendered 3D transform gizmo. Native window creation/rendering requires a working desktop display and OpenGL 3.3 driver, so headless CI can compile and test the host but cannot prove a real hardware-rendered frame.
