#pragma once

#include "math/CoonsWarp.hpp"

#include <array>
#include <cstdint>

namespace qrp::generators {

struct GeneratorInput {
    math::Vec2 parameter;
    math::Vec2 world;
    std::uint64_t tileSeed = 0;
    // 固定顺序为 south、north、west、east；生成器不依赖 WangGrid 的私有表示。
    std::array<std::uint32_t, 4> edgeColors{};
};

class TorusGenerator {
public:
    virtual ~TorusGenerator() = default;

    [[nodiscard]] virtual double evaluate(const GeneratorInput& input) const noexcept = 0;
};

} // namespace qrp::generators
