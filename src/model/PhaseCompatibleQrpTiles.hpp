#pragma once

#include "model/ParametricWangQrpField.hpp"

#include <array>

namespace qrp::model {

struct PhaseCompatibleQrpTileParameters {
    ParametricWangQrpParameters qrp;
    double sourceSpan = 4.0;
    std::array<math::Vec2, 2> vertexOffsets{{{0.2, 0.3}, {0.9, 0.4}}};
    // 内部状态可编码 QRP 的 (A,B) 相对相位，而不局限于源坐标平移；默认保持旧库。
    std::array<math::Vec2, 2> vertexPhaseOffsets{};
};

struct CompatibleQrpMode {
    math::Vec2 wave;
    math::Vec2 winding;
    math::Vec2 closure;
    double amplitude;
    double phase;
    double vertexShift;
};

struct QrpBandCertificate {
    double dominantWeight;
    double remainderWeight;
    math::Vec2 axis;
    double minimumPhaseSlope;
    // 保证两色都有贯通的核心带，不保证全部零轮廓无分支或局部小岛。
    bool signedCoreBands;
};

// 使用 EndpointWangTiles 的 16 种端点编码类型；此模型不混合标量来源。
// 每个模态先满足相位模 2π 与梯度接边，再以原 QRP 振幅作余弦和。
class PhaseCompatibleQrpTiles final {
public:
    explicit PhaseCompatibleQrpTiles(PhaseCompatibleQrpTileParameters parameters);
    [[nodiscard]] ScalarFieldEvaluation evaluate(std::uint32_t tileId, math::Vec2 local) const noexcept;
    // 未闭合的全局 QRP 参考，仅使用基础 qrp 参数与给定源偏移，不混入内部状态相位。
    [[nodiscard]] ScalarFieldEvaluation source(math::Vec2 local, math::Vec2 offset) const noexcept;
    [[nodiscard]] const PhaseCompatibleQrpTileParameters& parameters() const noexcept { return parameters_; }
    [[nodiscard]] std::span<const CompatibleQrpMode> modes() const noexcept { return modes_; }
    [[nodiscard]] QrpBandCertificate bandCertificate() const noexcept;

private:
    PhaseCompatibleQrpTileParameters parameters_;
    ParametricWangQrpField source_;
    std::vector<CompatibleQrpMode> modes_;
    std::array<std::array<double, 4>, 16> corners_;
};

} // namespace qrp::model
