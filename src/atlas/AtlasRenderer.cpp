#include "atlas/AtlasRenderer.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace qrp::atlas {
namespace {

void validateInputs(
    const WangAtlasTiling& tiling,
    const WangTileAtlas& atlas) {
    if (!tiling.hasValidAdjacency()) {
        throw std::invalid_argument("Atlas rendering requires a valid Wang grid.");
    }
    if (tiling.edgeLabelCount() != atlas.edgeLabelCount()) {
        throw std::invalid_argument(
            "Wang grid and image atlas edge-label counts do not match.");
    }
}

[[nodiscard]] const WangImageTile& selectedTile(
    const WangEdgeSignature edges,
    const WangTileAtlas& atlas) {
    return atlas.select(edges);
}

[[nodiscard]] std::uint8_t channelDifference(
    const render::Rgb8 first,
    const render::Rgb8 second) noexcept {
    const auto difference = [](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(a > b ? a - b : b - a);
    };
    return std::max({
        difference(first.red, second.red),
        difference(first.green, second.green),
        difference(first.blue, second.blue),
    });
}

void accumulateDifference(
    AtlasSeamMetrics& metrics,
    const render::Rgb8 first,
    const render::Rgb8 second) noexcept {
    const std::uint8_t difference = channelDifference(first, second);
    ++metrics.sampleCount;
    if (difference != 0) {
        ++metrics.mismatchedPixelCount;
    }
    metrics.maximumChannelDifference = std::max(
        metrics.maximumChannelDifference,
        difference);
}

} // namespace

render::Image AtlasRenderer::render(
    const WangAtlasTiling& tiling,
    const WangTileAtlas& atlas) {
    validateInputs(tiling, atlas);
    const std::size_t tileSize = atlas.tileSize();
    if (tiling.width() > std::numeric_limits<std::size_t>::max() / tileSize
        || tiling.height() > std::numeric_limits<std::size_t>::max() / tileSize) {
        throw std::length_error("Atlas render dimensions overflow.");
    }

    render::Image output(tiling.width() * tileSize, tiling.height() * tileSize);
    // Image row zero is North, while the tiling stores logical row zero at
    // South. Flip only the row placement; individual tile images are not
    // rotated or mirrored.
    for (std::size_t gridY = 0; gridY < tiling.height(); ++gridY) {
        const std::size_t destinationY = (tiling.height() - gridY - 1) * tileSize;
        for (std::size_t gridX = 0; gridX < tiling.width(); ++gridX) {
            const auto& tile = selectedTile(tiling.tile(gridX, gridY), atlas);
            const std::size_t destinationX = gridX * tileSize;
            for (std::size_t imageY = 0; imageY < tileSize; ++imageY) {
                for (std::size_t imageX = 0; imageX < tileSize; ++imageX) {
                    output.pixel(destinationX + imageX, destinationY + imageY)
                        = tile.image.pixel(imageX, imageY);
                }
            }
        }
    }
    return output;
}

AtlasSeamMetrics AtlasRenderer::measureSeams(
    const WangAtlasTiling& tiling,
    const WangTileAtlas& atlas) {
    validateInputs(tiling, atlas);
    AtlasSeamMetrics metrics;
    const std::size_t tileSize = atlas.tileSize();

    for (std::size_t y = 0; y < tiling.height(); ++y) {
        for (std::size_t x = 0; x + 1 < tiling.width(); ++x) {
            const auto& left = selectedTile(tiling.tile(x, y), atlas).image;
            const auto& right = selectedTile(tiling.tile(x + 1, y), atlas).image;
            for (std::size_t imageY = 0; imageY < tileSize; ++imageY) {
                accumulateDifference(
                    metrics,
                    left.pixel(tileSize - 1, imageY),
                    right.pixel(0, imageY));
            }
        }
    }

    for (std::size_t y = 0; y + 1 < tiling.height(); ++y) {
        for (std::size_t x = 0; x < tiling.width(); ++x) {
            const auto& bottom = selectedTile(tiling.tile(x, y), atlas).image;
            const auto& top = selectedTile(tiling.tile(x, y + 1), atlas).image;
            for (std::size_t imageX = 0; imageX < tileSize; ++imageX) {
                accumulateDifference(
                    metrics,
                    bottom.pixel(imageX, 0),
                    top.pixel(imageX, tileSize - 1));
            }
        }
    }
    return metrics;
}

AtlasSeamMetrics AtlasRenderer::measureAtlasCompatibility(
    const WangTileAtlas& atlas) {
    AtlasSeamMetrics metrics;
    const std::size_t tileSize = atlas.tileSize();

    for (const auto& first : atlas.tiles()) {
        for (const auto& second : atlas.tiles()) {
            if (first.edges.east == second.edges.west) {
                for (std::size_t imageY = 0; imageY < tileSize; ++imageY) {
                    accumulateDifference(
                        metrics,
                        first.image.pixel(tileSize - 1, imageY),
                        second.image.pixel(0, imageY));
                }
            }
            if (first.edges.north == second.edges.south) {
                for (std::size_t imageX = 0; imageX < tileSize; ++imageX) {
                    accumulateDifference(
                        metrics,
                        first.image.pixel(imageX, 0),
                        second.image.pixel(imageX, tileSize - 1));
                }
            }
        }
    }
    return metrics;
}

} // namespace qrp::atlas
