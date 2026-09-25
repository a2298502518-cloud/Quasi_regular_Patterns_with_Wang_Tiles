#include "model/ParametricQrpField.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace qrp::model {

ParametricQrpField::ParametricQrpField(
    const ParametricQrpParameters parameters)
    : parameters_(parameters) {
    const auto finite = [](const math::Vec2 value) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    };
    if (parameters_.resonanceCount < 5
        || !std::isfinite(parameters_.spatialFrequency)
        || parameters_.spatialFrequency <= 0.0
        || !finite(parameters_.globalPhase)
        || !std::isfinite(parameters_.directionalBias)
        || parameters_.directionalBias < 0.0
        || !std::isfinite(parameters_.orientationRadians)
        || !std::isfinite(parameters_.crossMix)
        || !std::isfinite(parameters_.commonPhase)
        || (parameters_.phaseHarmonicOrder != 2 && parameters_.phaseHarmonicOrder != 3)
        || parameters_.crossMix < 0.0 || parameters_.crossMix > 1.0) {
        throw std::invalid_argument(
            "Parametric QRP parameters require q >= 5, finite phases, "
            "positive frequency, finite non-negative bias, finite orientation, crossMix in [0,1], "
            "and phase harmonic order 2 or 3.");
    }

    // 方向与权重仅依赖构造参数，不在每个像素重新计算。
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    const double inverseCount = 1.0 / static_cast<double>(parameters_.resonanceCount);
    const double rotationCos = std::cos(parameters_.orientationRadians);
    const double rotationSin = std::sin(parameters_.orientationRadians);
    double weightSum = 0.0;
    double minimumHarmonic = 1.0;
    modes_.reserve(parameters_.resonanceCount);
    for (std::uint32_t index = 0; index < parameters_.resonanceCount; ++index) {
        const double theta = tau * static_cast<double>(index) * inverseCount;
        const math::Vec2 direction{std::cos(theta), std::sin(theta)};
        // 方向权重固定用二阶，相位基的选择不能连带改变谱幅度。
        const math::Vec2 weightHarmonic{std::cos(2.0 * theta), std::sin(2.0 * theta)};
        const double phaseAngle = parameters_.phaseHarmonicOrder * theta;
        const math::Vec2 phaseHarmonic{std::cos(phaseAngle), std::sin(phaseAngle)};
        const double weight = std::exp(parameters_.directionalBias * (weightHarmonic.x - 1.0));
        modes_.push_back({
            parameters_.orientationRadians == 0.0 ? direction : math::Vec2{
                rotationCos * direction.x - rotationSin * direction.y,
                rotationSin * direction.x + rotationCos * direction.y},
            weightHarmonic, phaseHarmonic, weight});
        weightSum += weight;
        minimumHarmonic = std::min(minimumHarmonic, weightHarmonic.x);
    }
    double crossWeightSum = 0.0;
    for (const auto& mode : modes_) {
        crossWeightSum += std::exp(parameters_.directionalBias * (minimumHarmonic - mode.weightHarmonic.x));
    }
    for (auto& mode : modes_) {
        // 零偏置严格恢复旧的 1/q；至少第 0 个方向权重为 1，分母不会为零。
        if (parameters_.directionalBias == 0.0) {
            mode.amplitude = inverseCount;
        } else {
            const double mainWeight = mode.amplitude / weightSum;
            const double crossWeight = std::exp(parameters_.directionalBias
                * (minimumHarmonic - mode.weightHarmonic.x)) / crossWeightSum;
            mode.amplitude = (1.0 - parameters_.crossMix) * mainWeight
                + parameters_.crossMix * crossWeight;
        }
    }
}

const ParametricQrpParameters& ParametricQrpField::parameters() const noexcept {
    return parameters_;
}

ScalarFieldEvaluation ParametricQrpField::evaluate(
    const math::Vec2 position) const noexcept {
    ScalarFieldEvaluation result;

    for (const auto& mode : modes_) {
        const double waveX = mode.direction.x;
        const double waveY = mode.direction.y;
        const double harmonicX = mode.phaseHarmonic.x;
        const double harmonicY = mode.phaseHarmonic.y;
        const double phase = parameters_.spatialFrequency
                * (waveX * position.x + waveY * position.y)
            + parameters_.globalPhase.x * harmonicX
            + parameters_.globalPhase.y * harmonicY
            + parameters_.commonPhase;
        const double derivative = -std::sin(phase) * mode.amplitude;

        result.value += std::cos(phase) * mode.amplitude;
        result.gradient.x += derivative * (parameters_.spatialFrequency * waveX);
        result.gradient.y += derivative * (parameters_.spatialFrequency * waveY);
    }
    return result;
}

} // namespace qrp::model
