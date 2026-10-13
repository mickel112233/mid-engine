#pragma once
#include "mid/rendering.hpp"
#include "mid/animation.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace mid::assets {
struct ModelMaterial { render::Material material; std::vector<std::string> texturePaths; };
struct ImportedNode { std::string name; std::int32_t parent{-1}; std::array<float,16> localTransform{}; std::vector<std::uint32_t> meshIndices; };
struct ImportedModel { std::vector<render::MeshData> meshes; std::vector<ModelMaterial> materials; std::vector<std::uint32_t> meshMaterialIndices; std::vector<ImportedNode> nodes; animation::Skeleton skeleton; std::vector<animation::Clip> animations; std::vector<std::string> warnings; };
[[nodiscard]] bool hasAssimpModelSupport() noexcept;
// Supports the formats enabled in the linked Assimp build, including glTF/GLB and FBX.
// Throws a descriptive error if Assimp support is not built or import fails.
[[nodiscard]] ImportedModel importModelFile(const std::filesystem::path& path);
}
