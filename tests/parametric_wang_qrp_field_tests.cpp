#include "generators/CanonicalQrpField.hpp"
#include "model/ParametricWangQrpField.hpp"
#include "model/OrderedQrpField.hpp"
#include "model/QrpChannelComposition.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <numbers>
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
        {7, 2.1, {0.4, -0.25}, {0.75, 0.3}, 0.5, 0.4, 3.0, 0.37, 0.35, 0.43});
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
        {9, 3.2, {0.7, -0.45}, {1.1, 0.6}, 0.5, 0.4, 8.0, 0.7, 0.5, 0.43});
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
    const std::array<ParametricWangQrpParameters, 8> invalid{{
        {4, 1.0, {}, {}, 0.5, 0.4},
        {5, 0.0, {}, {}, 0.5, 0.4},
        {5, nan, {}, {}, 0.5, 0.4},
        {5, 1.0, {nan, 0.0}, {}, 0.5, 0.4},
        {5, 1.0, {}, {0.0, nan}, 0.5, 0.4},
        {5, 1.0, {}, {}, 0.5, 0.0},
        {5, 1.0, {}, {}, 0.5, 0.4, -1.0, 0.0},
        {5, 1.0, {}, {}, 0.5, 0.4, 1.0, nan},
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

void testDirectionAndSeam() {
    constexpr double angle = 0.37;
    const ParametricWangQrpField original({7, 3.15, {3.0, 0.85}, {}, 0.5, 0.4, 3.0, 0.0, 0.5});
    const ParametricWangQrpField rotated({7, 3.15, {3.0, 0.85}, {}, 0.5, 0.4, 3.0, angle, 0.5});
    constexpr Vec2 point{2.31, 1.47};
    const Vec2 inverseRotated{
        std::cos(angle) * point.x + std::sin(angle) * point.y,
        -std::sin(angle) * point.x + std::cos(angle) * point.y};
    requireNear(rotated.evaluate(point, {0.5, {}}).value,
        original.evaluate(inverseRotated, {0.5, {}}).value, 2.0e-14, "carrier rotation");

    const qrp::model::WangGrid grid(2, 1, 5, 123);
    const qrp::model::WangContentWeight weight({5, 0.5, 0.1, 0.4});
    const ParametricWangQrpField coupled({7, 3.15, {3.0, 0.85}, {1.15, -0.8}, 0.5, 0.4, 3.0, angle, 0.5, 0.43});
    for (const double y : {0.0, 0.37, 1.0}) {
        const auto left = coupled.evaluate({1.0, y}, weight.evaluate(grid.tile(0, 0), {1.0, y}));
        const auto right = coupled.evaluate({1.0, y}, weight.evaluate(grid.tile(1, 0), {0.0, y}));
        requireNear(left.value, right.value, 1.0e-13, "directional shared-edge value");
        requireNear(left.gradient.x, right.gradient.x, 1.0e-13, "directional shared-edge dx");
        requireNear(left.gradient.y, right.gradient.y, 1.0e-13, "directional shared-edge dy");
    }
}

void testCommonPhase() {
    constexpr double phase = 0.43;
    const ParametricWangQrpField field({5, 3.15, {}, {}, 0.5, 0.4, 0, 0, 0, phase});
    const ParametricWangQrpField opposite({5, 3.15, {}, {}, 0.5, 0.4, 0, 0, 0,
        phase + std::numbers::pi_v<double>});
    requireNear(field.evaluate({}, {0.5, {}}).value, std::cos(phase), 1.0e-14, "common phase at origin");
    constexpr Vec2 point{2.31, 1.47};
    const auto a = field.evaluate(point, {0.5, {}});
    const auto b = opposite.evaluate(point, {0.5, {}});
    requireNear(a.value, -b.value, 2.0e-14, "phase pi reverses value");
    requireNear(a.gradient.x, -b.gradient.x, 2.0e-14, "phase pi reverses dx");
    requireNear(a.gradient.y, -b.gradient.y, 2.0e-14, "phase pi reverses dy");
}

void testPhaseBasis() {
    // 旧公式并非实现错误：反向波同相叠加时确实可完全消失。保留此反例防止混淆参数含义。
    ParametricWangQrpParameters p;
    p.resonanceCount = 8;
    p.globalPhase = {std::numbers::pi_v<double>/2, std::numbers::pi_v<double>/2};
    const ParametricWangQrpField cancelled(p);
    for (const Vec2 point : {Vec2{}, Vec2{0.37,-0.82}, Vec2{5.1,3.7}}) {
        requireNear(cancelled.evaluate(point, {0.5,{}}).value, 0, 3.0e-14, "even second-harmonic cancellation");
    }
    p.phaseHarmonicOrder = 3;
    require(std::abs(ParametricWangQrpField(p).evaluate({}, {0.5,{}}).value) > 0.05,
        "third-harmonic phase must not reproduce the cancelled field");

    for (const auto q : {5U,8U,12U}) {
        p = {};
        p.resonanceCount = q;
        p.directionalBias = 1.2;
        p.crossMix = 0.35;
        const ParametricWangQrpField old(p);
        p.phaseHarmonicOrder = 3;
        const ParametricWangQrpField candidate(p);
        constexpr Vec2 point{0.37,-0.82};
        const auto a = old.evaluate(point, {0.5,{}}), b = candidate.evaluate(point, {0.5,{}});
        requireNear(a.value, b.value, 0, "zero-phase value unchanged");
        requireNear(a.gradient.x, b.gradient.x, 0, "zero-phase dx unchanged");
        requireNear(a.gradient.y, b.gradient.y, 0, "zero-phase dy unchanged");
        for (std::size_t i = 0; i < q; ++i) {
            requireNear(candidate.modes()[i].amplitude, old.modes()[i].amplitude, 0, "phase basis must not alter weights");
            if (q % 2 == 0) {
                const auto h = candidate.modes()[i].phaseHarmonic;
                const auto opposite = candidate.modes()[(i+q/2)%q].phaseHarmonic;
                requireNear(h.x, -opposite.x, 5.0e-14, "antipodal phase x");
                requireNear(h.y, -opposite.y, 5.0e-14, "antipodal phase y");
            }
        }
        p.globalPhase = {0.6,-0.3};
        p.wangPhase = {0.2,0.15};
        const ParametricWangQrpField phased(p);
        constexpr double step = 1.0e-6;
        const auto value = [&](Vec2 xy) { return phased.evaluate(xy, weightAt(xy)).value; };
        const auto actual = phased.evaluate(point, weightAt(point));
        requireNear(actual.gradient.x, (value({point.x+step,point.y})-value({point.x-step,point.y}))/(2*step),
            3.0e-10, "third-harmonic analytic dx");
        requireNear(actual.gradient.y, (value({point.x,point.y+step})-value({point.x,point.y-step}))/(2*step),
            3.0e-10, "third-harmonic analytic dy");
    }
}

void testOrderedQrp() {
    using qrp::model::OrderedMotif;
    using qrp::model::OrderedQrpField;
    constexpr Vec2 point{0.37, 0.52};
    constexpr double step = 1.0e-6;
    for (const auto motif : {OrderedMotif::Spots, OrderedMotif::Ribbons}) {
        for (const double variation : {0.0, 1.0}) {
            const OrderedQrpField field({5, 2.2, {0.4, -0.2}, 0.37, variation, 0.45, motif});
            const auto evaluate = [&field](const Vec2 p) { return field.evaluate(p, weightAt(p)); };
            const auto actual = evaluate(point);
            const double dx = (evaluate({point.x + step, point.y}).value
                - evaluate({point.x - step, point.y}).value) / (2.0 * step);
            const double dy = (evaluate({point.x, point.y + step}).value
                - evaluate({point.x, point.y - step}).value) / (2.0 * step);
            requireNear(actual.gradient.x, dx, 4.0e-10, "ordered derivative x");
            requireNear(actual.gradient.y, dy, 4.0e-10, "ordered derivative y");
            if (variation == 0.0) {
                const double u = 2.2 * (std::cos(0.37) * point.x + std::sin(0.37) * point.y);
                const double v = 2.2 * 0.45 * (-std::sin(0.37) * point.x + std::cos(0.37) * point.y);
                const double expected = motif == OrderedMotif::Ribbons ? std::cos(u)
                    : 0.5 * (1.0 + std::cos(u)) * (1.0 + std::cos(v)) - 1.0;
                requireNear(actual.value, expected, 1.0e-14, "periodic carrier recovery");
            }
            for (int y = 0; y <= 10; ++y) {
                for (int x = 0; x <= 10; ++x) {
                    require(std::abs(evaluate({x * 0.31, y * 0.27}).value) <= 1.0 + 1.0e-14,
                        "ordered range");
                }
            }
        }
    }
    const qrp::model::WangGrid grid(2, 2, 5, 123);
    const qrp::model::WangContentWeight weight({5, 0.5, 0.1, 0.4});
    const OrderedQrpField field({7, 3.15, {3.0, 0.85}, 0.37, 1.0, 1.0, OrderedMotif::Spots});
    for (const double t : {0.0, 0.37, 1.0}) {
        for (const bool vertical : {true, false}) {
            const Vec2 position = vertical ? Vec2{1.0, t} : Vec2{t, 1.0};
            const auto a = field.evaluate(position, weight.evaluate(grid.tile(0, 0), position));
            const auto b = field.evaluate(position, vertical
                ? weight.evaluate(grid.tile(1, 0), {0.0, t})
                : weight.evaluate(grid.tile(0, 1), {t, 0.0}));
            requireNear(a.value, b.value, 1.0e-13, "ordered seam value");
            requireNear(a.gradient.x, b.gradient.x, 1.0e-13, "ordered seam dx");
            requireNear(a.gradient.y, b.gradient.y, 1.0e-13, "ordered seam dy");
        }
    }
}

void testChannelRelations() {
    using qrp::model::QrpChannelComposition;
    using qrp::model::QrpChannelRelation;
    const ParametricWangQrpField first({5, 2.7, {0.2, -0.1}, {0.1, -0.08}, 0.5, 0.4, 1.5, 0.15});
    const ParametricWangQrpField second({5, 4.2, {0.2, -0.1}, {0.15, -0.11}, 0.5, 0.4, 0.0, 1.25, 0.0, 0.6});
    constexpr Vec2 point{0.37, -0.52};
    constexpr double step = 1.0e-6;
    const double gateCenter = first.evaluate(point, weightAt(point)).value;
    const qrp::model::WangGrid grid(2, 2, 5, 123);
    const qrp::model::WangContentWeight weights({5, 0.5, 0.1, 0.4});
    for (const auto relation : {QrpChannelRelation::Direct, QrpChannelRelation::Product,
        QrpChannelRelation::JointEnergy, QrpChannelRelation::Nested}) {
        const QrpChannelComposition composition({relation, gateCenter, 0.4});
        const auto evaluate = [&](const Vec2 p, const WangContentWeightEvaluation w) {
            return composition.evaluate(first.evaluate(p, w), second.evaluate(p, w));
        };
        const auto sample = evaluate(point, weightAt(point));
        const auto value = [&](const Vec2 p) { return evaluate(p, weightAt(p)).value; };
        requireNear(sample.gradient.x, (value({point.x + step, point.y}) - value({point.x - step, point.y}))
            / (2 * step), 1.0e-8, "channel relation derivative x");
        requireNear(sample.gradient.y, (value({point.x, point.y + step}) - value({point.x, point.y - step}))
            / (2 * step), 1.0e-8, "channel relation derivative y");
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x) require(std::abs(value({x * 0.31, y * 0.27})) <= 1 + 1.0e-14,
                "channel relation range");
        }
        for (const bool vertical : {true, false}) {
            const Vec2 p = vertical ? Vec2{1, 0.37} : Vec2{0.37, 1};
            const auto a = evaluate(p, weights.evaluate(grid.tile(0, 0), p));
            const auto b = evaluate(p, vertical ? weights.evaluate(grid.tile(1, 0), {0, 0.37})
                : weights.evaluate(grid.tile(0, 1), {0.37, 0}));
            requireNear(a.value, b.value, 1.0e-13, "channel seam value");
            requireNear(a.gradient.x, b.gradient.x, 1.0e-13, "channel seam dx");
            requireNear(a.gradient.y, b.gradient.y, 1.0e-13, "channel seam dy");
        }
    }
    const QrpChannelComposition nested({QrpChannelRelation::Nested, 0, 0.4});
    const auto inside = nested.evaluate({-0.8, {1, 2}}, {-0.3, {2, 3}});
    const auto outside = nested.evaluate({0.8, {1, 2}}, {-0.3, {2, 3}});
    requireNear(inside.value, -0.3, 1.0e-14, "nested active region restores child");
    requireNear(inside.gradient.x, 2, 1.0e-14, "nested child gradient");
    requireNear(outside.value, 1, 1.0e-14, "nested inactive region is background");
    requireNear(outside.gradient.y, 0, 1.0e-14, "nested background gradient");
    requireNear(QrpChannelComposition({QrpChannelRelation::Product}).evaluate({0.3, {}}, {-0.4, {}}).value,
        -0.12, 1.0e-14, "product relation");
    requireNear(QrpChannelComposition({QrpChannelRelation::JointEnergy}).evaluate({0.3, {}}, {-0.4, {}}).value,
        0.75, 1.0e-14, "joint energy relation");
}

constexpr std::array<TestCase, 10> kTests{{
    {"canonical recovery", testCanonicalRecovery},
    {"analytic gradient", testAnalyticGradient},
    {"parameters change field", testParametersChangeField},
    {"certified range samples", testRange},
    {"invalid parameters", testInvalidParameters},
    {"direction and seam", testDirectionAndSeam},
    {"common phase", testCommonPhase},
    {"phase basis semantics", testPhaseBasis},
    {"ordered QRP candidate", testOrderedQrp},
    {"QRP channel relations", testChannelRelations},
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
