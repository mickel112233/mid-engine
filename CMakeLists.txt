cmake_minimum_required(VERSION 3.24)
project(MidEngine VERSION 0.2.0 LANGUAGES CXX)

option(MID_ENGINE_BUILD_TESTS "Build Mid Engine native tests" ON)

add_library(mid_engine_core
    native/src/scene.cpp
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
endif()
