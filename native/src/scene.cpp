#include "mid/scene.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace mid {
namespace {

std::string escapeJson(const std::string& input) {
    std::ostringstream out;
    for (const unsigned char ch : input) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

void writeVec3(std::ostringstream& out, const Vec3& value) {
    out << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

} // namespace

Scene::Scene(std::string name) : name_(std::move(name)) {}

const std::string& Scene::name() const noexcept { return name_; }

void Scene::setName(std::string name) { name_ = std::move(name); }

EntityId Scene::createEntity(std::string name) {
    if (nextId_ == 0 || nextId_ == std::numeric_limits<EntityId>::max()) {
        throw std::overflow_error("Scene entity ID space exhausted");
    }
    const EntityId id = nextId_++;
    Entity entity;
    entity.id = id;
    entity.name = std::move(name);
    entities_.emplace(id, std::move(entity));
    return id;
}

bool Scene::destroyEntity(EntityId id) {
    const auto it = entities_.find(id);
    if (it == entities_.end()) {
        return false;
    }

    Entity& entity = it->second;
    detachFromParent(entity);

    // Preserve descendants by promoting them to roots rather than deleting them implicitly.
    for (const EntityId childId : entity.children) {
        const auto childIt = entities_.find(childId);
        if (childIt != entities_.end()) {
            childIt->second.parent.reset();
        }
    }
    entities_.erase(it);
    return true;
}

bool Scene::contains(EntityId id) const noexcept {
    return entities_.contains(id);
}

Entity& Scene::get(EntityId id) {
    const auto it = entities_.find(id);
    if (it == entities_.end()) {
        throw std::out_of_range("Scene entity does not exist");
    }
    return it->second;
}

const Entity& Scene::get(EntityId id) const {
    const auto it = entities_.find(id);
    if (it == entities_.end()) {
        throw std::out_of_range("Scene entity does not exist");
    }
    return it->second;
}

const std::unordered_map<EntityId, Entity>& Scene::entities() const noexcept {
    return entities_;
}

void Scene::detachFromParent(Entity& entity) {
    if (!entity.parent.has_value()) {
        return;
    }
    const auto parentIt = entities_.find(*entity.parent);
    if (parentIt != entities_.end()) {
        auto& siblings = parentIt->second.children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), entity.id), siblings.end());
    }
    entity.parent.reset();
}

void Scene::setParent(EntityId childId, std::optional<EntityId> parentId) {
    Entity& child = get(childId);
    if (!parentId.has_value()) {
        detachFromParent(child);
        return;
    }
    if (*parentId == childId) {
        throw std::invalid_argument("An entity cannot be its own parent");
    }

    Entity& parent = get(*parentId);
    EntityId cursor = parent.id;
    while (true) {
        if (cursor == childId) {
            throw std::invalid_argument("Reparenting would create a scene hierarchy cycle");
        }
        const Entity& ancestor = get(cursor);
        if (!ancestor.parent.has_value()) {
            break;
        }
        cursor = *ancestor.parent;
    }

    if (child.parent == parentId) {
        return;
    }
    detachFromParent(child);
    child.parent = parentId;
    parent.children.push_back(childId);
}

void Scene::validateTransform(const Transform& transform) {
    const double values[] = {
        transform.position.x, transform.position.y, transform.position.z,
        transform.rotationDegrees.x, transform.rotationDegrees.y, transform.rotationDegrees.z,
        transform.scale.x, transform.scale.y, transform.scale.z
    };
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("Transform values must be finite numbers");
        }
    }
}

void Scene::setTransform(EntityId id, const Transform& transform) {
    validateTransform(transform);
    get(id).transform = transform;
}

std::string Scene::toJson() const {
    std::vector<EntityId> ids;
    ids.reserve(entities_.size());
    for (const auto& [id, entity] : entities_) {
        (void)entity;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "{\"schemaVersion\":1,\"name\":\"" << escapeJson(name_) << "\",\"entities\":[";
    bool first = true;
    for (const EntityId id : ids) {
        const Entity& entity = entities_.at(id);
        if (!first) {
            out << ',';
        }
        first = false;
        out << "{\"id\":" << entity.id
            << ",\"name\":\"" << escapeJson(entity.name) << "\""
            << ",\"active\":" << (entity.active ? "true" : "false")
            << ",\"position\":";
        writeVec3(out, entity.transform.position);
        out << ",\"rotationDegrees\":";
        writeVec3(out, entity.transform.rotationDegrees);
        out << ",\"scale\":";
        writeVec3(out, entity.transform.scale);
        out << ",\"parent\":";
        if (entity.parent.has_value()) {
            out << *entity.parent;
        } else {
            out << "null";
        }
        out << ",\"children\":[";
        for (std::size_t i = 0; i < entity.children.size(); ++i) {
            if (i != 0) out << ',';
            out << entity.children[i];
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

} // namespace mid
