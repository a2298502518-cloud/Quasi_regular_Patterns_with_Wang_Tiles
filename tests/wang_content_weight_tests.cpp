#include "model/WangContentWeight.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using qrp::math::Vec2;
using qrp::model::WangContentWeight;
using qrp::model::WangContentWeightEvaluation;
using qrp::model::WangContentWeightParameters;
using qrp::model::WangTile;

constexpr std::uint32_t kLabelCount = 5;
constexpr double kTolerance = 2.0e-12;

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

[[nodiscard]] WangContentWeightParameters parameters() noexcept {
    return {kLabelCount, 0.5, 0.1, 0.2};
}

[[nodiscard]] WangTile tile(
    const std::uint32_t south,
    const std::uint32_t north,
    const std::uint32_t west,
    const std::uint32_t east) noexcept {
    return {south, north, west, east, 0};
}

template <typename Function>
void forEachSignature(Function&& function) {
    for (std::uint32_t south = 0; south < kLabelCount; ++south) {
        for (std::uint32_t north = 0; north < kLabelCount; ++north) {
            for (std::uint32_t west = 0; west < kLabelCount; ++west) {
                for (std::uint32_t east = 0; east < kLabelCount; ++east) {
                    function(tile(south, north, west, east));
                }
            }
        }
    }
}

void requireSameEvaluation(
    const WangContentWeightEvaluation& first,
    const WangContentWeightEvaluation& second,
    const std::string& label) {
    requireNear(first.value, second.value, kTolerance, label + ".value");
    requireNear(
        first.localGradient.x,
        second.localGradient.x,
        kTolerance,
        label + ".du");
    requireNear(
        first.localGradient.y,
        second.localGradient.y,
        kTolerance,
        label + ".dv");
}

void testAnalyticGradientAgainstFiniteDifferences() {
    const WangContentWeight weight(parameters());
    const std::array<WangTile, 4> signatures{{
        tile(0, 1, 2, 4),
        tile(4, 3, 1, 0),
        tile(2, 2, 3, 1),
        tile(1, 4, 0, 3),
    }};
    const std::array<Vec2, 4> samples{{
        {0.13, 0.21},
        {0.37, 0.61},
        {0.72, 0.44},
        {0.89, 0.83},
    }};
    constexpr double step = 1.0e-6;

    for (const WangTile& signature : signatures) {
        for (const Vec2 sample : samples) {
            const auto evaluation = weight.evaluate(signature, sample);
            const double finiteDifferenceU = (
                weight.evaluate(signature, {sample.x + step, sample.y}).value
                - weight.evaluate(signature, {sample.x - step, sample.y}).value)
                / (2.0 * step);
            const double finiteDifferenceV = (
                weight.evaluate(signature, {sample.x, sample.y + step}).value
                - weight.evaluate(signature, {sample.x, sample.y - step}).value)
                / (2.0 * step);
            requireNear(
                evaluation.localGradient.x,
                finiteDifferenceU,
                3.0e-9,
                "analytic du");
            requireNear(
                evaluation.localGradient.y,
                finiteDifferenceV,
                3.0e-9,
                "analytic dv");
        }
    }
}

void testAllSignaturesRemainFiniteAndInRange() {
    const WangContentWeight weight(parameters());
    requireNear(weight.maximumDeviationUpperBound(), 0.3, 1.0e-15, "deviation");
    requireNear(weight.certifiedMinimum(), 0.2, 1.0e-15, "minimum");
    requireNear(weight.certifiedMaximum(), 0.8, 1.0e-15, "maximum");

    std::size_t signatureCount = 0;
    forEachSignature([&](const WangTile& signature) {
        ++signatureCount;
        for (std::size_t y = 0; y <= 4; ++y) {
            for (std::size_t x = 0; x <= 4; ++x) {
                const auto evaluation = weight.evaluate(
                    signature,
                    {static_cast<double>(x) / 4.0, static_cast<double>(y) / 4.0});
                require(
                    std::isfinite(evaluation.value)
                        && std::isfinite(evaluation.localGradient.x)
                        && std::isfinite(evaluation.localGradient.y),
                    "weight evaluation must remain finite");
                require(
                    evaluation.value >= weight.certifiedMinimum() - kTolerance
                        && evaluation.value <= weight.certifiedMaximum() + kTolerance,
                    "sampled value must respect the analytic certificate");
                require(
                    evaluation.value >= -kTolerance
                        && evaluation.value <= 1.0 + kTolerance,
                    "sampled value must remain in [0, 1]");
            }
        }
    });
    require(signatureCount == 625, "range test must cover all 5^4 signatures");
}

