#include "atlas/AtlasRenderer.hpp"
#include "atlas/WangAtlasTiling.hpp"
#include "atlas/WangTileAtlas.hpp"
#include "export/PngWriter.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kEdgeLabelCount = 2;
constexpr std::size_t kTileSize = 96;
constexpr std::size_t kGridWidth = 10;
constexpr std::size_t kGridHeight = 10;
constexpr std::uint64_t kGridSeed = 0x4d595df4d0f33173ULL;

struct ColorAccumulator {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double weight = 0.0;
};

void addColor(
    ColorAccumulator& accumulator,
    const qrp::render::Rgb8 color,
    const double weight) noexcept {
    accumulator.red += weight * static_cast<double>(color.red);
    accumulator.green += weight * static_cast<double>(color.green);
    accumulator.blue += weight * static_cast<double>(color.blue);
    accumulator.weight += weight;
}

[[nodiscard]] qrp::render::Rgb8 finishColor(
    const ColorAccumulator& accumulator) noexcept {
    const auto channel = [weight = accumulator.weight](const double value) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(
            value / weight,
            0.0,
            255.0)));
    };
    return {
        channel(accumulator.red),
        channel(accumulator.green),
        channel(accumulator.blue),
    };
}

[[nodiscard]] qrp::render::Rgb8 northSouthColor(const std::uint32_t label) {
    constexpr std::array<qrp::render::Rgb8, 2> colors{{
        {226, 73, 91},
        {66, 185, 123},
    }};
    return colors.at(label);
}

[[nodiscard]] qrp::render::Rgb8 westEastColor(const std::uint32_t label) {
    constexpr std::array<qrp::render::Rgb8, 2> colors{{
        {242, 184, 61},
        {62, 130, 230},
    }};
    return colors.at(label);
}

[[nodiscard]] qrp::render::Image createDiagnosticTile(
    const qrp::atlas::WangEdgeSignature edges) {
    constexpr std::size_t borderWidth = 8;
    constexpr qrp::render::Rgb8 cornerColor{28, 31, 42};
    constexpr qrp::render::Rgb8 centerColor{42, 48, 61};
    qrp::render::Image image(kTileSize, kTileSize);

    for (std::size_t y = 0; y < kTileSize; ++y) {
        for (std::size_t x = 0; x < kTileSize; ++x) {
            const bool nearWestOrEastEdge = x < borderWidth
                || x >= kTileSize - borderWidth;
            const bool nearNorthOrSouthEdge = y < borderWidth
                || y >= kTileSize - borderWidth;
            if (nearWestOrEastEdge && nearNorthOrSouthEdge) {
                image.pixel(x, y) = cornerColor;
            } else if (y < borderWidth) {
                image.pixel(x, y) = northSouthColor(edges.north);
            } else if (y >= kTileSize - borderWidth) {
                image.pixel(x, y) = northSouthColor(edges.south);
            } else if (x < borderWidth) {
                image.pixel(x, y) = westEastColor(edges.west);
            } else if (x >= kTileSize - borderWidth) {
                image.pixel(x, y) = westEastColor(edges.east);
            } else {
                const double u = (static_cast<double>(x) + 0.5)
                    / static_cast<double>(kTileSize);
                const double v = (static_cast<double>(y) + 0.5)
                    / static_cast<double>(kTileSize);
                ColorAccumulator accumulator;
                addColor(accumulator, centerColor, 1.8);
                addColor(
                    accumulator,
                    northSouthColor(edges.north),
                    std::exp(-4.5 * v));
                addColor(
                    accumulator,
                    northSouthColor(edges.south),
                    std::exp(-4.5 * (1.0 - v)));
                addColor(
                    accumulator,
                    westEastColor(edges.west),
                    std::exp(-4.5 * u));
                addColor(
                    accumulator,
                    westEastColor(edges.east),
                    std::exp(-4.5 * (1.0 - u)));
                image.pixel(x, y) = finishColor(accumulator);
            }
        }
    }
    return image;
}

[[nodiscard]] std::string tileName(
    const qrp::atlas::WangEdgeSignature edges) {
    return "S" + std::to_string(edges.south)
        + "_N" + std::to_string(edges.north)
        + "_W" + std::to_string(edges.west)
        + "_E" + std::to_string(edges.east);
}

