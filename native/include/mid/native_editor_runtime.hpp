#pragma once
#include "mid/editor_bridge.hpp"
#include "mid/editor_session.hpp"
#include "mid/opengl33_device.hpp"
#include <unordered_map>
namespace mid::editor {
// Native frame loop selects the authoring scene or isolated play-mode scene,
// builds the render snapshot, uploads entity transforms, and submits GPU batches.
class NativeEditorRuntime final {
public:
 NativeEditorRuntime(EditorSession& session,SceneRenderBridge& bridge,render::gl33::OpenGL33Device& device,
                     const std::unordered_map<render::ResourceId,render::Material>& materials,
                     render::Camera camera={},render::DirectionalLight light={});
 void setCamera(render::Camera camera);
 void setLight(render::DirectionalLight light);
 [[nodiscard]] render::RenderStats renderFrame();
 [[nodiscard]] const render::RenderStats& lastStats() const noexcept;
private:
 EditorSession* session_; SceneRenderBridge* bridge_; render::gl33::OpenGL33Device* device_;
 const std::unordered_map<render::ResourceId,render::Material>* materials_;
 render::RenderPlanner planner_; render::Camera camera_; render::DirectionalLight light_; render::RenderStats stats_{};
};
}
