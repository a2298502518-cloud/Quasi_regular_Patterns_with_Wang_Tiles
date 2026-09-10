#include "atlas/WangAtlasTiling.hpp"

#include <limits>
#include <stdexcept>
#include <vector>

namespace qrp::atlas {
namespace {

[[nodiscard]] std::uint64_t nextRandom(std::uint64_t& state) noexcept {
    state += 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

} // namespace

WangAtlasTiling::WangAtlasTiling(
    const std::size_t width,
    const std::size_t height,
    const WangTileAtlas& atlas,
    const std::uint64_t seed)
    : width_(width),
      height_(height),
      edgeLabelCount_(atlas.edgeLabelCount()),
      seed_(seed) {
    if (width_ == 0 || height_ == 0) {
        throw std::invalid_argument("Wang atlas tiling dimensions must be positive.");
    }
    if (width_ > std::numeric_limits<std::size_t>::max() / height_) {
        throw std::length_error("Wang atlas tiling dimensions overflow.");
    }

    tiles_.reserve(width_ * height_);
    std::uint64_t randomState = seed_;
    // Logical y grows northward. Scanning from y = 0 therefore constrains the
    // current south edge from the already placed tile below.
    for (std::size_t y = 0; y < height_; ++y) {
        for (std::size_t x = 0; x < width_; ++x) {
            std::vector<std::size_t> candidates;
            candidates.reserve(atlas.tiles().size());
            for (std::size_t index = 0; index < atlas.tiles().size(); ++index) {
                const auto& edges = atlas.tiles()[index].edges;
                if (x > 0 && edges.west != tiles_.back().east) {
                    continue;
                }
                if (y > 0 && edges.south != tiles_[(y - 1) * width_ + x].north) {
                    continue;
                }
                candidates.push_back(index);
            }
            if (candidates.empty()) {
                throw std::runtime_error(
                    "The Wang tile set cannot continue the requested scanline tiling.");
            }
            const std::size_t choice = candidates[static_cast<std::size_t>(
                nextRandom(randomState) % candidates.size())];
            tiles_.push_back(atlas.tiles()[choice].edges);
        }
    }
}

std::size_t WangAtlasTiling::width() const noexcept {
    return width_;
}

std::size_t WangAtlasTiling::height() const noexcept {
    return height_;
}

std::uint32_t WangAtlasTiling::edgeLabelCount() const noexcept {
    return edgeLabelCount_;
}

std::uint64_t WangAtlasTiling::seed() const noexcept {
    return seed_;
}

const WangEdgeSignature& WangAtlasTiling::tile(
    const std::size_t x,
    const std::size_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Wang atlas tile coordinates are outside the grid.");
    }
    return tiles_[y * width_ + x];
}

const std::vector<WangEdgeSignature>& WangAtlasTiling::tiles() const noexcept {
    return tiles_;
}

bool WangAtlasTiling::hasValidAdjacency() const noexcept {
    for (std::size_t y = 0; y < height_; ++y) {
        for (std::size_t x = 0; x < width_; ++x) {
            const auto& current = tiles_[y * width_ + x];
            if (current.south >= edgeLabelCount_
                || current.north >= edgeLabelCount_
                || current.west >= edgeLabelCount_
                || current.east >= edgeLabelCount_) {
                return false;
            }
            if (x > 0 && tiles_[y * width_ + x - 1].east != current.west) {
                return false;
            }
            if (y > 0 && tiles_[(y - 1) * width_ + x].north != current.south) {
                return false;
            }
        }
    }
    return true;
}

} // namespace qrp::atlas
