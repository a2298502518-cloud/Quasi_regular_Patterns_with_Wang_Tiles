#include "atlas/AtlasRenderer.hpp"
#include "atlas/WangAtlasTiling.hpp"
#include "atlas/WangTextureAtlasBuilder.hpp"
#include "atlas/WangTextureSampleOptimizer.hpp"
#include "color/GradientPalette.hpp"
#include "export/PngWriter.hpp"
#include "generators/TorusFourier.hpp"
#include "math/CoonsWarp.hpp"
#include "math/EdgeFunction.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kSourceSize = 640;
constexpr std::size_t kPatchSize = 128;
constexpr std::size_t kOverlapPixels = 32;
constexpr std::size_t kPixelsPerTile = 96;
constexpr std::size_t kGridWidth = 10;
constexpr std::size_t kGridHeight = 10;
constexpr std::size_t kShiftLagCount = 5;
constexpr std::size_t kCandidateGroupCount = 96;
constexpr std::size_t kMinimumOriginDistancePixels = 112;

struct ExperimentConfig {
    std::string_view id;
    std::array<double, 5> phases;
    std::uint64_t sampleSearchSeed = 0;
    std::uint64_t gridSeed = 0;
};

constexpr ExperimentConfig kPrimaryConfig{
    "primary",
    {0.17, 1.31, 2.46, 3.72, 5.09},
    0x243f6a8885a308d3ULL,
    0x13198a2e03707344ULL,
};

constexpr ExperimentConfig kHoldoutConfig{
    "holdout",
    {0.73, 2.02, 3.11, 4.28, 5.67},
    0xa4093822299f31d0ULL,
    0x082efa98ec4e6c89ULL,
};

struct ShiftDifference {
    double horizontal = 0.0;
    double vertical = 0.0;
};

struct SeamMetrics {
    std::size_t sampleCount = 0;
    std::size_t inverseFailureCount = 0;
    double maximumScalarDifference = 0.0;
    std::uint8_t maximumQuantizedChannelDifference = 0;
};

struct RenderCase {
    qrp::render::Image image;
    SeamMetrics seams;
    std::array<ShiftDifference, kShiftLagCount> tileShifts{};
    bool reproducible = false;
};

struct SignatureUsageMetrics {
    std::size_t uniqueSignatureCount = 0;
    std::size_t maximumSignatureUseCount = 0;
    double effectiveSignatureCount = 0.0;
    std::array<ShiftDifference, kShiftLagCount> sameSignatureRates{};
};

enum class ContentSelection {
    FixedZeroSignature,
    WangSignature,
};

[[nodiscard]] double sampleQrpSource(
    const double x,
    const double y,
    const std::array<double, 5>& phases) noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    constexpr double cyclesPerPixel = 1.0 / 46.0;

    double sum = 0.0;
    for (std::size_t index = 0; index < phases.size(); ++index) {
        const double angle = std::numbers::pi_v<double>
            * static_cast<double>(index)
            / static_cast<double>(phases.size());
        const double projected = std::cos(angle) * x + std::sin(angle) * y;
        sum += std::cos(tau * cyclesPerPixel * projected + phases[index]);
    }
    return sum / static_cast<double>(phases.size());
}

[[nodiscard]] std::uint8_t encodeScalar(const double scalar) noexcept {
    return static_cast<std::uint8_t>(std::lround(
        255.0 * std::clamp(0.5 * scalar + 0.5, 0.0, 1.0)));
}

[[nodiscard]] double decodeScalar(const std::uint8_t encoded) noexcept {
    return 2.0 * static_cast<double>(encoded) / 255.0 - 1.0;
}

[[nodiscard]] qrp::render::Rgb8 quantize(
    const qrp::color::Color3 color) noexcept {
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

[[nodiscard]] qrp::render::Image createQrpSource(
    const std::array<double, 5>& phases) {
    qrp::render::Image image(kSourceSize, kSourceSize);
    for (std::size_t y = 0; y < kSourceSize; ++y) {
        for (std::size_t x = 0; x < kSourceSize; ++x) {
            const std::uint8_t value = encodeScalar(sampleQrpSource(
                static_cast<double>(x),
                static_cast<double>(y),
                phases));
            image.pixel(x, y) = {value, value, value};
        }
    }
    return image;
}

[[nodiscard]] qrp::render::Image colorizeScalarImage(
    const qrp::render::Image& scalarImage,
    const qrp::color::GradientPalette& palette) {
    qrp::render::Image image(scalarImage.width(), scalarImage.height());
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            image.pixel(x, y) = quantize(palette.sample(
                decodeScalar(scalarImage.pixel(x, y).red)));
        }
    }
    return image;
}

[[nodiscard]] std::array<qrp::math::EdgeFunction, 2> createEdgeFunctions() {
    using qrp::math::EdgeFunction;
    using qrp::math::EdgeParameters;
    return {
        EdgeFunction(EdgeParameters{+0.08, 0.0}),
        EdgeFunction(EdgeParameters{-0.08, 0.0}),
    };
}

