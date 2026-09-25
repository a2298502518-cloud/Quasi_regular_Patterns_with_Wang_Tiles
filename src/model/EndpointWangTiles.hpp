#pragma once

#include "model/WangGrid.hpp"

#include <stdexcept>

namespace qrp::model {

// 固定 16 种端点编码类型，与内容生成方法无关。
// ID 的四位依次是 SW、SE、NW、NE；南北边向东读，西东边向北读。
struct EndpointWangTiles {
    static constexpr std::uint32_t count = 16;

    [[nodiscard]] static constexpr std::uint32_t corner(
        const std::uint32_t id, const int x, const int y) noexcept {
        return (id >> (x + 2 * y)) & 1U;
    }

    [[nodiscard]] static constexpr WangTile labels(const std::uint32_t id) {
        if (id >= count) throw std::out_of_range("Endpoint Wang tile ID must be in [0,15].");
        const auto sw = corner(id, 0, 0), se = corner(id, 1, 0);
        const auto nw = corner(id, 0, 1), ne = corner(id, 1, 1);
        return {2 * sw + se, 2 * nw + ne, 2 * sw + nw, 2 * se + ne, 0};
    }
};

} // namespace qrp::model
