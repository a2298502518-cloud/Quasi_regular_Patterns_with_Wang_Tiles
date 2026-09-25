#pragma once

#include "math/Vec2.hpp"
#include "model/ScalarFieldEvaluation.hpp"
#include "model/WangContentWeight.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace qrp::model {

struct ParametricWangQrpParameters {
    std::uint32_t resonanceCount = 5;
    double spatialFrequency = 1.0;
    math::Vec2 globalPhase;
    math::Vec2 wangPhase;
    double weightCenter = 0.5;
    double weightRadius = 0.4;
    double directionalBias = 0.0;
    // 旋转 QRP 载波方向；0 时偏置方向为 x，条带走向为 y。Wang 网格不旋转。
    double orientationRadians = 0.0;
    double crossMix = 0.0;
    // 所有有向模态共同增加的相位；偶数 q 的反向波配对时不一定是独立形态控制。
    double commonPhase = 0.0;
    // 2 保留历史公式；3 用于 q=8/12 的反向波共轭相位实验。不是所有 q 都有两个独立控制。
    std::uint32_t phaseHarmonicOrder = 2;

    [[nodiscard]] bool operator==(
        const ParametricWangQrpParameters&) const noexcept = default;
};

// QRP 的只读谱定义；相位兼容候选复用方向和振幅，不另写一套权重公式。
struct QrpMode {
    math::Vec2 direction;
    math::Vec2 weightHarmonic;
    math::Vec2 phaseHarmonic;
    double amplitude = 0.0;
};

class ParametricWangQrpField final {
public:
    explicit ParametricWangQrpField(ParametricWangQrpParameters parameters);

    [[nodiscard]] const ParametricWangQrpParameters& parameters() const noexcept;
    [[nodiscard]] std::span<const QrpMode> modes() const noexcept { return modes_; }

    // position and weight.localGradient must use the same coordinate basis.
    // Non-negative normalized mode weights give the range certificate [-1,1].
    [[nodiscard]] ScalarFieldEvaluation evaluate(
        math::Vec2 position,
        WangContentWeightEvaluation weight) const noexcept;

private:
    ParametricWangQrpParameters parameters_;
    std::vector<QrpMode> modes_;
};

} // namespace qrp::model
