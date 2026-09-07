#pragma once

#include "color/GradientPalette.hpp"
#include "generators/TorusGenerator.hpp"
#include "math/CoonsWarp.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "render/Image.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace qrp::render {

struct CpuRenderSettings {
    std::uint32_t pixelsPerTile = 96;
    math::InverseWarpOptions inverseOptions;
};

struct CpuRenderDiagnostics {
    std::size_t inverseFailureCount = 0;
    double maximumInverseResidual = 0.0;
    double minimumDeterminant = std::numeric_limits<double>::infinity();
    std::uint32_t maximumInverseIterations = 0;
};

struct CpuRenderResult {
    Image image;
    CpuRenderDiagnostics diagnostics;
};

struct SeamMetrics {
    std::size_t sampleCount = 0;
    std::size_t inverseFailureCount = 0;
    double maximumScalarDifference = 0.0;
    double maximumColorDifference = 0.0;
    std::uint8_t maximumQuantizedChannelDifference = 0;
};

struct SeamHeatmapSettings {
    std::uint32_t pixelsPerTile = 72;
    std::uint32_t lineThickness = 2;
    double errorAmplification = 1.0e14;
};

class CpuReferenceRenderer final {
public:
    [[nodiscard]] static CpuRenderResult render(
        const model::WangGrid& grid,
        const model::EdgePalette& edgePalette,
        const generators::TorusGenerator& generator,
        const color::GradientPalette& colorPalette,
        const CpuRenderSettings& settings = {});

    [[nodiscard]] static SeamMetrics measureSeams(
        const model::WangGrid& grid,
        const model::EdgePalette& edgePalette,
        const generators::TorusGenerator& generator,
        const color::GradientPalette& colorPalette,
        std::uint32_t samplesPerEdge = 257);

    [[nodiscard]] static Image renderSeamHeatmap(
        const model::WangGrid& grid,
        const model::EdgePalette& edgePalette,
        const generators::TorusGenerator& generator,
        const color::GradientPalette& colorPalette,
        const SeamHeatmapSettings& settings = {});
};

} // namespace qrp::render
