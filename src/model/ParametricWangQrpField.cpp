#include "model/ParametricWangQrpField.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace qrp::model {

ParametricWangQrpField::ParametricWangQrpField(
    const ParametricWangQrpParameters parameters)
    : parameters_(parameters) {
    const auto finite = [](const math::Vec2 value) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    };
    if (parameters_.resonanceCount < 5
        || !std::isfinite(parameters_.spatialFrequency)
        || parameters_.spatialFrequency <= 0.0
        || !finite(parameters_.globalPhase)
        || !finite(parameters_.wangPhase)
        || !std::isfinite(parameters_.weightCenter)
        || !std::isfinite(parameters_.weightRadius)
        || parameters_.weightRadius <= 0.0) {
        throw std::invalid_argument(
            "Parametric Wang-QRP parameters require q >= 5, finite phases, "
            "spatialFrequency > 0, and weightRadius > 0.");
    }
}

const ParametricWangQrpParameters& ParametricWangQrpField::parameters() const noexcept {
    return parameters_;
}

ScalarFieldEvaluation ParametricWangQrpField::evaluate(
    const math::Vec2 position,
    const WangContentWeightEvaluation weight) const noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    const double inverseCount = 1.0 / static_cast<double>(parameters_.resonanceCount);
    const double inverseRadius = 1.0 / parameters_.weightRadius;
    const double normalizedWeight = (weight.value - parameters_.weightCenter)
        * inverseRadius;
    ScalarFieldEvaluation result;

    for (std::uint32_t index = 0; index < parameters_.resonanceCount; ++index) {
        const double theta = tau * static_cast<double>(index) * inverseCount;
        const double waveX = std::cos(theta);
        const double waveY = std::sin(theta);
        const double harmonicX = std::cos(2.0 * theta);
        const double harmonicY = std::sin(2.0 * theta);
        const double wangHarmonic = parameters_.wangPhase.x * harmonicX
            + parameters_.wangPhase.y * harmonicY;
        const double phase = parameters_.spatialFrequency
                * (waveX * position.x + waveY * position.y)
            + parameters_.globalPhase.x * harmonicX
            + parameters_.globalPhase.y * harmonicY
            + normalizedWeight * wangHarmonic;
        const double derivative = -std::sin(phase) * inverseCount;
        const double weightDerivative = wangHarmonic * inverseRadius;

        result.value += std::cos(phase) * inverseCount;
        result.gradient.x += derivative
            * (parameters_.spatialFrequency * waveX
                + weightDerivative * weight.localGradient.x);
        result.gradient.y += derivative
            * (parameters_.spatialFrequency * waveY
                + weightDerivative * weight.localGradient.y);
    }
    return result;
}

} // namespace qrp::model
