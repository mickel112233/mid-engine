#pragma once

#include "mid/rendering.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mid::animation {
using namespace mid::render;
struct Joint { std::string name; std::int32_t parent{-1}; Vec3f bindPosition{}; Vec3f position{}; };
struct Skeleton { std::vector<Joint> joints; void validate() const; };
struct Vec3Key { float time{0}; Vec3f value{}; };
struct JointTrack { std::size_t joint{0}; std::vector<Vec3Key> positions; };
struct Clip { std::string name; float duration{0}; std::vector<JointTrack> tracks; void validate(const Skeleton& skeleton) const; };
class Animator final {
public:
    explicit Animator(Skeleton skeleton);
    void play(const Clip& clip,bool looping=true);
    void stop() noexcept;
    void update(float deltaSeconds);
    [[nodiscard]] const Skeleton& skeleton() const noexcept;
    [[nodiscard]] float time() const noexcept;
private:
    Skeleton skeleton_;
    std::optional<Clip> clip_;
    float time_{0};
    bool looping_{true};
};
struct IkResult { bool converged{false}; std::size_t iterations{0}; float finalError{0}; };
// CCD adjusts joint positions along a chain, leaving the root fixed.
[[nodiscard]] IkResult solveCcd(Skeleton& skeleton,const std::vector<std::size_t>& chain,
                                Vec3f target,std::size_t maxIterations=16,float tolerance=0.001F);
} // namespace mid::animation
