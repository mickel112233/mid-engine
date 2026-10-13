#pragma once

#include "mid/rendering.hpp"

#include <cstdint>
#include <vector>

namespace mid::physics {
using namespace mid::render;

struct Aabb { Vec3f minimum{}; Vec3f maximum{}; };
struct PlaneCollider { Vec3f normal{0,1,0}; float offset{0}; float restitution{0}; };
struct RigidBody {
    std::uint64_t entityId{0};
    Vec3f position{};
    Vec3f velocity{};
    Vec3f force{};
    float inverseMass{1.0F};
    float restitution{0.0F};
    float linearDamping{0.02F};
    float radius{0.5F};
    bool kinematic{false};
    bool sleeping{false};
};
struct PhysicsStats { std::size_t bodies{0}; std::size_t broadphaseCandidates{0}; std::size_t narrowphaseTests{0}; std::size_t contacts{0}; std::size_t fixedSteps{0}; };
struct Contact { std::uint64_t a{0}; std::uint64_t b{0}; Vec3f normal{}; float penetration{0.0F}; };

class PhysicsWorld final {
public:
    explicit PhysicsWorld(Vec3f gravity={0.0F,-9.81F,0.0F},float fixedStep=1.0F/60.0F);
    void addBody(RigidBody body);
    bool removeBody(std::uint64_t entityId);
    [[nodiscard]] RigidBody* body(std::uint64_t entityId) noexcept;
    [[nodiscard]] const RigidBody* body(std::uint64_t entityId) const noexcept;
    void addPlane(PlaneCollider plane);
    void clearPlanes() noexcept;
    void applyForce(std::uint64_t entityId,Vec3f force);
    // Accumulates elapsed time, simulates fixed steps, and caps catch-up work.
    std::size_t advance(float elapsedSeconds,std::size_t maxSubsteps=8);
    [[nodiscard]] const std::vector<Contact>& contacts() const noexcept;
    [[nodiscard]] float interpolationAlpha() const noexcept;
    [[nodiscard]] std::size_t bodyCount() const noexcept;
    [[nodiscard]] const PhysicsStats& stats() const noexcept;
private:
    void step();
    Vec3f gravity_;
    float fixedStep_;
    float accumulator_{0.0F};
    std::vector<RigidBody> bodies_;
    std::vector<PlaneCollider> planes_;
    std::vector<Contact> contacts_;
};
} // namespace mid::physics
