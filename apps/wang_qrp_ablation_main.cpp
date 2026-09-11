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
constexpr std::size_t kCandidateGroupCount = 96;
constexpr std::size_t kMinimumOriginDistancePixels = 112;
constexpr std::uint64_t kSampleSearchSeed = 0x243f6a8885a308d3ULL;
constexpr std::uint64_t kGridSeed = 0x13198a2e03707344ULL;

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
    ShiftDifference oneTileShift;
    bool reproducible = false;
};

[[nodiscard]] double sampleQrpSource(
    const double x,
    const double y) noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    constexpr double cyclesPerPixel = 1.0 / 46.0;
    constexpr std::array<double, 5> phases{{
        0.17,
        1.31,
        2.46,
        3.72,
        5.09,
    }};

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

[[nodiscard]] qrp::render::Image createQrpSource() {
    qrp::render::Image image(kSourceSize, kSourceSize);
    for (std::size_t y = 0; y < kSourceSize; ++y) {
        for (std::size_t x = 0; x < kSourceSize; ++x) {
            const std::uint8_t value = encodeScalar(sampleQrpSource(
                static_cast<double>(x),
                static_cast<double>(y)));
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

[[nodiscard]] ShiftDifference measureOneTileShift(
    const qrp::render::Image& image) {
    return {
        meanShiftDifference(image, kPixelsPerTile, 0),
        meanShiftDifference(image, 0, kPixelsPerTile),
    };
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
    const bool enableCoons) {
    qrp::render::Image image(
        tiling.width() * kPixelsPerTile,
        tiling.height() * kPixelsPerTile);
    const double pixelsPerTile = static_cast<double>(kPixelsPerTile);
    for (std::size_t gridY = 0; gridY < tiling.height(); ++gridY) {
        const std::size_t tileTop = (tiling.height() - gridY - 1) * kPixelsPerTile;
        for (std::size_t gridX = 0; gridX < tiling.width(); ++gridX) {
            const auto edges = tiling.tile(gridX, gridY);
            const auto& content = atlas.select(edges).image;
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
        return sampleAtlasScalar(atlas.select(edges).image, parameter);
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
    constexpr std::size_t rows = 2;
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
    const bool enableCoons) {
    auto image = renderWangContent(
        tiling, atlas, edgeFunctions, palette, enableCoons);
    const auto repeated = renderWangContent(
        tiling, atlas, edgeFunctions, palette, enableCoons);
    const bool reproducible = imagesEqual(image, repeated);
    return {
        std::move(image),
        measureWangContentSeams(
            tiling, atlas, edgeFunctions, palette, enableCoons),
        {},
        reproducible,
    };
}

void writeCaseJson(
    std::ostream& output,
    const std::string_view id,
    const RenderCase& renderCase) {
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
           << renderCase.oneTileShift.horizontal << ",\n"
           << "      \"meanOneTileShiftDifferenceY\": "
           << renderCase.oneTileShift.vertical << "\n"
           << "    }";
}

void writeMetrics(
    const std::filesystem::path& path,
    const qrp::atlas::WangTextureSampleOptimizationReport& optimization,
    const qrp::atlas::WangTextureAtlasBuildResult& build,
    const qrp::atlas::AtlasSeamMetrics& atlasSeams,
    const RenderCase& common,
    const RenderCase& content,
    const RenderCase& combined) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to open Wang-QRP ablation metrics output.");
    }
    output << std::setprecision(17)
           << "{\n"
           << "  \"schemaVersion\": 1,\n"
           << "  \"experiment\": \"wang-conditioned-qrp-ablation\",\n"
           << "  \"gridWidth\": " << kGridWidth << ",\n"
           << "  \"gridHeight\": " << kGridHeight << ",\n"
           << "  \"pixelsPerTile\": " << kPixelsPerTile << ",\n"
           << "  \"edgeLabelCount\": 2,\n"
           << "  \"tileCount\": " << build.atlas.tiles().size() << ",\n"
           << "  \"noiseEnabled\": false,\n"
           << "  \"sampleCandidateGroupCount\": "
           << optimization.evaluatedCandidateGroupCount << ",\n"
           << "  \"firstCandidateCutCost\": "
           << optimization.firstCandidateCutCost << ",\n"
           << "  \"bestCandidateCutCost\": "
           << optimization.bestCutCost << ",\n"
           << "  \"atlasMaximumQuantizedSeamDifference\": "
           << static_cast<unsigned int>(atlasSeams.maximumChannelDifference)
           << ",\n"
           << "  \"cases\": [\n";
    writeCaseJson(output, "A_common_qrp_coons", common);
    output << ",\n";
    writeCaseJson(output, "B_wang_content_identity", content);
    output << ",\n";
    writeCaseJson(output, "C_wang_content_coons", combined);
    output << "\n  ]\n}\n";
}

void validateCase(const std::string_view id, const RenderCase& renderCase) {
    if (!renderCase.reproducible) {
        throw std::runtime_error(std::string(id) + " is not reproducible.");
    }
    if (renderCase.seams.inverseFailureCount != 0
        || renderCase.seams.maximumQuantizedChannelDifference != 0) {
        throw std::runtime_error(std::string(id) + " contains a visible seam.");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/wang-qrp-ablation");
        std::filesystem::create_directories(outputDirectory);

        const auto palette = qrp::color::GradientPalette::createMidnightGold();
        const auto scalarSource = createQrpSource();
        const auto edgeFunctions = createEdgeFunctions();
        const qrp::atlas::WangTextureSampleOptimizationOptions searchOptions{
            kPatchSize,
            qrp::atlas::WangTextureAtlasBuildOptions{
                kOverlapPixels,
                kPixelsPerTile,
                {128, 128, 128},
                false,
            },
            kSampleSearchSeed,
            kCandidateGroupCount,
            kMinimumOriginDistancePixels,
        };
        const auto optimized = qrp::atlas::WangTextureSampleOptimizer::optimize(
            scalarSource,
            searchOptions);
        const auto build = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
            optimized.samples,
            searchOptions.buildOptions);
        const qrp::atlas::WangAtlasTiling tiling(
            kGridWidth,
            kGridHeight,
            build.atlas,
            kGridSeed);
        const auto atlasSeams = qrp::atlas::AtlasRenderer::measureAtlasCompatibility(
            build.atlas);
        if (atlasSeams.maximumChannelDifference != 0) {
            throw std::runtime_error("The constructed scalar atlas has a non-zero seam.");
        }

        const auto commonGenerator = qrp::generators::TorusFourier::createQuasiRegular();
        auto commonImage = renderCommonQrp(
            tiling, edgeFunctions, commonGenerator, palette);
        auto commonRepeated = renderCommonQrp(
            tiling, edgeFunctions, commonGenerator, palette);
        const bool commonReproducible = imagesEqual(commonImage, commonRepeated);
        RenderCase common{
            std::move(commonImage),
            measureCommonQrpSeams(
                tiling, edgeFunctions, commonGenerator, palette),
            {},
            commonReproducible,
        };
        common.oneTileShift = measureOneTileShift(common.image);

        auto content = makeWangContentCase(
            tiling, build.atlas, edgeFunctions, palette, false);
        content.oneTileShift = measureOneTileShift(content.image);
        auto combined = makeWangContentCase(
            tiling, build.atlas, edgeFunctions, palette, true);
        combined.oneTileShift = measureOneTileShift(combined.image);

        validateCase("A_common_qrp_coons", common);
        validateCase("B_wang_content_identity", content);
        validateCase("C_wang_content_coons", combined);

        qrp::exporting::writePng(
            colorizeScalarImage(scalarSource, palette),
            outputDirectory / "source_qrp.png");
        qrp::exporting::writePng(
            createAtlasSheet(build.atlas, palette),
            outputDirectory / "wang_content_tiles.png");
        qrp::exporting::writePng(
            common.image,
            outputDirectory / "A_common_qrp_coons.png");
        qrp::exporting::writePng(
            content.image,
            outputDirectory / "B_wang_content_identity.png");
        qrp::exporting::writePng(
            combined.image,
            outputDirectory / "C_wang_content_coons.png");
        writeMetrics(
            outputDirectory / "metrics.json",
            optimized.report,
            build,
            atlasSeams,
            common,
            content,
            combined);

        const auto printCase = [](const std::string_view id, const RenderCase& value) {
            std::cout << id
                      << ": shift_x=" << value.oneTileShift.horizontal
                      << ", shift_y=" << value.oneTileShift.vertical
                      << ", scalar_seam=" << value.seams.maximumScalarDifference
                      << ", quantized_seam="
                      << static_cast<unsigned int>(
                             value.seams.maximumQuantizedChannelDifference)
                      << '\n';
        };
        printCase("A_common_qrp_coons", common);
        printCase("B_wang_content_identity", content);
        printCase("C_wang_content_coons", combined);
        std::cout << "Wrote Wang-QRP ablation to "
                  << outputDirectory.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Wang-QRP ablation failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
