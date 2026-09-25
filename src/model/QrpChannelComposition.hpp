#pragma once

#include "model/ScalarFieldEvaluation.hpp"

namespace qrp::model {

enum class QrpChannelRelation { Direct, Product, JointEnergy, Nested };

[[nodiscard]] const char* relationName(QrpChannelRelation relation);

struct QrpChannelCompositionParameters {
    QrpChannelRelation relation = QrpChannelRelation::Direct;
    double parentLevel = 0.0;
    double transitionWidth = 0.35;
};

// 组合两组已有 QRP 的值和解析梯度；不生成新噪声，也不负责着色。
// 两个输入均在 [-1,1] 时，四种关系的结果也在 [-1,1]。
class QrpChannelComposition final {
public:
    explicit QrpChannelComposition(QrpChannelCompositionParameters parameters);
    [[nodiscard]] ScalarFieldEvaluation evaluate(
        ScalarFieldEvaluation first, ScalarFieldEvaluation second) const noexcept;

private:
    QrpChannelCompositionParameters parameters_;
};

} // namespace qrp::model
