#include "mid/native_editor_runtime.hpp"
#include <stdexcept>
#include <cmath>
#include <utility>
namespace mid::editor {
NativeEditorRuntime::NativeEditorRuntime(EditorSession& session,SceneRenderBridge& bridge,render::gl33::OpenGL33Device& device,
 const std::unordered_map<render::ResourceId,render::Material>& materials,render::Camera camera,render::DirectionalLight light)
 :session_(&session),bridge_(&bridge),device_(&device),materials_(&materials),camera_(camera),light_(light){camera_.validate();setLight(light);}
void NativeEditorRuntime::setCamera(render::Camera camera){camera.validate();camera_=camera;}
void NativeEditorRuntime::setLight(render::DirectionalLight light){
 if(!std::isfinite(light.direction.x)||!std::isfinite(light.direction.y)||!std::isfinite(light.direction.z)||
    !std::isfinite(light.color.x)||!std::isfinite(light.color.y)||!std::isfinite(light.color.z)||
    !std::isfinite(light.intensity)||light.intensity<0.0F||light.direction.x*light.direction.x+light.direction.y*light.direction.y+light.direction.z*light.direction.z<1.0e-8F||light.color.x<0.0F||light.color.y<0.0F||light.color.z<0.0F)throw std::invalid_argument("Directional light values are invalid");
 light_=light;
}
render::RenderStats NativeEditorRuntime::renderFrame(){
 const Scene* runtime=session_->runtimeScene();const Scene& active=(session_->playing()&&runtime)?*runtime:session_->editScene();
 const auto snapshot=bridge_->build(active,camera_,light_);
 for(const auto& renderable:snapshot.renderables)device_->setEntityTransform(renderable.entityId,bridge_->modelMatrix(active,static_cast<EntityId>(renderable.entityId)));
 const auto plan=planner_.build(camera_,snapshot.renderables,*materials_);
 device_->beginFrame();
 try{device_->submit(plan,camera_,light_);device_->endFrame();}
 catch(...){try{device_->endFrame();}catch(...){}throw;}
 stats_=device_->lastStats();return stats_;
}
const render::RenderStats& NativeEditorRuntime::lastStats()const noexcept{return stats_;}
}
