#include "mid/native_desktop_host.hpp"
#ifdef MID_ENGINE_WITH_SDL2
#include <SDL.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace {
using namespace mid;
using namespace mid::render;

void addFace(MeshData& mesh,Vec3f normal,Vec3f a,Vec3f b,Vec3f c,Vec3f d) {
    const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({a,normal,{0,0},{1,1,1,1}});
    mesh.vertices.push_back({b,normal,{1,0},{1,1,1,1}});
    mesh.vertices.push_back({c,normal,{1,1},{1,1,1,1}});
    mesh.vertices.push_back({d,normal,{0,1},{1,1,1,1}});
    mesh.indices.insert(mesh.indices.end(),{base,base+1U,base+2U,base,base+2U,base+3U});
}
MeshData makeCube() {
    MeshData mesh;mesh.name="StarterCube";
    addFace(mesh,{0,0,1},{-0.5F,-0.5F,0.5F},{0.5F,-0.5F,0.5F},{0.5F,0.5F,0.5F},{-0.5F,0.5F,0.5F});
    addFace(mesh,{0,0,-1},{0.5F,-0.5F,-0.5F},{-0.5F,-0.5F,-0.5F},{-0.5F,0.5F,-0.5F},{0.5F,0.5F,-0.5F});
    addFace(mesh,{1,0,0},{0.5F,-0.5F,0.5F},{0.5F,-0.5F,-0.5F},{0.5F,0.5F,-0.5F},{0.5F,0.5F,0.5F});
    addFace(mesh,{-1,0,0},{-0.5F,-0.5F,-0.5F},{-0.5F,-0.5F,0.5F},{-0.5F,0.5F,0.5F},{-0.5F,0.5F,-0.5F});
    addFace(mesh,{0,1,0},{-0.5F,0.5F,0.5F},{0.5F,0.5F,0.5F},{0.5F,0.5F,-0.5F},{-0.5F,0.5F,-0.5F});
    addFace(mesh,{0,-1,0},{-0.5F,-0.5F,-0.5F},{0.5F,-0.5F,-0.5F},{0.5F,-0.5F,0.5F},{-0.5F,-0.5F,0.5F});
    mesh.bounds={{0,0,0},0.8660254F};mesh.validate();return mesh;
}
}

int main() {
    try {
        Scene scene("Starter Scene");
        const EntityId cube=scene.createEntity("Cube");
        editor::EditorSession session(scene);
        editor::SceneRenderBridge bridge;
        std::unordered_map<ResourceId,Material> materials;
        platform::NativeDesktopHost host(session,bridge,materials,Camera{},DirectionalLight{},
                                         {"Mid Engine - Native Starter",1280,720,true});
        Material material;material.name="Starter Material";material.baseColor={0.22F,0.62F,0.92F,1.0F};
        material.metallic=0.05F;material.roughness=0.42F;
        host.registerRenderable(cube,1,1,makeCube(),material,{{0,0,0},0.8660254F});
        host.setSelectedEntity(cube);
        auto previous=std::chrono::steady_clock::now();
        while(!host.shouldClose()) {
            (void)host.pumpEvents();
            const auto now=std::chrono::steady_clock::now();
            const float elapsed=std::chrono::duration<float>(now-previous).count();previous=now;
            (void)host.frame(elapsed);
            SDL_Delay(1);
        }
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Mid Engine native desktop failed: "<<error.what()<<'\n';
        return 1;
    }
}
#endif