void testEveryCompatibleEdgeHasMatchingFirstJet() {
    const WangContentWeight weight(parameters());
    constexpr std::array<double, 5> samples{0.0, 0.19, 0.5, 0.81, 1.0};
    std::size_t verticalPairs = 0;
    std::size_t horizontalPairs = 0;

    forEachSignature([&](const WangTile& left) {
        for (std::uint32_t south = 0; south < kLabelCount; ++south) {
            for (std::uint32_t north = 0; north < kLabelCount; ++north) {
                for (std::uint32_t east = 0; east < kLabelCount; ++east) {
                    const WangTile right = tile(south, north, left.east, east);
                    for (const double parameter : samples) {
                        requireSameEvaluation(
                            weight.evaluate(left, {1.0, parameter}),
                            weight.evaluate(right, {0.0, parameter}),
                            "vertical edge first jet");
                    }
                    ++verticalPairs;
                }
            }
        }
    });

    forEachSignature([&](const WangTile& lower) {
        for (std::uint32_t north = 0; north < kLabelCount; ++north) {
            for (std::uint32_t west = 0; west < kLabelCount; ++west) {
                for (std::uint32_t east = 0; east < kLabelCount; ++east) {
                    const WangTile upper = tile(lower.north, north, west, east);
                    for (const double parameter : samples) {
                        requireSameEvaluation(
                            weight.evaluate(lower, {parameter, 1.0}),
                            weight.evaluate(upper, {parameter, 0.0}),
                            "horizontal edge first jet");
                    }
                    ++horizontalPairs;
                }
            }
        }
    });

    constexpr std::size_t compatiblePairCount = 625 * 125;
    require(
        verticalPairs == compatiblePairCount,
        "vertical test must cover every compatible signature pair");
    require(
        horizontalPairs == compatiblePairCount,
        "horizontal test must cover every compatible signature pair");
}

void testInvalidParametersAreRejected() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::array<WangContentWeightParameters, 7> invalid{{
        {2, 0.5, 0.1, 0.2},
        {kLabelCount, 0.0, 0.0, 0.0},
        {kLabelCount, 1.0, 0.0, 0.0},
        {kLabelCount, nan, 0.0, 0.0},
        {kLabelCount, 0.5, nan, 0.0},
        {kLabelCount, 0.5, 0.0, infinity},
        {kLabelCount, 0.5, 0.3, 0.0},
    }};

    for (const auto& candidate : invalid) {
        requireInvalidArgument(
            [&candidate]() {
                static_cast<void>(WangContentWeight(candidate));
            },
            "invalid parameter set must be rejected");
    }
}

} // namespace

int main() {
    const std::array<TestCase, 4> tests{{
        {"analytic gradient finite differences", testAnalyticGradientAgainstFiniteDifferences},
        {"all signatures remain in range", testAllSignaturesRemainFiniteAndInRange},
        {"all compatible edges preserve the first jet", testEveryCompatibleEdgeHasMatchingFirstJet},
        {"invalid parameters are rejected", testInvalidParametersAreRejected},
    }};

    int failures = 0;
    for (const TestCase& test : tests) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << tests.size() << " test(s) passed.\n";
    return EXIT_SUCCESS;
}
