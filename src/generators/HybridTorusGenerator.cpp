#include "generators/HybridTorusGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace qrp::generators {
namespace {

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] double unitDouble(const std::uint64_t value) noexcept {
    return static_cast<double>(value >> 11U) * 0x1.0p-53;
}

[[nodiscard]] double boundaryWindow(const math::Vec2 parameter) noexcept {
    constexpr double fadeWidth = 0.18;
    const auto quinticFade = [](const double distance) noexcept {
        const double t = std::clamp(distance / fadeWidth, 0.0, 1.0);
        return t * t * t * (10.0 + t * (-15.0 + 6.0 * t));
    };
    return quinticFade(parameter.x)
        * quinticFade(1.0 - parameter.x)
        * quinticFade(parameter.y)
        * quinticFade(1.0 - parameter.y);
}

} // namespace

HybridTorusGenerator::HybridTorusGenerator(
    TorusFourier fourier,
    PeriodicGradientNoise noise,
    const HybridGeneratorSettings settings)
    : fourier_(std::move(fourier)),
      noise_(std::move(noise)),
      settings_(settings) {
    if (!std::isfinite(settings_.fourierWeight)
        || !std::isfinite(settings_.noiseWeight)
        || !std::isfinite(settings_.tileVariationAmplitude)
        || !std::isfinite(settings_.tileDomainWarpAmplitude)
        || !std::isfinite(settings_.worldModulationAmplitude)
        || !std::isfinite(settings_.worldDomainWarpAmplitude)
        || !std::isfinite(settings_.worldFrequencyX)
        || !std::isfinite(settings_.worldFrequencyY)) {
        throw std::invalid_argument("Hybrid generator settings must be finite.");
    }
}

const HybridGeneratorSettings& HybridTorusGenerator::settings() const noexcept {
    return settings_;
}

const TorusFourier& HybridTorusGenerator::fourier() const noexcept {
    return fourier_;
}

const PeriodicGradientNoise& HybridTorusGenerator::noise() const noexcept {
    return noise_;
}

TileVariationDescriptor HybridTorusGenerator::describeTile(
    const std::uint64_t tileSeed) noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    TileVariationDescriptor descriptor;
    descriptor.firstDomainPhase = tau
        * unitDouble(mix64(tileSeed ^ 0x243f6a8885a308d3ULL));
    descriptor.secondDomainPhase = tau
        * unitDouble(mix64(tileSeed ^ 0x13198a2e03707344ULL));

    std::uint64_t state = tileSeed;
    for (std::size_t mode = 0; mode < descriptor.phase.size(); ++mode) {
        state = mix64(state + 0x9e3779b97f4a7c15ULL);
        descriptor.frequencyU[mode] = 1 + static_cast<int>(state & 3U);
        state = mix64(state);
        descriptor.frequencyV[mode] = 1 + static_cast<int>(state & 3U);
        state = mix64(state);
        descriptor.phase[mode] = tau * unitDouble(state);
    }
    return descriptor;
}

double HybridTorusGenerator::evaluateBase(const math::Vec2 parameter) const noexcept {
    return settings_.fourierWeight * fourier_.evaluate(parameter)
        + settings_.noiseWeight * noise_.evaluate(parameter);
}

double HybridTorusGenerator::evaluate(const GeneratorInput& input) const noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    const double worldPhase = tau * (
        settings_.worldFrequencyX * input.world.x
        + settings_.worldFrequencyY * input.world.y);
    const double secondaryWorldPhase = tau * (
        -0.73 * settings_.worldFrequencyY * input.world.x
        + 0.91 * settings_.worldFrequencyX * input.world.y + 0.17);
    const TileVariationDescriptor tile = describeTile(input.tileSeed);
    const math::Vec2 tileOffset = tileDomainOffset(input.parameter, tile);
    const double window = boundaryWindow(input.parameter);
    const math::Vec2 warpedParameter{
        input.parameter.x
            + settings_.tileDomainWarpAmplitude * window * tileOffset.x
            + settings_.worldDomainWarpAmplitude * std::sin(worldPhase),
        input.parameter.y
            + settings_.tileDomainWarpAmplitude * window * tileOffset.y
            + settings_.worldDomainWarpAmplitude * std::cos(secondaryWorldPhase),
    };
    return evaluateBase(warpedParameter)
        + settings_.tileVariationAmplitude
            * window
            * tileVariation(input.parameter, tile)
        + settings_.worldModulationAmplitude * std::sin(worldPhase);
}

math::Vec2 HybridTorusGenerator::tileDomainOffset(
    const math::Vec2 parameter,
    const TileVariationDescriptor& descriptor) noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    return {
        0.68 * std::sin(tau * (2.0 * parameter.x + parameter.y)
            + descriptor.firstDomainPhase)
            + 0.32 * std::cos(tau * (parameter.x - 2.0 * parameter.y)
            + descriptor.secondDomainPhase),
        0.68 * std::cos(tau * (parameter.x + 2.0 * parameter.y)
            + descriptor.secondDomainPhase)
            + 0.32 * std::sin(tau * (2.0 * parameter.x - parameter.y)
            + descriptor.firstDomainPhase),
    };
}

double HybridTorusGenerator::tileVariation(
    const math::Vec2 parameter,
    const TileVariationDescriptor& descriptor) noexcept {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    double value = 0.0;
    for (std::size_t mode = 0; mode < descriptor.phase.size(); ++mode) {
        const double amplitude = 1.0 / static_cast<double>(mode + 1U);
        value += amplitude * std::cos(tau * (
            static_cast<double>(descriptor.frequencyU[mode]) * parameter.x
            + static_cast<double>(descriptor.frequencyV[mode]) * parameter.y)
            + descriptor.phase[mode]);
    }
    return value / 2.0833333333333333333;
}

} // namespace qrp::generators