[[nodiscard]] qrp::math::CoonsWarp createWarp(
    const qrp::atlas::WangEdgeSignature edges,
    const std::array<qrp::math::EdgeFunction, 2>& functions) {
    return qrp::math::CoonsWarp(qrp::math::CoonsEdges{
        functions.at(edges.south),
        functions.at(edges.north),
        functions.at(edges.west),
        functions.at(edges.east),
    });
}

[[nodiscard]] double sampleAtlasScalar(
    const qrp::render::Image& tile,
    const qrp::math::Vec2 parameter) noexcept {
    const double last = static_cast<double>(tile.width() - 1);
    const double sourceX = std::clamp(parameter.x, 0.0, 1.0) * last;
    const double sourceY = (1.0 - std::clamp(parameter.y, 0.0, 1.0)) * last;
    const std::size_t x0 = static_cast<std::size_t>(std::floor(sourceX));
    const std::size_t y0 = static_cast<std::size_t>(std::floor(sourceY));
    const std::size_t x1 = std::min(x0 + 1, tile.width() - 1);
    const std::size_t y1 = std::min(y0 + 1, tile.height() - 1);
    const double tx = sourceX - static_cast<double>(x0);
    const double ty = sourceY - static_cast<double>(y0);
    const double north = std::lerp(
        static_cast<double>(tile.pixel(x0, y0).red),
        static_cast<double>(tile.pixel(x1, y0).red),
        tx);
    const double south = std::lerp(
        static_cast<double>(tile.pixel(x0, y1).red),
        static_cast<double>(tile.pixel(x1, y1).red),
        tx);
    return 2.0 * std::lerp(north, south, ty) / 255.0 - 1.0;
}

[[nodiscard]] std::uint8_t maximumChannelDifference(
    const qrp::render::Rgb8 first,
    const qrp::render::Rgb8 second) noexcept {
    const auto difference = [](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(a > b ? a - b : b - a);
    };
    return std::max({
        difference(first.red, second.red),
        difference(first.green, second.green),
        difference(first.blue, second.blue),
    });
}

[[nodiscard]] double meanShiftDifference(
    const qrp::render::Image& image,
    const std::size_t shiftX,
    const std::size_t shiftY) {
    const std::size_t comparedWidth = image.width() - shiftX;
    const std::size_t comparedHeight = image.height() - shiftY;
    long double difference = 0.0;
    for (std::size_t y = 0; y < comparedHeight; ++y) {
        for (std::size_t x = 0; x < comparedWidth; ++x) {
            const auto first = image.pixel(x, y);
            const auto second = image.pixel(x + shiftX, y + shiftY);
            difference += std::abs(
                static_cast<int>(first.red) - static_cast<int>(second.red));
            difference += std::abs(
                static_cast<int>(first.green) - static_cast<int>(second.green));
            difference += std::abs(
                static_cast<int>(first.blue) - static_cast<int>(second.blue));
        }
    }
    const long double channelCount = 3.0L
        * static_cast<long double>(comparedWidth)
        * static_cast<long double>(comparedHeight);
    return static_cast<double>(difference / (255.0L * channelCount));
}

[[nodiscard]] std::array<ShiftDifference, kShiftLagCount> measureTileShifts(
    const qrp::render::Image& image) {
    std::array<ShiftDifference, kShiftLagCount> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const std::size_t shift = (index + 1) * kPixelsPerTile;
        result[index] = {
            meanShiftDifference(image, shift, 0),
            meanShiftDifference(image, 0, shift),
        };
    }
    return result;
}

[[nodiscard]] std::size_t binarySignatureIndex(
    const qrp::atlas::WangEdgeSignature edges) noexcept {
    return static_cast<std::size_t>(
        (edges.south << 3U)
        | (edges.north << 2U)
        | (edges.west << 1U)
        | edges.east);
}

