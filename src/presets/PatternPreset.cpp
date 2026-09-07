#include "presets/PatternPreset.hpp"

namespace qrp::presets {

std::vector<PatternPreset> createBaselinePresets() {
    return {
        {
            "single_qrp",
            1,
            1,
            384,
            0x1199aacc55ee7711ULL,
            {1.10, 0.08, 0.08, 0.04, 0.04, 0.03, 0.137, 0.091},
            color::GradientPalette::createMidnightGold(),
        },
        {
            "neighbors_2x2",
            2,
            2,
            192,
            0x90ab12cd34ef5678ULL,
            {0.90, 0.25, 0.18, 0.09, 0.08, 0.24, 0.137, 0.091},
            color::GradientPalette::createAurora(),
        },
        {
            "tiling_qrp_10x10",
            10,
            10,
            72,
            0xdecafbad98765432ULL,
            {1.05, 0.15, 0.16, 0.10, 0.12, 0.28, 0.137, 0.091},
            color::GradientPalette::createMidnightGold(),
        },
        {
            "tiling_mineral_10x10",
            10,
            10,
            72,
            0xa17e5c4962bd308fULL,
            {0.34, 1.18, 0.20, 0.11, 0.16, 0.34, 0.083, -0.119},
            color::GradientPalette::createMineral(),
        },
        {
            "tiling_aurora_10x10",
            10,
            10,
            72,
            0x6712e4ad09bc53f8ULL,
            {0.70, 0.68, 0.18, 0.10, 0.18, 0.32, -0.101, 0.073},
            color::GradientPalette::createAurora(),
        },
    };
}

} // namespace qrp::presets
