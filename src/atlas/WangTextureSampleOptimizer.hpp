#pragma once

#include "atlas/WangTextureAtlasBuilder.hpp"
#include "render/Image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace qrp::atlas {

struct WangTextureSampleOrigin {
    // render::Image coordinates: x grows east/right, y grows south/down, and
    // row zero is North. The crop is [x, x + patchSize) x [y, y + patchSize).
    std::size_t x = 0;
    std::size_t y = 0;

    friend bool operator==(
        const WangTextureSampleOrigin&,
        const WangTextureSampleOrigin&) = default;
};

struct WangTextureSampleOrigins {
    std::array<WangTextureSampleOrigin, 2> northSouth{};
    std::array<WangTextureSampleOrigin, 2> westEast{};

    friend bool operator==(
        const WangTextureSampleOrigins&,
        const WangTextureSampleOrigins&) = default;
};

struct WangTextureSampleOptimizationOptions {
    std::size_t patchSize = 0;
    // includeDebugImages is deliberately ignored while scoring candidates.
    WangTextureAtlasBuildOptions buildOptions;
    // Candidate sampling uses the repository-defined SplitMix64 sequence.
    std::uint64_t searchSeed = 0;
    std::size_t candidateGroupCount = 0;
    // Pairwise Euclidean distance between crop top-left origins. Even when
    // zero, NS0, NS1, WE0, and WE1 must be four distinct origins.
    std::size_t minimumOriginDistancePixels = 0;
};

struct WangTextureSampleOptimizationReport {
    // Both values are sums over the 32 minimum-error seam paths constructed
    // for the S8 set; they are not an overall perceptual-quality score.
    std::uint64_t firstCandidateCutCost = 0;
    std::uint64_t bestCutCost = 0;
    std::size_t evaluatedCandidateGroupCount = 0;
    WangTextureSampleOrigins bestOrigins;
};

struct WangTextureSampleOptimizationResult {
    WangEdgeSampleBank samples;
    WangTextureSampleOptimizationReport report;
};

class WangTextureSampleOptimizer final {
public:
    [[nodiscard]] static WangTextureSampleOptimizationResult optimize(
        const render::Image& source,
        const WangTextureSampleOptimizationOptions& options);
};

} // namespace qrp::atlas
