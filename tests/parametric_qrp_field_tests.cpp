#include "generators/CanonicalQrpField.hpp"
#include "model/ParametricQrpField.hpp"
#include "model/QrpChannelComposition.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
using namespace qrp::model;
using qrp::math::Vec2;

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void near(const double actual, const double expected, const double tolerance, const char* message) {
    require(std::isfinite(actual) && std::abs(actual-expected) <= tolerance, message);
}
template <typename Field>
void checkGradient(const Field& field, const Vec2 p, const double tolerance) {
    constexpr double h = 1e-6;
    const auto sample = field(p);
    near(sample.gradient.x, (field({p.x+h,p.y}).value-field({p.x-h,p.y}).value)/(2*h), tolerance, "analytic dx");
    near(sample.gradient.y, (field({p.x,p.y+h}).value-field({p.x,p.y-h}).value)/(2*h), tolerance, "analytic dy");
}

void canonicalRecovery() {
    constexpr unsigned q = 7;
    constexpr double frequency = 2.3;
    const ParametricQrpField field({.resonanceCount=q, .spatialFrequency=frequency});
    const qrp::generators::CanonicalQrpField canonical({double(q), 1.0, {}});
    for (const Vec2 p : {Vec2{}, Vec2{0.37,-0.82}, Vec2{-1.2,0.61}}) {
        const auto actual = field.evaluate(p);
        const auto expected = canonical.evaluateModelWithGradient({frequency*p.x,frequency*p.y});
        near(actual.value, expected.value/q, 2e-14, "canonical value");
        near(actual.gradient.x, frequency*expected.modelGradient.x/q, 3e-14, "canonical dx");
        near(actual.gradient.y, frequency*expected.modelGradient.y/q, 3e-14, "canonical dy");
    }
}

void parametersAndGradient() {
    ParametricQrpParameters p{.resonanceCount=7, .spatialFrequency=2.1,
        .globalPhase={0.4,-0.25}, .directionalBias=3, .orientationRadians=0.37,
        .crossMix=0.35, .commonPhase=0.43};
    const ParametricQrpField field(p);
    checkGradient([&](Vec2 xy) { return field.evaluate(xy); }, {0.37,-0.52}, 3e-10);
    for (int y=-12; y<=12; ++y) for (int x=-12; x<=12; ++x) {
        require(std::abs(field.evaluate({x/5.0,y/5.0}).value) <= 1+2e-15, "cosine average range");
    }
    // Rotation must act on wave directions, not on the phase basis.
    p.orientationRadians = 0;
    const ParametricQrpField original(p);
    constexpr Vec2 xy{2.31,1.47};
    const Vec2 inverse{std::cos(0.37)*xy.x+std::sin(0.37)*xy.y,
        -std::sin(0.37)*xy.x+std::cos(0.37)*xy.y};
    near(field.evaluate(xy).value, original.evaluate(inverse).value, 2e-14, "carrier rotation");
    p.commonPhase += std::numbers::pi;
    const auto a = original.evaluate(xy), b = ParametricQrpField(p).evaluate(xy);
    near(a.value, -b.value, 2e-14, "common phase value reversal");
    near(a.gradient.x, -b.gradient.x, 2e-14, "common phase gradient reversal");
}

