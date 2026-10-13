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