[[nodiscard]] SignatureUsageMetrics measureSignatureUsage(
    const qrp::atlas::WangAtlasTiling& tiling) {
    std::array<std::size_t, 16> counts{};
    for (const auto edges : tiling.tiles()) {
        ++counts.at(binarySignatureIndex(edges));
    }

    SignatureUsageMetrics result;
    long double sumOfSquaredCounts = 0.0L;
    for (const std::size_t count : counts) {
        result.uniqueSignatureCount += count != 0;
        result.maximumSignatureUseCount = std::max(
            result.maximumSignatureUseCount,
            count);
        sumOfSquaredCounts += static_cast<long double>(count)
            * static_cast<long double>(count);
    }
    const long double tileCount = static_cast<long double>(tiling.tiles().size());
    result.effectiveSignatureCount = static_cast<double>(
        tileCount * tileCount / sumOfSquaredCounts);

    for (std::size_t index = 0; index < result.sameSignatureRates.size(); ++index) {
        const std::size_t lag = index + 1;
        std::size_t horizontalMatches = 0;
        std::size_t verticalMatches = 0;
        for (std::size_t y = 0; y < tiling.height(); ++y) {
            for (std::size_t x = 0; x + lag < tiling.width(); ++x) {
                horizontalMatches += tiling.tile(x, y) == tiling.tile(x + lag, y);
            }
        }
        for (std::size_t y = 0; y + lag < tiling.height(); ++y) {
            for (std::size_t x = 0; x < tiling.width(); ++x) {
                verticalMatches += tiling.tile(x, y) == tiling.tile(x, y + lag);
            }
        }
        result.sameSignatureRates[index] = {
            static_cast<double>(horizontalMatches)
                / static_cast<double>((tiling.width() - lag) * tiling.height()),
            static_cast<double>(verticalMatches)
                / static_cast<double>(tiling.width() * (tiling.height() - lag)),
        };
    }
    return result;
}

