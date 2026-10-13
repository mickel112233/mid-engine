#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mid {

using EntityId = std::uint64_t;

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    friend bool operator==(const Vec3&, const Vec3&) = default;
};

struct Transform {
    Vec3 position{};
    Vec3 rotationDegrees{};
    Vec3 scale{1.0, 1.0, 1.0};

    friend bool operator==(const Transform&, const Transform&) = default;
};

struct Entity {
    EntityId id{0};
    std::string name{"Entity"};
    bool active{true};
    Transform transform{};
    std::optional<EntityId> parent{};
    std::vector<EntityId> children{};
};

class Scene final {
public:
    explicit Scene(std::string name = "Untitled Scene");

    [[nodiscard]] const std::string& name() const noexcept;
    void setName(std::string name);

    [[nodiscard]] EntityId createEntity(std::string name = "Entity");
    [[nodiscard]] bool destroyEntity(EntityId id);
    [[nodiscard]] bool contains(EntityId id) const noexcept;

    [[nodiscard]] Entity& get(EntityId id);
    [[nodiscard]] const Entity& get(EntityId id) const;
    [[nodiscard]] const std::unordered_map<EntityId, Entity>& entities() const noexcept;

    // Reparenting rejects missing entities, self-parenting, and hierarchy cycles.
    void setParent(EntityId child, std::optional<EntityId> parent);
    void setTransform(EntityId id, const Transform& transform);

    // Produces deterministic JSON sorted by entity ID; no third-party dependency required.
    [[nodiscard]] std::string toJson() const;

private:
    std::string name_;
    EntityId nextId_{1};
    std::unordered_map<EntityId, Entity> entities_;

    static void validateTransform(const Transform& transform);
    void detachFromParent(Entity& entity);
};

} // namespace mid
