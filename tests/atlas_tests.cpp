#include "atlas/AtlasRenderer.hpp"
#include "atlas/WangAtlasTiling.hpp"
#include "atlas/WangTileAtlas.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kTileSize = 12;

struct TestCase {
    std::string_view name;
    void (*function)();
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void requireThrows(Function&& function, const std::string& message) {
    bool threw = false;
    try {
        std::forward<Function>(function)();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, message);
}

[[nodiscard]] bool equal(
    const qrp::render::Rgb8 first,
    const qrp::render::Rgb8 second) noexcept {
    return first.red == second.red
        && first.green == second.green
        && first.blue == second.blue;
}

[[nodiscard]] qrp::render::Rgb8 northSouthColor(const std::uint32_t label) {
    return label == 0
        ? qrp::render::Rgb8{210, 30, 60}
        : qrp::render::Rgb8{30, 190, 100};
}

[[nodiscard]] qrp::render::Rgb8 westEastColor(const std::uint32_t label) {
    return label == 0
        ? qrp::render::Rgb8{240, 180, 30}
        : qrp::render::Rgb8{40, 110, 230};
}

[[nodiscard]] qrp::render::Image makeTile(
    const qrp::atlas::WangEdgeSignature edges) {
    constexpr qrp::render::Rgb8 corner{9, 11, 17};
    qrp::render::Image image(kTileSize, kTileSize);
    const qrp::render::Rgb8 interior{
        static_cast<std::uint8_t>(40 + 30 * edges.south + 10 * edges.north),
        static_cast<std::uint8_t>(50 + 25 * edges.west + 15 * edges.east),
        70,
    };
    for (std::size_t y = 0; y < kTileSize; ++y) {
        for (std::size_t x = 0; x < kTileSize; ++x) {
            const bool horizontalEdge = x == 0 || x + 1 == kTileSize;
            const bool verticalEdge = y == 0 || y + 1 == kTileSize;
            if (horizontalEdge && verticalEdge) {
                image.pixel(x, y) = corner;
            } else if (y == 0) {
                image.pixel(x, y) = northSouthColor(edges.north);
            } else if (y + 1 == kTileSize) {
                image.pixel(x, y) = northSouthColor(edges.south);
            } else if (x == 0) {
                image.pixel(x, y) = westEastColor(edges.west);
            } else if (x + 1 == kTileSize) {
                image.pixel(x, y) = westEastColor(edges.east);
            } else {
                image.pixel(x, y) = interior;
            }
        }
    }
    return image;
}

[[nodiscard]] std::vector<qrp::atlas::WangImageTile> makeEightTiles() {
    std::vector<qrp::atlas::WangImageTile> tiles;
    for (std::uint32_t south = 0; south < 2; ++south) {
        for (std::uint32_t north = 0; north < 2; ++north) {
            for (std::uint32_t west = 0; west < 2; ++west) {
                for (std::uint32_t east = 0; east < 2; ++east) {
                    if ((south ^ north ^ west ^ east) != 0U) {
                        continue;
                    }
                    const auto edges =
                        qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(
                            north,
                            east,
                            south,
                            west);
                    tiles.push_back({edges, makeTile(edges), "test-tile"});
                }
            }
        }
    }
    return tiles;
}

[[nodiscard]] qrp::atlas::WangTileAtlas makeAtlas() {
    return qrp::atlas::WangTileAtlas(2, makeEightTiles());
}

[[nodiscard]] bool imagesEqual(
    const qrp::render::Image& first,
    const qrp::render::Image& second) {
    if (first.width() != second.width() || first.height() != second.height()) {
        return false;
    }
    for (std::size_t index = 0; index < first.pixels().size(); ++index) {
        if (!equal(first.pixels()[index], second.pixels()[index])) {
            return false;
        }
    }
    return true;
}

void testMinimalEightTileSet() {
    const auto atlas = makeAtlas();
    const auto coverage = atlas.coverage();
    require(atlas.edgeLabelCount() == 2, "classic set must use two labels per orientation");
    require(atlas.tiles().size() == 8, "classic minimal set must contain eight tiles");
    require(coverage.tileCount == 8, "coverage must report all eight tiles");
    require(coverage.signatureCount == 8, "all eight signatures must be distinct");
    require(coverage.expectedSignatureCount == 16, "two labels imply sixteen combinations");
    require(!coverage.complete, "minimal eight-tile set is intentionally not Cartesian-complete");

    for (const auto& tile : atlas.tiles()) {
        require(
            (tile.edges.south ^ tile.edges.north
                ^ tile.edges.west ^ tile.edges.east) == 0U,
            "every tile must satisfy the even-parity rule");
        require(
            atlas.select(tile.edges).edges == tile.edges,
            "atlas lookup must preserve the exact edge signature");
    }

    for (std::uint32_t first = 0; first < 2; ++first) {
        for (std::uint32_t second = 0; second < 2; ++second) {
            std::size_t incoming = 0;
            std::size_t outgoing = 0;
            for (const auto& tile : atlas.tiles()) {
                incoming += tile.edges.south == first && tile.edges.west == second;
                outgoing += tile.edges.north == first && tile.edges.east == second;
            }
            require(incoming == 2, "each south/west pair must have two candidates");
            require(outgoing == 2, "each north/east pair must have two candidates");
        }
    }

    requireThrows(
        [&atlas]() {
            static_cast<void>(atlas.select(
                qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(
                    0,
                    1,
                    0,
                    0)));
        },
        "missing odd-parity signature must not fall back to another tile");
}

void testAtlasRejectsInvalidTileSets() {
    auto duplicateTiles = makeEightTiles();
    duplicateTiles.push_back(duplicateTiles.front());
    requireThrows(
        [&duplicateTiles]() {
            qrp::atlas::WangTileAtlas duplicate(2, duplicateTiles);
        },
        "duplicate edge signatures must be rejected");

    auto outOfRangeTiles = makeEightTiles();
    outOfRangeTiles.front().edges.east = 2;
    requireThrows(
        [&outOfRangeTiles]() {
            qrp::atlas::WangTileAtlas outOfRange(2, outOfRangeTiles);
        },
        "out-of-range edge labels must be rejected");

    auto mismatchedSizeTiles = makeEightTiles();
    mismatchedSizeTiles.front().image = qrp::render::Image(8, 8);
    requireThrows(
        [&mismatchedSizeTiles]() {
            qrp::atlas::WangTileAtlas mismatchedSize(2, mismatchedSizeTiles);
        },
        "mismatched tile dimensions must be rejected");
}

void testEveryCompatibleBoundaryMatches() {
    const auto atlas = makeAtlas();
    for (const auto& first : atlas.tiles()) {
        for (const auto& second : atlas.tiles()) {
            if (first.edges.east == second.edges.west) {
                for (std::size_t y = 0; y < kTileSize; ++y) {
                    require(
                        equal(
                            first.image.pixel(kTileSize - 1, y),
                            second.image.pixel(0, y)),
                        "matching east/west atlas boundaries must be identical");
                }
            }
            if (first.edges.north == second.edges.south) {
                for (std::size_t x = 0; x < kTileSize; ++x) {
                    require(
                        equal(
                            first.image.pixel(x, 0),
                            second.image.pixel(x, kTileSize - 1)),
                        "matching north/south atlas boundaries must be identical");
                }
            }
        }
    }

    const auto metrics =
        qrp::atlas::AtlasRenderer::measureAtlasCompatibility(atlas);
    require(
        metrics.sampleCount == 64 * kTileSize,
        "atlas-wide validation must sample every compatible ordered boundary pair");
    require(
        metrics.mismatchedPixelCount == 0,
        "atlas-wide validation must find no mismatched compatible boundaries");
    require(
        metrics.maximumChannelDifference == 0,
        "atlas-wide compatible boundaries must be bitwise identical");

    auto mismatchedTiles = makeEightTiles();
    mismatchedTiles.front().image.pixel(kTileSize - 1, 2) = {255, 0, 255};
    const qrp::atlas::WangTileAtlas mismatchedAtlas(2, std::move(mismatchedTiles));
    const auto mismatchedMetrics =
        qrp::atlas::AtlasRenderer::measureAtlasCompatibility(mismatchedAtlas);
    require(
        mismatchedMetrics.mismatchedPixelCount != 0,
        "atlas-wide validation must detect a bad edge even if a grid seed misses it");
    require(
        mismatchedMetrics.maximumChannelDifference != 0,
        "atlas-wide validation must report the bad edge magnitude");
}

void testConstrainedScanlineTilingAndRender() {
    const auto atlas = makeAtlas();
    const qrp::atlas::WangAtlasTiling first(10, 7, atlas, 0x123456789abcdef0ULL);
    const qrp::atlas::WangAtlasTiling same(10, 7, atlas, 0x123456789abcdef0ULL);
    const qrp::atlas::WangAtlasTiling different(10, 7, atlas, 0x123456789abcdef1ULL);
    require(first.tiles() == same.tiles(), "same seed must reproduce the same classic tiling");
    require(first.tiles() != different.tiles(), "different seed should change the classic tiling");
    require(first.hasValidAdjacency(), "classic scanline tiling must satisfy every edge match");

    auto reversedTiles = makeEightTiles();
    std::reverse(reversedTiles.begin(), reversedTiles.end());
    const qrp::atlas::WangTileAtlas reorderedAtlas(2, std::move(reversedTiles));
    const qrp::atlas::WangAtlasTiling reordered(
        10,
        7,
        reorderedAtlas,
        0x123456789abcdef0ULL);
    require(
        first.tiles() == reordered.tiles(),
        "seeded selection must not depend on atlas input order");

    for (const auto edges : first.tiles()) {
        require(
            atlas.select(edges).edges == edges,
            "every placed signature must refer to an actual atlas tile");
    }

    const auto seams = qrp::atlas::AtlasRenderer::measureSeams(first, atlas);
    const std::size_t seamCount = (first.width() - 1) * first.height()
        + first.width() * (first.height() - 1);
    require(
        seams.sampleCount == seamCount * kTileSize,
        "atlas seam metric must sample every internal edge pixel");
    require(seams.mismatchedPixelCount == 0, "classic atlas seams must match bit-for-bit");
    require(seams.maximumChannelDifference == 0, "classic atlas seam channel error must be zero");

    const auto image = qrp::atlas::AtlasRenderer::render(first, atlas);
    require(image.width() == first.width() * kTileSize, "atlas render width is incorrect");
    require(image.height() == first.height() * kTileSize, "atlas render height is incorrect");

    const auto& logicalTop = atlas.select(first.tile(0, first.height() - 1)).image;
    const auto& logicalBottom = atlas.select(first.tile(0, 0)).image;
    require(
        equal(image.pixel(1, 0), logicalTop.pixel(1, 0)),
        "logical top row must map to the top of the output image");
    require(
        equal(
            image.pixel(1, image.height() - 1),
            logicalBottom.pixel(1, kTileSize - 1)),
        "logical bottom row must map to the bottom of the output image");

    const qrp::atlas::WangAtlasTiling row(17, 1, atlas, 7);
    const qrp::atlas::WangAtlasTiling column(1, 17, atlas, 7);
    const qrp::atlas::WangAtlasTiling single(1, 1, atlas, 7);
    require(row.hasValidAdjacency(), "one-row classic tiling must be valid");
    require(column.hasValidAdjacency(), "one-column classic tiling must be valid");
    require(single.hasValidAdjacency(), "single classic tile must be valid");
}

void testClassicAtlasDoesNotUseInstanceSeeds() {
    const auto edges =
        qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(0, 0, 0, 0);
    std::vector<qrp::atlas::WangImageTile> tiles;
    tiles.push_back({edges, makeTile(edges), "single"});
    const qrp::atlas::WangTileAtlas atlas(1, std::move(tiles));
    const qrp::atlas::WangAtlasTiling first(4, 3, atlas, 17);
    const qrp::atlas::WangAtlasTiling second(4, 3, atlas, 0xcafef00d12345678ULL);
    require(first.seed() != second.seed(), "seed-independence control needs distinct seeds");
    require(first.tiles() == second.tiles(), "single-signature tilings must place the same content");
    require(
        imagesEqual(
            qrp::atlas::AtlasRenderer::render(first, atlas),
            qrp::atlas::AtlasRenderer::render(second, atlas)),
        "classic atlas rendering must not add per-instance random content");
}

void testImpossibleContinuationFailsClearly() {
    const auto oneWay =
        qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(1, 1, 0, 0);
    std::vector<qrp::atlas::WangImageTile> tiles;
    tiles.push_back({oneWay, makeTile(oneWay), "one-way"});
    const qrp::atlas::WangTileAtlas atlas(2, std::move(tiles));

    requireThrows(
        [&atlas]() {
            qrp::atlas::WangAtlasTiling row(2, 1, atlas, 0);
        },
        "an atlas with no west-compatible continuation must fail instead of looping");
    requireThrows(
        [&atlas]() {
            qrp::atlas::WangAtlasTiling column(1, 2, atlas, 0);
        },
        "an atlas with no south-compatible continuation must fail instead of looping");
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"minimal eight-tile set", testMinimalEightTileSet},
        {"invalid tile set rejection", testAtlasRejectsInvalidTileSets},
        {"compatible atlas boundaries", testEveryCompatibleBoundaryMatches},
        {"constrained scanline tiling", testConstrainedScanlineTilingAndRender},
        {"instance seed independence", testClassicAtlasDoesNotUseInstanceSeeds},
        {"impossible continuation failure", testImpossibleContinuationFailsClearly},
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << tests.size() << " test(s) passed.\n";
    return EXIT_SUCCESS;
}
