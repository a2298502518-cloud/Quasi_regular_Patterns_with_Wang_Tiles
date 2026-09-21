#include "model/HierarchicalQrpComposition.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace qrp::model {
namespace {

[[nodiscard]] math::Vec2 add(
    const math::Vec2 first,
    const math::Vec2 second) noexcept {
    return {first.x + second.x, first.y + second.y};
}

[[nodiscard]] math::Vec2 scale(
    const math::Vec2 value,
    const double amount) noexcept {
    return {amount * value.x, amount * value.y};
}

} // namespace

HierarchicalQrpComposition::HierarchicalQrpComposition(
    const HierarchicalQrpCompositionParameters parameters)
    : parameters_(parameters) {
    constexpr double rootTwo = std::numbers::sqrt2_v<double>;
    if (!std::isfinite(parameters_.mediumStrength)
        || !std::isfinite(parameters_.fineStrength)
        || parameters_.mediumStrength < 0.0
        || parameters_.fineStrength < 0.0
        || 2.0 * rootTwo
                * (parameters_.mediumStrength + parameters_.fineStrength)
            > 1.0) {
        throw std::invalid_argument(
            "Hierarchical QRP strengths must be finite, non-negative, and range-safe.");
    }
}

const HierarchicalQrpCompositionParameters&
HierarchicalQrpComposition::parameters() const noexcept {
    return parameters_;
}

double HierarchicalQrpComposition::certifiedMaximumMagnitude() const noexcept {
    return 1.0;
}

ScalarFieldEvaluation HierarchicalQrpComposition::evaluate(
    const ScalarFieldEvaluation coarse,
    const ScalarFieldEvaluation medium,
    const ScalarFieldEvaluation fine) const noexcept {
    const double envelope = 1.0 - coarse.value * coarse.value;
    const double mediumScale = parameters_.mediumStrength * envelope;
    const double fineScale = parameters_.fineStrength * envelope * envelope;
    const double coarseGradientScale = 1.0
        - 2.0 * parameters_.mediumStrength * coarse.value * medium.value
        - 4.0 * parameters_.fineStrength
            * coarse.value * envelope * fine.value;
    return {
        coarse.value + mediumScale * medium.value + fineScale * fine.value,
        add(
            add(
                scale(coarse.gradient, coarseGradientScale),
                scale(medium.gradient, mediumScale)),
            scale(fine.gradient, fineScale)),
    };
}

} // namespace qrp::model
