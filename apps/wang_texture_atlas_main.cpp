#include "atlas/AtlasRenderer.hpp"
#include "atlas/WangAtlasTiling.hpp"
#include "atlas/WangTextureAtlasBuilder.hpp"
#include "atlas/WangTextureSampleOptimizer.hpp"
#include "export/PngWriter.hpp"
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
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kPatchSize = 128;
constexpr std::size_t kOverlapPixels = 32;
constexpr std::size_t kSourceSize = 512;
constexpr std::size_t kSampleCandidateGroupCount = 128;
constexpr std::size_t kMinimumOriginDistancePixels = 96;
constexpr std::size_t kGridWidth = 10;
constexpr std::size_t kGridHeight = 10;
constexpr std::uint64_t kSampleSearchSeed = 0xbb67ae8584caa73bULL;
constexpr std::uint64_t kGridSeed = 0x6a09e667f3bcc909ULL;
constexpr qrp::render::Rgb8 kUniversalCornerColor{110, 125, 92};

struct SheetEntry {
    qrp::atlas::WangEdgeSignature edges;
    const qrp::render::Image* image = nullptr;
};

[[nodiscard]] double smoothstep(const double value) noexcept {
    const double t = std::clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] qrp::render::Rgb8 mixColor(
    const qrp::render::Rgb8 first,
    const qrp::render::Rgb8 second,
    const double amount) noexcept {
    const double t = std::clamp(amount, 0.0, 1.0);
    const auto channel = [t](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(std::lerp(
            static_cast<double>(a),
            static_cast<double>(b),
            t)));
    };
    return {
        channel(first.red, second.red),
        channel(first.green, second.green),
        channel(first.blue, second.blue),
    };
}

[[nodiscard]] qrp::render::Rgb8 sampleAnalyticTexture(
    const double sourceX,
    const double sourceY) noexcept {
    const double warpedX = sourceX
        + 8.0 * std::sin(0.027 * sourceY)
        + 3.5 * std::sin(0.061 * sourceX + 0.019 * sourceY);
    const double warpedY = sourceY
        + 7.0 * std::cos(0.025 * sourceX)
        - 3.0 * std::sin(0.022 * sourceX - 0.057 * sourceY);
    const double axisA = std::cos(
        0.105 * warpedX + 0.35 * std::sin(0.031 * warpedY));
    const double axisB = std::cos(
        0.096 * (-0.52 * warpedX + 0.854 * warpedY) + 0.9);
    const double axisC = std::cos(
        0.089 * (0.61 * warpedX + 0.793 * warpedY) - 0.7);
    const double broad = (axisA + axisB + axisC) / 3.0;
    const double fine = 0.5 + 0.5 * std::sin(
        0.176 * warpedX - 0.143 * warpedY
        + 0.58 * std::sin(0.047 * warpedY));
    const double layer = smoothstep(0.48 + 0.42 * broad);
    const double detail = std::clamp(0.84 * layer + 0.16 * fine, 0.0, 1.0);

    constexpr qrp::render::Rgb8 deep{25, 45, 53};
    constexpr qrp::render::Rgb8 moss{72, 111, 88};
    constexpr qrp::render::Rgb8 sand{190, 158, 101};
    if (detail < 0.52) {
        return mixColor(deep, moss, smoothstep(detail / 0.52));
    }
    return mixColor(moss, sand, smoothstep((detail - 0.52) / 0.48));
}

[[nodiscard]] qrp::render::Image createAnalyticSource() {
    qrp::render::Image image(kSourceSize, kSourceSize);
    for (std::size_t y = 0; y < kSourceSize; ++y) {
        for (std::size_t x = 0; x < kSourceSize; ++x) {
            image.pixel(x, y) = sampleAnalyticTexture(
                static_cast<double>(x),
                static_cast<double>(y));
        }
    }
    return image;
}

void fillImage(qrp::render::Image& image, const qrp::render::Rgb8 color) {
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            image.pixel(x, y) = color;
        }
    }
}

void blit(
    qrp::render::Image& destination,
    const qrp::render::Image& source,
    const std::size_t destinationX,
    const std::size_t destinationY) {
    for (std::size_t y = 0; y < source.height(); ++y) {
        for (std::size_t x = 0; x < source.width(); ++x) {
            destination.pixel(destinationX + x, destinationY + y) = source.pixel(x, y);
        }
    }
}

