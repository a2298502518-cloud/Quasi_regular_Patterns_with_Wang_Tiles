#pragma once

#include <cmath>

namespace qrp::color {

struct MaterialSettings {
    double contourFrequency = 7.0;
    double contourStrength = 0.06;
    double contourWidth = 0.075;
    double reliefStrength = 0.16;

    [[nodiscard]] bool operator==(const MaterialSettings&) const noexcept = default;
};

[[nodiscard]] inline bool isValid(const MaterialSettings& settings) noexcept {
    return std::isfinite(settings.contourFrequency)
        && settings.contourFrequency >= 0.0
        && settings.contourFrequency <= 24.0
        && std::isfinite(settings.contourStrength)
        && settings.contourStrength >= 0.0
        && settings.contourStrength <= 0.35
        && std::isfinite(settings.contourWidth)
        && settings.contourWidth >= 0.01
        && settings.contourWidth <= 0.25
        && std::isfinite(settings.reliefStrength)
        && settings.reliefStrength >= 0.0
        && settings.reliefStrength <= 0.65;
}

} // namespace qrp::color
