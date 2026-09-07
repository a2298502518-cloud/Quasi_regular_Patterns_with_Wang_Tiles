#include "color/GradientPalette.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace qrp::color {
namespace {

[[nodiscard]] double decodeSrgb(const double value) noexcept {
    return value <= 0.04045
        ? value / 12.92
        : std::pow((value + 0.055) / 1.055, 2.4);
}

[[nodiscard]] double encodeSrgb(const double value) noexcept {
    const double bounded = std::clamp(value, 0.0, 1.0);
    return bounded <= 0.0031308
        ? 12.92 * bounded
        : 1.055 * std::pow(bounded, 1.0 / 2.4) - 0.055;
}

[[nodiscard]] Color3 linearRgbFromSrgb8(
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue) noexcept {
    return {
        decodeSrgb(static_cast<double>(red) / 255.0),
        decodeSrgb(static_cast<double>(green) / 255.0),
        decodeSrgb(static_cast<double>(blue) / 255.0),
    };
}

[[nodiscard]] double smoothstep(const double value) noexcept {
    return value * value * (3.0 - 2.0 * value);
}

[[nodiscard]] Color3 interpolate(
    const Color3 first,
    const Color3 second,
    const double amount) noexcept {
    return {
        first.red + amount * (second.red - first.red),
        first.green + amount * (second.green - first.green),
        first.blue + amount * (second.blue - first.blue),
    };
}

} // namespace

GradientPalette::GradientPalette(
    std::vector<ColorStop> stops,
    const ToneSettings tone)
    : stops_(std::move(stops)),
      tone_(tone) {
    if (stops_.size() < 2
        || !std::isfinite(tone_.center)
        || !std::isfinite(tone_.contrast)
        || tone_.contrast <= 0.0
        || !std::isfinite(tone_.bandFrequency)
        || tone_.bandFrequency < 0.0
        || !std::isfinite(tone_.bandStrength)
        || tone_.bandStrength < 0.0
        || tone_.bandStrength > 1.0) {
        throw std::invalid_argument("Invalid gradient palette settings.");
    }

    std::sort(stops_.begin(), stops_.end(), [](const ColorStop& a, const ColorStop& b) {
        return a.position < b.position;
    });
    if (stops_.front().position != 0.0 || stops_.back().position != 1.0) {
        throw std::invalid_argument("Gradient palette stops must cover positions zero and one.");
    }
    double previous = -1.0;
    for (const auto& stop : stops_) {
        if (!std::isfinite(stop.position)
            || stop.position <= previous
            || stop.position < 0.0
            || stop.position > 1.0
            || !std::isfinite(stop.color.red)
            || !std::isfinite(stop.color.green)
            || !std::isfinite(stop.color.blue)) {
            throw std::invalid_argument("Gradient palette stops must be finite and ordered.");
        }
        previous = stop.position;
    }
}

GradientPalette GradientPalette::createMidnightGold() {
    return GradientPalette({
        {0.00, linearRgbFromSrgb8(4, 8, 24)},
        {0.20, linearRgbFromSrgb8(15, 31, 58)},
        {0.43, linearRgbFromSrgb8(13, 88, 91)},
        {0.68, linearRgbFromSrgb8(210, 142, 45)},
        {0.86, linearRgbFromSrgb8(244, 211, 139)},
        {1.00, linearRgbFromSrgb8(255, 246, 219)},
    }, ToneSettings{0.0, 2.10, 8.0, 0.10});
}

GradientPalette GradientPalette::createMineral() {
    return GradientPalette({
        {0.00, linearRgbFromSrgb8(7, 26, 31)},
        {0.22, linearRgbFromSrgb8(18, 72, 75)},
        {0.45, linearRgbFromSrgb8(57, 126, 116)},
        {0.67, linearRgbFromSrgb8(184, 195, 153)},
        {0.84, linearRgbFromSrgb8(214, 133, 91)},
        {1.00, linearRgbFromSrgb8(246, 222, 189)},
    }, ToneSettings{-0.02, 2.35, 5.0, 0.08});
}

GradientPalette GradientPalette::createAurora() {
    return GradientPalette({
        {0.00, linearRgbFromSrgb8(12, 4, 31)},
        {0.20, linearRgbFromSrgb8(48, 17, 105)},
        {0.42, linearRgbFromSrgb8(49, 76, 180)},
        {0.62, linearRgbFromSrgb8(41, 188, 204)},
        {0.80, linearRgbFromSrgb8(137, 238, 190)},
        {1.00, linearRgbFromSrgb8(255, 153, 198)},
    }, ToneSettings{0.0, 1.95, 7.0, 0.12});
}

const std::vector<ColorStop>& GradientPalette::stops() const noexcept {
    return stops_;
}

const ToneSettings& GradientPalette::tone() const noexcept {
    return tone_;
}

Color3 GradientPalette::sample(const double scalar) const noexcept {
    double coordinate = 0.5 + 0.5 * std::tanh(tone_.contrast * (scalar - tone_.center));
    if (tone_.bandFrequency > 0.0 && tone_.bandStrength > 0.0) {
        constexpr double tau = 2.0 * std::numbers::pi_v<double>;
        const double band = 0.5 + 0.5 * std::sin(tau * tone_.bandFrequency * coordinate);
        coordinate = (1.0 - tone_.bandStrength) * coordinate
            + tone_.bandStrength * band;
    }
    coordinate = std::clamp(coordinate, 0.0, 1.0);

    auto upper = std::upper_bound(
        stops_.begin(),
        stops_.end(),
        coordinate,
        [](const double value, const ColorStop& stop) {
            return value < stop.position;
        });
    if (upper == stops_.begin()) {
        return upper->color;
    }
    if (upper == stops_.end()) {
        return stops_.back().color;
    }

    const auto& second = *upper;
    const auto& first = *(upper - 1);
    const double local = (coordinate - first.position)
        / (second.position - first.position);
    return interpolate(first.color, second.color, smoothstep(local));
}

double maximumChannelDifference(const Color3 first, const Color3 second) noexcept {
    return std::max({
        std::abs(first.red - second.red),
        std::abs(first.green - second.green),
        std::abs(first.blue - second.blue),
    });
}

Color3 linearFromSrgb(const Color3 color) noexcept {
    return {
        decodeSrgb(std::clamp(color.red, 0.0, 1.0)),
        decodeSrgb(std::clamp(color.green, 0.0, 1.0)),
        decodeSrgb(std::clamp(color.blue, 0.0, 1.0)),
    };
}

Color3 srgbFromLinear(const Color3 color) noexcept {
    return {
        encodeSrgb(color.red),
        encodeSrgb(color.green),
        encodeSrgb(color.blue),
    };
}

} // namespace qrp::color