[[nodiscard]] qrp::render::Rgb8 northSouthLabelColor(
    const std::uint32_t label) {
    constexpr std::array<qrp::render::Rgb8, 2> colors{{
        {226, 73, 91},
        {66, 185, 123},
    }};
    return colors.at(label);
}

[[nodiscard]] qrp::render::Rgb8 westEastLabelColor(
    const std::uint32_t label) {
    constexpr std::array<qrp::render::Rgb8, 2> colors{{
        {242, 184, 61},
        {62, 130, 230},
    }};
    return colors.at(label);
}

void drawTileFrame(
    qrp::render::Image& sheet,
    const qrp::atlas::WangEdgeSignature edges,
    const std::size_t originX,
    const std::size_t originY,
    const std::size_t imageSize,
    const std::size_t frameWidth) {
    constexpr qrp::render::Rgb8 corner{15, 18, 24};
    const std::size_t outerSize = imageSize + 2 * frameWidth;
    for (std::size_t y = 0; y < outerSize; ++y) {
        for (std::size_t x = 0; x < outerSize; ++x) {
            const bool horizontalFrame = y < frameWidth || y >= outerSize - frameWidth;
            const bool verticalFrame = x < frameWidth || x >= outerSize - frameWidth;
            if (horizontalFrame && verticalFrame) {
                sheet.pixel(originX + x, originY + y) = corner;
            } else if (y < frameWidth) {
                sheet.pixel(originX + x, originY + y)
                    = northSouthLabelColor(edges.north);
            } else if (y >= outerSize - frameWidth) {
                sheet.pixel(originX + x, originY + y)
                    = northSouthLabelColor(edges.south);
            } else if (x < frameWidth) {
                sheet.pixel(originX + x, originY + y)
                    = westEastLabelColor(edges.west);
            } else if (x >= outerSize - frameWidth) {
                sheet.pixel(originX + x, originY + y)
                    = westEastLabelColor(edges.east);
            }
        }
    }
}

[[nodiscard]] qrp::render::Image createTileSheet(
    const std::vector<SheetEntry>& entries) {
    if (entries.empty() || entries.front().image == nullptr) {
        throw std::invalid_argument("A tile sheet requires image entries.");
    }
    constexpr std::size_t columns = 4;
    constexpr std::size_t gap = 10;
    constexpr std::size_t frameWidth = 4;
    const std::size_t imageSize = entries.front().image->width();
    if (entries.front().image->height() != imageSize) {
        throw std::invalid_argument("Tile sheet entries must be square.");
    }
    for (const auto& entry : entries) {
        if (entry.image == nullptr
            || entry.image->width() != imageSize
            || entry.image->height() != imageSize) {
            throw std::invalid_argument("Tile sheet entries must share square dimensions.");
        }
    }
    const std::size_t rows = (entries.size() + columns - 1) / columns;
    const std::size_t cellSize = imageSize + 2 * frameWidth;
    qrp::render::Image sheet(
        columns * cellSize + (columns + 1) * gap,
        rows * cellSize + (rows + 1) * gap);
    fillImage(sheet, qrp::render::Rgb8{10, 13, 18});
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const std::size_t column = index % columns;
        const std::size_t row = index / columns;
        const std::size_t originX = gap + column * (cellSize + gap);
        const std::size_t originY = gap + row * (cellSize + gap);
        drawTileFrame(
            sheet,
            entries[index].edges,
            originX,
            originY,
            imageSize,
            frameWidth);
        blit(
            sheet,
            *entries[index].image,
            originX + frameWidth,
            originY + frameWidth);
    }
    return sheet;
}

