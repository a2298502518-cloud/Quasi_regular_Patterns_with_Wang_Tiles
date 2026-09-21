#include "model/HierarchicalQrpComposition.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using qrp::math::Vec2;
using qrp::model::HierarchicalQrpComposition;
using qrp::model::ScalarFieldEvaluation;

constexpr double kTolerance = 2.0e-10;

struct TestCase {
    std::string_view name;
    void (*function)();
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(
    const double actual,
    const double expected,
    const double tolerance,
    const std::string& label) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            label + ": expected " + std::to_string(expected)
            + ", got " + std::to_string(actual));
    }
}

void requireNear(
    const Vec2 actual,
    const Vec2 expected,
    const double tolerance,
    const std::string& label) {
    requireNear(actual.x, expected.x, tolerance, label + " x");
    requireNear(actual.y, expected.y, tolerance, label + " y");
}

template <typename Function>
void requireInvalidArgument(Function&& function, const std::string& label) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, label);
}

[[nodiscard]] ScalarFieldEvaluation coarseField(const Vec2 point) noexcept {
    return {
        0.62 * std::sin(0.8 * point.x) * std::cos(0.9 * point.y),
        {
            0.496 * std::cos(0.8 * point.x) * std::cos(0.9 * point.y),
            -0.558 * std::sin(0.8 * point.x) * std::sin(0.9 * point.y),
        },
    };
}

[[nodiscard]] ScalarFieldEvaluation mediumField(const Vec2 point) noexcept {
    return {
        0.7 * std::cos(1.7 * point.x + 0.3 * point.y),
        {
            -1.19 * std::sin(1.7 * point.x + 0.3 * point.y),
            -0.21 * std::sin(1.7 * point.x + 0.3 * point.y),
        },
    };
}

[[nodiscard]] ScalarFieldEvaluation fineField(const Vec2 point) noexcept {
    return {
        0.5 * std::sin(2.1 * point.x - 1.3 * point.y),
        {
            1.05 * std::cos(2.1 * point.x - 1.3 * point.y),
            -0.65 * std::cos(2.1 * point.x - 1.3 * point.y),
        },
    };
}

void testHierarchyGradientAndCoarseRecovery() {
    const HierarchicalQrpComposition composition;
    constexpr Vec2 point{-0.28, 0.43};
    constexpr double step = 1.0e-6;
    const auto evaluate = [&composition](const Vec2 position) {
        return composition.evaluate(
            coarseField(position),
            mediumField(position),
            fineField(position));
    };
    const ScalarFieldEvaluation evaluation = evaluate(point);
    const double dx = (evaluate({point.x + step, point.y}).value
        - evaluate({point.x - step, point.y}).value) / (2.0 * step);
    const double dy = (evaluate({point.x, point.y + step}).value
        - evaluate({point.x, point.y - step}).value) / (2.0 * step);
    requireNear(evaluation.gradient, {dx, dy}, 2.0e-9, "hierarchy finite difference");

    const HierarchicalQrpComposition disabled({0.0, 0.0});
    const ScalarFieldEvaluation coarse = coarseField(point);
    const ScalarFieldEvaluation recovered = disabled.evaluate(
        coarse,
        mediumField(point),
        fineField(point));
    requireNear(recovered.value, coarse.value, kTolerance, "coarse recovery value");
    requireNear(recovered.gradient, coarse.gradient, kTolerance, "coarse recovery gradient");
}

void testCertifiedRangeAndInvalidParameters() {
    const HierarchicalQrpComposition composition;
    requireNear(composition.certifiedMaximumMagnitude(), 1.0, 0.0, "range certificate");
    constexpr double rootTwo = std::numbers::sqrt2_v<double>;
    for (int coarseIndex = -100; coarseIndex <= 100; ++coarseIndex) {
        const double coarse = static_cast<double>(coarseIndex) / 100.0;
        for (const double medium : {-rootTwo, rootTwo}) {
            for (const double fine : {-rootTwo, rootTwo}) {
                const double value = composition.evaluate(
                    {coarse, {}},
                    {medium, {}},
                    {fine, {}}).value;
                require(std::abs(value) <= 1.0 + 2.0e-15, "certified range sample");
            }
        }
    }

    requireInvalidArgument(
        [] { HierarchicalQrpComposition invalid({-0.01, 0.0}); },
        "negative strength");
    requireInvalidArgument(
        [] { HierarchicalQrpComposition invalid({0.3, 0.1}); },
        "unsafe strength sum");
}

constexpr std::array<TestCase, 2> kTests{{
    {"hierarchy gradient and coarse recovery", testHierarchyGradientAndCoarseRecovery},
    {"hierarchy range and invalid parameters", testCertifiedRangeAndInvalidParameters},
}};

} // namespace

int main() {
    std::size_t passed = 0;
    for (const TestCase& test : kTests) {
        try {
            test.function();
            ++passed;
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << passed << '/' << kTests.size() << " hierarchical QRP tests passed\n";
    return passed == kTests.size() ? EXIT_SUCCESS : EXIT_FAILURE;
}
