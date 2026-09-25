#include "model/PhaseCompatibleQrpTiles.hpp"
#include "model/EndpointWangTiles.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace qrp::model {

PhaseCompatibleQrpTiles::PhaseCompatibleQrpTiles(const PhaseCompatibleQrpTileParameters parameters)
    : parameters_(parameters), source_(parameters.qrp) {
    if (!std::isfinite(parameters.sourceSpan) || parameters.sourceSpan <= 0.0) {
        throw std::invalid_argument("Phase-compatible tiles require positive span.");
    }
    for (std::size_t i = 0; i < parameters.vertexOffsets.size(); ++i) {
        const auto offset = parameters.vertexOffsets[i], phase = parameters.vertexPhaseOffsets[i];
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y)
            || !std::isfinite(phase.x) || !std::isfinite(phase.y)) {
            throw std::invalid_argument("Finite vertex source and phase offsets required.");
        }
    }
    const double frequency = parameters.qrp.spatialFrequency;
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    const auto a = parameters.vertexOffsets[0], b = parameters.vertexOffsets[1];
    const auto phaseA = parameters.vertexPhaseOffsets[0], phaseB = parameters.vertexPhaseOffsets[1];
    for (const auto& mode : source_.modes()) {
        const math::Vec2 wave{frequency * parameters.sourceSpan * mode.direction.x,
            frequency * parameters.sourceSpan * mode.direction.y};
        const math::Vec2 winding{std::round(wave.x / tau), std::round(wave.y / tau)};
        modes_.push_back({wave, winding, {tau*winding.x-wave.x, tau*winding.y-wave.y},
            mode.amplitude, frequency*(mode.direction.x*a.x+mode.direction.y*a.y)
                + parameters.qrp.globalPhase.x*mode.phaseHarmonic.x
                + parameters.qrp.globalPhase.y*mode.phaseHarmonic.y + parameters.qrp.commonPhase
                + phaseA.x*mode.phaseHarmonic.x + phaseA.y*mode.phaseHarmonic.y,
            frequency*(mode.direction.x*(b.x-a.x)+mode.direction.y*(b.y-a.y))
                + (phaseB.x-phaseA.x)*mode.phaseHarmonic.x + (phaseB.y-phaseA.y)*mode.phaseHarmonic.y});
    }
    for (std::uint32_t id = 0; id < corners_.size(); ++id) {
        const auto e = EndpointWangTiles::labels(id);
        corners_[id] = {static_cast<double>(e.south/2), static_cast<double>(e.south%2),
            static_cast<double>(e.north/2), static_cast<double>(e.north%2)};
    }
}

ScalarFieldEvaluation PhaseCompatibleQrpTiles::evaluate(const std::uint32_t id, const math::Vec2 local) const noexcept {
    // 热路径契约与固定库相同：id∈[0,15]，local∈[0,1]²。
    const auto h = [](double t) { return t*t*(3.0-2.0*t); };
    const auto dh = [](double t) { return 6.0*t*(1.0-t); };
    const double x = h(local.x), y = h(local.y), dx = dh(local.x), dy = dh(local.y);
    const auto& a = corners_[id];
    const double south = std::lerp(a[0],a[1],x), north = std::lerp(a[2],a[3],x);
    const double state = std::lerp(south,north,y);
    const math::Vec2 stateGradient{dx*std::lerp(a[1]-a[0],a[3]-a[2],y),dy*(north-south)};
    ScalarFieldEvaluation result;
    for (const auto& mode : modes_) {
        const double phase = mode.wave.x*local.x + mode.wave.y*local.y
            + mode.closure.x*x + mode.closure.y*y + mode.phase + mode.vertexShift*state;
        const double derivative = -mode.amplitude*std::sin(phase);
        result.value += mode.amplitude*std::cos(phase);
        result.gradient.x += derivative*(mode.wave.x+mode.closure.x*dx+mode.vertexShift*stateGradient.x);
        result.gradient.y += derivative*(mode.wave.y+mode.closure.y*dy+mode.vertexShift*stateGradient.y);
    }
    return result;
}

ScalarFieldEvaluation PhaseCompatibleQrpTiles::source(const math::Vec2 local, const math::Vec2 offset) const noexcept {
    const double span = parameters_.sourceSpan;
    auto sample = source_.evaluate({span*local.x+offset.x,span*local.y+offset.y});
    sample.gradient.x *= span;
    sample.gradient.y *= span;
    return sample;
}

QrpBandCertificate PhaseCompatibleQrpTiles::bandCertificate() const noexcept {
    const auto& mode = *std::max_element(modes_.begin(),modes_.end(),
        [](const auto& a, const auto& b) { return a.amplitude < b.amplitude; });
    const double length = std::hypot(mode.wave.x,mode.wave.y);
    const math::Vec2 axis{mode.wave.x/length,mode.wave.y/length};
    // h′∈[0,1.5]，二值角点的平滑插值在每个方向的梯度绝对值≤1.5。
    const double slope = length - 1.5*(std::abs(axis.x*mode.closure.x)+std::abs(axis.y*mode.closure.y))
        - 1.5*std::abs(mode.vertexShift)*(std::abs(axis.x)+std::abs(axis.y));
    const double remainder = 1.0-mode.amplitude;
    return {mode.amplitude,remainder,axis,slope,mode.amplitude > remainder && slope > 0.0};
}

} // namespace qrp::model
