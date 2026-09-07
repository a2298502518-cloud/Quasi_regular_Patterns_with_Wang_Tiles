#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace qrp::model {

struct WangTile {
    std::uint32_t south = 0;
    std::uint32_t north = 0;
    std::uint32_t west = 0;
    std::uint32_t east = 0;
    std::uint64_t seed = 0;

    [[nodiscard]] bool operator==(const WangTile&) const noexcept = default;
};

class WangGrid final {
public:
    WangGrid(
        std::size_t width,
        std::size_t height,
        std::uint32_t colorCount,
        std::uint64_t seed);

    [[nodiscard]] std::size_t width() const noexcept;
    [[nodiscard]] std::size_t height() const noexcept;
    [[nodiscard]] std::uint32_t colorCount() const noexcept;
    [[nodiscard]] std::uint64_t seed() const noexcept;
    [[nodiscard]] const WangTile& tile(std::size_t x, std::size_t y) const;
    [[nodiscard]] const std::vector<WangTile>& tiles() const noexcept;
    [[nodiscard]] bool hasValidAdjacency() const noexcept;

private:
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    std::uint32_t colorCount_ = 0;
    std::uint64_t seed_ = 0;
    std::vector<WangTile> tiles_;
};

} // namespace qrp::model
