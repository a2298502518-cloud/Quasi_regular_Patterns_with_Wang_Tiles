#pragma once

#include "render/Image.hpp"

#include <filesystem>

namespace qrp::exporting {

void writePpm(const render::Image& image, const std::filesystem::path& path);

} // namespace qrp::exporting
