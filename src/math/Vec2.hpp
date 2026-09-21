#pragma once

namespace qrp::math {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    [[nodiscard]] bool operator==(const Vec2&) const noexcept = default;
};

} // namespace qrp::math
