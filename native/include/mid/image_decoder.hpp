#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace mid::assets {
struct ImageRgba8 { std::uint32_t width{0}; std::uint32_t height{0}; std::vector<std::uint8_t> pixels; };
[[nodiscard]] ImageRgba8 decodeBmp(std::span<const std::byte> bytes);
[[nodiscard]] ImageRgba8 decodeImage(std::span<const std::byte> bytes);
[[nodiscard]] bool hasPngSupport() noexcept;
}
