#pragma once
#include "mid/rendering.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace mid::assets {
struct ModelMaterial { render::Material material; std::vector<std::string> texturePaths; };
struct ImportedModel { std::vector<render::MeshData> meshes; std::vector<ModelMaterial> materials; std::vector<std::string> warnings; };
[[nodiscard]] bool hasAssimpModelSupport() noexcept;
// Supports the formats enabled in the linked Assimp build, including glTF/GLB and FBX.
// Throws a descriptive error if Assimp support is not built or import fails.
[[nodiscard]] ImportedModel importModelFile(const std::filesystem::path& path);
}