void phaseBasis() {
    ParametricQrpParameters p;
    p.resonanceCount = 8;
    p.globalPhase = {std::numbers::pi/2,std::numbers::pi/2};
    const ParametricQrpField cancelled(p);
    for (const Vec2 xy : {Vec2{}, Vec2{0.37,-0.82}, Vec2{5.1,3.7}}) {
        near(cancelled.evaluate(xy).value, 0, 3e-14, "legacy second-harmonic cancellation");
    }
    p.phaseHarmonicOrder = 3;
    require(std::abs(ParametricQrpField(p).evaluate({}).value) > 0.05, "third-harmonic cancellation");
    for (const auto q : {5U,8U,12U}) {
        p = {};
        p.resonanceCount = q;
        p.directionalBias = 1.2;
        p.crossMix = 0.35;
        const ParametricQrpField old(p);
        p.phaseHarmonicOrder = 3;
        const ParametricQrpField candidate(p);
        constexpr Vec2 xy{0.37,-0.82};
        const auto a = old.evaluate(xy), b = candidate.evaluate(xy);
        near(a.value,b.value,0,"zero-phase value unchanged");
        near(a.gradient.x,b.gradient.x,0,"zero-phase dx unchanged");
        near(a.gradient.y,b.gradient.y,0,"zero-phase dy unchanged");
        for (std::size_t i=0; i<q; ++i) {
            near(candidate.modes()[i].amplitude,old.modes()[i].amplitude,0,"unchanged weights");
            if (q%2 == 0) {
                const auto h = candidate.modes()[i].phaseHarmonic;
                const auto opposite = candidate.modes()[(i+q/2)%q].phaseHarmonic;
                near(h.x,-opposite.x,5e-14,"antipodal phase x");
                near(h.y,-opposite.y,5e-14,"antipodal phase y");
            }
        }
        p.globalPhase = {0.6,-0.3};
        const ParametricQrpField phased(p);
        checkGradient([&](Vec2 point) { return phased.evaluate(point); }, xy, 3e-10);
    }
}

void channelRelations() {
    const ParametricQrpField first({.spatialFrequency=2.7, .globalPhase={0.2,-0.1},
        .directionalBias=1.5, .orientationRadians=0.15});
    const ParametricQrpField second({.spatialFrequency=4.2, .globalPhase={0.2,-0.1},
        .orientationRadians=1.25, .commonPhase=0.6});
    constexpr Vec2 xy{0.37,-0.52};
    for (const auto relation : {QrpChannelRelation::Direct,QrpChannelRelation::Product,
        QrpChannelRelation::JointEnergy,QrpChannelRelation::Nested}) {
        const QrpChannelComposition composition({relation,first.evaluate(xy).value,0.4});
        const auto evaluate = [&](Vec2 p) { return composition.evaluate(first.evaluate(p),second.evaluate(p)); };
        checkGradient(evaluate,xy,1e-8);
        for (int y=0; y<10; ++y) for (int x=0; x<10; ++x) {
            require(std::abs(evaluate({x*0.31,y*0.27}).value) <= 1+1e-14,"composition range");
        }
    }
    const QrpChannelComposition nested({QrpChannelRelation::Nested,0,0.4});
    const auto inside = nested.evaluate({-0.8,{1,2}},{-0.3,{2,3}});
    const auto outside = nested.evaluate({0.8,{1,2}},{-0.3,{2,3}});
    near(inside.value,-0.3,1e-14,"nested child value");
    near(inside.gradient.x,2,1e-14,"nested child dx");
    near(outside.value,1,1e-14,"nested background");
    near(outside.gradient.y,0,1e-14,"nested background dy");
    near(QrpChannelComposition({QrpChannelRelation::Product}).evaluate({0.3,{}},{-0.4,{}}).value,
        -0.12,1e-14,"product relation");
    near(QrpChannelComposition({QrpChannelRelation::JointEnergy}).evaluate({0.3,{}},{-0.4,{}}).value,
        0.75,1e-14,"joint energy relation");
}

void invalidParameters() {
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    const std::array<ParametricQrpParameters,7> invalid{{
        {.resonanceCount=4}, {.spatialFrequency=0}, {.spatialFrequency=nan},
        {.globalPhase={nan,0}}, {.directionalBias=-1}, {.orientationRadians=nan}, {.phaseHarmonicOrder=1}
    }};
    for (const auto p : invalid) {
        bool rejected = false;
        try { [[maybe_unused]] const ParametricQrpField field(p); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected,"invalid parameter accepted");
    }
}
} // namespace

int main() {
    try {
        canonicalRecovery();
        parametersAndGradient();
        phaseBasis();
        channelRelations();
        invalidParameters();
        std::cout << "QRP: canonical recovery, gradients, rotation, phase basis and composition passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
