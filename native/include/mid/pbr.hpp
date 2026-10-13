#pragma once
#include "mid/rendering.hpp"
namespace mid::render::pbr {
struct Surface { Vec3f albedo{1,1,1}; float metallic{0}; float roughness{0.5F}; float ambientOcclusion{1}; };
struct Lighting { Vec3f normal{0,1,0}; Vec3f viewDirection{0,0,1}; Vec3f lightDirection{0,1,0}; Vec3f radiance{1,1,1}; Vec3f irradiance{0.03F,0.03F,0.03F}; };
[[nodiscard]] Vec3f evaluate(const Surface& surface,const Lighting& lighting);
}
