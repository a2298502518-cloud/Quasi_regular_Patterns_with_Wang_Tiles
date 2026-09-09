#pragma once

#include "math/CoonsWarp.hpp"

#include <cstdint>

namespace qrp::generators {

struct GeneratorInput {
    math::Vec2 parameter;
    math::Vec2 world;
    std::uint64_t tileSeed = 0;
};

class TorusGenerator {
public:
    virtual ~TorusGenerator() = default;

    [[nodiscard]] virtual double evaluate(const GeneratorInput& input) const noexcept = 0;
};

} // namespace qrp::generators
