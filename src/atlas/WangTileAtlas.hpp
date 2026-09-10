#pragma once

#include "render/Image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace qrp::atlas {

struct WangEdgeSignature {
    std::uint32_t south = 0;
    std::uint32_t north = 0;
    std::uint32_t west = 0;
    std::uint32_t east = 0;

    [[nodiscard]] static constexpr WangEdgeSignature fromNorthEastSouthWest(
        const std::uint32_t northLabel,
        const std::uint32_t eastLabel,
        const std::uint32_t southLabel,
        const std::uint32_t westLabel) noexcept {
        return {southLabel, northLabel, westLabel, eastLabel};
    }

    [[nodiscard]] bool operator==(const WangEdgeSignature&) const noexcept = default;
};

struct WangImageTile {
    WangEdgeSignature edges;
    render::Image image;
    std::string name;
};

struct AtlasCoverageReport {
    bool complete = false;
    std::size_t tileCount = 0;
    std::size_t signatureCount = 0;
    std::size_t expectedSignatureCount = 0;
    std::string message;
};

class WangTileAtlas final {
public:
    WangTileAtlas(std::uint32_t edgeLabelCount, std::vector<WangImageTile> tiles);

    [[nodiscard]] std::uint32_t edgeLabelCount() const noexcept;
    [[nodiscard]] std::size_t tileSize() const noexcept;
    [[nodiscard]] const std::vector<WangImageTile>& tiles() const noexcept;
    [[nodiscard]] const WangImageTile& select(WangEdgeSignature edges) const;
    [[nodiscard]] AtlasCoverageReport coverage() const;

private:
    using SignatureKey = std::array<std::uint32_t, 4>;

    [[nodiscard]] static SignatureKey key(WangEdgeSignature edges) noexcept;

    std::uint32_t edgeLabelCount_ = 0;
    std::size_t tileSize_ = 0;
    std::vector<WangImageTile> tiles_;
    std::map<SignatureKey, std::size_t> indexBySignature_;
};

} // namespace qrp::atlas
