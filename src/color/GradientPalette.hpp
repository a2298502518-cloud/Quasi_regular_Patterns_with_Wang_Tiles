#pragma once

#include <cstdint>
#include <vector>

namespace qrp::color {

struct Color3 {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

struct ColorStop {
    double position = 0.0;
    Color3 color;
};

struct ToneSettings {
    double center = 0.0;
    double contrast = 1.8;
    double bandFrequency = 0.0;
    double bandStrength = 0.0;
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

} // namespace qrp::color