[[nodiscard]] qrp::render::Image createSampleSheet(
    const qrp::atlas::WangEdgeSampleBank& samples) {
    constexpr std::size_t gap = 10;
    constexpr std::size_t frameWidth = 5;
    constexpr std::size_t columns = 2;
    constexpr std::size_t rows = 2;
    const std::size_t cellSize = kPatchSize + 2 * frameWidth;
    qrp::render::Image sheet(
        columns * cellSize + (columns + 1) * gap,
        rows * cellSize + (rows + 1) * gap);
    fillImage(sheet, qrp::render::Rgb8{10, 13, 18});
    const std::array<const qrp::render::Image*, 4> images{{
        &samples.northSouth[0],
        &samples.northSouth[1],
        &samples.westEast[0],
        &samples.westEast[1],
    }};
    const std::array<qrp::render::Rgb8, 4> colors{{
        northSouthLabelColor(0),
        northSouthLabelColor(1),
        westEastLabelColor(0),
        westEastLabelColor(1),
    }};
    for (std::size_t index = 0; index < images.size(); ++index) {
        const std::size_t column = index % columns;
        const std::size_t row = index / columns;
        const std::size_t originX = gap + column * (cellSize + gap);
        const std::size_t originY = gap + row * (cellSize + gap);
        for (std::size_t y = 0; y < cellSize; ++y) {
            for (std::size_t x = 0; x < cellSize; ++x) {
                if (x < frameWidth || x >= cellSize - frameWidth
                    || y < frameWidth || y >= cellSize - frameWidth) {
                    sheet.pixel(originX + x, originY + y) = colors[index];
                }
            }
        }
        blit(sheet, *images[index], originX + frameWidth, originY + frameWidth);
    }
    return sheet;
}

[[nodiscard]] std::vector<SheetEntry> atlasEntries(
    const qrp::atlas::WangTileAtlas& atlas) {
    std::vector<SheetEntry> entries;
    entries.reserve(atlas.tiles().size());
    for (const auto& tile : atlas.tiles()) {
        entries.push_back({tile.edges, &tile.image});
    }
    return entries;
}

[[nodiscard]] std::vector<SheetEntry> debugEntries(
    const std::vector<qrp::atlas::WangTextureTileDebugImage>& debugImages) {
    std::vector<SheetEntry> entries;
    entries.reserve(debugImages.size());
    for (const auto& item : debugImages) {
        entries.push_back({item.edges, &item.image});
    }
    return entries;
}

[[nodiscard]] std::size_t uniqueSignaturesUsed(
    const qrp::atlas::WangAtlasTiling& tiling) {
    std::set<std::array<std::uint32_t, 4>> signatures;
    for (const auto edges : tiling.tiles()) {
        signatures.insert({edges.south, edges.north, edges.west, edges.east});
    }
    return signatures.size();
}

void validateResult(
    const qrp::atlas::WangTextureAtlasBuildResult& result,
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::AtlasSeamMetrics& atlasSeams,
    const qrp::atlas::AtlasSeamMetrics& renderedSeams) {
    const auto coverage = result.atlas.coverage();
    if (coverage.complete
        || coverage.tileCount != 8
        || coverage.signatureCount != 8
        || coverage.expectedSignatureCount != 16) {
        throw std::runtime_error("The texture builder did not produce the expected S8 atlas.");
    }
    if (result.report.independentCutCount != 32
        || result.report.independentCutPixelCount != 32 * kPatchSize) {
        throw std::runtime_error("The texture builder did not report all 32 cuts.");
    }
    if (result.cutPathImages.size() != 8) {
        throw std::runtime_error("Debug output must contain one cut-path image per tile.");
    }
    if (!tiling.hasValidAdjacency()) {
        throw std::runtime_error("The generated texture tiling violates Wang adjacency.");
    }
    if (atlasSeams.mismatchedPixelCount != 0
        || atlasSeams.maximumChannelDifference != 0
        || renderedSeams.mismatchedPixelCount != 0
        || renderedSeams.maximumChannelDifference != 0) {
        throw std::runtime_error("The generated texture atlas contains a non-zero RGB seam.");
    }
}

