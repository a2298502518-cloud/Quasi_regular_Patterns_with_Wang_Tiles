#include "generators/CanonicalQrpField.hpp"
#include "model/ParametricWangQrpField.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using qrp::generators::CanonicalQrpField;
using qrp::generators::CanonicalQrpParameters;
using qrp::math::Vec2;
using qrp::model::ParametricWangQrpField;
using qrp::model::ParametricWangQrpParameters;
using qrp::model::WangContentWeightEvaluation;

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
        throw std::runtime_error(label + ": expected " + std::to_string(expected)
            + ", got " + std::to_string(actual));
    }
}

[[nodiscard]] WangContentWeightEvaluation weightAt(const Vec2 point) noexcept {
    return {
        0.5 + 0.18 * std::sin(0.7 * point.x) * std::cos(0.9 * point.y),
        {
            0.126 * std::cos(0.7 * point.x) * std::cos(0.9 * point.y),
            -0.162 * std::sin(0.7 * point.x) * std::sin(0.9 * point.y),
        },
    };
}

void testCanonicalRecovery() {
    constexpr std::uint32_t q = 7;
    constexpr double frequency = 2.3;
    const ParametricWangQrpField field({q, frequency, {}, {}, 0.5, 0.4});
    const CanonicalQrpField canonical(
        CanonicalQrpParameters{static_cast<double>(q), 1.0, {}});
    for (const Vec2 point : std::array<Vec2, 3>{{{0.0, 0.0}, {0.37, -0.82}, {-1.2, 0.61}}}) {
        const auto actual = field.evaluate(point, weightAt(point));
        const auto expected = canonical.evaluateModelWithGradient(
            {frequency * point.x, frequency * point.y});
        requireNear(actual.value, expected.value / q, 2.0e-14, "canonical value");
        requireNear(
            actual.gradient.x,
            frequency * expected.modelGradient.x / q,
            3.0e-14,
            "canonical gradient x");
        requireNear(
            actual.gradient.y,
            frequency * expected.modelGradient.y / q,
            3.0e-14,
            "canonical gradient y");
    }
}

void testAnalyticGradient() {
    const ParametricWangQrpField field(
        {7, 2.1, {0.4, -0.25}, {0.75, 0.3}, 0.5, 0.4});
    constexpr Vec2 point{0.37, -0.52};
    constexpr double step = 1.0e-6;
    const auto evaluate = [&field](const Vec2 position) {
        return field.evaluate(position, weightAt(position));
    };
    const auto actual = evaluate(point);
    const double dx = (evaluate({point.x + step, point.y}).value
        - evaluate({point.x - step, point.y}).value) / (2.0 * step);
    const double dy = (evaluate({point.x, point.y + step}).value
        - evaluate({point.x, point.y - step}).value) / (2.0 * step);
    requireNear(actual.gradient.x, dx, 3.0e-10, "finite-difference x");
    requireNear(actual.gradient.y, dy, 3.0e-10, "finite-difference y");
}

void testParametersChangeField() {
    constexpr Vec2 point{2.31, -1.47};
    constexpr WangContentWeightEvaluation weight{0.78, {0.0, 0.0}};
    const ParametricWangQrpField baseline({5, 1.8, {}, {}, 0.5, 0.4});
    const double reference = baseline.evaluate(point, weight).value;
    const std::array<ParametricWangQrpParameters, 3> variants{{
        {7, 1.8, {}, {}, 0.5, 0.4},
        {5, 2.4, {0.55, -0.2}, {}, 0.5, 0.4},
        {5, 1.8, {}, {0.8, 0.35}, 0.5, 0.4},
    }};
    for (const auto& parameters : variants) {
        const double changed = ParametricWangQrpField(parameters).evaluate(point, weight).value;
        require(std::abs(changed - reference) > 1.0e-4, "parameter variant must change field");
    }
}

void testRange() {
    const ParametricWangQrpField field(
        {9, 3.2, {0.7, -0.45}, {1.1, 0.6}, 0.5, 0.4});
    for (int y = -12; y <= 12; ++y) {
        for (int x = -12; x <= 12; ++x) {
            const Vec2 point{static_cast<double>(x) / 5.0, static_cast<double>(y) / 5.0};
            const double value = field.evaluate(point, weightAt(point)).value;
            require(std::abs(value) <= 1.0 + 2.0e-15, "cosine average range");
        }
    }
}

void testInvalidParameters() {
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    const std::array<ParametricWangQrpParameters, 6> invalid{{
        {4, 1.0, {}, {}, 0.5, 0.4},
        {5, 0.0, {}, {}, 0.5, 0.4},
        {5, nan, {}, {}, 0.5, 0.4},
        {5, 1.0, {nan, 0.0}, {}, 0.5, 0.4},
        {5, 1.0, {}, {0.0, nan}, 0.5, 0.4},
        {5, 1.0, {}, {}, 0.5, 0.0},
    }};
    for (const auto& parameters : invalid) {
        bool rejected = false;
        try {
            [[maybe_unused]] const ParametricWangQrpField field(parameters);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "invalid parameter row must be rejected");
    }
}

constexpr std::array<TestCase, 5> kTests{{
    {"canonical recovery", testCanonicalRecovery},
    {"analytic gradient", testAnalyticGradient},
    {"parameters change field", testParametersChangeField},
    {"certified range samples", testRange},
    {"invalid parameters", testInvalidParameters},
}};

} // namespace

int main() {
    std::size_t passed = 0;
    for (const auto& test : kTests) {
        try {
            test.function();
            ++passed;
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << passed << '/' << kTests.size() << " parametric Wang-QRP tests passed\n";
    return passed == kTests.size() ? EXIT_SUCCESS : EXIT_FAILURE;
}
