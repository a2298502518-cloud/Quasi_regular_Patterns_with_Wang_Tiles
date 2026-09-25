#include "color/InkCoverage.hpp"

#include <cmath>

namespace qrp::color {

render::Rgb8 inkCoverage(const double coverage) {
    const auto channel = [coverage](const double ink, const double paper) {
        const auto decode = [](double s) {
            s /= 255.0;
            return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        const double linear = std::lerp(decode(paper), decode(ink), coverage);
        const double srgb = linear <= 0.0031308
            ? 12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
        return static_cast<std::uint8_t>(std::lround(255.0 * srgb));
    };
    return {channel(kInk.red, kPaper.red), channel(kInk.green, kPaper.green),
        channel(kInk.blue, kPaper.blue)};
}

} // namespace qrp::color
