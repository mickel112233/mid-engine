#pragma once
#include "mid/editor_bridge.hpp"
#include "mid/editor_session.hpp"
#include "mid/image_decoder.hpp"
#include "mid/native_editor_runtime.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#ifdef MID_ENGINE_WITH_SDL2
namespace mid::platform {
struct DesktopHostConfig { std::string title{"Mid Engine"}; int width{1280}; int height{720}; bool vsync{true}; };
class NativeDesktopHost final {
public:
 NativeDesktopHost(editor::EditorSession& session,editor::SceneRenderBridge& bridge,
  std::unordered_map<render::ResourceId,render::Material>& materials,
  render::Camera camera={},render::DirectionalLight light={},DesktopHostConfig config={});
 ~NativeDesktopHost();
 NativeDesktopHost(const NativeDesktopHost&)=delete;
 NativeDesktopHost& operator=(const NativeDesktopHost&)=delete;
 void registerRenderable(EntityId entity,render::ResourceId meshId,render::ResourceId materialId,
  const render::MeshData& mesh,const render::Material& material,render::SphereBounds bounds={});
 void registerTexture(render::ResourceId textureId,render::ResourceId materialId,const assets::ImageRgba8& image);
 void registerTextureFile(render::ResourceId textureId,render::ResourceId materialId,const std::string& path);
 [[nodiscard]] bool pumpEvents();
 [[nodiscard]] render::RenderStats frame(float elapsedSeconds);
 void setSelectedEntity(EntityId entity) noexcept;
 [[nodiscard]] bool shouldClose() const noexcept;
 [[nodiscard]] void* nativeWindow() const noexcept;
private:
 struct Impl;
 std::unique_ptr<Impl> impl_;
};
}
#endif
