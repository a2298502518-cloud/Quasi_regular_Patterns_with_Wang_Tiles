#pragma once

#include "math/Vec2.hpp"
#include "model/ScalarFieldEvaluation.hpp"
#include "model/WangContentWeight.hpp"

#include <cstdint>

namespace qrp::model {

struct ParametricWangQrpParameters {
    std::uint32_t resonanceCount = 5;
    double spatialFrequency = 1.0;
    math::Vec2 globalPhase;
    math::Vec2 wangPhase;
    double weightCenter = 0.5;
    double weightRadius = 0.4;

    [[nodiscard]] bool operator==(
        const ParametricWangQrpParameters&) const noexcept = default;
};

class ParametricWangQrpField final {
public:
    explicit ParametricWangQrpField(ParametricWangQrpParameters parameters);

    [[nodiscard]] const ParametricWangQrpParameters& parameters() const noexcept;

    // position and weight.localGradient must use the same coordinate basis.
    // The average of q cosine modes gives the exact range certificate [-1,1].
    [[nodiscard]] ScalarFieldEvaluation evaluate(
        math::Vec2 position,
        WangContentWeightEvaluation weight) const noexcept;

private:
    ParametricWangQrpParameters parameters_;
};

} // namespace qrp::model
