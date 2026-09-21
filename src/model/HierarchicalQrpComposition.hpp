#pragma once

#include "model/ScalarFieldEvaluation.hpp"

namespace qrp::model {

struct HierarchicalQrpCompositionParameters {
    double mediumStrength = 0.22;
    double fineStrength = 0.07;

    [[nodiscard]] bool operator==(
        const HierarchicalQrpCompositionParameters&) const noexcept = default;
};

class HierarchicalQrpComposition final {
public:
    explicit HierarchicalQrpComposition(
        HierarchicalQrpCompositionParameters parameters = {});

    [[nodiscard]] const HierarchicalQrpCompositionParameters& parameters() const noexcept;

    // 在 |coarse|<=1 且 |medium|,|fine|<=sqrt(2) 的前提下返回 [-1,1] 证书。
    [[nodiscard]] double certifiedMaximumMagnitude() const noexcept;

    [[nodiscard]] ScalarFieldEvaluation evaluate(
        ScalarFieldEvaluation coarse,
        ScalarFieldEvaluation medium,
        ScalarFieldEvaluation fine) const noexcept;

private:
    HierarchicalQrpCompositionParameters parameters_;
};

} // namespace qrp::model
