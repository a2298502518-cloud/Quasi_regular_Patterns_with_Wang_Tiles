#pragma once

#include "atlas/WangAtlasTiling.hpp"
#include "atlas/WangTileAtlas.hpp"
#include "render/Image.hpp"

#include <cstddef>
#include <cstdint>

namespace qrp::atlas {

struct AtlasSeamMetrics {
    std::size_t sampleCount = 0;
    std::size_t mismatchedPixelCount = 0;
    std::uint8_t maximumChannelDifference = 0;
};

class AtlasRenderer final {
public:
    [[nodiscard]] static render::Image render(
        const WangAtlasTiling& tiling,
        const WangTileAtlas& atlas);

    [[nodiscard]] static AtlasSeamMetrics measureSeams(
        const WangAtlasTiling& tiling,
        const WangTileAtlas& atlas);

    [[nodiscard]] static AtlasSeamMetrics measureAtlasCompatibility(
        const WangTileAtlas& atlas);
};

} // namespace qrp::atlas
