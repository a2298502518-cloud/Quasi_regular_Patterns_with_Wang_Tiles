#pragma once

#include "math/Vec2.hpp"

#include <cstddef>
#include <vector>

namespace qrp::render {

struct ContourScanline {
    bool insideAtLeft = false;
    std::vector<double> crossings;
};

// 从固定采样网格提取一次轮廓，预览和导出复用相同的几何定义。
// 内部定义为 scalar < level；距离单位与输入坐标一致，不是场值宽度。
class ContourGeometry final {
public:
    ContourGeometry(std::size_t cells, double spacing, math::Vec2 origin,
        std::vector<double> values, double level);

    [[nodiscard]] double sample(math::Vec2 position) const noexcept;
    [[nodiscard]] ContourScanline scanline(double y) const;
    [[nodiscard]] double distance(math::Vec2 position, double limit) const;
    [[nodiscard]] std::size_t segmentCount() const noexcept;

private:
    struct Segment { math::Vec2 a; math::Vec2 b; };
    void addSegment(math::Vec2 a, math::Vec2 b);
    [[nodiscard]] std::size_t bin(double coordinate, double origin) const noexcept;

    std::size_t cells_;
    double spacing_;
    math::Vec2 origin_;
    std::vector<double> values_;
    double level_;
    double binWidth_;
    std::size_t binCount_;
    std::vector<Segment> segments_;
    std::vector<std::vector<std::size_t>> bins_;
};

} // namespace qrp::render
