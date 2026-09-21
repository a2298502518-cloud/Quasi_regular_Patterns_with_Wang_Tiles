#pragma once

#include <vector>

namespace qrp::color {

struct Color3 {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    [[nodiscard]] bool operator==(const Color3&) const noexcept = default;
};

class GradientPalette final {
public:
    [[nodiscard]] static GradientPalette createMidnightGold();
    [[nodiscard]] Color3 sample(double scalar) const noexcept;

private:
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

    GradientPalette(std::vector<ColorStop> stops, ToneSettings tone);

    std::vector<ColorStop> stops_;
    ToneSettings tone_;
};

} // namespace qrp::color
