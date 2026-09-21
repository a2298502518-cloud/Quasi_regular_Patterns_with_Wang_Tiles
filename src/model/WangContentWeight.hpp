#pragma once

#include "math/Vec2.hpp"
#include "model/WangGrid.hpp"

#include <cstdint>

namespace qrp::model {

struct WangContentWeightParameters {
    std::uint32_t labelCount = 3;
    double neutralWeight = 0.5;
    double edgeValueAmplitude = 0.0;
    double transverseDerivativeAmplitude = 0.0;

    [[nodiscard]] bool operator==(const WangContentWeightParameters&) const noexcept = default;
};

struct WangContentWeightEvaluation {
    double value = 0.0;
    // 导数相对于单位 tile 局部坐标 (u,v)。
    math::Vec2 localGradient;
};

class WangContentWeight final {
public:
    explicit WangContentWeight(WangContentWeightParameters parameters);

    [[nodiscard]] const WangContentWeightParameters& parameters() const noexcept;
    [[nodiscard]] double maximumDeviationUpperBound() const noexcept;
    [[nodiscard]] double certifiedMinimum() const noexcept;
    [[nodiscard]] double certifiedMaximum() const noexcept;

    // 热路径假设 local 位于 [0,1]^2，且四条边标签均小于 labelCount；
    // 调用方在接受网格和参数时统一验证，不在逐像素求值中重复检查。
    [[nodiscard]] WangContentWeightEvaluation evaluate(
        const WangTile& tile,
        math::Vec2 local) const noexcept;

private:
    WangContentWeightParameters parameters_;
};

} // namespace qrp::model
