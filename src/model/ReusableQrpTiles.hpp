#pragma once

#include "model/ParametricWangQrpField.hpp"

#include <array>
#include <cstdint>

namespace qrp::model {

enum class QrpSourceSelection { Fixed, BoundaryMatched };

struct ReusableQrpTileParameters {
    ParametricWangQrpParameters qrp;
    // 单位瓦片在 QRP 源坐标中覆盖的长度；不是全局铺砌位置。
    double sourceSpan = 4.0;
    double transitionWidth = 0.2;
    QrpSourceSelection selection = QrpSourceSelection::BoundaryMatched;
};

struct QrpTileSources {
    std::array<math::Vec2, 2> vertices;
    std::array<math::Vec2, 4> verticalEdges;
    std::array<math::Vec2, 4> horizontalEdges;
    std::array<math::Vec2, 16> interiors;
};

struct QrpSourceFit {
    double edgeFixed = 0.0;
    double edgeSelected = 0.0;
    double interiorFixed = 0.0;
    double interiorSelected = 0.0;
};

// 固定 16 种瓦片：ID 的四位依次表示 SW、SE、NW、NE 角点状态。
// 每条边的颜色编码两个有序端点；相同颜色同时约束边与角点。
// 求值只接收类型与局部坐标，不能读取铺砌位置或邻居。
class ReusableQrpTiles final {
public:
    explicit ReusableQrpTiles(ReusableQrpTileParameters parameters);

    [[nodiscard]] ScalarFieldEvaluation evaluate(std::uint32_t tileId, math::Vec2 local) const noexcept;
    [[nodiscard]] ScalarFieldEvaluation source(math::Vec2 local, math::Vec2 offset) const noexcept;
    [[nodiscard]] const ReusableQrpTileParameters& parameters() const noexcept { return parameters_; }
    [[nodiscard]] const QrpTileSources& sources() const noexcept { return sources_; }
    [[nodiscard]] const QrpSourceFit& fit() const noexcept { return fit_; }

private:
    void selectSources();
    ReusableQrpTileParameters parameters_;
    ParametricWangQrpField field_;
    QrpTileSources sources_;
    QrpSourceFit fit_;
};

} // namespace qrp::model
