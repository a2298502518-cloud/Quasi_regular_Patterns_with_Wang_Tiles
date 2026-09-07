#include "render/CpuReferenceRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace qrp::render {
namespace {

[[nodiscard]] math::CoonsWarp createWarp(
    const model::WangTile& tile,
    const model::EdgePalette& palette) {
    const auto& colors = palette.colors();
    return math::CoonsWarp(math::CoonsEdges{
        colors.at(tile.south),
        colors.at(tile.north),
        colors.at(tile.west),
        colors.at(tile.east),
    });
}

[[nodiscard]] std::vector<math::CoonsWarp> createWarps(
    const model::WangGrid& grid,
    const model::EdgePalette& palette) {
    if (grid.colorCount() != palette.colors().size()) {
        throw std::invalid_argument("Wang grid and edge palette color counts do not match.");
    }
    if (!grid.hasValidAdjacency()) {
        throw std::invalid_argument("Wang grid adjacency is invalid.");
    }
    if (!palette.validateAllCombinations().valid) {
        throw std::invalid_argument("Edge palette contains an unsafe warp combination.");
    }

    std::vector<math::CoonsWarp> warps;
    warps.reserve(grid.tiles().size());
    for (const auto& tile : grid.tiles()) {
        warps.push_back(createWarp(tile, palette));
    }
    return warps;
}

[[nodiscard]] Rgb8 quantize(const color::Color3 color) noexcept {
    const auto channel = [](const double value) {
        const double linear = std::clamp(value, 0.0, 1.0);
        const double srgb = linear <= 0.0031308
            ? 12.92 * linear
            : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
        return static_cast<std::uint8_t>(std::lround(
            255.0 * std::clamp(srgb, 0.0, 1.0)));
    };
    return {channel(color.red), channel(color.green), channel(color.blue)};
}

[[nodiscard]] std::uint8_t maximumChannelDifference(
    const Rgb8 first,
    const Rgb8 second) noexcept {
    const auto difference = [](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(a > b ? a - b : b - a);
    };
    return std::max({
        difference(first.red, second.red),
        difference(first.green, second.green),
        difference(first.blue, second.blue),
    });
}

struct EvaluatedSample {
    bool valid = false;
    double scalar = 0.0;
    color::Color3 color;
};

[[nodiscard]] Rgb8 seamErrorColor(
    const EvaluatedSample& first,
    const EvaluatedSample& second,
    const double amplification) noexcept {
    if (!first.valid || !second.valid) {
        return {255, 0, 255};
    }
    const double error = std::max(
        std::abs(first.scalar - second.scalar),
        color::maximumChannelDifference(first.color, second.color));
    const double intensity = std::clamp(error * amplification, 0.0, 1.0);
    return {
        static_cast<std::uint8_t>(std::lround(8.0 + 247.0 * intensity)),
        static_cast<std::uint8_t>(std::lround(42.0 + 150.0 * intensity)),
        static_cast<std::uint8_t>(std::lround(72.0 - 56.0 * intensity)),
    };
}

[[nodiscard]] EvaluatedSample evaluateSample(
    const math::CoonsWarp& warp,
    const model::WangTile& tile,
    const generators::TorusGenerator& generator,
    const color::GradientPalette& colorPalette,
    const math::Vec2 physical,
    const math::Vec2 world) noexcept {
    const auto inverse = warp.inverse(physical);
    if (!inverse.converged) {
        return {};
    }

    const double scalar = generator.evaluate(generators::GeneratorInput{
        inverse.parameter,
        world,
        tile.seed,
    });
    return {true, scalar, colorPalette.sample(scalar)};
}

} // namespace

