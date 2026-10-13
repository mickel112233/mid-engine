cmake_minimum_required(VERSION 3.24)
project(MidEngine VERSION 0.2.0 LANGUAGES CXX)

option(MID_ENGINE_BUILD_TESTS "Build Mid Engine native tests" ON)

add_library(mid_engine_core
    native/src/scene.cpp
    native/src/rendering.cpp
    native/src/asset_manager.cpp
    native/src/editor_bridge.cpp
    native/src/physics.cpp
    native/src/animation.cpp
    native/src/script_runtime.cpp
)
add_library(MidEngine::Core ALIAS mid_engine_core)
target_compile_features(mid_engine_core PUBLIC cxx_std_20)
target_include_directories(mid_engine_core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/native/include>
)
set_target_properties(mid_engine_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
if(MSVC)
    target_compile_options(mid_engine_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(mid_engine_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(MID_ENGINE_BUILD_TESTS)
    enable_testing()
    add_executable(mid_engine_scene_tests native/tests/scene_tests.cpp)
    target_link_libraries(mid_engine_scene_tests PRIVATE MidEngine::Core)
    target_compile_features(mid_engine_scene_tests PRIVATE cxx_std_20)
    add_test(NAME mid_engine.scene COMMAND mid_engine_scene_tests)
    add_executable(mid_engine_rendering_tests native/tests/rendering_tests.cpp)
    target_link_libraries(mid_engine_rendering_tests PRIVATE MidEngine::Core)
    target_compile_features(mid_engine_rendering_tests PRIVATE cxx_std_20)
    add_test(NAME mid_engine.rendering COMMAND mid_engine_rendering_tests)
    add_executable(mid_engine_asset_manager_tests native/tests/asset_manager_tests.cpp)
    target_link_libraries(mid_engine_asset_manager_tests PRIVATE MidEngine::Core)
    target_compile_features(mid_engine_asset_manager_tests PRIVATE cxx_std_20)
    add_test(NAME mid_engine.assets COMMAND mid_engine_asset_manager_tests)
    add_executable(mid_engine_physics_tests native/tests/physics_tests.cpp)
    target_link_libraries(mid_engine_physics_tests PRIVATE MidEngine::Core)
    target_compile_features(mid_engine_physics_tests PRIVATE cxx_std_20)
    add_test(NAME mid_engine.physics COMMAND mid_engine_physics_tests)
    add_executable(mid_engine_animation_script_tests native/tests/animation_script_tests.cpp)
    target_link_libraries(mid_engine_animation_script_tests PRIVATE MidEngine::Core)
    target_compile_features(mid_engine_animation_script_tests PRIVATE cxx_std_20)
    add_test(NAME mid_engine.animation_script COMMAND mid_engine_animation_script_tests)
endif()