[[nodiscard]] bool imagesEqual(
    const qrp::render::Image& first,
    const qrp::render::Image& second) noexcept {
    if (first.width() != second.width() || first.height() != second.height()) {
        return false;
    }
    for (std::size_t y = 0; y < first.height(); ++y) {
        for (std::size_t x = 0; x < first.width(); ++x) {
            const auto a = first.pixel(x, y);
            const auto b = second.pixel(x, y);
            if (a.red != b.red || a.green != b.green || a.blue != b.blue) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::size_t countUniqueTileImages(
    const qrp::atlas::WangTileAtlas& atlas) {
    std::size_t uniqueCount = 0;
    for (std::size_t index = 0; index < atlas.tiles().size(); ++index) {
        bool matchesEarlier = false;
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            matchesEarlier = matchesEarlier || imagesEqual(
                atlas.tiles()[index].image,
                atlas.tiles()[earlier].image);
        }
        uniqueCount += !matchesEarlier;
    }
    return uniqueCount;
}

[[nodiscard]] qrp::render::Image renderCommonQrp(
    const qrp::atlas::WangAtlasTiling& tiling,
    const std::array<qrp::math::EdgeFunction, 2>& edgeFunctions,
    const qrp::generators::TorusFourier& generator,
    const qrp::color::GradientPalette& palette) {
    qrp::render::Image image(
        tiling.width() * kPixelsPerTile,
        tiling.height() * kPixelsPerTile);
    const double pixelsPerTile = static_cast<double>(kPixelsPerTile);
    for (std::size_t gridY = 0; gridY < tiling.height(); ++gridY) {
        const std::size_t tileTop = (tiling.height() - gridY - 1) * kPixelsPerTile;
        for (std::size_t gridX = 0; gridX < tiling.width(); ++gridX) {
            const auto warp = createWarp(tiling.tile(gridX, gridY), edgeFunctions);
            for (std::size_t imageY = 0; imageY < kPixelsPerTile; ++imageY) {
                for (std::size_t imageX = 0; imageX < kPixelsPerTile; ++imageX) {
                    const qrp::math::Vec2 physical{
                        (static_cast<double>(imageX) + 0.5) / pixelsPerTile,
                        1.0 - (static_cast<double>(imageY) + 0.5) / pixelsPerTile,
                    };
                    const auto inverse = warp.inverse(physical);
                    if (!inverse.converged) {
                        image.pixel(
                            gridX * kPixelsPerTile + imageX,
                            tileTop + imageY) = {255, 0, 255};
                        continue;
                    }
                    image.pixel(
                        gridX * kPixelsPerTile + imageX,
                        tileTop + imageY) = quantize(
                            palette.sample(generator.evaluate(inverse.parameter)));
                }
            }
        }
    }
    return image;
}

[[nodiscard]] qrp::render::Image renderWangContent(
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::WangTileAtlas& atlas,
    const std::array<qrp::math::EdgeFunction, 2>& edgeFunctions,
    const qrp::color::GradientPalette& palette,
    const ContentSelection contentSelection,
    const bool enableCoons) {
    qrp::render::Image image(
        tiling.width() * kPixelsPerTile,
        tiling.height() * kPixelsPerTile);
    const double pixelsPerTile = static_cast<double>(kPixelsPerTile);
    for (std::size_t gridY = 0; gridY < tiling.height(); ++gridY) {
        const std::size_t tileTop = (tiling.height() - gridY - 1) * kPixelsPerTile;
        for (std::size_t gridX = 0; gridX < tiling.width(); ++gridX) {
            const auto edges = tiling.tile(gridX, gridY);
            const auto contentEdges = contentSelection == ContentSelection::WangSignature
                ? edges
                : qrp::atlas::WangEdgeSignature{};
            const auto& content = atlas.select(contentEdges).image;
            const auto warp = createWarp(edges, edgeFunctions);
            for (std::size_t imageY = 0; imageY < kPixelsPerTile; ++imageY) {
                for (std::size_t imageX = 0; imageX < kPixelsPerTile; ++imageX) {
                    const qrp::math::Vec2 physical{
                        (static_cast<double>(imageX) + 0.5) / pixelsPerTile,
                        1.0 - (static_cast<double>(imageY) + 0.5) / pixelsPerTile,
                    };
                    qrp::math::Vec2 parameter = physical;
                    if (enableCoons) {
                        const auto inverse = warp.inverse(physical);
                        if (!inverse.converged) {
                            image.pixel(
                                gridX * kPixelsPerTile + imageX,
                                tileTop + imageY) = {255, 0, 255};
                            continue;
                        }
                        parameter = inverse.parameter;
                    }
                    image.pixel(
                        gridX * kPixelsPerTile + imageX,
                        tileTop + imageY) = quantize(
                            palette.sample(sampleAtlasScalar(content, parameter)));
                }
            }
        }
    }
    return image;
}

[[nodiscard]] SeamMetrics measureCommonQrpSeams(
    const qrp::atlas::WangAtlasTiling& tiling,
    const std::array<qrp::math::EdgeFunction, 2>& edgeFunctions,
    const qrp::generators::TorusFourier& generator,
    const qrp::color::GradientPalette& palette) {
    constexpr std::size_t samplesPerEdge = 257;
    SeamMetrics metrics;
    const double denominator = static_cast<double>(samplesPerEdge - 1);
    const auto evaluate = [&](const qrp::atlas::WangEdgeSignature edges,
                              const qrp::math::Vec2 physical,
                              bool& valid) {
        const auto inverse = createWarp(edges, edgeFunctions).inverse(physical);
        valid = inverse.converged;
        return valid ? generator.evaluate(inverse.parameter) : 0.0;
    };
    const auto accumulate = [&](const double first,
                                const double second,
                                const bool firstValid,
                                const bool secondValid) {
        ++metrics.sampleCount;
        if (!firstValid || !secondValid) {
            ++metrics.inverseFailureCount;
            return;
        }
        metrics.maximumScalarDifference = std::max(
            metrics.maximumScalarDifference,
            std::abs(first - second));
        metrics.maximumQuantizedChannelDifference = std::max(
            metrics.maximumQuantizedChannelDifference,
            maximumChannelDifference(
                quantize(palette.sample(first)),
                quantize(palette.sample(second))));
    };

    for (std::size_t y = 0; y < tiling.height(); ++y) {
        for (std::size_t x = 0; x + 1 < tiling.width(); ++x) {
            for (std::size_t sample = 0; sample < samplesPerEdge; ++sample) {
                const double t = static_cast<double>(sample) / denominator;
                bool leftValid = false;
                bool rightValid = false;
                const double left = evaluate(
                    tiling.tile(x, y), {1.0, t}, leftValid);
                const double right = evaluate(
                    tiling.tile(x + 1, y), {0.0, t}, rightValid);
                accumulate(left, right, leftValid, rightValid);
            }
        }
    }
    for (std::size_t y = 0; y + 1 < tiling.height(); ++y) {
        for (std::size_t x = 0; x < tiling.width(); ++x) {
            for (std::size_t sample = 0; sample < samplesPerEdge; ++sample) {
                const double t = static_cast<double>(sample) / denominator;
                bool bottomValid = false;
                bool topValid = false;
                const double bottom = evaluate(
                    tiling.tile(x, y), {t, 1.0}, bottomValid);
                const double top = evaluate(
                    tiling.tile(x, y + 1), {t, 0.0}, topValid);
                accumulate(bottom, top, bottomValid, topValid);
            }
        }
    }
    return metrics;
}

[[nodiscard]] SeamMetrics measureWangContentSeams(
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::WangTileAtlas& atlas,
    const std::array<qrp::math::EdgeFunction, 2>& edgeFunctions,
    const qrp::color::GradientPalette& palette,
    const ContentSelection contentSelection,
    const bool enableCoons) {
    constexpr std::size_t samplesPerEdge = 257;
    SeamMetrics metrics;
    const double denominator = static_cast<double>(samplesPerEdge - 1);
    const auto evaluate = [&](const qrp::atlas::WangEdgeSignature edges,
                              const qrp::math::Vec2 physical,
                              bool& valid) {
        qrp::math::Vec2 parameter = physical;
        if (enableCoons) {
            const auto inverse = createWarp(edges, edgeFunctions).inverse(physical);
            valid = inverse.converged;
            if (!valid) {
                return 0.0;
            }
            parameter = inverse.parameter;
        } else {
            valid = true;
        }
        const auto contentEdges = contentSelection == ContentSelection::WangSignature
            ? edges
            : qrp::atlas::WangEdgeSignature{};
        return sampleAtlasScalar(atlas.select(contentEdges).image, parameter);
    };
    const auto accumulate = [&](const double first,
                                const double second,
                                const bool firstValid,
                                const bool secondValid) {
        ++metrics.sampleCount;
        if (!firstValid || !secondValid) {
            ++metrics.inverseFailureCount;
            return;
        }
        metrics.maximumScalarDifference = std::max(
            metrics.maximumScalarDifference,
            std::abs(first - second));
        metrics.maximumQuantizedChannelDifference = std::max(
            metrics.maximumQuantizedChannelDifference,
            maximumChannelDifference(
                quantize(palette.sample(first)),
                quantize(palette.sample(second))));
    };

    for (std::size_t y = 0; y < tiling.height(); ++y) {
        for (std::size_t x = 0; x + 1 < tiling.width(); ++x) {
            for (std::size_t sample = 0; sample < samplesPerEdge; ++sample) {
                const double t = static_cast<double>(sample) / denominator;
                bool leftValid = false;
                bool rightValid = false;
                const double left = evaluate(
                    tiling.tile(x, y), {1.0, t}, leftValid);
                const double right = evaluate(
                    tiling.tile(x + 1, y), {0.0, t}, rightValid);
                accumulate(left, right, leftValid, rightValid);
            }
        }
    }
    for (std::size_t y = 0; y + 1 < tiling.height(); ++y) {
        for (std::size_t x = 0; x < tiling.width(); ++x) {
            for (std::size_t sample = 0; sample < samplesPerEdge; ++sample) {
                const double t = static_cast<double>(sample) / denominator;
                bool bottomValid = false;
                bool topValid = false;
                const double bottom = evaluate(
                    tiling.tile(x, y), {t, 1.0}, bottomValid);
                const double top = evaluate(
                    tiling.tile(x, y + 1), {t, 0.0}, topValid);
                accumulate(bottom, top, bottomValid, topValid);
            }
        }
    }
    return metrics;
}

[[nodiscard]] qrp::render::Image createAtlasSheet(
    const qrp::atlas::WangTileAtlas& atlas,
    const qrp::color::GradientPalette& palette) {
    constexpr std::size_t columns = 4;
    const std::size_t rows = (atlas.tiles().size() + columns - 1) / columns;
    qrp::render::Image image(
        columns * atlas.tileSize(),
        rows * atlas.tileSize());
    for (std::size_t index = 0; index < atlas.tiles().size(); ++index) {
        const auto colored = colorizeScalarImage(atlas.tiles()[index].image, palette);
        const std::size_t offsetX = (index % columns) * atlas.tileSize();
        const std::size_t offsetY = (index / columns) * atlas.tileSize();
        for (std::size_t y = 0; y < atlas.tileSize(); ++y) {
            for (std::size_t x = 0; x < atlas.tileSize(); ++x) {
                image.pixel(offsetX + x, offsetY + y) = colored.pixel(x, y);
            }
        }
    }
    return image;
}

[[nodiscard]] RenderCase makeWangContentCase(
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::WangTileAtlas& atlas,
    const std::array<qrp::math::EdgeFunction, 2>& edgeFunctions,
    const qrp::color::GradientPalette& palette,
    const ContentSelection contentSelection,
    const bool enableCoons) {
    auto image = renderWangContent(
        tiling, atlas, edgeFunctions, palette, contentSelection, enableCoons);
    const auto repeated = renderWangContent(
        tiling, atlas, edgeFunctions, palette, contentSelection, enableCoons);
    const bool reproducible = imagesEqual(image, repeated);
    return {
        std::move(image),
        measureWangContentSeams(
            tiling,
            atlas,
            edgeFunctions,
            palette,
            contentSelection,
            enableCoons),
        {},
        reproducible,
    };
}

void writeCaseJson(
    std::ostream& output,
    const std::string_view id,
    const RenderCase& renderCase) {
    const auto writeShiftValues = [&](const bool horizontal) {
        output << '[';
        for (std::size_t index = 0; index < renderCase.tileShifts.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output << (horizontal
                ? renderCase.tileShifts[index].horizontal
                : renderCase.tileShifts[index].vertical);
        }
        output << ']';
    };
    output << "    {\n"
           << "      \"id\": \"" << id << "\",\n"
           << "      \"reproducible\": "
           << (renderCase.reproducible ? "true" : "false") << ",\n"
           << "      \"seamSampleCount\": "
           << renderCase.seams.sampleCount << ",\n"
           << "      \"inverseFailureCount\": "
           << renderCase.seams.inverseFailureCount << ",\n"
           << "      \"maximumScalarSeamDifference\": "
           << renderCase.seams.maximumScalarDifference << ",\n"
           << "      \"maximumQuantizedSeamDifference\": "
           << static_cast<unsigned int>(
                  renderCase.seams.maximumQuantizedChannelDifference)
           << ",\n"
           << "      \"meanOneTileShiftDifferenceX\": "
           << renderCase.tileShifts.front().horizontal << ",\n"
           << "      \"meanOneTileShiftDifferenceY\": "
           << renderCase.tileShifts.front().vertical << ",\n"
           << "      \"meanTileShiftDifferencesX\": ";
    writeShiftValues(true);
    output << ",\n"
           << "      \"meanTileShiftDifferencesY\": ";
    writeShiftValues(false);
    output << "\n    }";
}

void writeSignatureUsageJson(
    std::ostream& output,
    const std::string_view id,
    const std::size_t dictionaryTileCount,
    const SignatureUsageMetrics& usage) {
    const auto writeRates = [&](const bool horizontal) {
        output << '[';
        for (std::size_t index = 0; index < usage.sameSignatureRates.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output << (horizontal
                ? usage.sameSignatureRates[index].horizontal
                : usage.sameSignatureRates[index].vertical);
        }
        output << ']';
    };
    output << "    {\n"
           << "      \"id\": \"" << id << "\",\n"
           << "      \"dictionaryTileCount\": " << dictionaryTileCount << ",\n"
           << "      \"uniqueSignaturesUsed\": "
           << usage.uniqueSignatureCount << ",\n"
           << "      \"maximumSignatureUseCount\": "
           << usage.maximumSignatureUseCount << ",\n"
           << "      \"effectiveSignatureCount\": "
           << usage.effectiveSignatureCount << ",\n"
           << "      \"sameSignatureRatesX\": ";
    writeRates(true);
    output << ",\n"
           << "      \"sameSignatureRatesY\": ";
    writeRates(false);
    output << "\n    }";
}

void writeMetrics(
    const std::filesystem::path& path,
    const ExperimentConfig& config,
    const qrp::atlas::WangTextureSampleOptimizationReport& optimization,
    const qrp::atlas::WangTextureAtlasBuildResult& minimalBuild,
    const qrp::atlas::AtlasSeamMetrics& minimalAtlasSeams,
    const SignatureUsageMetrics& minimalUsage,
    const std::size_t minimalUniqueTileImages,
    const qrp::atlas::WangTextureAtlasBuildResult& completeBuild,
    const qrp::atlas::AtlasSeamMetrics& completeAtlasSeams,
    const SignatureUsageMetrics& completeUsage,
    const std::size_t completeUniqueTileImages,
    const RenderCase& common,
    const RenderCase& signatureBlind,
    const RenderCase& content,
    const RenderCase& combined,
    const RenderCase& completeContent) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to open Wang-QRP ablation metrics output.");
    }
    output << std::setprecision(17)
           << "{\n"
           << "  \"schemaVersion\": 4,\n"
           << "  \"experiment\": \"wang-conditioned-qrp-ablation\",\n"
           << "  \"configuration\": \"" << config.id << "\",\n"
           << "  \"sourcePhases\": [";
    for (std::size_t index = 0; index < config.phases.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << config.phases[index];
    }
    output << "],\n"
           << "  \"sampleSearchSeedHex\": \"0x"
           << std::hex << config.sampleSearchSeed << std::dec << "\",\n"
           << "  \"gridSeedHex\": \"0x"
           << std::hex << config.gridSeed << std::dec << "\",\n"
           << "  \"gridWidth\": " << kGridWidth << ",\n"
           << "  \"gridHeight\": " << kGridHeight << ",\n"
           << "  \"pixelsPerTile\": " << kPixelsPerTile << ",\n"
           << "  \"edgeLabelCount\": 2,\n"
           << "  \"minimalAtlasTileCount\": "
           << minimalBuild.atlas.tiles().size() << ",\n"
           << "  \"completeAtlasTileCount\": "
           << completeBuild.atlas.tiles().size() << ",\n"
           << "  \"minimalAtlasUniqueTileImageCount\": "
           << minimalUniqueTileImages << ",\n"
           << "  \"completeAtlasUniqueTileImageCount\": "
           << completeUniqueTileImages << ",\n"
           << "  \"noiseEnabled\": false,\n"
           << "  \"sampleOptimizationTarget\": \"minimal-even-parity-eight\",\n"
           << "  \"completeAtlasUsesSharedSampleBank\": true,\n"
           << "  \"sampleCandidateGroupCount\": "
           << optimization.evaluatedCandidateGroupCount << ",\n"
           << "  \"firstCandidateCutCost\": "
           << optimization.firstCandidateCutCost << ",\n"
           << "  \"bestCandidateCutCost\": "
           << optimization.bestCutCost << ",\n"
           << "  \"minimalAtlasMeanSquaredRgbCutCostPerPathPixel\": "
           << static_cast<double>(minimalBuild.report.sumOfIndependentCutCosts)
                / static_cast<double>(minimalBuild.report.independentCutPixelCount)
           << ",\n"
           << "  \"completeAtlasMeanSquaredRgbCutCostPerPathPixel\": "
           << static_cast<double>(completeBuild.report.sumOfIndependentCutCosts)
                / static_cast<double>(completeBuild.report.independentCutPixelCount)
           << ",\n"
           << "  \"minimalAtlasMaximumQuantizedSeamDifference\": "
           << static_cast<unsigned int>(minimalAtlasSeams.maximumChannelDifference)
           << ",\n"
           << "  \"completeAtlasMaximumQuantizedSeamDifference\": "
           << static_cast<unsigned int>(completeAtlasSeams.maximumChannelDifference)
           << ",\n"
           << "  \"cases\": [\n";
    writeCaseJson(output, "A_common_qrp_coons", common);
    output << ",\n";
    writeCaseJson(output, "B0_signature_blind_identity", signatureBlind);
    output << ",\n";
    writeCaseJson(output, "B_wang_content_identity", content);
    output << ",\n";
    writeCaseJson(output, "C_wang_content_coons", combined);
    output << ",\n";
    writeCaseJson(output, "D_complete16_shared_samples_identity", completeContent);
    output << "\n  ],\n"
           << "  \"signatureUsage\": [\n";
    writeSignatureUsageJson(
        output,
        "B_minimal8",
        minimalBuild.atlas.tiles().size(),
        minimalUsage);
    output << ",\n";
    writeSignatureUsageJson(
        output,
        "D_complete16_shared_samples",
        completeBuild.atlas.tiles().size(),
        completeUsage);
    output << "\n  ]\n}\n";
}

