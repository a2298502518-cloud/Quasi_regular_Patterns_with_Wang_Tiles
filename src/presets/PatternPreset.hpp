#pragma once

#include "color/GradientPalette.hpp"
#include "generators/HybridTorusGenerator.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qrp::presets {

struct PatternPreset {
    std::string name;
    std::size_t gridWidth = 10;
    std::size_t gridHeight = 10;
    std::uint32_t pixelsPerTile = 72;
    std::uint64_t gridSeed = 0;
    generators::HybridGeneratorSettings generator;
    color::GradientPalette palette;
};

[[nodiscard]] std::vector<PatternPreset> createBaselinePresets();

} // namespace qrp::presets
