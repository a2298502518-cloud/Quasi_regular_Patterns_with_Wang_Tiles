#include "model/ReusableQrpTiles.hpp"
#include "model/EndpointWangTiles.hpp"
#include "model/PhaseCompatibleQrpTiles.hpp"
#include "model/QrpChannelComposition.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace qrp::model;
using qrp::math::Vec2;

void near(const double a, const double b, const double tolerance, const char* label) {
    if (!std::isfinite(a) || !std::isfinite(b) || std::abs(a - b) > tolerance) throw std::runtime_error(label);
}

void same(const ScalarFieldEvaluation a, const ScalarFieldEvaluation b, const double tolerance) {
    near(a.value, b.value, tolerance, "value mismatch");
    near(a.gradient.x, b.gradient.x, tolerance, "dx mismatch");
    near(a.gradient.y, b.gradient.y, tolerance, "dy mismatch");
}

void check(const QrpSourceSelection selection) {
    ReusableQrpTileParameters p;
    p.qrp.spatialFrequency = 3.15;
    p.qrp.directionalBias = 0.7;
    p.qrp.orientationRadians = 0.19;
    p.qrp.commonPhase = 0.2;
    p.selection = selection;
    const ReusableQrpTiles tiles(p);
    std::size_t vertical = 0, horizontal = 0;
    // 检查全部 16×16 类型对中的合法邻接，不借用同一个全局坐标。
    for (std::uint32_t a = 0; a < 16; ++a) for (std::uint32_t b = 0; b < 16; ++b) {
        const auto left = EndpointWangTiles::labels(a), right = EndpointWangTiles::labels(b);
        for (const double t : {0.0, 0.03, 0.13, 0.5, 0.87, 0.97, 1.0}) {
            if (left.east == right.west) same(tiles.evaluate(a, {1,t}), tiles.evaluate(b, {0,t}), 2.0e-12);
            if (left.north == right.south) same(tiles.evaluate(a, {t,1}), tiles.evaluate(b, {t,0}), 2.0e-12);
        }
        vertical += left.east == right.west;
        horizontal += left.north == right.south;
    }
    if (vertical != 64 || horizontal != 64) throw std::runtime_error("restricted tile-set adjacency count");
    for (std::uint32_t id = 0; id < 16; ++id) {
        const Vec2 interior{0.37,0.52};
        same(tiles.evaluate(id, interior), tiles.source(interior, tiles.sources().interiors[id]), 1.0e-14);
        for (int y = 0; y <= 8; ++y) for (int x = 0; x <= 8; ++x) {
            const double value = tiles.evaluate(id, {x/8.0,y/8.0}).value;
            if (!std::isfinite(value) || std::abs(value) > 1.0 + 1.0e-14) throw std::runtime_error("convex range");
        }
    }
    constexpr double h = 1.0e-6;
    // 位于两条过渡带的交叠处，覆盖权重导数，不能只测内部。
    const Vec2 q{0.08,0.91};
    const auto sample = tiles.evaluate(6,q);
    near(sample.gradient.x, (tiles.evaluate(6,{q.x+h,q.y}).value - tiles.evaluate(6,{q.x-h,q.y}).value)/(2*h), 1.0e-7, "transition dx");
    near(sample.gradient.y, (tiles.evaluate(6,{q.x,q.y+h}).value - tiles.evaluate(6,{q.x,q.y-h}).value)/(2*h), 1.0e-7, "transition dy");
    if (tiles.fit().edgeSelected > tiles.fit().edgeFixed + 1.0e-12
        || tiles.fit().interiorSelected > tiles.fit().interiorFixed + 1.0e-12) throw std::runtime_error("source fit regressed");
}

void checkPhaseCompatible() {
    PhaseCompatibleQrpTileParameters p;
    p.qrp.spatialFrequency = 3.15;
    p.qrp.directionalBias = 0.7;
    p.qrp.orientationRadians = 0.19;
    p.qrp.commonPhase = 0.2;
    const PhaseCompatibleQrpTiles tiles(p);
    const ParametricWangQrpField reference(p.qrp);
    for (std::size_t i = 0; i < tiles.modes().size(); ++i) {
        near(tiles.modes()[i].amplitude,reference.modes()[i].amplitude,0,"mode amplitude changed");
    }
    for (std::uint32_t a = 0; a < 16; ++a) for (std::uint32_t b = 0; b < 16; ++b) {
        const auto left = EndpointWangTiles::labels(a), right = EndpointWangTiles::labels(b);
        for (const double t : {0.0,0.03,0.13,0.5,0.87,0.97,1.0}) {
            if (left.east == right.west) same(tiles.evaluate(a,{1,t}),tiles.evaluate(b,{0,t}),2.0e-12);
            if (left.north == right.south) same(tiles.evaluate(a,{t,1}),tiles.evaluate(b,{t,0}),2.0e-12);
        }
    }
    for (std::uint32_t id = 0; id < 16; ++id) {
        for (int y = 0; y <= 8; ++y) for (int x = 0; x <= 8; ++x) {
            const double value = tiles.evaluate(id,{x/8.0,y/8.0}).value;
            if (!std::isfinite(value) || std::abs(value) > 1.0+1.0e-14) throw std::runtime_error("phase range");
        }
    }
    constexpr double h = 1.0e-6;
    const Vec2 q{0.28,0.71};
    const auto sample = tiles.evaluate(6,q);
    near(sample.gradient.x,(tiles.evaluate(6,{q.x+h,q.y}).value-tiles.evaluate(6,{q.x-h,q.y}).value)/(2*h),1.0e-7,"phase dx");
    near(sample.gradient.y,(tiles.evaluate(6,{q.x,q.y+h}).value-tiles.evaluate(6,{q.x,q.y-h}).value)/(2*h),1.0e-7,"phase dy");
    p.qrp.orientationRadians = 0.0;
    p.qrp.directionalBias = 1.5;
    const auto certificate = PhaseCompatibleQrpTiles(p).bandCertificate();
    if (!certificate.signedCoreBands || certificate.minimumPhaseSlope < 9.0) throw std::runtime_error("directional band certificate");
    p.qrp.directionalBias = 0.0;
    if (PhaseCompatibleQrpTiles(p).bandCertificate().signedCoreBands) throw std::runtime_error("unjustified isotropic band certificate");
}

