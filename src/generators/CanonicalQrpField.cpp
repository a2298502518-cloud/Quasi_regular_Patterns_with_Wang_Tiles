#include "generators/CanonicalQrpField.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace qrp::generators {

CanonicalQrpField::CanonicalQrpField(const CanonicalQrpParameters parameters)
    : parameters_(parameters) {
    if (!std::isfinite(parameters_.resonanceCount)
        || parameters_.resonanceCount < 1.0
        || parameters_.resonanceCount
            > static_cast<double>(std::numeric_limits<std::uint32_t>::max())
        || !std::isfinite(parameters_.scale)
        || parameters_.scale <= 0.0
        || !std::isfinite(parameters_.translation.x)
        || !std::isfinite(parameters_.translation.y)) {
        throw std::invalid_argument(
            "Canonical QRP parameters require finite 1 <= q <= uint32_max and s > 0.");
    }
    modeCount_ = static_cast<std::size_t>(std::floor(parameters_.resonanceCount));
}

const CanonicalQrpParameters& CanonicalQrpField::parameters() const noexcept {
    return parameters_;
}

std::size_t CanonicalQrpField::modeCount() const noexcept {
    return modeCount_;
}

math::Vec2 CanonicalQrpField::modelCoordinate(const math::Vec2 normalized) const noexcept {
    const double extent = parameters_.scale * std::numbers::pi_v<double>;
    return {
        parameters_.translation.x + normalized.x * extent,
        parameters_.translation.y + normalized.y * extent,
    };
}

CanonicalQrpEvaluation CanonicalQrpField::evaluateModelWithGradient(
    const math::Vec2 modelCoordinate) const noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    CanonicalQrpEvaluation result;
    for (std::size_t index = 1; index <= modeCount_; ++index) {
        const double angle = tau * static_cast<double>(index)
            / parameters_.resonanceCount;
        const double waveX = std::cos(angle);
        const double waveY = std::sin(angle);
        const double phase = modelCoordinate.x * waveX
            + modelCoordinate.y * waveY;
        result.value += std::cos(phase);
        const double derivative = -std::sin(phase);
        result.modelGradient.x += derivative * waveX;
        result.modelGradient.y += derivative * waveY;
    }
    return result;
}

double CanonicalQrpField::evaluateModel(const math::Vec2 modelCoordinate) const noexcept {
    return evaluateModelWithGradient(modelCoordinate).value;
}

double CanonicalQrpField::evaluateNormalized(const math::Vec2 normalized) const noexcept {
    return evaluateModel(modelCoordinate(normalized));
}

} // namespace qrp::generators
