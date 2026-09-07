#pragma once

#include <cstdint>

namespace qrp::math {

struct EdgeParameters {
    double epsilon = 0.0;
    double delta = 0.0;

    [[nodiscard]] bool operator==(const EdgeParameters&) const noexcept = default;
};

struct ScalarInverseOptions {
    double residualTolerance = 1.0e-13;
    double derivativeTolerance = 1.0e-14;
    std::uint32_t maxIterations = 64;
};

enum class ScalarInverseFailure {
    None,
    NonFiniteInput,
    InvalidOptions,
    TargetOutsideDomain,
    NonMonotoneDerivative,
    MaximumIterations,
};

struct ScalarInverseResult {
    double parameter = 0.0;
    double residual = 0.0;
    std::uint32_t iterations = 0;
    bool converged = false;
    bool usedBisection = false;
    ScalarInverseFailure failure = ScalarInverseFailure::None;
};

class EdgeFunction final {
public:
    explicit EdgeFunction(EdgeParameters parameters = {});

    [[nodiscard]] const EdgeParameters& parameters() const noexcept;
    [[nodiscard]] double evaluate(double parameter) const noexcept;
    [[nodiscard]] double derivative(double parameter) const noexcept;

    // 这是解析保守下界，用于提交参数前的安全判定，而不是采样估计。
    [[nodiscard]] double conservativeDerivativeLowerBound() const noexcept;
    [[nodiscard]] bool hasPositiveDerivative(double margin = 0.0) const noexcept;

    [[nodiscard]] ScalarInverseResult inverse(
        double value,
        const ScalarInverseOptions& options = {}) const noexcept;

private:
    EdgeParameters parameters_;
};

} // namespace qrp::math
