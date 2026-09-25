#include "model/OrderedQrpField.hpp"

#include <cmath>
#include <stdexcept>

namespace qrp::model {
namespace {

[[nodiscard]] ParametricWangQrpParameters displacementParameters(
    const OrderedQrpParameters& parameters, const math::Vec2 phaseOffset) {
    const double frequency = parameters.spatialFrequency;
    // Wang 标签仍参与相位，但内部耦合随载波频率缩放，限制相对形变梯度。
    return {parameters.resonanceCount, 0.28 * frequency,
        {parameters.globalPhase.x + phaseOffset.x, parameters.globalPhase.y + phaseOffset.y},
        {0.035 * frequency, -0.025 * frequency}, 0.5, 0.4,
        0.0, parameters.orientationRadians, 0.0};
}

} // namespace

OrderedQrpField::OrderedQrpField(const OrderedQrpParameters parameters)
    : parameters_(parameters),
      displacementX_(displacementParameters(parameters, {})),
      displacementY_(displacementParameters(parameters, {1.3, -0.9})),
      axisX_{std::cos(parameters.orientationRadians), std::sin(parameters.orientationRadians)},
      axisY_{-axisX_.y, axisX_.x} {
    if (!std::isfinite(parameters_.variation) || parameters_.variation < 0.0
        || parameters_.variation > 1.0 || !std::isfinite(parameters_.aspect)
        || parameters_.aspect <= 0.0
        || (parameters_.motif != OrderedMotif::Ribbons && parameters_.motif != OrderedMotif::Spots)) {
        throw std::invalid_argument("Ordered QRP requires variation in [0,1], positive aspect and a known motif.");
    }
}

const OrderedQrpParameters& OrderedQrpField::parameters() const noexcept {
    return parameters_;
}

ScalarFieldEvaluation OrderedQrpField::evaluate(
    const math::Vec2 position, const WangContentWeightEvaluation weight) const noexcept {
    const double frequency = parameters_.spatialFrequency;
    const double variation = parameters_.variation;
    const auto dx = variation == 0.0 ? ScalarFieldEvaluation{} : displacementX_.evaluate(position, weight);
    const auto dy = variation == 0.0 ? ScalarFieldEvaluation{} : displacementY_.evaluate(position, weight);
    const double u = frequency * (axisX_.x * position.x + axisX_.y * position.y) + variation * dx.value;
    const double v = parameters_.aspect
        * (frequency * (axisY_.x * position.x + axisY_.y * position.y) + variation * dy.value);
    const math::Vec2 du{frequency * axisX_.x + variation * dx.gradient.x,
        frequency * axisX_.y + variation * dx.gradient.y};
    const math::Vec2 dv{parameters_.aspect * (frequency * axisY_.x + variation * dy.gradient.x),
        parameters_.aspect * (frequency * axisY_.y + variation * dy.gradient.y)};
    if (parameters_.motif == OrderedMotif::Ribbons) {
        return {std::cos(u), {-std::sin(u) * du.x, -std::sin(u) * du.y}};
    }
    // 乘积而不是相加：正值单元被稳定的负值间隔分开，避免鞍点处任意粘连。
    const double a = 0.5 + 0.5 * std::cos(u);
    const double b = 0.5 + 0.5 * std::cos(v);
    const double derivativeU = -std::sin(u) * b;
    const double derivativeV = -std::sin(v) * a;
    return {2.0 * a * b - 1.0,
        {derivativeU * du.x + derivativeV * dv.x,
         derivativeU * du.y + derivativeV * dv.y}};
}

} // namespace qrp::model