void checkNestedCompatible(const bool relativePhase, const std::uint32_t resonanceCount = 5, const std::uint32_t phaseOrder = 2) {
    PhaseCompatibleQrpTileParameters p;
    p.qrp.resonanceCount = resonanceCount;
    p.qrp.phaseHarmonicOrder = phaseOrder;
    if (phaseOrder == 3) p.qrp.globalPhase = {0.3,-0.2};
    p.sourceSpan = 16.0;
    p.qrp.spatialFrequency = 1.15;
    if (relativePhase) p.vertexPhaseOffsets = {{{-1.0,-0.5},{1.0,0.5}}};
    const PhaseCompatibleQrpTiles parent(p);
    // 全 0 / 全 1 类型角点应恢复各自编码的 QRP 状态，含完整梯度。
    for (std::uint32_t state = 0; state < 2; ++state) {
        auto expectedParameters = p.qrp;
        expectedParameters.globalPhase.x += p.vertexPhaseOffsets[state].x;
        expectedParameters.globalPhase.y += p.vertexPhaseOffsets[state].y;
        auto expected = ParametricWangQrpField(expectedParameters).evaluate(
            p.vertexOffsets[state], {p.qrp.weightCenter,{}});
        expected.gradient.x *= p.sourceSpan;
        expected.gradient.y *= p.sourceSpan;
        same(parent.evaluate(15*state,{double(state),double(state)}),expected,1.0e-10);
    }
    p.qrp.spatialFrequency = 6.9;
    p.qrp.directionalBias = 1.5;
    p.qrp.orientationRadians = 0.3;
    p.vertexPhaseOffsets = {};
    const PhaseCompatibleQrpTiles child(p);
    const QrpChannelComposition nested({QrpChannelRelation::Nested});
    const auto field = [&](const std::uint32_t id, const Vec2 q) {
        return nested.evaluate(parent.evaluate(id,q),child.evaluate(id,q));
    };
    for (std::uint32_t a = 0; a < 16; ++a) for (std::uint32_t b = 0; b < 16; ++b) {
        const auto left = EndpointWangTiles::labels(a), right = EndpointWangTiles::labels(b);
        for (const double t : {0.0,0.21,0.5,0.79,1.0}) {
            if (left.east == right.west) same(field(a,{1,t}),field(b,{0,t}),2.0e-10);
            if (left.north == right.south) same(field(a,{t,1}),field(b,{t,0}),2.0e-10);
        }
    }
    for (std::uint32_t id = 0; id < 16; ++id) {
        for (int y = 0; y <= 8; ++y) for (int x = 0; x <= 8; ++x) {
            const Vec2 q{x/8.0,y/8.0};
            const double value = field(id,q).value;
            if (!std::isfinite(value) || std::abs(value) > 1.0+1.0e-14) throw std::runtime_error("nested tile range");
            if (parent.evaluate(id,q).value >= 0.0 && value < -1.0e-14) throw std::runtime_error("nested ink escaped parent region");
        }
    }
    const Vec2 q{0.31,0.57};
    // 将门控中心放在实际父场值，确保检查链式项而非平坦背景。
    const QrpChannelComposition transition({QrpChannelRelation::Nested,parent.evaluate(6,q).value,0.35});
    const auto sample = [&](const Vec2 position) {
        return transition.evaluate(parent.evaluate(6,position),child.evaluate(6,position));
    };
    constexpr double h = 1.0e-7;
    near(sample(q).gradient.x,(sample({q.x+h,q.y}).value-sample({q.x-h,q.y}).value)/(2*h),2.0e-6,"nested phase dx");
    near(sample(q).gradient.y,(sample({q.x,q.y+h}).value-sample({q.x,q.y-h}).value)/(2*h),2.0e-6,"nested phase dy");
}
} // namespace

int main() {
    try {
        check(QrpSourceSelection::Fixed);
        check(QrpSourceSelection::BoundaryMatched);
        checkPhaseCompatible();
        checkNestedCompatible(false);
        checkNestedCompatible(true);
        checkNestedCompatible(true,8,3);
        checkNestedCompatible(true,12,3);
        std::cout << "Reusable tiles: legal edges, full gradients, interior recovery, range and source fit passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
