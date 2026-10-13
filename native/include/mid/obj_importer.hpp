#pragma once
#include "mid/rendering.hpp"
#include <string_view>
namespace mid::assets {
// Imports triangulated or polygonal Wavefront OBJ text (v/vt/vn/f); fan-triangulates polygons.
[[nodiscard]] render::MeshData importObj(std::string_view source,std::string name="Imported OBJ");
}
