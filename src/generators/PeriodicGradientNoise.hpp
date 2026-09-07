#pragma once

#include "math/CoonsWarp.hpp"

#include <cstdint>

namespace qrp::generators {

struct PeriodicNoiseSettings {
    std::uint32_t baseFrequency = 3;
    std::uint32_t octaves = 4;
    double persistence = 0.52;
    std::uint64_t seed = 0x8f3f73b5cf1c9adeULL;
};

class PeriodicGradientNoise final {
public:
    explicit PeriodicGradientNoise(PeriodicNoiseSettings settings = {});

    [[nodiscard]] const PeriodicNoiseSettings& settings() const noexcept;
    [[nodiscard]] math::Vec2 gradientAt(
        std::uint32_t x,
        std::uint32_t y) const noexcept;
    [[nodiscard]] double evaluate(math::Vec2 parameter) const noexcept;

private:
    [[nodiscard]] double evaluateOctave(
        math::Vec2 parameter,
        std::uint32_t frequency) const noexcept;

    PeriodicNoiseSettings settings_;
    double amplitudeNormalizer_ = 1.0;
};

} // namespace qrp::generators
