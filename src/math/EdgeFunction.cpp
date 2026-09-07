#include "math/EdgeFunction.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace qrp::math {
namespace {

constexpr double kPi = std::numbers::pi_v<double>;

[[nodiscard]] bool validOptions(const ScalarInverseOptions& options) noexcept {
    return std::isfinite(options.residualTolerance) && options.residualTolerance > 0.0
        && std::isfinite(options.derivativeTolerance) && options.derivativeTolerance >= 0.0
        && options.maxIterations > 0;
}

} // namespace

EdgeFunction::EdgeFunction(const EdgeParameters parameters)
    : parameters_(parameters) {}

const EdgeParameters& EdgeFunction::parameters() const noexcept {
    return parameters_;
}

double EdgeFunction::evaluate(const double parameter) const noexcept {
    return parameter
        + parameters_.epsilon * std::sin(kPi * parameter)
        + parameters_.delta * std::sin(2.0 * kPi * parameter);
}

double EdgeFunction::derivative(const double parameter) const noexcept {
    return 1.0
        + kPi * parameters_.epsilon * std::cos(kPi * parameter)
        + 2.0 * kPi * parameters_.delta * std::cos(2.0 * kPi * parameter);
}

double EdgeFunction::conservativeDerivativeLowerBound() const noexcept {
    return 1.0 - kPi * (
        std::abs(parameters_.epsilon) + 2.0 * std::abs(parameters_.delta));
}

bool EdgeFunction::hasPositiveDerivative(const double margin) const noexcept {
    return std::isfinite(parameters_.epsilon)
        && std::isfinite(parameters_.delta)
        && std::isfinite(margin)
        && margin >= 0.0
        && conservativeDerivativeLowerBound() > margin;
}

ScalarInverseResult EdgeFunction::inverse(
    const double value,
    const ScalarInverseOptions& options) const noexcept {
    ScalarInverseResult result;

    if (!std::isfinite(value)) {
        result.failure = ScalarInverseFailure::NonFiniteInput;
        return result;
    }
    if (!validOptions(options)) {
        result.failure = ScalarInverseFailure::InvalidOptions;
        return result;
    }
    if (value < 0.0 || value > 1.0) {
        result.failure = ScalarInverseFailure::TargetOutsideDomain;
        return result;
    }
    if (!hasPositiveDerivative(options.derivativeTolerance)) {
        result.failure = ScalarInverseFailure::NonMonotoneDerivative;
        return result;
    }

    double lower = 0.0;
    double upper = 1.0;
    double parameter = value;

    for (std::uint32_t iteration = 0; iteration < options.maxIterations; ++iteration) {
        const double residual = evaluate(parameter) - value;
        result.parameter = parameter;
        result.residual = std::abs(residual);
        result.iterations = iteration;

        if (result.residual <= options.residualTolerance) {
            result.converged = true;
            result.failure = ScalarInverseFailure::None;
            return result;
        }

        if (residual < 0.0) {
            lower = parameter;
        } else {
            upper = parameter;
        }

        const double slope = derivative(parameter);
        const double newtonCandidate = parameter - residual / slope;
        if (std::isfinite(newtonCandidate)
            && newtonCandidate > lower
            && newtonCandidate < upper) {
            parameter = newtonCandidate;
        } else {
            parameter = 0.5 * (lower + upper);
            result.usedBisection = true;
        }
    }

    result.parameter = parameter;
    result.residual = std::abs(evaluate(parameter) - value);
    result.iterations = options.maxIterations;
    result.converged = result.residual <= options.residualTolerance;
    result.failure = result.converged
        ? ScalarInverseFailure::None
        : ScalarInverseFailure::MaximumIterations;
    return result;
}

} // namespace qrp::math
