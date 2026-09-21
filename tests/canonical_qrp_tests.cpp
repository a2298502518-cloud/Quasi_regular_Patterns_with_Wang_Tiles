#include "generators/CanonicalQrpField.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using qrp::generators::CanonicalQrpField;
using qrp::generators::CanonicalQrpParameters;
using qrp::math::Vec2;

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

void testOriginAndModeCount() {
    const CanonicalQrpField integerField(CanonicalQrpParameters{5.0, 18.0, {0.0, 0.0}});
    require(integerField.modeCount() == 5, "q=5 must contain five resonance modes");
    requireNear(integerField.evaluateModel({0.0, 0.0}), 5.0, 1.0e-15, "H_5(0,0)");

    const CanonicalQrpField realField(CanonicalQrpParameters{4.8, 8.0, {0.0, 0.0}});
    require(realField.modeCount() == 4, "real q must use floor(q) modes");
    requireNear(realField.evaluateModel({0.0, 0.0}), 4.0, 1.0e-15, "H_4.8(0,0)");
    requireNear(
        realField.evaluateModel({0.37, -1.12}),
        2.4553939365984809,
        2.0e-14,
        "real q keeps q in the angular denominator");
}

void testFourFoldClosedForm() {
    const CanonicalQrpField field(CanonicalQrpParameters{4.0, 1.0, {0.0, 0.0}});
    const std::vector<Vec2> samples{{0.3, -0.8}, {1.2, 0.7}, {-2.1, 0.4}};
    for (const auto sample : samples) {
        const double expected = 2.0 * std::cos(sample.x) + 2.0 * std::cos(sample.y);
        requireNear(field.evaluateModel(sample), expected, 2.0e-14, "four-fold closed form");
    }
}

void testFourFoldClosedFormGradient() {
    const CanonicalQrpField field(CanonicalQrpParameters{4.0, 1.0, {0.0, 0.0}});
    const std::vector<Vec2> samples{{0.3, -0.8}, {1.2, 0.7}, {-2.1, 0.4}};
    for (const auto sample : samples) {
        const auto evaluation = field.evaluateModelWithGradient(sample);
        requireNear(
            evaluation.value,
            2.0 * std::cos(sample.x) + 2.0 * std::cos(sample.y),
            2.0e-14,
            "four-fold gradient value");
        requireNear(
            evaluation.modelGradient.x,
            -2.0 * std::sin(sample.x),
            2.0e-14,
            "four-fold gradient x");
        requireNear(
            evaluation.modelGradient.y,
            -2.0 * std::sin(sample.y),
            2.0e-14,
            "four-fold gradient y");
    }
}

void testIntegerRotationalSymmetry() {
    constexpr double q = 5.0;
    // Odd q gains a 2q-fold field symmetry because cos(k dot x) is unchanged by k -> -k.
    constexpr double angle = std::numbers::pi_v<double> / q;
    const CanonicalQrpField field(CanonicalQrpParameters{q, 1.0, {0.0, 0.0}});
    const Vec2 point{0.73, -1.21};
    const Vec2 rotated{
        std::cos(angle) * point.x - std::sin(angle) * point.y,
        std::sin(angle) * point.x + std::cos(angle) * point.y,
    };
    requireNear(
        field.evaluateModel(rotated),
        field.evaluateModel(point),
        2.0e-14,
        "ten-fold field rotational symmetry");
}

void testCrystallographicSpecialCasePeriodicity() {
    const CanonicalQrpField field(CanonicalQrpParameters{4.0, 1.0, {0.0, 0.0}});
    const Vec2 point{0.37, -1.12};
    requireNear(
        field.evaluateModel({point.x + 2.0 * std::numbers::pi_v<double>, point.y}),
        field.evaluateModel(point),
        2.0e-14,
        "q=4 x period");
    requireNear(
        field.evaluateModel({point.x, point.y + 2.0 * std::numbers::pi_v<double>}),
        field.evaluateModel(point),
        2.0e-14,
        "q=4 y period");
}

void testEvenSymmetryAndAmplitudeBound() {
    const CanonicalQrpField field(CanonicalQrpParameters{4.8, 1.0, {0.0, 0.0}});
    const std::vector<Vec2> samples{{0.0, 0.0}, {0.37, -1.12}, {-2.4, 0.91}};
    for (const auto sample : samples) {
        const double value = field.evaluateModel(sample);
        requireNear(
            field.evaluateModel({-sample.x, -sample.y}),
            value,
            2.0e-14,
            "central inversion symmetry");
        require(
            std::abs(value) <= static_cast<double>(field.modeCount()) + 1.0e-14,
            "canonical QRP value must respect the sum-of-cosines bound");
    }
}

void testSamplingDomainMapping() {
    const CanonicalQrpField field(CanonicalQrpParameters{5.0, 18.0, {-2.0, 1.5}});
    const Vec2 mapped = field.modelCoordinate({0.25, 0.75});
    requireNear(
        mapped.x,
        -2.0 + 4.5 * std::numbers::pi_v<double>,
        1.0e-14,
        "sample-domain x");
    requireNear(
        mapped.y,
        1.5 + 13.5 * std::numbers::pi_v<double>,
        1.0e-14,
        "sample-domain y");
    requireNear(
        field.evaluateNormalized({0.25, 0.75}),
        field.evaluateModel(mapped),
        1.0e-15,
        "normalized evaluation");
}

void testCanonicalFieldIsNotUnitTorusPeriodic() {
    const CanonicalQrpField field(CanonicalQrpParameters{5.0, 1.0, {0.0, 0.0}});
    const double mismatch = std::abs(
        field.evaluateModel({0.0, 0.0}) - field.evaluateModel({1.0, 0.0}));
    require(
        mismatch > 1.0,
        "canonical q=5 QRP must not be silently treated as unit-torus periodic");

    const CanonicalQrpField paperExample(
        CanonicalQrpParameters{5.0, 18.0, {0.0, 0.0}});
    const double sampledWindowMismatch = std::abs(
        paperExample.evaluateNormalized({0.0, 0.0})
        - paperExample.evaluateNormalized({1.0, 0.0}));
    require(
        sampledWindowMismatch > 3.9,
        "the q=5, s=18 paper window must not be silently wrapped as a tile");
}

void testInvalidParameters() {
    bool rejected = false;
    try {
        [[maybe_unused]] const CanonicalQrpField invalid(
            CanonicalQrpParameters{0.5, 18.0, {0.0, 0.0}});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "q < 1 must be rejected");
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"origin and mode count", testOriginAndModeCount},
        {"four-fold closed form", testFourFoldClosedForm},
        {"four-fold closed-form gradient", testFourFoldClosedFormGradient},
        {"integer rotational symmetry", testIntegerRotationalSymmetry},
        {"crystallographic special-case periodicity", testCrystallographicSpecialCasePeriodicity},
        {"even symmetry and amplitude bound", testEvenSymmetryAndAmplitudeBound},
        {"sampling-domain mapping", testSamplingDomainMapping},
        {"non-periodic unit boundary", testCanonicalFieldIsNotUnitTorusPeriodic},
        {"invalid parameters", testInvalidParameters},
    };

    int failures = 0;
    for (const auto& test : tests) {
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
