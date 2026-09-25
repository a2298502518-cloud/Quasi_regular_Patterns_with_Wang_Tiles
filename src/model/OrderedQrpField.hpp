#pragma once

#include "model/ParametricWangQrpField.hpp"

namespace qrp::model {

enum class OrderedMotif { Ribbons, Spots };

struct OrderedQrpParameters {
    std::uint32_t resonanceCount = 7;
    double spatialFrequency = 3.15;
    math::Vec2 globalPhase{3.0, 0.85};
    double orientationRadians = 0.0;
    double variation = 0.8;
    double aspect = 1.0;
    OrderedMotif motif = OrderedMotif::Spots;
};

// 研究候选：规则载波负责造型，低频 Wang-QRP 负责有限的坐标形变。
// 不是 canonical QRP 的等价改写，也不替代旧的直接干涉模型。
class OrderedQrpField final {
public:
    explicit OrderedQrpField(OrderedQrpParameters parameters);

    [[nodiscard]] const OrderedQrpParameters& parameters() const noexcept;
    [[nodiscard]] ScalarFieldEvaluation evaluate(
        math::Vec2 position, WangContentWeightEvaluation weight) const noexcept;

private:
    OrderedQrpParameters parameters_;
    ParametricWangQrpField displacementX_;
    ParametricWangQrpField displacementY_;
    math::Vec2 axisX_;
    math::Vec2 axisY_;
};

} // namespace qrp::model
