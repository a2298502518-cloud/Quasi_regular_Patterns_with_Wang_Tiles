#include "model/WangGrid.hpp"

#include <limits>
#include <stdexcept>

namespace qrp::model {
namespace {

[[nodiscard]] std::uint64_t nextRandom(std::uint64_t& state) noexcept {
    // SplitMix64 固定了位级过程，避免标准库分布在不同实现之间产生不同网格。
    state += 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] std::uint32_t nextColor(
    std::uint64_t& state,
    const std::uint32_t colorCount) noexcept {
    return static_cast<std::uint32_t>(nextRandom(state) % colorCount);
}

} // namespace

WangGrid::WangGrid(
    const std::size_t width,
    const std::size_t height,
    const std::uint32_t colorCount,
    const std::uint64_t seed)
    : width_(width),
      height_(height),
      colorCount_(colorCount),
      seed_(seed) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Wang grid dimensions must be positive.");
    }
    if (colorCount == 0) {
        throw std::invalid_argument("A Wang grid requires at least one edge color.");
    }
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        throw std::length_error("Wang grid dimensions overflow the cell count.");
    }

    tiles_.resize(width * height);
    std::uint64_t randomState = seed;
    for (std::size_t y = 0; y < height_; ++y) {
        for (std::size_t x = 0; x < width_; ++x) {
            WangTile& current = tiles_[y * width_ + x];
            current.west = x == 0
                ? nextColor(randomState, colorCount_)
                : tiles_[y * width_ + (x - 1)].east;
            current.south = y == 0
                ? nextColor(randomState, colorCount_)
                : tiles_[(y - 1) * width_ + x].north;
            current.north = nextColor(randomState, colorCount_);
            current.east = nextColor(randomState, colorCount_);
            current.seed = nextRandom(randomState);
        }
    }
}

std::size_t WangGrid::width() const noexcept {
    return width_;
}

std::size_t WangGrid::height() const noexcept {
    return height_;
}

std::uint32_t WangGrid::colorCount() const noexcept {
    return colorCount_;
}

std::uint64_t WangGrid::seed() const noexcept {
    return seed_;
}

const WangTile& WangGrid::tile(const std::size_t x, const std::size_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Wang tile coordinates are outside the grid.");
    }
    return tiles_[y * width_ + x];
}

const std::vector<WangTile>& WangGrid::tiles() const noexcept {
    return tiles_;
}

bool WangGrid::hasValidAdjacency() const noexcept {
    for (std::size_t y = 0; y < height_; ++y) {
        for (std::size_t x = 0; x < width_; ++x) {
            const WangTile& current = tiles_[y * width_ + x];
            if (current.south >= colorCount_ || current.north >= colorCount_
                || current.west >= colorCount_ || current.east >= colorCount_) {
                return false;
            }
            if (x > 0 && tiles_[y * width_ + (x - 1)].east != current.west) {
                return false;
            }
            if (y > 0 && tiles_[(y - 1) * width_ + x].north != current.south) {
                return false;
            }
        }
    }
    return true;
}

} // namespace qrp::model
