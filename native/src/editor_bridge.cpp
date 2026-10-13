#include "mid/editor_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace mid::editor {
namespace {
using render::Vec3f;
Vec3f add(Vec3f a,Vec3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3f mul(Vec3f a,float s){return {a.x*s,a.y*s,a.z*s};}
Vec3f rotate(Vec3f v,Vec3f degrees) {
    constexpr float pi=3.14159265358979323846F;
    const float x=degrees.x*pi/180.0F,y=degrees.y*pi/180.0F,z=degrees.z*pi/180.0F;
    const float cx=std::cos(x),sx=std::sin(x),cy=std::cos(y),sy=std::sin(y),cz=std::cos(z),sz=std::sin(z);
    const Vec3f rx{v.x,v.y*cx-v.z*sx,v.y*sx+v.z*cx};
    const Vec3f ry{rx.x*cy+rx.z*sy,rx.y,-rx.x*sy+rx.z*cy};
    return {ry.x*cz-ry.y*sz,ry.x*sz+ry.y*cz,ry.z};
}
Vec3f asFloat(const Vec3& v){return {static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)};}
struct WorldTransform { Vec3f position{}; Vec3f scale{1,1,1}; Vec3f rotation{}; };
WorldTransform resolve(const Scene& scene,EntityId id,std::unordered_set<EntityId>& visiting) {
    if(!visiting.insert(id).second) throw std::logic_error("Cycle detected while resolving scene transforms");
    const Entity& e=scene.get(id);
    const auto localPos=asFloat(e.transform.position),localScale=asFloat(e.transform.scale),localRot=asFloat(e.transform.rotationDegrees);
    WorldTransform out{localPos,localScale,localRot};
    if(e.parent) {
        const auto parent=resolve(scene,*e.parent,visiting);
        const Vec3f scaled{localPos.x*parent.scale.x,localPos.y*parent.scale.y,localPos.z*parent.scale.z};
        out.position=add(parent.position,rotate(scaled,parent.rotation));
        out.scale={parent.scale.x*localScale.x,parent.scale.y*localScale.y,parent.scale.z*localScale.z};
        out.rotation=add(parent.rotation,localRot);
    }
    visiting.erase(id);
    return out;
}
}
void SceneRenderBridge::setRenderComponents(EntityId entity,RenderComponents components) {
    if(components.mesh==0 || components.material==0) throw std::invalid_argument("Renderable components require non-zero mesh and material IDs");
    if(!std::isfinite(components.localBounds.radius)||components.localBounds.radius<0.0F) throw std::invalid_argument("Renderable bounds are invalid");
    components_[entity]=std::move(components);
}
bool SceneRenderBridge::removeRenderComponents(EntityId entity){return components_.erase(entity)!=0;}
RenderSceneSnapshot SceneRenderBridge::build(const Scene& scene,const render::Camera& camera,const render::DirectionalLight& sun) const {
    camera.validate();
    RenderSceneSnapshot snapshot{camera,sun,{}};
    snapshot.renderables.reserve(components_.size());
    for(const auto& [id,components]:components_) {
        if(!scene.contains(id)) continue;
        const Entity& entity=scene.get(id);
        if(!entity.active||!components.visible) continue;
        std::unordered_set<EntityId> visiting;
        const auto world=resolve(scene,id,visiting);
        const auto local=components.localBounds.center;
        const Vec3f scaled{local.x*world.scale.x,local.y*world.scale.y,local.z*world.scale.z};
        const Vec3f center=add(world.position,rotate(scaled,world.rotation));
        const float maxScale=std::max({std::abs(world.scale.x),std::abs(world.scale.y),std::abs(world.scale.z)});
        snapshot.renderables.push_back({id,components.mesh,components.material,{center,components.localBounds.radius*maxScale},true,components.lods});
    }
    return snapshot;
}
} // namespace mid::editor
