#pragma once

#include "render/Image.hpp"

namespace qrp::color {

inline constexpr render::Rgb8 kInk{24, 55, 67};
inline constexpr render::Rgb8 kPaper{241, 226, 193};

// coverage∈[0,1]，在线性光空间混合固定双色；不改变标量场。
[[nodiscard]] render::Rgb8 inkCoverage(double coverage);

} // namespace qrp::color
