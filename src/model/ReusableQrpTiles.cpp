#include "model/ReusableQrpTiles.hpp"
#include "model/EndpointWangTiles.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace qrp::model {
namespace {

struct Weight { double value; double derivative; };

[[nodiscard]] Weight bump(const double t, const double width) noexcept {
    if (t >= width) return {};
    const double z = t / width;
    return {1.0 - 3.0 * z * z + 2.0 * z * z * z, (-6.0 * z + 6.0 * z * z) / width};
}

[[nodiscard]] std::array<Weight, 3> weights(const double t, const double width) noexcept {
    const auto a = bump(t, width);
    const auto b = bump(1.0 - t, width);
    return {a, Weight{1.0 - a.value - b.value, -a.derivative + b.derivative}, Weight{b.value, -b.derivative}};
}


[[nodiscard]] math::Vec2 mean(const math::Vec2 a, const math::Vec2 b) noexcept {
    return {0.5 * (a.x + b.x), 0.5 * (a.y + b.y)};
}

struct FitSample { math::Vec2 local; ScalarFieldEvaluation target; };

} // namespace

ReusableQrpTiles::ReusableQrpTiles(const ReusableQrpTileParameters parameters)
    : parameters_(parameters), field_(parameters.qrp) {
    if (!std::isfinite(parameters.sourceSpan) || parameters.sourceSpan <= 0.0
        || !std::isfinite(parameters.transitionWidth) || parameters.transitionWidth <= 0.0
        || parameters.transitionWidth >= 0.5) {
        throw std::invalid_argument("Reusable QRP tiles require positive span and transition width in (0,0.5).");
    }
    if (parameters.qrp.wangPhase.x != 0.0 || parameters.qrp.wangPhase.y != 0.0) {
        throw std::invalid_argument("Reusable QRP sources do not use the old global Wang phase modulation.");
    }
    if (parameters.selection != QrpSourceSelection::Fixed && parameters.selection != QrpSourceSelection::BoundaryMatched) {
        throw std::invalid_argument("Unknown QRP source selection policy.");
    }
    // 有限源目录由瓦片类型定义，不由实例的全局位置或随机相位定义。
    sources_.vertices = {{{0.2, 0.3}, {0.9, 0.4}}};
    const double span = parameters.sourceSpan;
    for (std::uint32_t c = 0; c < 4; ++c) {
        const auto a = sources_.vertices[c / 2];
        const auto b = sources_.vertices[c % 2];
        sources_.verticalEdges[c] = mean(a, {b.x, b.y - span});
        sources_.horizontalEdges[c] = mean(a, {b.x - span, b.y});
    }
    for (std::uint32_t id = 0; id < 16; ++id) {
        math::Vec2 offset;
        for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
            const auto vertex = sources_.vertices[EndpointWangTiles::corner(id, x, y)];
            offset.x += 0.25 * (vertex.x - span * x);
            offset.y += 0.25 * (vertex.y - span * y);
        }
        sources_.interiors[id] = offset;
    }
    selectSources();
}


ScalarFieldEvaluation ReusableQrpTiles::source(const math::Vec2 local, const math::Vec2 offset) const noexcept {
    const double span = parameters_.sourceSpan;
    auto sample = field_.evaluate({span * local.x + offset.x, span * local.y + offset.y},
        {parameters_.qrp.weightCenter, {}});
    sample.gradient.x *= span;
    sample.gradient.y *= span;
    return sample;
}

