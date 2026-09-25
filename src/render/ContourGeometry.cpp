#include "render/ContourGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace qrp::render {

ContourGeometry::ContourGeometry(const std::size_t cells, const double spacing,
    const math::Vec2 origin, std::vector<double> values, const double level)
    : cells_(cells), spacing_(spacing), origin_(origin), values_(std::move(values)), level_(level),
      binWidth_(8.0 * spacing), binCount_((cells + 7) / 8) {
    if (cells == 0 || !std::isfinite(spacing) || spacing <= 0.0
        || !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(level)
        || values_.size() != (cells + 1) * (cells + 1)
        || !std::all_of(values_.begin(), values_.end(), [](double v) { return std::isfinite(v); })) {
        throw std::invalid_argument("Contour geometry requires a finite square vertex grid.");
    }
    bins_.resize(binCount_ * binCount_);
    const std::size_t stride = cells + 1;
    for (std::size_t y = 0; y < cells; ++y) {
        for (std::size_t x = 0; x < cells; ++x) {
            // 角点依次为左下、右下、右上、左上；边也按逆时针编号。
            const std::array<double, 4> f{values_[y * stride + x] - level,
                values_[y * stride + x + 1] - level,
                values_[(y + 1) * stride + x + 1] - level,
                values_[(y + 1) * stride + x] - level};
            const double px = origin.x + spacing * static_cast<double>(x);
            const double py = origin.y + spacing * static_cast<double>(y);
            const std::array<math::Vec2, 4> p{{{px, py}, {px + spacing, py},
                {px + spacing, py + spacing}, {px, py + spacing}}};
            std::array<math::Vec2, 4> intersections{};
            std::array<std::size_t, 4> active{};
            std::size_t count = 0;
            for (std::size_t edge = 0; edge < 4; ++edge) {
                const std::size_t next = (edge + 1) % 4;
                if ((f[edge] < 0.0) == (f[next] < 0.0)) continue;
                const double t = f[edge] / (f[edge] - f[next]);
                intersections[edge] = {p[edge].x + t * (p[next].x - p[edge].x),
                    p[edge].y + t * (p[next].y - p[edge].y)};
                active[count++] = edge;
            }
            if (count == 2) {
                addSegment(intersections[active[0]], intersections[active[1]]);
            } else if (count == 4) {
                // 双线性渐近判别，不用固定对角线替鞍点决定连接关系。
                const double determinant = f[0] * f[2] - f[1] * f[3];
                if (determinant > 0.0) {
                    addSegment(intersections[0], intersections[1]);
                    addSegment(intersections[2], intersections[3]);
                } else if (determinant < 0.0) {
                    addSegment(intersections[0], intersections[3]);
                    addSegment(intersections[1], intersections[2]);
                } else {
                    // 临界等值线确实经过鞍点时保留交叉，不凭空选择一种拓扑。
                    const double denominator = f[0] - f[1] + f[2] - f[3];
                    const math::Vec2 saddle{px + spacing * (f[0] - f[3]) / denominator,
                        py + spacing * (f[0] - f[1]) / denominator};
                    for (const auto& intersection : intersections) addSegment(intersection, saddle);
                }
            }
        }
    }
}

std::size_t ContourGeometry::bin(const double coordinate, const double origin) const noexcept {
    return static_cast<std::size_t>(std::clamp(std::floor((coordinate - origin) / binWidth_),
        0.0, static_cast<double>(binCount_ - 1)));
}

void ContourGeometry::addSegment(const math::Vec2 a, const math::Vec2 b) {
    if (a.x == b.x && a.y == b.y) return;
    const auto index = segments_.size();
    segments_.push_back({a, b});
    for (auto y = bin(std::min(a.y, b.y), origin_.y); y <= bin(std::max(a.y, b.y), origin_.y); ++y) {
        for (auto x = bin(std::min(a.x, b.x), origin_.x); x <= bin(std::max(a.x, b.x), origin_.x); ++x) {
            bins_[y * binCount_ + x].push_back(index);
        }
    }
}

double ContourGeometry::sample(const math::Vec2 position) const noexcept {
    const double u = std::clamp((position.x - origin_.x) / spacing_, 0.0, static_cast<double>(cells_));
    const double v = std::clamp((position.y - origin_.y) / spacing_, 0.0, static_cast<double>(cells_));
    const auto x = std::min(static_cast<std::size_t>(u), cells_ - 1);
    const auto y = std::min(static_cast<std::size_t>(v), cells_ - 1);
    const double tx = u - static_cast<double>(x), ty = v - static_cast<double>(y);
    const auto stride = cells_ + 1;
    const double south = std::lerp(values_[y * stride + x], values_[y * stride + x + 1], tx);
    const double north = std::lerp(values_[(y + 1) * stride + x], values_[(y + 1) * stride + x + 1], tx);
    return std::lerp(south, north, ty);
}

ContourScanline ContourGeometry::scanline(const double y) const {
    ContourScanline row{sample({origin_.x, y}) < level_, {}};
    for (const auto& s : segments_) {
        // 半开区间使共享端点只被计数一次，水平线段不参与奇偶切换。
        if ((s.a.y <= y && y < s.b.y) || (s.b.y <= y && y < s.a.y)) {
            row.crossings.push_back(s.a.x + (y - s.a.y) * (s.b.x - s.a.x) / (s.b.y - s.a.y));
        }
    }
    std::sort(row.crossings.begin(), row.crossings.end());
    return row;
}

double ContourGeometry::distance(const math::Vec2 position, const double limit) const {
    double squared = limit * limit;
    const auto endX = bin(position.x + limit, origin_.x), endY = bin(position.y + limit, origin_.y);
    for (auto y = bin(position.y - limit, origin_.y); y <= endY; ++y) {
        for (auto x = bin(position.x - limit, origin_.x); x <= endX; ++x) {
            for (const auto index : bins_[y * binCount_ + x]) {
                const auto& s = segments_[index];
                const double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
                const double t = std::clamp(((position.x - s.a.x) * dx + (position.y - s.a.y) * dy)
                    / (dx * dx + dy * dy), 0.0, 1.0);
                const double ex = position.x - s.a.x - t * dx, ey = position.y - s.a.y - t * dy;
                squared = std::min(squared, ex * ex + ey * ey);
            }
        }
    }
    return std::sqrt(squared);
}

std::size_t ContourGeometry::segmentCount() const noexcept { return segments_.size(); }

} // namespace qrp::render
