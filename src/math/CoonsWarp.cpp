#include "math/CoonsWarp.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace qrp::math {
namespace {

[[nodiscard]] double norm(const Vec2 value) noexcept {
    return std::hypot(value.x, value.y);
}

[[nodiscard]] bool finite(const Vec2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool insideUnitSquare(const Vec2 value) noexcept {
    return value.x >= 0.0 && value.x <= 1.0
        && value.y >= 0.0 && value.y <= 1.0;
}

[[nodiscard]] bool validOptions(const InverseWarpOptions& options) noexcept {
    return std::isfinite(options.residualTolerance) && options.residualTolerance > 0.0
        && std::isfinite(options.determinantTolerance) && options.determinantTolerance > 0.0
        && std::isfinite(options.domainTolerance) && options.domainTolerance >= 0.0
        && std::isfinite(options.boundaryTolerance) && options.boundaryTolerance >= 0.0
        && options.boundaryTolerance <= options.domainTolerance
        && options.maxIterations > 0;
}

[[nodiscard]] double edgeDifferenceUpperBound(
    const EdgeFunction& first,
    const EdgeFunction& second) noexcept {
    const auto& a = first.parameters();
    const auto& b = second.parameters();
    // 两个单位项相消，系数绝对值之和给出全区间解析上界。
    return std::abs(a.epsilon - b.epsilon) + std::abs(a.delta - b.delta);
}

[[nodiscard]] InverseWarpResult boundaryResult(
    const CoonsWarp& warp,
    const Vec2 physical,
    const Vec2 parameter,
    const ScalarInverseResult& scalarResult) noexcept {
    InverseWarpResult result;
    result.parameter = parameter;
    result.iterations = scalarResult.iterations;
    result.boundarySolveUsed = true;
    const Vec2 mapped = warp.map(parameter);
    result.residualNorm = norm(Vec2{mapped.x - physical.x, mapped.y - physical.y});
    result.minimumDeterminant = warp.determinant(parameter);
    result.converged = scalarResult.converged;
    result.failure = scalarResult.converged
        ? InverseWarpFailure::None
        : InverseWarpFailure::BoundarySolveFailed;
    return result;
}

} // namespace

double Jacobian2::determinant() const noexcept {
    return dXdu * dYdv - dXdv * dYdu;
}

CoonsWarp::CoonsWarp(CoonsEdges edges)
    : edges_(std::move(edges)) {}

const CoonsEdges& CoonsWarp::edges() const noexcept {
    return edges_;
}

Vec2 CoonsWarp::map(const Vec2 parameter) const noexcept {
    const double u = parameter.x;
    const double v = parameter.y;
    return {
        (1.0 - v) * edges_.south.evaluate(u) + v * edges_.north.evaluate(u),
        (1.0 - u) * edges_.west.evaluate(v) + u * edges_.east.evaluate(v),
    };
}

Jacobian2 CoonsWarp::jacobian(const Vec2 parameter) const noexcept {
    const double u = parameter.x;
    const double v = parameter.y;
    return {
        (1.0 - v) * edges_.south.derivative(u) + v * edges_.north.derivative(u),
        edges_.north.evaluate(u) - edges_.south.evaluate(u),
        edges_.east.evaluate(v) - edges_.west.evaluate(v),
        (1.0 - u) * edges_.west.derivative(v) + u * edges_.east.derivative(v),
    };
}

double CoonsWarp::determinant(const Vec2 parameter) const noexcept {
    return jacobian(parameter).determinant();
}

WarpSafetyReport CoonsWarp::validateSafety(const WarpSafetyOptions& options) const {
    WarpSafetyReport report;
    if (!std::isfinite(options.derivativeMargin) || options.derivativeMargin < 0.0
        || !std::isfinite(options.jacobianMargin) || options.jacobianMargin <= 0.0) {
        report.message = "Invalid safety options.";
        return report;
    }

    report.lambdaHorizontal = std::min(
        edges_.south.conservativeDerivativeLowerBound(),
        edges_.north.conservativeDerivativeLowerBound());
    report.lambdaVertical = std::min(
        edges_.west.conservativeDerivativeLowerBound(),
        edges_.east.conservativeDerivativeLowerBound());
    report.deltaSouthNorthUpperBound = edgeDifferenceUpperBound(
        edges_.south,
        edges_.north);
    report.deltaEastWestUpperBound = edgeDifferenceUpperBound(
        edges_.west,
        edges_.east);
    report.determinantLowerBound = report.lambdaHorizontal * report.lambdaVertical
        - report.deltaSouthNorthUpperBound * report.deltaEastWestUpperBound;

    if (!std::isfinite(report.lambdaHorizontal)
        || !std::isfinite(report.lambdaVertical)
        || !std::isfinite(report.deltaSouthNorthUpperBound)
        || !std::isfinite(report.deltaEastWestUpperBound)
        || !std::isfinite(report.determinantLowerBound)) {
        report.message = "The edge parameters produce non-finite safety bounds.";
        return report;
    }

    if (report.lambdaHorizontal <= options.derivativeMargin
        || report.lambdaVertical <= options.derivativeMargin) {
        report.message = "At least one edge derivative lacks the required positive margin.";
        return report;
    }
    if (report.determinantLowerBound < options.jacobianMargin) {
        report.message = "The conservative Jacobian lower bound is below the required margin.";
        return report;
    }

    report.valid = true;
    report.message = "The warp satisfies the conservative analytic safety bounds.";
    return report;
}

double CoonsWarp::sampledMinimumDeterminant(const std::uint32_t samplesPerAxis) const noexcept {
    if (samplesPerAxis < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double minimum = std::numeric_limits<double>::infinity();
    const double denominator = static_cast<double>(samplesPerAxis - 1);
    for (std::uint32_t y = 0; y < samplesPerAxis; ++y) {
        for (std::uint32_t x = 0; x < samplesPerAxis; ++x) {
            const Vec2 parameter{
                static_cast<double>(x) / denominator,
                static_cast<double>(y) / denominator,
            };
            minimum = std::min(minimum, determinant(parameter));
        }
    }
    return minimum;
}

InverseWarpResult CoonsWarp::inverse(
    Vec2 physical,
    const InverseWarpOptions& options) const noexcept {
    InverseWarpResult result;
    if (!finite(physical)) {
        result.failure = InverseWarpFailure::NonFiniteInput;
        return result;
    }
    if (!validOptions(options)) {
        result.failure = InverseWarpFailure::InvalidOptions;
        return result;
    }
    if (physical.x < -options.domainTolerance
        || physical.x > 1.0 + options.domainTolerance
        || physical.y < -options.domainTolerance
        || physical.y > 1.0 + options.domainTolerance) {
        result.failure = InverseWarpFailure::TargetOutsideDomain;
        return result;
    }

    physical.x = std::clamp(physical.x, 0.0, 1.0);
    physical.y = std::clamp(physical.y, 0.0, 1.0);

    const ScalarInverseOptions scalarOptions{
        options.residualTolerance,
        options.determinantTolerance,
        std::max<std::uint32_t>(options.maxIterations, 32),
    };

    // 边界直接求一维逆，避免两块瓦片因二维迭代路径不同产生接缝误差。
    if (physical.y <= options.boundaryTolerance) {
        const auto scalar = edges_.south.inverse(physical.x, scalarOptions);
        result = boundaryResult(*this, physical, Vec2{scalar.parameter, 0.0}, scalar);
        return result;
    }
    if (1.0 - physical.y <= options.boundaryTolerance) {
        const auto scalar = edges_.north.inverse(physical.x, scalarOptions);
        result = boundaryResult(*this, physical, Vec2{scalar.parameter, 1.0}, scalar);
        return result;
    }
    if (physical.x <= options.boundaryTolerance) {
        const auto scalar = edges_.west.inverse(physical.y, scalarOptions);
        result = boundaryResult(*this, physical, Vec2{0.0, scalar.parameter}, scalar);
        return result;
    }
    if (1.0 - physical.x <= options.boundaryTolerance) {
        const auto scalar = edges_.east.inverse(physical.y, scalarOptions);
        result = boundaryResult(*this, physical, Vec2{1.0, scalar.parameter}, scalar);
        return result;
    }

    Vec2 parameter = physical;
    for (std::uint32_t iteration = 0; iteration < options.maxIterations; ++iteration) {
        const Vec2 mapped = map(parameter);
        const Vec2 residual{physical.x - mapped.x, physical.y - mapped.y};
        const double residualNorm = norm(residual);
        result.parameter = parameter;
        result.residualNorm = residualNorm;
        result.iterations = iteration;

        const Jacobian2 matrix = jacobian(parameter);
        const double determinantValue = matrix.determinant();
        result.minimumDeterminant = std::min(
            result.minimumDeterminant,
            determinantValue);
        if (!std::isfinite(determinantValue)
            || determinantValue <= options.determinantTolerance) {
            result.failure = InverseWarpFailure::SingularJacobian;
            return result;
        }

        if (residualNorm <= options.residualTolerance) {
            result.converged = true;
            result.failure = InverseWarpFailure::None;
            return result;
        }

        const Vec2 delta{
            (residual.x * matrix.dYdv - matrix.dXdv * residual.y) / determinantValue,
            (matrix.dXdu * residual.y - residual.x * matrix.dYdu) / determinantValue,
        };

        bool accepted = false;
        double scale = 1.0;
        for (std::uint32_t step = 0; step <= options.maxBacktrackingSteps; ++step) {
            const Vec2 candidate{
                parameter.x + scale * delta.x,
                parameter.y + scale * delta.y,
            };
            if (insideUnitSquare(candidate)) {
                const Vec2 candidateMapped = map(candidate);
                const double candidateResidual = norm(Vec2{
                    physical.x - candidateMapped.x,
                    physical.y - candidateMapped.y,
                });
                if (std::isfinite(candidateResidual) && candidateResidual < residualNorm) {
                    parameter = candidate;
                    accepted = true;
                    if (scale < 1.0) {
                        result.stepLimited = true;
                    }
                    break;
                }
            }
            scale *= 0.5;
        }

        if (!accepted) {
            result.failure = InverseWarpFailure::NoProgress;
            return result;
        }
    }

    result.parameter = parameter;
    const Vec2 mapped = map(parameter);
    result.residualNorm = norm(Vec2{physical.x - mapped.x, physical.y - mapped.y});
    result.minimumDeterminant = std::min(
        result.minimumDeterminant,
        determinant(parameter));
    result.iterations = options.maxIterations;
    result.converged = result.residualNorm <= options.residualTolerance;
    result.failure = result.converged
        ? InverseWarpFailure::None
        : InverseWarpFailure::MaximumIterations;
    return result;
}

} // namespace qrp::math