ScalarFieldEvaluation ReusableQrpTiles::evaluate(const std::uint32_t id, const math::Vec2 local) const noexcept {
    // 热路径契约：id 属于 [0,15]，local 属于单位正方形。
    const auto wx = weights(local.x, parameters_.transitionWidth);
    const auto wy = weights(local.y, parameters_.transitionWidth);
    ScalarFieldEvaluation result;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
        if (wx[i].value == 0.0 && wx[i].derivative == 0.0) continue;
        if (wy[j].value == 0.0 && wy[j].derivative == 0.0) continue;
        math::Vec2 p = local;
        math::Vec2 offset;
        if (i == 1 && j == 1) {
            offset = sources_.interiors[id];
        } else if (i != 1 && j != 1) {
            const int x = i == 2 ? 1 : 0, y = j == 2 ? 1 : 0;
            offset = sources_.vertices[EndpointWangTiles::corner(id, x, y)];
            p.x -= x;
            p.y -= y;
        } else if (i != 1) {
            const int x = i == 2 ? 1 : 0;
            offset = sources_.verticalEdges[2 * EndpointWangTiles::corner(id, x, 0) + EndpointWangTiles::corner(id, x, 1)];
            p.x -= x;
        } else {
            const int y = j == 2 ? 1 : 0;
            offset = sources_.horizontalEdges[2 * EndpointWangTiles::corner(id, 0, y) + EndpointWangTiles::corner(id, 1, y)];
            p.y -= y;
        }
        const auto sample = source(p, offset);
        const double w = wx[i].value * wy[j].value;
        result.value += w * sample.value;
        result.gradient.x += wx[i].derivative * wy[j].value * sample.value + w * sample.gradient.x;
        result.gradient.y += wx[i].value * wy[j].derivative * sample.value + w * sample.gradient.y;
    }
    return result;
}

void ReusableQrpTiles::selectSources() {
    const double width = parameters_.transitionWidth;
    const double frequency = parameters_.sourceSpan * parameters_.qrp.spatialFrequency;
    // 同一目标函数只用来比较边界邻域的内容差异，不作为美感评分。
    const double gradientWeight = 0.1 / (frequency * frequency);
    const auto choose = [&](const std::vector<FitSample>& samples, math::Vec2& offset, double& before, double& after) {
        const auto score = [&](const math::Vec2 candidate) {
            double error = 0.0;
            for (const auto& sample : samples) {
                const auto actual = source(sample.local, candidate);
                const double dv = actual.value - sample.target.value;
                const double dx = actual.gradient.x - sample.target.gradient.x;
                const double dy = actual.gradient.y - sample.target.gradient.y;
                error += dv * dv + gradientWeight * (dx * dx + dy * dy);
            }
            return error / static_cast<double>(samples.size());
        };
        double best = score(offset);
        before += best;
        if (parameters_.selection == QrpSourceSelection::BoundaryMatched) {
            // 固定源也在候选中；有限目录搜索不会增加此局部匹配目标。
            for (int y = -6; y <= 6; ++y) for (int x = -6; x <= 6; ++x) {
                const math::Vec2 candidate{static_cast<double>(x), static_cast<double>(y)};
                const double error = score(candidate);
                if (error < best) { best = error; offset = candidate; }
            }
        }
        after += best;
    };
    for (const bool vertical : {true, false}) for (std::uint32_t c = 0; c < 4; ++c) {
        std::vector<FitSample> samples;
        for (int end = 0; end < 2; ++end) for (const double distance : {0.4 * width, 0.8 * width}) {
            const double t = end == 0 ? distance : 1.0 - distance;
            for (const double n : {-0.4 * width, 0.0, 0.4 * width}) {
                const math::Vec2 p = vertical ? math::Vec2{n, t} : math::Vec2{t, n};
                const math::Vec2 vertexLocal = vertical ? math::Vec2{n, t - end} : math::Vec2{t - end, n};
                samples.push_back({p, source(vertexLocal, sources_.vertices[end == 0 ? c / 2 : c % 2])});
            }
        }
        auto& offset = vertical ? sources_.verticalEdges[c] : sources_.horizontalEdges[c];
        choose(samples, offset, fit_.edgeFixed, fit_.edgeSelected);
    }
    for (std::uint32_t id = 0; id < 16; ++id) {
        const auto edges = EndpointWangTiles::labels(id);
        std::vector<FitSample> samples;
        for (const double t : {width, 0.35, 0.5, 0.65, 1.0 - width}) {
            const double n = 0.5 * width;
            samples.push_back({{n, t}, source({n, t}, sources_.verticalEdges[edges.west])});
            samples.push_back({{1.0 - n, t}, source({-n, t}, sources_.verticalEdges[edges.east])});
            samples.push_back({{t, n}, source({t, n}, sources_.horizontalEdges[edges.south])});
            samples.push_back({{t, 1.0 - n}, source({t, -n}, sources_.horizontalEdges[edges.north])});
        }
        choose(samples, sources_.interiors[id], fit_.interiorFixed, fit_.interiorSelected);
    }
}

} // namespace qrp::model
