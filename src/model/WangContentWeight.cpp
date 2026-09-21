#include "model/WangContentWeight.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace qrp::model {
namespace {

struct HermiteBasis {
    double h00 = 0.0;
    double h10 = 0.0;
    double h01 = 0.0;
    double h11 = 0.0;
};

struct EdgeData {
    double valueDeviation = 0.0;
    double valueDeviationDerivative = 0.0;
    double transverseDerivative = 0.0;
    double transverseDerivativeAlongEdgeDerivative = 0.0;
};

[[nodiscard]] double bubble(const double parameter) noexcept {
    const double complement = 1.0 - parameter;
    return 16.0 * parameter * parameter * complement * complement;
}

[[nodiscard]] double bubbleDerivative(const double parameter) noexcept {
    return 32.0 * parameter
        - 96.0 * parameter * parameter
        + 64.0 * parameter * parameter * parameter;
}

[[nodiscard]] HermiteBasis hermiteBasis(const double parameter) noexcept {
    const double squared = parameter * parameter;
    const double cubed = squared * parameter;
    return {
        2.0 * cubed - 3.0 * squared + 1.0,
        cubed - 2.0 * squared + parameter,
        -2.0 * cubed + 3.0 * squared,
        cubed - squared,
    };
}

[[nodiscard]] HermiteBasis hermiteBasisDerivative(const double parameter) noexcept {
    const double squared = parameter * parameter;
    return {
        6.0 * squared - 6.0 * parameter,
        3.0 * squared - 4.0 * parameter + 1.0,
        -6.0 * squared + 6.0 * parameter,
        3.0 * squared - 2.0 * parameter,
    };
}

[[nodiscard]] EdgeData edgeData(
    const WangContentWeightParameters& parameters,
    const std::uint32_t label,
    const double edgeParameter) noexcept {
    const double angle = 2.0 * std::numbers::pi_v<double>
        * static_cast<double>(label) / static_cast<double>(parameters.labelCount);
    const double valueScale = parameters.edgeValueAmplitude * std::cos(angle);
    const double transverseScale = parameters.transverseDerivativeAmplitude
        * std::sin(angle);
    const double envelope = bubble(edgeParameter);
    const double envelopeDerivative = bubbleDerivative(edgeParameter);
    return {
        valueScale * envelope,
        valueScale * envelopeDerivative,
        transverseScale * envelope,
        transverseScale * envelopeDerivative,
    };
}

[[nodiscard]] double blend(
    const HermiteBasis& basis,
    const double startValue,
    const double startDerivative,
    const double endValue,
    const double endDerivative) noexcept {
    return basis.h00 * startValue
        + basis.h10 * startDerivative
        + basis.h01 * endValue
        + basis.h11 * endDerivative;
}

} // namespace

WangContentWeight::WangContentWeight(const WangContentWeightParameters parameters)
    : parameters_(parameters) {
    if (parameters_.labelCount < 3) {
        throw std::invalid_argument("Wang content weights require at least three labels.");
    }
    if (!std::isfinite(parameters_.neutralWeight)
        || !std::isfinite(parameters_.edgeValueAmplitude)
        || !std::isfinite(parameters_.transverseDerivativeAmplitude)) {
        throw std::invalid_argument("Wang content weight parameters must be finite.");
    }
    if (parameters_.neutralWeight <= 0.0 || parameters_.neutralWeight >= 1.0) {
        throw std::invalid_argument("The neutral Wang content weight must lie in (0, 1).");
    }
    const double availableDeviation = std::min(
        parameters_.neutralWeight,
        1.0 - parameters_.neutralWeight);
    if (maximumDeviationUpperBound() > availableDeviation) {
        throw std::invalid_argument(
            "The analytic Wang content weight bound leaves the interval [0, 1].");
    }
}

const WangContentWeightParameters& WangContentWeight::parameters() const noexcept {
    return parameters_;
}

double WangContentWeight::maximumDeviationUpperBound() const noexcept {
    return 2.0 * std::abs(parameters_.edgeValueAmplitude)
        + 0.5 * std::abs(parameters_.transverseDerivativeAmplitude);
}

double WangContentWeight::certifiedMinimum() const noexcept {
    return parameters_.neutralWeight - maximumDeviationUpperBound();
}

double WangContentWeight::certifiedMaximum() const noexcept {
    return parameters_.neutralWeight + maximumDeviationUpperBound();
}

WangContentWeightEvaluation WangContentWeight::evaluate(
    const WangTile& tile,
    const math::Vec2 local) const noexcept {
    const EdgeData south = edgeData(parameters_, tile.south, local.x);
    const EdgeData north = edgeData(parameters_, tile.north, local.x);
    const EdgeData west = edgeData(parameters_, tile.west, local.y);
    const EdgeData east = edgeData(parameters_, tile.east, local.y);

    const HermiteBasis horizontal = hermiteBasis(local.x);
    const HermiteBasis horizontalDerivative = hermiteBasisDerivative(local.x);
    const HermiteBasis vertical = hermiteBasis(local.y);
    const HermiteBasis verticalDerivative = hermiteBasisDerivative(local.y);

    const double px = blend(
        horizontal,
        west.valueDeviation,
        west.transverseDerivative,
        east.valueDeviation,
        east.transverseDerivative);
    const double py = blend(
        vertical,
        south.valueDeviation,
        south.transverseDerivative,
        north.valueDeviation,
        north.transverseDerivative);

    const double pxU = blend(
        horizontalDerivative,
        west.valueDeviation,
        west.transverseDerivative,
        east.valueDeviation,
        east.transverseDerivative);
    const double pxV = blend(
        horizontal,
        west.valueDeviationDerivative,
        west.transverseDerivativeAlongEdgeDerivative,
        east.valueDeviationDerivative,
        east.transverseDerivativeAlongEdgeDerivative);
    const double pyU = blend(
        vertical,
        south.valueDeviationDerivative,
        south.transverseDerivativeAlongEdgeDerivative,
        north.valueDeviationDerivative,
        north.transverseDerivativeAlongEdgeDerivative);
    const double pyV = blend(
        verticalDerivative,
        south.valueDeviation,
        south.transverseDerivative,
        north.valueDeviation,
        north.transverseDerivative);

    return {
        parameters_.neutralWeight + px + py,
        {pxU + pyU, pxV + pyV},
    };
}

} // namespace qrp::model
