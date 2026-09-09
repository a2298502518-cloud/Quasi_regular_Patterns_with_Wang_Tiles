#pragma once

#include <cstdint>
#include <vector>

namespace qrp::color {

struct Color3 {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    [[nodiscard]] bool operator==(const Color3&) const noexcept = default;
};

struct ColorStop {
    double position = 0.0;
    Color3 color;

    [[nodiscard]] bool operator==(const ColorStop&) const noexcept = default;
};

struct ToneSettings {
    double center = 0.0;
    double contrast = 1.8;
    double bandFrequency = 0.0;
    double bandStrength = 0.0;

    [[nodiscard]] bool operator==(const ToneSettings&) const noexcept = default;
};

class GradientPalette final {
public:
    GradientPalette(std::vector<ColorStop> stops, ToneSettings tone = {});

    [[nodiscard]] static GradientPalette createMidnightGold();
    [[nodiscard]] static GradientPalette createMineral();
    [[nodiscard]] static GradientPalette createAurora();

    [[nodiscard]] const std::vector<ColorStop>& stops() const noexcept;
    [[nodiscard]] const ToneSettings& tone() const noexcept;
    [[nodiscard]] Color3 sample(double scalar) const noexcept;

private:
    std::vector<ColorStop> stops_;
    ToneSettings tone_;
};

[[nodiscard]] double maximumChannelDifference(Color3 first, Color3 second) noexcept;
[[nodiscard]] Color3 linearFromSrgb(Color3 color) noexcept;
[[nodiscard]] Color3 srgbFromLinear(Color3 color) noexcept;

} // namespace qrp::color
