#pragma once

#include "atlas/WangTileAtlas.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace qrp::atlas {

class WangAtlasTiling final {
public:
    WangAtlasTiling(
        std::size_t width,
        std::size_t height,
        const WangTileAtlas& atlas,
        std::uint64_t seed);

    [[nodiscard]] std::size_t width() const noexcept;
    [[nodiscard]] std::size_t height() const noexcept;
    [[nodiscard]] std::uint32_t edgeLabelCount() const noexcept;
    [[nodiscard]] std::uint64_t seed() const noexcept;
    [[nodiscard]] const WangEdgeSignature& tile(std::size_t x, std::size_t y) const;
    [[nodiscard]] const std::vector<WangEdgeSignature>& tiles() const noexcept;
    [[nodiscard]] bool hasValidAdjacency() const noexcept;

private:
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    std::uint32_t edgeLabelCount_ = 0;
    std::uint64_t seed_ = 0;
    std::vector<WangEdgeSignature> tiles_;
};

} // namespace qrp::atlas
