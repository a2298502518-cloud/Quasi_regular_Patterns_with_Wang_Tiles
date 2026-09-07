#include "generators/PeriodicGradientNoise.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qrp::generators {
namespace {

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] std::uint32_t wrap(
    const std::int64_t value,
    const std::uint32_t period) noexcept {
    const auto signedPeriod = static_cast<std::int64_t>(period);
    const auto remainder = value % signedPeriod;
    return static_cast<std::uint32_t>(remainder < 0 ? remainder + signedPeriod : remainder);
}

[[nodiscard]] double fade(const double value) noexcept {
    return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
}

[[nodiscard]] double lerp(const double first, const double second, const double amount) noexcept {
    return first + amount * (second - first);
}

[[nodiscard]] math::Vec2 gradient(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint64_t seed) noexcept {
    constexpr double diagonal = 0.70710678118654752440;
    constexpr std::array<math::Vec2, 8> gradients{{
        {1.0, 0.0},
        {-1.0, 0.0},
        {0.0, 1.0},
        {0.0, -1.0},
        {diagonal, diagonal},
        {-diagonal, diagonal},
        {diagonal, -diagonal},
        {-diagonal, -diagonal},
    }};
    const std::uint64_t coordinate = (static_cast<std::uint64_t>(x) << 32U) | y;
    return gradients[mix64(seed ^ coordinate) & 7U];
}

[[nodiscard]] double dot(const math::Vec2 first, const math::Vec2 second) noexcept {
    return first.x * second.x + first.y * second.y;
}

} // namespace

PeriodicGradientNoise::PeriodicGradientNoise(const PeriodicNoiseSettings settings)
    : settings_(settings) {
    if (settings_.baseFrequency == 0 || settings_.octaves == 0 || settings_.octaves > 16
        || !std::isfinite(settings_.persistence)
        || settings_.persistence <= 0.0
        || settings_.persistence > 1.0) {
        throw std::invalid_argument("Invalid periodic noise settings.");
    }
    if (settings_.baseFrequency
        > std::numeric_limits<std::uint32_t>::max() >> (settings_.octaves - 1U)) {
        throw std::invalid_argument("Periodic noise octave frequency overflows.");
    }

    amplitudeNormalizer_ = 0.0;
    double amplitude = 1.0;
    for (std::uint32_t octave = 0; octave < settings_.octaves; ++octave) {
        amplitudeNormalizer_ += amplitude;
        amplitude *= settings_.persistence;
    }
}

const PeriodicNoiseSettings& PeriodicGradientNoise::settings() const noexcept {
    return settings_;
}

math::Vec2 PeriodicGradientNoise::gradientAt(
    const std::uint32_t x,
    const std::uint32_t y) const noexcept {
    return gradient(x, y, settings_.seed);
}

double PeriodicGradientNoise::evaluate(const math::Vec2 parameter) const noexcept {
    double value = 0.0;
    double amplitude = 1.0;
    std::uint32_t frequency = settings_.baseFrequency;
    for (std::uint32_t octave = 0; octave < settings_.octaves; ++octave) {
        value += amplitude * evaluateOctave(parameter, frequency);
        amplitude *= settings_.persistence;
        if (octave + 1U < settings_.octaves) {
            frequency <<= 1U;
        }
    }
    return value / amplitudeNormalizer_;
}

double PeriodicGradientNoise::evaluateOctave(
    const math::Vec2 parameter,
    const std::uint32_t frequency) const noexcept {
    const double scaledX = parameter.x * static_cast<double>(frequency);
    const double scaledY = parameter.y * static_cast<double>(frequency);
    const auto x0 = static_cast<std::int64_t>(std::floor(scaledX));
    const auto y0 = static_cast<std::int64_t>(std::floor(scaledY));
    const double localX = scaledX - std::floor(scaledX);
    const double localY = scaledY - std::floor(scaledY);

    const auto gx0 = wrap(x0, frequency);
    const auto gy0 = wrap(y0, frequency);
    const auto gx1 = wrap(x0 + 1, frequency);
    const auto gy1 = wrap(y0 + 1, frequency);
    const auto g00 = gradientAt(gx0, gy0);
    const auto g10 = gradientAt(gx1, gy0);
    const auto g01 = gradientAt(gx0, gy1);
    const auto g11 = gradientAt(gx1, gy1);

    const double n00 = dot(g00, math::Vec2{localX, localY});
    const double n10 = dot(g10, math::Vec2{localX - 1.0, localY});
    const double n01 = dot(g01, math::Vec2{localX, localY - 1.0});
    const double n11 = dot(g11, math::Vec2{localX - 1.0, localY - 1.0});
    const double xBlend = fade(localX);
    const double yBlend = fade(localY);
    return lerp(lerp(n00, n10, xBlend), lerp(n01, n11, xBlend), yBlend);
}

} // namespace qrp::generators