CpuRenderResult CpuReferenceRenderer::render(
    const model::WangGrid& grid,
    const model::EdgePalette& edgePalette,
    const generators::TorusGenerator& generator,
    const color::GradientPalette& colorPalette,
    const CpuRenderSettings& settings) {
    if (settings.pixelsPerTile == 0) {
        throw std::invalid_argument("Pixels per tile must be positive.");
    }
    if (grid.width() > std::numeric_limits<std::size_t>::max() / settings.pixelsPerTile
        || grid.height() > std::numeric_limits<std::size_t>::max() / settings.pixelsPerTile) {
        throw std::length_error("Rendered image dimensions overflow.");
    }
    const auto warps = createWarps(grid, edgePalette);
    const std::size_t width = grid.width() * settings.pixelsPerTile;
    const std::size_t height = grid.height() * settings.pixelsPerTile;
    CpuRenderResult result{Image(width, height), {}};
    const double pixelsPerTile = static_cast<double>(settings.pixelsPerTile);

    for (std::size_t pixelY = 0; pixelY < height; ++pixelY) {
        const double worldY = static_cast<double>(grid.height())
            - (static_cast<double>(pixelY) + 0.5) / pixelsPerTile;
        const std::size_t tileY = std::min(
            grid.height() - 1,
            static_cast<std::size_t>(std::floor(worldY)));
        const double localY = worldY - static_cast<double>(tileY);

        for (std::size_t pixelX = 0; pixelX < width; ++pixelX) {
            const double worldX = (static_cast<double>(pixelX) + 0.5) / pixelsPerTile;
            const std::size_t tileX = std::min(
                grid.width() - 1,
                static_cast<std::size_t>(std::floor(worldX)));
            const double localX = worldX - static_cast<double>(tileX);
            const std::size_t tileIndex = tileY * grid.width() + tileX;
            const auto inverse = warps[tileIndex].inverse(
                math::Vec2{localX, localY},
                settings.inverseOptions);

            result.diagnostics.maximumInverseResidual = std::max(
                result.diagnostics.maximumInverseResidual,
                inverse.residualNorm);
            result.diagnostics.minimumDeterminant = std::min(
                result.diagnostics.minimumDeterminant,
                inverse.minimumDeterminant);
            result.diagnostics.maximumInverseIterations = std::max(
                result.diagnostics.maximumInverseIterations,
                inverse.iterations);

            if (!inverse.converged) {
                ++result.diagnostics.inverseFailureCount;
                result.image.pixel(pixelX, pixelY) = Rgb8{255, 0, 255};
                continue;
            }

            const auto& tile = grid.tiles()[tileIndex];
            const double scalar = generator.evaluate(generators::GeneratorInput{
                inverse.parameter,
                math::Vec2{worldX, worldY},
                tile.seed,
            });
            result.image.pixel(pixelX, pixelY) = quantize(colorPalette.sample(scalar));
        }
    }

    return result;
}

SeamMetrics CpuReferenceRenderer::measureSeams(
    const model::WangGrid& grid,
    const model::EdgePalette& edgePalette,
    const generators::TorusGenerator& generator,
    const color::GradientPalette& colorPalette,
    const std::uint32_t samplesPerEdge) {
    if (samplesPerEdge < 2) {
        throw std::invalid_argument("Seam measurement requires at least two samples per edge.");
    }
    const auto warps = createWarps(grid, edgePalette);
    SeamMetrics metrics;
    const double denominator = static_cast<double>(samplesPerEdge - 1);

    for (std::size_t y = 0; y < grid.height(); ++y) {
        for (std::size_t x = 0; x + 1 < grid.width(); ++x) {
            const std::size_t leftIndex = y * grid.width() + x;
            const std::size_t rightIndex = leftIndex + 1;
            for (std::uint32_t sample = 0; sample < samplesPerEdge; ++sample) {
                const double t = static_cast<double>(sample) / denominator;
                const math::Vec2 world{static_cast<double>(x + 1), static_cast<double>(y) + t};
                const auto left = evaluateSample(
                    warps[leftIndex], grid.tiles()[leftIndex], generator, colorPalette,
                    math::Vec2{1.0, t}, world);
                const auto right = evaluateSample(
                    warps[rightIndex], grid.tiles()[rightIndex], generator, colorPalette,
                    math::Vec2{0.0, t}, world);
                ++metrics.sampleCount;
                if (!left.valid || !right.valid) {
                    ++metrics.inverseFailureCount;
                    continue;
                }
                metrics.maximumScalarDifference = std::max(
                    metrics.maximumScalarDifference,
                    std::abs(left.scalar - right.scalar));
                metrics.maximumColorDifference = std::max(
                    metrics.maximumColorDifference,
                    color::maximumChannelDifference(left.color, right.color));
                metrics.maximumQuantizedChannelDifference = std::max(
                    metrics.maximumQuantizedChannelDifference,
                    maximumChannelDifference(quantize(left.color), quantize(right.color)));
            }
        }
    }

    for (std::size_t y = 0; y + 1 < grid.height(); ++y) {
        for (std::size_t x = 0; x < grid.width(); ++x) {
            const std::size_t lowerIndex = y * grid.width() + x;
            const std::size_t upperIndex = lowerIndex + grid.width();
            for (std::uint32_t sample = 0; sample < samplesPerEdge; ++sample) {
                const double t = static_cast<double>(sample) / denominator;
                const math::Vec2 world{static_cast<double>(x) + t, static_cast<double>(y + 1)};
                const auto lower = evaluateSample(
                    warps[lowerIndex], grid.tiles()[lowerIndex], generator, colorPalette,
                    math::Vec2{t, 1.0}, world);
                const auto upper = evaluateSample(
                    warps[upperIndex], grid.tiles()[upperIndex], generator, colorPalette,
                    math::Vec2{t, 0.0}, world);
                ++metrics.sampleCount;
                if (!lower.valid || !upper.valid) {
                    ++metrics.inverseFailureCount;
                    continue;
                }
                metrics.maximumScalarDifference = std::max(
                    metrics.maximumScalarDifference,
                    std::abs(lower.scalar - upper.scalar));
                metrics.maximumColorDifference = std::max(
                    metrics.maximumColorDifference,
                    color::maximumChannelDifference(lower.color, upper.color));
                metrics.maximumQuantizedChannelDifference = std::max(
                    metrics.maximumQuantizedChannelDifference,
                    maximumChannelDifference(quantize(lower.color), quantize(upper.color)));
            }
        }
    }

    return metrics;
}

