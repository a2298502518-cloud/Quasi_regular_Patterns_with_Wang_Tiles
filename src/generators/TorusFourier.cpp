#include "generators/TorusFourier.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace qrp::generators {

TorusFourier::TorusFourier(std::vector<FourierMode> modes)
    : modes_(std::move(modes)) {
    if (modes_.empty()) {
        throw std::invalid_argument("A torus Fourier generator requires at least one mode.");
    }

    amplitudeNormalizer_ = 0.0;
    for (const auto& mode : modes_) {
        if ((mode.frequencyU == 0 && mode.frequencyV == 0)
            || !std::isfinite(mode.amplitude)
            || !std::isfinite(mode.phase)) {
            throw std::invalid_argument("Torus Fourier modes must be finite and non-constant.");
        }
        amplitudeNormalizer_ += std::abs(mode.amplitude);
    }
    if (amplitudeNormalizer_ <= 0.0) {
        throw std::invalid_argument("Torus Fourier amplitudes cannot all be zero.");
    }
}

TorusFourier TorusFourier::createQuasiRegular() {
    // 整数波矢逼近多方向共振，同时严格保留单位环面周期性。
    return TorusFourier({
        {1, 0, 1.00, 0.10},
        {2, 1, 0.86, 1.30},
        {1, 2, 0.86, 2.10},
        {0, 1, 1.00, 2.80},
        {-1, 2, 0.86, 3.70},
        {-2, 1, 0.86, 4.60},
        {3, 1, 0.38, 0.70},
        {1, 3, 0.38, 2.45},
        {-1, 3, 0.38, 4.20},
    });
}

const std::vector<FourierMode>& TorusFourier::modes() const noexcept {
    return modes_;
}

double TorusFourier::evaluate(const math::Vec2 parameter) const noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    double value = 0.0;
    for (const auto& mode : modes_) {
        const double argument = tau * (
            static_cast<double>(mode.frequencyU) * parameter.x
            + static_cast<double>(mode.frequencyV) * parameter.y)
            + mode.phase;
        value += mode.amplitude * std::cos(argument);
    }
    return value / amplitudeNormalizer_;
}

} // namespace qrp::generators
