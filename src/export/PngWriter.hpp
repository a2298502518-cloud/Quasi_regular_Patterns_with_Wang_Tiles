#pragma once

#include "render/Image.hpp"

#include <filesystem>

namespace qrp::exporting {

void writePng(const render::Image& image, const std::filesystem::path& path);

} // namespace qrp::exporting
