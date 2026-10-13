#pragma once

#include "mid/rendering.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mid::animation {
using namespace mid::render;
struct Joint { std::string name; std::int32_t parent{-1}; Vec3f bindPosition{}; Vec3f position{}; Vec3f bindRotationDegrees{}; Vec3f rotationDegrees{}; Vec3f bindScale{1,1,1}; Vec3f scale{1,1,1}; };
struct Skeleton { std::vector<Joint> joints; void validate() const; };
struct JointPose { Vec3f position{}; Vec4f rotationQuaternion{0,0,0,1}; Vec3f scale{1,1,1}; };
struct Pose { std::vector<JointPose> joints; };
struct SkinInfluence { std::array<std::uint32_t,4> joints{}; std::array<float,4> weights{1,0,0,0}; };
// Positions/rotations in Skeleton are joint-local for hierarchical evaluation.
[[nodiscard]] Pose evaluateWorldPose(const Skeleton& skeleton,bool bindPose=false);
[[nodiscard]] Pose blendPoses(const Pose& a,const Pose& b,float alpha);
[[nodiscard]] std::vector<Vertex> skinMesh(const Skeleton& skeleton,const std::vector<Vertex>& bindVertices,
                                            const std::vector<SkinInfluence>& influences);
struct Vec3Key { float time{0}; Vec3f value{}; };
struct JointTrack { std::size_t joint{0}; std::vector<Vec3Key> positions; std::vector<Vec3Key> rotations; std::vector<Vec3Key> scales; };
struct Clip { std::string name; float duration{0}; std::vector<JointTrack> tracks; void validate(const Skeleton& skeleton) const; };
struct AnimatorDebugSnapshot { bool playing{false}; bool looping{true}; bool blending{false}; float blendProgress{1.0F}; std::string clipName; float time{0}; float duration{0}; Pose worldPose; std::vector<Vec3f> localPositions; std::vector<Vec3f> localRotations; std::vector<Vec3f> localScales; };
class Animator final {
public:
    explicit Animator(Skeleton skeleton);
    void play(const Clip& clip,bool looping=true);
    // Starts a timed cross-fade from the current local pose to a new clip.
    void crossFade(const Clip& clip,float blendDuration,bool looping=true);
    void stop() noexcept;
    void update(float deltaSeconds);
    [[nodiscard]] const Skeleton& skeleton() const noexcept;
    [[nodiscard]] float time() const noexcept;
    // Captures sampled local channels and hierarchical world pose for editor inspection.
    [[nodiscard]] AnimatorDebugSnapshot debugSnapshot() const;
private:
    Skeleton skeleton_;
    std::optional<Clip> clip_;
    float time_{0};
    bool looping_{true};
    Skeleton blendSource_;
    float blendDuration_{0.0F};
    float blendElapsed_{0.0F};
    bool blending_{false};
};
struct IkResult { bool converged{false}; std::size_t iterations{0}; float finalError{0}; };
struct IkJointConstraint { std::size_t joint{0}; Vec3f minimumRotationDegrees{-180,-180,-180}; Vec3f maximumRotationDegrees{180,180,180}; float maximumStepDegrees{45}; };
// CCD adjusts joint positions along a chain, leaving the root fixed.
[[nodiscard]] IkResult solveCcd(Skeleton& skeleton,const std::vector<std::size_t>& chain,
                                Vec3f target,std::size_t maxIterations=16,float tolerance=0.001F);
[[nodiscard]] IkResult solveCcdConstrained(Skeleton& skeleton,const std::vector<std::size_t>& chain,Vec3f target,
                                           const std::vector<IkJointConstraint>& constraints,
                                           std::size_t maxIterations=16,float tolerance=0.001F);
} // namespace mid::animation
