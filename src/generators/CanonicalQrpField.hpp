#pragma once

#include "math/Vec2.hpp"

#include <cstddef>

namespace qrp::generators {

struct CanonicalQrpParameters {
    double resonanceCount = 5.0;
    double scale = 18.0;
    math::Vec2 translation;

    [[nodiscard]] bool operator==(const CanonicalQrpParameters&) const noexcept = default;
};

struct CanonicalQrpEvaluation {
    double value = 0.0;
    // 对模型坐标 (x,y) 的解析梯度，不包含 normalized 采样尺度。
    math::Vec2 modelGradient;
};

class CanonicalQrpField final {
public:
    explicit CanonicalQrpField(CanonicalQrpParameters parameters);

    [[nodiscard]] const CanonicalQrpParameters& parameters() const noexcept;
    [[nodiscard]] std::size_t modeCount() const noexcept;

    // 论文式 (2)：normalized 对应 (n_x / W_x, n_y / W_y)。
    [[nodiscard]] math::Vec2 modelCoordinate(math::Vec2 normalized) const noexcept;
    // 论文式 (1)：直接在 R^2 模型域求值，不施加环面周期化或归一化。
    [[nodiscard]] CanonicalQrpEvaluation evaluateModelWithGradient(
        math::Vec2 modelCoordinate) const noexcept;
    [[nodiscard]] double evaluateModel(math::Vec2 modelCoordinate) const noexcept;
    [[nodiscard]] double evaluateNormalized(math::Vec2 normalized) const noexcept;

private:
    CanonicalQrpParameters parameters_;
    std::size_t modeCount_ = 0;
};

} // namespace qrp::generators
