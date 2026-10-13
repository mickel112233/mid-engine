#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace mid::assets {
struct ImageRgba8 { std::uint32_t width{0}; std::uint32_t height{0}; std::vector<std::uint8_t> pixels; };
struct ImageRgba32F { std::uint32_t width{0}; std::uint32_t height{0}; std::vector<float> pixels; };
// Always available: BMP; PNG/JPEG support depends on optional system libraries.
[[nodiscard]] ImageRgba8 decodeBmp(std::span<const std::byte> bytes);
[[nodiscard]] ImageRgba8 decodeImage(std::span<const std::byte> bytes);
// Radiance RGBE decoder preserves linear high-dynamic-range values as float RGBA.
[[nodiscard]] ImageRgba32F decodeHdr(std::span<const std::byte> bytes);
[[nodiscard]] bool hasPngSupport() noexcept;
[[nodiscard]] bool hasJpegSupport() noexcept;
[[nodiscard]] bool hasHdrSupport() noexcept;
}
