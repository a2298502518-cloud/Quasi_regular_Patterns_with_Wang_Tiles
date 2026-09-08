#pragma once

#include "project/PatternProject.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace qrp::exporting {

struct ExportView {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    double originX = 0.0;
    double originY = 0.0;
    double pixelsPerTile = 1.0;
    bool materialEffects = true;
};

[[nodiscard]] std::string serializeProjectMetadata(
    const project::PatternConfiguration& configuration,
    std::uint64_t revision,
    const ExportView& view);
void writeProjectMetadata(
    const project::PatternConfiguration& configuration,
    std::uint64_t revision,
    const ExportView& view,
    const std::filesystem::path& path);

} // namespace qrp::exporting
