#include "atlas/WangTileAtlas.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace qrp::atlas {
namespace {

[[nodiscard]] std::size_t checkedFourthPower(const std::uint32_t value) {
    std::size_t result = 1;
    for (int exponent = 0; exponent < 4; ++exponent) {
        if (value > std::numeric_limits<std::size_t>::max() / result) {
            throw std::length_error("Wang atlas signature count overflows size_t.");
        }
        result *= value;
    }
    return result;
}

} // namespace

WangTileAtlas::WangTileAtlas(
    const std::uint32_t edgeLabelCount,
    std::vector<WangImageTile> tiles)
    : edgeLabelCount_(edgeLabelCount),
      tiles_(std::move(tiles)) {
    if (edgeLabelCount_ == 0) {
        throw std::invalid_argument("A Wang tile atlas requires edge labels.");
    }
    if (tiles_.empty()) {
        throw std::invalid_argument("A Wang tile atlas requires at least one tile image.");
    }

    std::sort(
        tiles_.begin(),
        tiles_.end(),
        [](const WangImageTile& first, const WangImageTile& second) {
            return key(first.edges) < key(second.edges);
        });

    tileSize_ = tiles_.front().image.width();
    if (tileSize_ == 0 || tiles_.front().image.height() != tileSize_) {
        throw std::invalid_argument("Wang atlas tile images must be square.");
    }

    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        const auto& tile = tiles_[index];
        if (tile.image.width() != tileSize_ || tile.image.height() != tileSize_) {
            throw std::invalid_argument(
                "Every Wang atlas tile image must have the same square dimensions.");
        }
        if (tile.edges.south >= edgeLabelCount_
            || tile.edges.north >= edgeLabelCount_
            || tile.edges.west >= edgeLabelCount_
            || tile.edges.east >= edgeLabelCount_) {
            throw std::invalid_argument(
                "A Wang atlas tile contains an out-of-range edge label.");
        }
        const bool inserted = indexBySignature_.emplace(key(tile.edges), index).second;
        if (!inserted) {
            throw std::invalid_argument(
                "The first Wang atlas baseline requires one tile per edge signature.");
        }
    }
}

std::uint32_t WangTileAtlas::edgeLabelCount() const noexcept {
    return edgeLabelCount_;
}

std::size_t WangTileAtlas::tileSize() const noexcept {
    return tileSize_;
}

const std::vector<WangImageTile>& WangTileAtlas::tiles() const noexcept {
    return tiles_;
}

const WangImageTile& WangTileAtlas::select(const WangEdgeSignature edges) const {
    const auto found = indexBySignature_.find(key(edges));
    if (found == indexBySignature_.end()) {
        throw std::out_of_range("The Wang tile atlas does not cover the requested edge signature.");
    }
    return tiles_[found->second];
}

AtlasCoverageReport WangTileAtlas::coverage() const {
    AtlasCoverageReport report;
    report.tileCount = tiles_.size();
    report.signatureCount = indexBySignature_.size();
    try {
        report.expectedSignatureCount = checkedFourthPower(edgeLabelCount_);
    } catch (const std::length_error&) {
        report.message = "The expected signature count overflows size_t.";
        return report;
    }
    report.complete = report.signatureCount == report.expectedSignatureCount;
    report.message = report.complete
        ? "Every four-edge signature has at least one image tile."
        : "The image tile atlas does not cover every four-edge signature.";
    return report;
}

WangTileAtlas::SignatureKey WangTileAtlas::key(
    const WangEdgeSignature edges) noexcept {
    return {edges.south, edges.north, edges.west, edges.east};
}

} // namespace qrp::atlas