Image CpuReferenceRenderer::renderSeamHeatmap(
    const model::WangGrid& grid,
    const model::EdgePalette& edgePalette,
    const generators::TorusGenerator& generator,
    const color::GradientPalette& colorPalette,
    const SeamHeatmapSettings& settings) {
    if (settings.pixelsPerTile == 0 || settings.lineThickness == 0) {
        throw std::invalid_argument("Seam heatmap dimensions must be positive.");
    }
    if (!std::isfinite(settings.errorAmplification) || settings.errorAmplification <= 0.0) {
        throw std::invalid_argument("Seam heatmap amplification must be finite and positive.");
    }
    if (grid.width() > std::numeric_limits<std::size_t>::max() / settings.pixelsPerTile
        || grid.height() > std::numeric_limits<std::size_t>::max() / settings.pixelsPerTile) {
        throw std::length_error("Seam heatmap dimensions overflow.");
    }

    const auto warps = createWarps(grid, edgePalette);
    const std::size_t width = grid.width() * settings.pixelsPerTile;
    const std::size_t height = grid.height() * settings.pixelsPerTile;
    Image image(width, height);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            image.pixel(x, y) = {3, 8, 14};
        }
    }

    const double samples = static_cast<double>(settings.pixelsPerTile);
    const auto paintVertical = [&](const std::size_t centerX, const std::size_t imageY, const Rgb8 color) {
        for (std::uint32_t offset = 0; offset < settings.lineThickness; ++offset) {
            if (centerX >= offset + 1) {
                image.pixel(centerX - offset - 1, imageY) = color;
            }
            if (centerX + offset < width) {
                image.pixel(centerX + offset, imageY) = color;
            }
        }
    };
    const auto paintHorizontal = [&](const std::size_t imageX, const std::size_t centerY, const Rgb8 color) {
        for (std::uint32_t offset = 0; offset < settings.lineThickness; ++offset) {
            if (centerY >= offset + 1) {
                image.pixel(imageX, centerY - offset - 1) = color;
            }
            if (centerY + offset < height) {
                image.pixel(imageX, centerY + offset) = color;
            }
        }
    };

    for (std::size_t y = 0; y < grid.height(); ++y) {
        for (std::size_t x = 0; x + 1 < grid.width(); ++x) {
            const std::size_t leftIndex = y * grid.width() + x;
            const std::size_t rightIndex = leftIndex + 1;
            for (std::uint32_t sample = 0; sample < settings.pixelsPerTile; ++sample) {
                const double t = (static_cast<double>(sample) + 0.5) / samples;
                const math::Vec2 world{static_cast<double>(x + 1), static_cast<double>(y) + t};
                const auto left = evaluateSample(
                    warps[leftIndex], grid.tiles()[leftIndex], generator, colorPalette,
                    math::Vec2{1.0, t}, world);
                const auto right = evaluateSample(
                    warps[rightIndex], grid.tiles()[rightIndex], generator, colorPalette,
                    math::Vec2{0.0, t}, world);
                const std::size_t imageY = height - 1
                    - (y * settings.pixelsPerTile + sample);
                paintVertical(
                    (x + 1) * settings.pixelsPerTile,
                    imageY,
                    seamErrorColor(left, right, settings.errorAmplification));
            }
        }
    }

    for (std::size_t y = 0; y + 1 < grid.height(); ++y) {
        for (std::size_t x = 0; x < grid.width(); ++x) {
            const std::size_t lowerIndex = y * grid.width() + x;
            const std::size_t upperIndex = lowerIndex + grid.width();
            for (std::uint32_t sample = 0; sample < settings.pixelsPerTile; ++sample) {
                const double t = (static_cast<double>(sample) + 0.5) / samples;
                const math::Vec2 world{static_cast<double>(x) + t, static_cast<double>(y + 1)};
                const auto lower = evaluateSample(
                    warps[lowerIndex], grid.tiles()[lowerIndex], generator, colorPalette,
                    math::Vec2{t, 1.0}, world);
                const auto upper = evaluateSample(
                    warps[upperIndex], grid.tiles()[upperIndex], generator, colorPalette,
                    math::Vec2{t, 0.0}, world);
                paintHorizontal(
                    x * settings.pixelsPerTile + sample,
                    height - (y + 1) * settings.pixelsPerTile,
                    seamErrorColor(lower, upper, settings.errorAmplification));
            }
        }
    }

    return image;
}

} // namespace qrp::render
