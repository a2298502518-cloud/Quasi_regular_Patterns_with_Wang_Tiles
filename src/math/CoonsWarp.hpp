#pragma once

#include "math/EdgeFunction.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace qrp::math {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Jacobian2 {
    double dXdu = 1.0;
    double dXdv = 0.0;
    double dYdu = 0.0;
    double dYdv = 1.0;

    [[nodiscard]] double determinant() const noexcept;
};

struct CoonsEdges {
    EdgeFunction south;
    EdgeFunction north;
    EdgeFunction west;
    EdgeFunction east;
};

struct WarpSafetyOptions {
    double derivativeMargin = 1.0e-8;
    double jacobianMargin = 1.0e-4;
};

struct WarpSafetyReport {
    bool valid = false;
    double lambdaHorizontal = 0.0;
    double lambdaVertical = 0.0;
    double deltaSouthNorthUpperBound = 0.0;
    double deltaEastWestUpperBound = 0.0;
    double determinantLowerBound = -std::numeric_limits<double>::infinity();
    std::string message;
};

struct InverseWarpOptions {
    double residualTolerance = 1.0e-12;
    double determinantTolerance = 1.0e-12;
    double domainTolerance = 1.0e-12;
    double boundaryTolerance = 1.0e-14;
    std::uint32_t maxIterations = 20;
    std::uint32_t maxBacktrackingSteps = 12;
};

enum class InverseWarpFailure {
    None,
    NonFiniteInput,
    TargetOutsideDomain,
    InvalidOptions,
    BoundarySolveFailed,
    SingularJacobian,
    NoProgress,
    MaximumIterations,
};

struct InverseWarpResult {
    Vec2 parameter;
    double residualNorm = std::numeric_limits<double>::infinity();
    double minimumDeterminant = std::numeric_limits<double>::infinity();
    std::uint32_t iterations = 0;
    bool converged = false;
    bool stepLimited = false;
    bool boundarySolveUsed = false;
    InverseWarpFailure failure = InverseWarpFailure::None;
};

class CoonsWarp final {
public:
    explicit CoonsWarp(CoonsEdges edges);

    [[nodiscard]] const CoonsEdges& edges() const noexcept;
    [[nodiscard]] Vec2 map(Vec2 parameter) const noexcept;
    [[nodiscard]] Jacobian2 jacobian(Vec2 parameter) const noexcept;
    [[nodiscard]] double determinant(Vec2 parameter) const noexcept;

    [[nodiscard]] WarpSafetyReport validateSafety(
        const WarpSafetyOptions& options = {}) const;
    [[nodiscard]] double sampledMinimumDeterminant(
        std::uint32_t samplesPerAxis) const noexcept;
    // 调用方应在参数提交时先执行 validateSafety，不能逐像素重复安全验证。
    [[nodiscard]] InverseWarpResult inverse(
        Vec2 physical,
        const InverseWarpOptions& options = {}) const noexcept;

private:
    CoonsEdges edges_;
};

} // namespace qrp::math
