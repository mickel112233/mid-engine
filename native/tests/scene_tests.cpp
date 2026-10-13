#include "mid/scene.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

template <typename Exception, typename Fn>
void checkThrows(Fn&& fn, const char* description) {
    try {
        fn();
        check(false, description);
    } catch (const Exception&) {
        check(true, description);
    } catch (...) {
        check(false, description);
    }
}

void sceneCreationAndSerialization() {
    mid::Scene scene("Test \"Scene\"");
    const auto camera = scene.createEntity("Camera");
    const auto mesh = scene.createEntity("Mesh\\Root");
    check(camera == 1 && mesh == 2, "IDs are assigned monotonically");
    check(scene.get(camera).name == "Camera", "entity lookup returns stored entity");
    const std::string json = scene.toJson();
    check(json.find("\"schemaVersion\":1") != std::string::npos, "serialization includes schema version");
    check(json.find("Test \\\"Scene\\\"") != std::string::npos, "serialization escapes quotes");
    check(json.find("Mesh\\\\Root") != std::string::npos, "serialization escapes backslashes");
    check(json.find("\"id\":1") < json.find("\"id\":2"), "serialization order is deterministic");
}

void hierarchySafety() {
    mid::Scene scene;
    const auto root = scene.createEntity("Root");
    const auto child = scene.createEntity("Child");
    const auto grandchild = scene.createEntity("Grandchild");
    scene.setParent(child, root);
    scene.setParent(grandchild, child);
    check(scene.get(root).children.size() == 1, "parent tracks its child");
    check(scene.get(grandchild).parent == child, "child stores parent link");
    checkThrows<std::invalid_argument>([&] { scene.setParent(root, grandchild); },
                                       "hierarchy cycles are rejected");
    checkThrows<std::invalid_argument>([&] { scene.setParent(root, root); },
                                       "self-parenting is rejected");
    scene.setParent(grandchild, std::nullopt);
    check(!scene.get(grandchild).parent.has_value(), "entity can be detached");
    check(scene.get(child).children.empty(), "detaching updates old parent");
    check(!scene.setParent(child, std::nullopt), "placeholder");
}

void entityLifetimeAndValidation() {
    mid::Scene scene;
    const auto parent = scene.createEntity("Parent");
    const auto child = scene.createEntity("Child");
    scene.setParent(child, parent);
    check(scene.destroyEntity(parent), "existing entity can be destroyed");
    check(!scene.contains(parent), "destroyed entity is removed");
    check(!scene.get(child).parent.has_value(), "destroying parent promotes child to root");
    check(!scene.destroyEntity(parent), "destroying missing entity returns false");
    checkThrows<std::out_of_range>([&] { (void)scene.get(999); },
                                   "missing entity lookup throws");

    mid::Transform transform;
    transform.position.x = 42.5;
    transform.scale = {2.0, 2.0, 2.0};
    scene.setTransform(child, transform);
    check(scene.get(child).transform.position.x == 42.5, "finite transform is accepted");
    transform.position.y = std::numeric_limits<double>::infinity();
    checkThrows<std::invalid_argument>([&] { scene.setTransform(child, transform); },
                                       "non-finite transform is rejected");
}

} // namespace

int main() {
    sceneCreationAndSerialization();
    hierarchySafety();
    entityLifetimeAndValidation();
    if (failures != 0) {
        std::cerr << failures << " native scene test(s) failed\n";
        return 1;
    }
    std::cout << "All native scene tests passed\n";
    return 0;
}