[[nodiscard]] qrp::atlas::WangTileAtlas createDiagnosticAtlas() {
    std::vector<qrp::atlas::WangImageTile> tiles;
    tiles.reserve(8);
    for (std::uint32_t south = 0; south < kEdgeLabelCount; ++south) {
        for (std::uint32_t north = 0; north < kEdgeLabelCount; ++north) {
            for (std::uint32_t west = 0; west < kEdgeLabelCount; ++west) {
                for (std::uint32_t east = 0; east < kEdgeLabelCount; ++east) {
                    if ((south ^ north ^ west ^ east) != 0U) {
                        continue;
                    }
                    const auto edges =
                        qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(
                            north,
                            east,
                            south,
                            west);
                    tiles.push_back({
                        edges,
                        createDiagnosticTile(edges),
                        tileName(edges),
                    });
                }
            }
        }
    }
    return qrp::atlas::WangTileAtlas(kEdgeLabelCount, std::move(tiles));
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

[[nodiscard]] qrp::render::Image createAtlasSheet(
    const qrp::atlas::WangTileAtlas& atlas) {
    constexpr std::size_t columns = 4;
    constexpr std::size_t gap = 8;
    const std::size_t rows = (atlas.tiles().size() + columns - 1) / columns;
    qrp::render::Image sheet(
        columns * atlas.tileSize() + (columns + 1) * gap,
        rows * atlas.tileSize() + (rows + 1) * gap);
    fillImage(sheet, qrp::render::Rgb8{12, 15, 22});
    for (std::size_t index = 0; index < atlas.tiles().size(); ++index) {
        const std::size_t column = index % columns;
        const std::size_t row = index / columns;
        blit(
            sheet,
            atlas.tiles()[index].image,
            gap + column * (atlas.tileSize() + gap),
            gap + row * (atlas.tileSize() + gap));
    }
    return sheet;
}

[[nodiscard]] std::size_t uniqueSignaturesUsed(
    const qrp::atlas::WangAtlasTiling& tiling) {
    std::set<std::array<std::uint32_t, 4>> signatures;
    for (const auto& tile : tiling.tiles()) {
        signatures.insert({tile.south, tile.north, tile.west, tile.east});
    }
    return signatures.size();
}

void validateSelection(
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::WangTileAtlas& atlas) {
    for (const auto expected : tiling.tiles()) {
        if (!(atlas.select(expected).edges == expected)) {
            throw std::runtime_error("Atlas lookup returned a mismatched Wang tile.");
        }
    }
}

void validateEightTileBalance(const qrp::atlas::WangTileAtlas& atlas) {
    for (std::uint32_t first = 0; first < kEdgeLabelCount; ++first) {
        for (std::uint32_t second = 0; second < kEdgeLabelCount; ++second) {
            std::size_t incomingCandidates = 0;
            std::size_t outgoingCandidates = 0;
            for (const auto& tile : atlas.tiles()) {
                if (tile.edges.south == first && tile.edges.west == second) {
                    ++incomingCandidates;
                }
                if (tile.edges.north == first && tile.edges.east == second) {
                    ++outgoingCandidates;
                }
            }
            if (incomingCandidates != 2 || outgoingCandidates != 2) {
                throw std::runtime_error(
                    "The minimal eight-tile set must provide two candidates per edge pair.");
            }
        }
    }
}

void writeMetrics(
    const std::filesystem::path& path,
    const qrp::atlas::WangTileAtlas& atlas,
    const qrp::atlas::WangAtlasTiling& tiling,
    const qrp::atlas::AtlasSeamMetrics& atlasSeams,
    const qrp::atlas::AtlasSeamMetrics& renderedSeams) {
    const auto coverage = atlas.coverage();
    std::ostringstream seedText;
    seedText << "0x" << std::hex << tiling.seed();
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to open classic Wang metrics output.");
    }
    output << "{\n"
           << "  \"schemaVersion\": 1,\n"
           << "  \"experiment\": \"classic-wang-image-atlas-reference\",\n"
           << "  \"methodRole\": \"related-method-baseline\",\n"
           << "  \"tileSetRule\": \"south XOR north XOR west XOR east = 0\",\n"
           << "  \"placementStrategy\": \"seeded scanline compatible choice\",\n"
           << "  \"edgeLabelCount\": " << atlas.edgeLabelCount() << ",\n"
           << "  \"atlasTileCount\": " << atlas.tiles().size() << ",\n"
           << "  \"distinctSignatureCount\": " << coverage.signatureCount << ",\n"
           << "  \"cartesianSignatureCount\": " << coverage.expectedSignatureCount << ",\n"
           << "  \"usesCompleteCartesianAtlas\": "
           << (coverage.complete ? "true" : "false") << ",\n"
           << "  \"candidateCountPerConstrainedEdgePair\": 2,\n"
           << "  \"tileSize\": " << atlas.tileSize() << ",\n"
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
           << "  \"usesCoonsWarp\": false,\n"
           << "  \"usesQrpGenerator\": false,\n"
           << "  \"usesTileNoise\": false,\n"
           << "  \"tileOrder\": [\n";
    for (std::size_t index = 0; index < atlas.tiles().size(); ++index) {
        output << "    \"" << atlas.tiles()[index].name << "\""
               << (index + 1 == atlas.tiles().size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    if (!output) {
        throw std::runtime_error("Unable to write classic Wang metrics output.");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/classic-wang-reference");
        std::filesystem::create_directories(outputDirectory);
        std::filesystem::remove(outputDirectory / "metrics.json");

        const auto atlas = createDiagnosticAtlas();
        const auto coverage = atlas.coverage();
        if (coverage.complete
            || coverage.tileCount != 8
            || coverage.signatureCount != 8
            || coverage.expectedSignatureCount != 16) {
            throw std::runtime_error("The diagnostic Wang tile set is not the expected S8 set.");
        }
        validateEightTileBalance(atlas);

        const qrp::atlas::WangAtlasTiling tiling(
            kGridWidth,
            kGridHeight,
            atlas,
            kGridSeed);
        validateSelection(tiling, atlas);
        const auto atlasSeams =
            qrp::atlas::AtlasRenderer::measureAtlasCompatibility(atlas);
        const auto renderedSeams =
            qrp::atlas::AtlasRenderer::measureSeams(tiling, atlas);
        if (atlasSeams.mismatchedPixelCount != 0
            || atlasSeams.maximumChannelDifference != 0
            || renderedSeams.mismatchedPixelCount != 0
            || renderedSeams.maximumChannelDifference != 0) {
            throw std::runtime_error("The diagnostic Wang atlas contains visible seams.");
        }

        const auto atlasSheet = createAtlasSheet(atlas);
        const auto image = qrp::atlas::AtlasRenderer::render(tiling, atlas);
        qrp::exporting::writePng(
            atlasSheet,
            outputDirectory / "A_minimal_8_tile_set.png");
        qrp::exporting::writePng(
            image,
            outputDirectory / "B_classic_wang_tiling_10x10.png");
        writeMetrics(
            outputDirectory / "metrics.json",
            atlas,
            tiling,
            atlasSeams,
            renderedSeams);

        std::cout << "classic_wang_tiles=" << atlas.tiles().size() << '\n'
                  << "distinct_signatures=" << coverage.signatureCount << '\n'
                  << "grid_unique_signatures=" << uniqueSignaturesUsed(tiling) << '\n'
                  << "atlas_boundary_samples=" << atlasSeams.sampleCount << '\n'
                  << "atlas_mismatched_boundary_pixels="
                  << atlasSeams.mismatchedPixelCount << '\n'
                  << "rendered_seam_samples=" << renderedSeams.sampleCount << '\n'
                  << "rendered_mismatched_seam_pixels="
                  << renderedSeams.mismatchedPixelCount << '\n'
                  << "maximum_rendered_seam_channel_difference="
                  << static_cast<unsigned int>(renderedSeams.maximumChannelDifference) << '\n'
                  << "Wrote classic Wang reference to "
                  << outputDirectory.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Classic Wang reference failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
