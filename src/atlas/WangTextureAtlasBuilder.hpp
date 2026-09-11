#pragma once

#include "atlas/WangTileAtlas.hpp"
#include "render/Image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace qrp::atlas {

struct WangEdgeSampleBank {
    std::array<render::Image, 2> northSouth;
    std::array<render::Image, 2> westEast;
};

struct WangTextureAtlasBuildOptions {
    std::size_t overlapPixels = 0;
    std::size_t outputResolutionPixels = 0;
    render::Rgb8 cornerColor{};
    bool includeDebugImages = false;
};

struct WangTextureCutCosts {
    std::uint64_t eastPlacementVertical = 0;
    std::uint64_t westPlacementHorizontal = 0;
    std::uint64_t southPlacementTop = 0;
    std::uint64_t southPlacementLeft = 0;
};

struct WangTextureTileBuildMetrics {
    WangEdgeSignature edges;
    WangTextureCutCosts cutCosts;
    std::size_t correctedBoundaryPixelCount = 0;
    std::uint8_t maximumBoundaryChannelCorrection = 0;
    double meanBoundaryChannelCorrection = 0.0;
    std::uint8_t maximumEdgeInwardChannelDifference = 0;
    double meanEdgeInwardChannelDifference = 0.0;
};

struct WangTextureAtlasBuildReport {
    std::size_t patchSize = 0;
    std::size_t overlapPixels = 0;
    std::size_t stridePixels = 0;
    std::size_t outputResolutionPixels = 0;
    std::size_t independentCutCount = 0;
    std::size_t independentCutPixelCount = 0;
    std::uint64_t sumOfIndependentCutCosts = 0;
    std::vector<WangTextureTileBuildMetrics> tiles;
};

struct WangTextureTileDebugImage {
    WangEdgeSignature edges;
    render::Image image;
};

struct WangTextureAtlasBuildResult {
    WangTileAtlas atlas;
    WangTextureAtlasBuildReport report;
    std::vector<WangTextureTileDebugImage> cutPathImages;
};

class WangTextureAtlasBuilder final {
public:
    [[nodiscard]] static WangTextureAtlasBuildResult buildMinimalEight(
        const WangEdgeSampleBank& samples,
        const WangTextureAtlasBuildOptions& options);
};

} // namespace qrp::atlas
