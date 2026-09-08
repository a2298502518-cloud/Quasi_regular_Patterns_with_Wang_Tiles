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
            {7.0, 0.045, 0.070, 0.15},
        },
        {
            "neighbors_2x2",
            2,
            2,
            192,
            0x90ab12cd34ef5678ULL,
            {0.90, 0.25, 0.18, 0.09, 0.08, 0.24, 0.137, 0.091},
            color::GradientPalette::createAurora(),
            {6.0, 0.045, 0.080, 0.13},
        },
        {
            "tiling_qrp_10x10",
            10,
            10,
            72,
            0xdecafbad98765432ULL,
            {1.05, 0.15, 0.05, 0.025, 0.32, 0.38, 0.167, 0.123},
            color::GradientPalette::createMidnightGold(),
            {8.0, 0.060, 0.070, 0.17},
        },
        {
            "tiling_mineral_10x10",
            10,
            10,
            72,
            0xa17e5c4962bd308fULL,
            {0.34, 1.18, 0.05, 0.025, 0.35, 0.42, 0.173, -0.127},
            color::GradientPalette::createMineral(),
            {5.0, 0.050, 0.085, 0.24},
        },
        {
            "tiling_aurora_10x10",
            10,
            10,
            72,
            0x6712e4ad09bc53f8ULL,
            {0.70, 0.68, 0.05, 0.025, 0.35, 0.40, -0.151, 0.113},
            color::GradientPalette::createAurora(),
            {7.0, 0.040, 0.075, 0.14},
        },
        {
            "tiling_graphic_screenprint_10x10",
            10,
            10,
            72,
            0x4d3c2b1a90e8f765ULL,
            {
                0.35, 0.02, 0.025, 0.015,
                0.18, 0.28, 0.110, 0.037,
                0.95,
                generators::ScalarProfile::Ridges,
            },
            color::GradientPalette::createGraphicPrimary(),
            {0.0, 0.0, 0.070, 0.0},
        },
        {
            "tiling_ink_wash_10x10",
            10,
            10,
            72,
            0x83c6a91e25d74bf0ULL,
            {
                0.02, 0.55, 0.025, 0.025,
                0.10, 0.35, 0.072, -0.049,
                0.65,
                generators::ScalarProfile::Natural,
            },
            color::GradientPalette::createInkWash(),
            {0.0, 0.0, 0.080, 0.0},
        },
        {
            "tiling_cellular_camo_10x10",
            10,
            10,
            72,
            0x29f14ac783b650deULL,
            {
                0.30, 0.22, 0.020, 0.015,
                0.12, 0.25, 0.130, 0.083,
                0.85,
                generators::ScalarProfile::Cells,
            },
            color::GradientPalette::createFieldCamo(),
            {0.0, 0.0, 0.080, 0.04},
        },
    };
}

} // namespace qrp::presets
