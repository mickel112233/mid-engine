#pragma once
#include "mid/rendering.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
namespace mid::assets {
struct ImportedMaterial { render::Material material; std::string baseColorTexture; };
[[nodiscard]] std::unordered_map<std::string,ImportedMaterial> importMtl(std::string_view source);
}
