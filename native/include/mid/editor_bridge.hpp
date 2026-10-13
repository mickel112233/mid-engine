#pragma once

#include "mid/rendering.hpp"
#include "mid/scene.hpp"

#include <unordered_map>
#include <vector>

namespace mid::editor {

struct RenderComponents {
    render::ResourceId mesh{0};
    render::ResourceId material{0};
    render::SphereBounds localBounds{};
    bool visible{true};
    std::vector<render::LodLevel> lods;
};

struct RenderSceneSnapshot {
    render::Camera camera{};
    render::DirectionalLight sun{};
    std::vector<render::Renderable> renderables;
};

// Adapter keeps editor scene ownership in mid::Scene while rendering consumes immutable snapshots.
class SceneRenderBridge final {
public:
    void setRenderComponents(EntityId entity, RenderComponents components);
    bool removeRenderComponents(EntityId entity);
    [[nodiscard]] RenderSceneSnapshot build(const Scene& scene, const render::Camera& camera,
                                             const render::DirectionalLight& sun) const;
    [[nodiscard]] std::array<float,16> modelMatrix(const Scene& scene,EntityId entity) const;
private:
    std::unordered_map<EntityId, RenderComponents> components_;
};

} // namespace mid::editor
