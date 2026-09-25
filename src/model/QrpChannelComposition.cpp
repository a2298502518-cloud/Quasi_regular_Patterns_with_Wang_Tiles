#include "model/QrpChannelComposition.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qrp::model {

const char* relationName(const QrpChannelRelation relation) {
    switch (relation) {
    case QrpChannelRelation::Direct: return "direct";
    case QrpChannelRelation::Product: return "product";
    case QrpChannelRelation::JointEnergy: return "joint-energy";
    case QrpChannelRelation::Nested: return "nested";
    }
    throw std::invalid_argument("Unknown QRP channel relation.");
}

QrpChannelComposition::QrpChannelComposition(const QrpChannelCompositionParameters parameters)
    : parameters_(parameters) {
    if (!std::isfinite(parameters.parentLevel) || !std::isfinite(parameters.transitionWidth)
        || parameters.transitionWidth <= 0.0) {
        throw std::invalid_argument("QRP channel gate requires a finite level and positive width.");
    }
    switch (parameters.relation) {
    case QrpChannelRelation::Direct:
    case QrpChannelRelation::Product:
    case QrpChannelRelation::JointEnergy:
    case QrpChannelRelation::Nested: break;
    default: throw std::invalid_argument("Unknown QRP channel relation.");
    }
}

ScalarFieldEvaluation QrpChannelComposition::evaluate(
    const ScalarFieldEvaluation first, const ScalarFieldEvaluation second) const noexcept {
    if (parameters_.relation == QrpChannelRelation::Direct) return first;
    double value = 0.0, firstScale = 0.0, secondScale = 0.0;
    switch (parameters_.relation) {
    case QrpChannelRelation::Product:
        value = first.value * second.value;
        firstScale = second.value;
        secondScale = first.value;
        break;
    case QrpChannelRelation::JointEnergy:
        value = 1.0 - first.value * first.value - second.value * second.value;
        firstScale = -2.0 * first.value;
        secondScale = -2.0 * second.value;
        break;
    case QrpChannelRelation::Nested: {
        // 父场只决定子场出现的位置。过渡端点的一阶导数为零，保留共享边 C¹。
        const double t = std::clamp(0.5 + (parameters_.parentLevel - first.value)
            / parameters_.transitionWidth, 0.0, 1.0);
        const double gate = t * t * (3.0 - 2.0 * t);
        const double gateDerivative = -6.0 * t * (1.0 - t) / parameters_.transitionWidth;
        value = 1.0 - gate * (1.0 - second.value);
        firstScale = -gateDerivative * (1.0 - second.value);
        secondScale = gate;
        break;
    }
    case QrpChannelRelation::Direct: break;
    }
    return {value, {
        firstScale * first.gradient.x + secondScale * second.gradient.x,
        firstScale * first.gradient.y + secondScale * second.gradient.y}};
}

} // namespace qrp::model