void validateCase(const std::string_view id, const RenderCase& renderCase) {
    if (!renderCase.reproducible) {
        throw std::runtime_error(std::string(id) + " is not reproducible.");
    }
    if (renderCase.seams.inverseFailureCount != 0
        || renderCase.seams.maximumQuantizedChannelDifference != 0) {
        throw std::runtime_error(
            std::string(id) + " contains a non-zero quantized boundary seam.");
    }
}

[[nodiscard]] const ExperimentConfig& selectConfig(
    const std::string_view id) {
    if (id == kPrimaryConfig.id) {
        return kPrimaryConfig;
    }
    if (id == kHoldoutConfig.id) {
        return kHoldoutConfig;
    }
    throw std::invalid_argument(
        "Unknown experiment configuration; use 'primary' or 'holdout'.");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 3) {
            throw std::invalid_argument(
                "Usage: qrp_wang_qrp_ablation [output-directory] [primary|holdout]");
        }
        const auto& config = selectConfig(argc > 2 ? argv[2] : "primary");
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/wang-qrp-ablation");
        std::filesystem::create_directories(outputDirectory);

        const auto palette = qrp::color::GradientPalette::createMidnightGold();
        const auto scalarSource = createQrpSource(config.phases);
        const auto edgeFunctions = createEdgeFunctions();
        const qrp::atlas::WangTextureSampleOptimizationOptions searchOptions{
            kPatchSize,
            qrp::atlas::WangTextureAtlasBuildOptions{
                kOverlapPixels,
                kPixelsPerTile,
                {128, 128, 128},
                false,
            },
            config.sampleSearchSeed,
            kCandidateGroupCount,
            kMinimumOriginDistancePixels,
        };
        const auto optimized = qrp::atlas::WangTextureSampleOptimizer::optimize(
            scalarSource,
            searchOptions);
        const auto minimalBuild =
            qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                optimized.samples,
                searchOptions.buildOptions);
        const auto completeBuild =
            qrp::atlas::WangTextureAtlasBuilder::buildCompleteSixteen(
                optimized.samples,
                searchOptions.buildOptions);
        const qrp::atlas::WangAtlasTiling minimalTiling(
            kGridWidth,
            kGridHeight,
            minimalBuild.atlas,
            config.gridSeed);
        const qrp::atlas::WangAtlasTiling completeTiling(
            kGridWidth,
            kGridHeight,
            completeBuild.atlas,
            config.gridSeed);
        const auto minimalAtlasSeams =
            qrp::atlas::AtlasRenderer::measureAtlasCompatibility(
                minimalBuild.atlas);
        const auto completeAtlasSeams =
            qrp::atlas::AtlasRenderer::measureAtlasCompatibility(
                completeBuild.atlas);
        if (minimalAtlasSeams.maximumChannelDifference != 0
            || completeAtlasSeams.maximumChannelDifference != 0) {
            throw std::runtime_error(
                "A constructed scalar atlas has a non-zero boundary seam.");
        }
        const auto completeCoverage = completeBuild.atlas.coverage();
        if (!completeCoverage.complete || completeCoverage.tileCount != 16) {
            throw std::runtime_error(
                "The expanded scalar atlas does not contain all 16 signatures.");
        }
        const auto minimalUsage = measureSignatureUsage(minimalTiling);
        const auto completeUsage = measureSignatureUsage(completeTiling);
        const std::size_t minimalUniqueTileImages = countUniqueTileImages(
            minimalBuild.atlas);
        const std::size_t completeUniqueTileImages = countUniqueTileImages(
            completeBuild.atlas);
        if (minimalUniqueTileImages != 8 || completeUniqueTileImages != 16) {
            throw std::runtime_error(
                "The scalar atlas contains duplicate tile images.");
        }

        const auto commonGenerator = qrp::generators::TorusFourier::createQuasiRegular();
        auto commonImage = renderCommonQrp(
            minimalTiling, edgeFunctions, commonGenerator, palette);
        auto commonRepeated = renderCommonQrp(
            minimalTiling, edgeFunctions, commonGenerator, palette);
        const bool commonReproducible = imagesEqual(commonImage, commonRepeated);
        RenderCase common{
            std::move(commonImage),
            measureCommonQrpSeams(
                minimalTiling, edgeFunctions, commonGenerator, palette),
            {},
            commonReproducible,
        };
        common.tileShifts = measureTileShifts(common.image);

        auto signatureBlind = makeWangContentCase(
            minimalTiling,
            minimalBuild.atlas,
            edgeFunctions,
            palette,
            ContentSelection::FixedZeroSignature,
            false);
        signatureBlind.tileShifts = measureTileShifts(signatureBlind.image);
        auto content = makeWangContentCase(
            minimalTiling,
            minimalBuild.atlas,
            edgeFunctions,
            palette,
            ContentSelection::WangSignature,
            false);
        content.tileShifts = measureTileShifts(content.image);
        auto combined = makeWangContentCase(
            minimalTiling,
            minimalBuild.atlas,
            edgeFunctions,
            palette,
            ContentSelection::WangSignature,
            true);
        combined.tileShifts = measureTileShifts(combined.image);
        auto completeContent = makeWangContentCase(
            completeTiling,
            completeBuild.atlas,
            edgeFunctions,
            palette,
            ContentSelection::WangSignature,
            false);
        completeContent.tileShifts = measureTileShifts(completeContent.image);

        validateCase("A_common_qrp_coons", common);
        validateCase("B0_signature_blind_identity", signatureBlind);
        validateCase("B_wang_content_identity", content);
        validateCase("C_wang_content_coons", combined);
        validateCase("D_complete16_shared_samples_identity", completeContent);
        for (const auto shift : signatureBlind.tileShifts) {
            if (shift.horizontal != 0.0 || shift.vertical != 0.0) {
                throw std::runtime_error(
                    "The signature-blind control must repeat at every tile lag.");
            }
        }

        qrp::exporting::writePng(
            colorizeScalarImage(scalarSource, palette),
            outputDirectory / "source_qrp.png");
        qrp::exporting::writePng(
            createAtlasSheet(minimalBuild.atlas, palette),
            outputDirectory / "wang_content_tiles.png");
        qrp::exporting::writePng(
            createAtlasSheet(completeBuild.atlas, palette),
            outputDirectory / "wang_content_tiles_16.png");
        qrp::exporting::writePng(
            common.image,
            outputDirectory / "A_common_qrp_coons.png");
        qrp::exporting::writePng(
            signatureBlind.image,
            outputDirectory / "B0_signature_blind_identity.png");
        qrp::exporting::writePng(
            content.image,
            outputDirectory / "B_wang_content_identity.png");
        qrp::exporting::writePng(
            combined.image,
            outputDirectory / "C_wang_content_coons.png");
        qrp::exporting::writePng(
            completeContent.image,
            outputDirectory / "D_complete16_shared_samples_identity.png");
        writeMetrics(
            outputDirectory / "metrics.json",
            config,
            optimized.report,
            minimalBuild,
            minimalAtlasSeams,
            minimalUsage,
            minimalUniqueTileImages,
            completeBuild,
            completeAtlasSeams,
            completeUsage,
            completeUniqueTileImages,
            common,
            signatureBlind,
            content,
            combined,
            completeContent);

        const auto printCase = [](const std::string_view id, const RenderCase& value) {
            std::cout << id
                      << ": shift_x=" << value.tileShifts.front().horizontal
                      << ", shift_y=" << value.tileShifts.front().vertical
                      << ", scalar_seam=" << value.seams.maximumScalarDifference
                      << ", quantized_seam="
                      << static_cast<unsigned int>(
                             value.seams.maximumQuantizedChannelDifference)
                      << '\n';
        };
        printCase("A_common_qrp_coons", common);
        printCase("B0_signature_blind_identity", signatureBlind);
        printCase("B_wang_content_identity", content);
        printCase("C_wang_content_coons", combined);
        printCase("D_complete16_shared_samples_identity", completeContent);
        std::cout << "configuration=" << config.id << '\n'
                  << "effective_signatures_8="
                  << minimalUsage.effectiveSignatureCount << '\n'
                  << "effective_signatures_16="
                  << completeUsage.effectiveSignatureCount << '\n'
                  << "Wrote Wang-QRP ablation to "
                  << outputDirectory.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Wang-QRP ablation failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