void writeMetrics(
    const std::filesystem::path& path,
    const qrp::render::Rgb8 cornerColor,
    const qrp::render::Image& source,
    const qrp::atlas::WangTextureSampleOptimizationReport& optimization,
    const qrp::atlas::WangTextureAtlasBuildResult& result,
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::AtlasSeamMetrics& atlasSeams,
    const qrp::atlas::AtlasSeamMetrics& renderedSeams) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to open Wang texture atlas metrics output.");
    }
    std::ostringstream seedText;
    seedText << "0x" << std::hex << tiling.seed();
    std::ostringstream searchSeedText;
    searchSeedText << "0x" << std::hex << kSampleSearchSeed;

    std::size_t correctedBoundaryPixels = 0;
    std::uint8_t maximumBoundaryCorrection = 0;
    double meanBoundaryCorrection = 0.0;
    std::uint8_t maximumEdgeInwardDifference = 0;
    double meanEdgeInwardDifference = 0.0;
    for (const auto& tile : result.report.tiles) {
        correctedBoundaryPixels += tile.correctedBoundaryPixelCount;
        maximumBoundaryCorrection = std::max(
            maximumBoundaryCorrection,
            tile.maximumBoundaryChannelCorrection);
        meanBoundaryCorrection += tile.meanBoundaryChannelCorrection;
        maximumEdgeInwardDifference = std::max(
            maximumEdgeInwardDifference,
            tile.maximumEdgeInwardChannelDifference);
        meanEdgeInwardDifference += tile.meanEdgeInwardChannelDifference;
    }
    meanBoundaryCorrection /= static_cast<double>(result.report.tiles.size());
    meanEdgeInwardDifference /= static_cast<double>(result.report.tiles.size());
    const double meanCutCost = static_cast<double>(
        result.report.sumOfIndependentCutCosts)
        / static_cast<double>(result.report.independentCutPixelCount);
    const std::uint64_t optimizationReduction =
        optimization.firstCandidateCutCost - optimization.bestCutCost;
    const double relativeOptimizationReduction =
        optimization.firstCandidateCutCost == 0
        ? 0.0
        : static_cast<double>(optimizationReduction)
            / static_cast<double>(optimization.firstCandidateCutCost);

    output << std::setprecision(12)
           << "{\n"
           << "  \"schemaVersion\": 1,\n"
           << "  \"experiment\": \"clean-room-wang-texture-atlas\",\n"
           << "  \"methodRole\": \"classic-related-method-baseline\",\n"
           << "  \"sampleConstruction\": \"optimized four-crop set from one deterministic analytic source\",\n"
           << "  \"randomnessRole\": \"seeded sample-set search only\",\n"
           << "  \"usesSeededRandomSampleSearch\": true,\n"
           << "  \"usesPerTileRandomContent\": false,\n"
           << "  \"usesMinimumErrorCuts\": true,\n"
           << "  \"includesDebugCutPaths\": true,\n"
           << "  \"cornerPolicy\": \"fixed universal RGB shared by every edge label\",\n"
           << "  \"sourceWidthPixels\": " << source.width() << ",\n"
           << "  \"sourceHeightPixels\": " << source.height() << ",\n"
           << "  \"sampleSearchSeedHex\": \""
           << searchSeedText.str() << "\",\n"
           << "  \"sampleCandidateGroupCount\": "
           << optimization.evaluatedCandidateGroupCount << ",\n"
           << "  \"minimumOriginDistancePixels\": "
           << kMinimumOriginDistancePixels << ",\n"
           << "  \"initialSampleSetSquaredRgbCutCost\": "
           << optimization.firstCandidateCutCost << ",\n"
           << "  \"bestSampleSetSquaredRgbCutCost\": "
           << optimization.bestCutCost << ",\n"
           << "  \"sampleSetCostReduction\": "
           << optimizationReduction << ",\n"
           << "  \"sampleSetRelativeCostReduction\": "
           << relativeOptimizationReduction << ",\n"
           << "  \"patchSize\": " << result.report.patchSize << ",\n"
           << "  \"overlapPixels\": " << result.report.overlapPixels << ",\n"
           << "  \"stridePixels\": " << result.report.stridePixels << ",\n"
           << "  \"outputResolutionPixels\": "
           << result.report.outputResolutionPixels << ",\n"
           << "  \"edgeLabelCount\": " << result.atlas.edgeLabelCount() << ",\n"
           << "  \"atlasTileCount\": " << result.atlas.tiles().size() << ",\n"
           << "  \"tileSetRule\": \"south XOR north XOR west XOR east = 0\",\n"
           << "  \"independentCutCount\": "
           << result.report.independentCutCount << ",\n"
           << "  \"independentCutPixelCount\": "
           << result.report.independentCutPixelCount << ",\n"
           << "  \"sumOfIndependentSquaredRgbCutCosts\": "
           << result.report.sumOfIndependentCutCosts << ",\n"
           << "  \"meanSquaredRgbCutCostPerPathPixel\": " << meanCutCost << ",\n"
           << "  \"cornerColor\": ["
           << static_cast<unsigned int>(cornerColor.red) << ", "
           << static_cast<unsigned int>(cornerColor.green) << ", "
           << static_cast<unsigned int>(cornerColor.blue) << "],\n"
           << "  \"correctedBoundaryPixelCount\": "
           << correctedBoundaryPixels << ",\n"
           << "  \"maximumBoundaryChannelCorrection\": "
           << static_cast<unsigned int>(maximumBoundaryCorrection) << ",\n"
           << "  \"meanBoundaryChannelCorrection\": "
           << meanBoundaryCorrection << ",\n"
           << "  \"maximumEdgeInwardChannelDifference\": "
           << static_cast<unsigned int>(maximumEdgeInwardDifference) << ",\n"
           << "  \"meanEdgeInwardChannelDifference\": "
           << meanEdgeInwardDifference << ",\n"
           << "  \"gridWidth\": " << tiling.width() << ",\n"
           << "  \"gridHeight\": " << tiling.height() << ",\n"
           << "  \"gridSeedHex\": \"" << seedText.str() << "\",\n"
           << "  \"gridAdjacencyValid\": "
           << (tiling.hasValidAdjacency() ? "true" : "false") << ",\n"
           << "  \"uniqueSignaturesUsed\": "
           << uniqueSignaturesUsed(tiling) << ",\n"
           << "  \"atlasCompatibilitySampleCount\": "
           << atlasSeams.sampleCount << ",\n"
           << "  \"atlasMismatchedBoundaryPixelCount\": "
           << atlasSeams.mismatchedPixelCount << ",\n"
           << "  \"atlasMaximumBoundaryChannelDifference\": "
           << static_cast<unsigned int>(atlasSeams.maximumChannelDifference) << ",\n"
           << "  \"renderedSeamSampleCount\": "
           << renderedSeams.sampleCount << ",\n"
           << "  \"renderedMismatchedSeamPixelCount\": "
           << renderedSeams.mismatchedPixelCount << ",\n"
           << "  \"renderedMaximumSeamChannelDifference\": "
           << static_cast<unsigned int>(renderedSeams.maximumChannelDifference) << ",\n"
           << "  \"sourceSamples\": [\n";
    const std::array<std::string, 4> sampleNames{{"NS0", "NS1", "WE0", "WE1"}};
    const std::array<qrp::atlas::WangTextureSampleOrigin, 4> sampleOrigins{{
        optimization.bestOrigins.northSouth[0],
        optimization.bestOrigins.northSouth[1],
        optimization.bestOrigins.westEast[0],
        optimization.bestOrigins.westEast[1],
    }};
    for (std::size_t index = 0; index < sampleOrigins.size(); ++index) {
        const auto origin = sampleOrigins[index];
        output << "    {\"name\": \"" << sampleNames[index]
               << "\", \"origin\": [" << origin.x
               << ", " << origin.y << "]}"
               << (index + 1 == sampleOrigins.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"tileMetrics\": [\n";
    for (std::size_t index = 0; index < result.report.tiles.size(); ++index) {
        const auto& tile = result.report.tiles[index];
        output << "    {\"edgesNESW\": ["
               << tile.edges.north << ", " << tile.edges.east << ", "
               << tile.edges.south << ", " << tile.edges.west << "], "
               << "\"cutCosts\": ["
               << tile.cutCosts.eastPlacementVertical << ", "
               << tile.cutCosts.westPlacementHorizontal << ", "
               << tile.cutCosts.southPlacementTop << ", "
               << tile.cutCosts.southPlacementLeft << "], "
               << "\"correctedBoundaryPixels\": "
               << tile.correctedBoundaryPixelCount << ", "
               << "\"maximumBoundaryChannelCorrection\": "
               << static_cast<unsigned int>(tile.maximumBoundaryChannelCorrection)
               << ", \"meanBoundaryChannelCorrection\": "
               << tile.meanBoundaryChannelCorrection << ", "
               << "\"maximumEdgeInwardChannelDifference\": "
               << static_cast<unsigned int>(tile.maximumEdgeInwardChannelDifference)
               << ", \"meanEdgeInwardChannelDifference\": "
               << tile.meanEdgeInwardChannelDifference << "}"
               << (index + 1 == result.report.tiles.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"outputs\": [\n"
           << "    \"A0_analytic_source.png\",\n"
           << "    \"A1_sample_NS0.png\",\n"
           << "    \"A2_sample_NS1.png\",\n"
           << "    \"A3_sample_WE0.png\",\n"
           << "    \"A4_sample_WE1.png\",\n"
           << "    \"A_edge_sample_bank.png\",\n"
           << "    \"B_quilted_8_tile_atlas.png\",\n"
           << "    \"C_minimum_error_cut_paths.png\",\n"
           << "    \"D_wang_texture_tiling_10x10.png\"\n"
           << "  ]\n"
           << "}\n";
    if (!output) {
        throw std::runtime_error("Unable to write Wang texture atlas metrics output.");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/wang-texture-atlas");
        std::filesystem::create_directories(outputDirectory);

        const auto source = createAnalyticSource();
        constexpr auto cornerColor = kUniversalCornerColor;
        const qrp::atlas::WangTextureAtlasBuildOptions buildOptions{
            kOverlapPixels,
            0,
            cornerColor,
            true,
        };
        const qrp::atlas::WangTextureSampleOptimizationOptions searchOptions{
            kPatchSize,
            buildOptions,
            kSampleSearchSeed,
            kSampleCandidateGroupCount,
            kMinimumOriginDistancePixels,
        };
        const auto optimization =
            qrp::atlas::WangTextureSampleOptimizer::optimize(
                source,
                searchOptions);
        const auto& samples = optimization.samples;
        const auto result = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
            samples,
            buildOptions);
        if (result.report.sumOfIndependentCutCosts
            != optimization.report.bestCutCost) {
            throw std::runtime_error(
                "The optimized sample set did not reproduce its search cost.");
        }
        const qrp::atlas::WangAtlasTiling tiling(
            kGridWidth,
            kGridHeight,
            result.atlas,
            kGridSeed);
        const auto atlasSeams =
            qrp::atlas::AtlasRenderer::measureAtlasCompatibility(result.atlas);
        const auto renderedSeams =
            qrp::atlas::AtlasRenderer::measureSeams(tiling, result.atlas);
        validateResult(result, tiling, atlasSeams, renderedSeams);

        qrp::exporting::writePng(
            source,
            outputDirectory / "A0_analytic_source.png");
        qrp::exporting::writePng(
            samples.northSouth[0],
            outputDirectory / "A1_sample_NS0.png");
        qrp::exporting::writePng(
            samples.northSouth[1],
            outputDirectory / "A2_sample_NS1.png");
        qrp::exporting::writePng(
            samples.westEast[0],
            outputDirectory / "A3_sample_WE0.png");
        qrp::exporting::writePng(
            samples.westEast[1],
            outputDirectory / "A4_sample_WE1.png");
        qrp::exporting::writePng(
            createSampleSheet(samples),
            outputDirectory / "A_edge_sample_bank.png");
        qrp::exporting::writePng(
            createTileSheet(atlasEntries(result.atlas)),
            outputDirectory / "B_quilted_8_tile_atlas.png");
        qrp::exporting::writePng(
            createTileSheet(debugEntries(result.cutPathImages)),
            outputDirectory / "C_minimum_error_cut_paths.png");
        qrp::exporting::writePng(
            qrp::atlas::AtlasRenderer::render(tiling, result.atlas),
            outputDirectory / "D_wang_texture_tiling_10x10.png");
        writeMetrics(
            outputDirectory / "metrics.json",
            cornerColor,
            source,
            optimization.report,
            result,
            tiling,
            atlasSeams,
            renderedSeams);

        std::cout << "wang_texture_tiles=" << result.atlas.tiles().size() << '\n'
                  << "output_resolution="
                  << result.report.outputResolutionPixels << '\n'
                  << "independent_cuts="
                  << result.report.independentCutCount << '\n'
                  << "sum_squared_rgb_cut_cost="
                  << result.report.sumOfIndependentCutCosts << '\n'
                  << "initial_sample_set_cost="
                  << optimization.report.firstCandidateCutCost << '\n'
                  << "sample_candidates_evaluated="
                  << optimization.report.evaluatedCandidateGroupCount << '\n'
                  << "atlas_mismatched_boundary_pixels="
                  << atlasSeams.mismatchedPixelCount << '\n'
                  << "rendered_mismatched_seam_pixels="
                  << renderedSeams.mismatchedPixelCount << '\n'
                  << "Wrote Wang texture atlas experiment to "
                  << outputDirectory.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Wang texture atlas experiment failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
