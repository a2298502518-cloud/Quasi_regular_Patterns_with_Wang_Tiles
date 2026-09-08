#pragma once

#include "render/Image.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace qrp::exporting {

[[nodiscard]] std::vector<std::uint8_t> encodePng(const render::Image& image);
void writePng(const render::Image& image, const std::filesystem::path& path);

} // namespace qrp::exporting
