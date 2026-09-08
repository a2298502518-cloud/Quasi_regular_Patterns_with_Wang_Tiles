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

[[nodiscard]] std::uint32_t hashCell(
    const int x,
    const int y,
    const std::uint32_t salt) noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9e3779b9U
        ^ static_cast<std::uint32_t>(y) * 0x85ebca6bU
        ^ salt;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

[[nodiscard]] double unitFloat24(const std::uint32_t value) noexcept {
    return static_cast<double>(value >> 8U) / 16777216.0;
}

[[nodiscard]] double softBump(
    const double coordinate,
    const double center,
    const double halfWidth) noexcept {
    const double distance = std::abs(coordinate - center) / halfWidth;
    const double amount = std::clamp(1.0 - distance, 0.0, 1.0);
    return amount * amount * (3.0 - 2.0 * amount);
}

[[nodiscard]] double edgeInkCenter(const std::uint32_t color) noexcept {
    const int key = static_cast<int>(color);
    return 0.18 + 0.64 * unitFloat24(hashCell(key, 0, 0x6d2b79f5U));
}

[[nodiscard]] double edgeInkWidth(const std::uint32_t color) noexcept {
    const int key = static_cast<int>(color);
    return 0.038 + 0.018 * unitFloat24(hashCell(key, 0, 0x1b873593U));
}

[[nodiscard]] double smoothUnit(const double value) noexcept {
    const double amount = std::clamp(value, 0.0, 1.0);
    return amount * amount * (3.0 - 2.0 * amount);
}

[[nodiscard]] double segmentInk(
    const math::Vec2 point,
    const math::Vec2 first,
    const math::Vec2 second,
    const double firstWidth,
    const double secondWidth) noexcept {
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double lengthSquared = dx * dx + dy * dy;
    const double amount = lengthSquared > 0.0
        ? std::clamp(
            ((point.x - first.x) * dx + (point.y - first.y) * dy) / lengthSquared,
            0.0,
            1.0)
        : 0.0;
    const double offsetX = point.x - (first.x + amount * dx);
    const double offsetY = point.y - (first.y + amount * dy);
    const double width = firstWidth + amount * (secondWidth - firstWidth);
    return softBump(
        std::sqrt(offsetX * offsetX + offsetY * offsetY),
        0.0,
        width);
}

[[nodiscard]] double cubicInk(
    const math::Vec2 point,
    const math::Vec2 start,
    const math::Vec2 firstControl,
    const math::Vec2 secondControl,
    const math::Vec2 end,
    const double startWidth,
    const double endWidth) noexcept {
    double coverage = 0.0;
    math::Vec2 previous = start;
    double previousWidth = startWidth;
    for (int segment = 1; segment <= 16; ++segment) {
        const double amount = static_cast<double>(segment) / 16.0;
        const double inverse = 1.0 - amount;
        const math::Vec2 current{
            inverse * inverse * inverse * start.x
                + 3.0 * inverse * inverse * amount * firstControl.x
                + 3.0 * inverse * amount * amount * secondControl.x
                + amount * amount * amount * end.x,
            inverse * inverse * inverse * start.y
                + 3.0 * inverse * inverse * amount * firstControl.y
                + 3.0 * inverse * amount * amount * secondControl.y
                + amount * amount * amount * end.y,
        };
        const double currentWidth = startWidth + amount * (endWidth - startWidth);
        coverage = std::max(
            coverage,
            segmentInk(point, previous, current, previousWidth, currentWidth));
        previous = current;
        previousWidth = currentWidth;
    }
    return coverage;
}

} // namespace

bool isValid(const ScalarProfile profile) noexcept {
    return profile == ScalarProfile::Natural
        || profile == ScalarProfile::Ridges
        || profile == ScalarProfile::Cells;
}

const char* scalarProfileName(const ScalarProfile profile) noexcept {
    switch (profile) {
    case ScalarProfile::Natural: return "natural";
    case ScalarProfile::Ridges: return "ridges";
    case ScalarProfile::Cells: return "cells";
    }
    return "invalid";
}

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
        || !std::isfinite(settings_.worldFrequencyY)
        || !std::isfinite(settings_.worldDetailAmplitude)
        || !std::isfinite(settings_.worldGrainAmplitude)
        || !std::isfinite(settings_.cellularScale)
        || settings_.cellularScale <= 0.0
        || settings_.cellularScale > 4.0
        || !std::isfinite(settings_.edgeStructureAmplitude)
        || settings_.edgeStructureAmplitude < 0.0
        || settings_.edgeStructureAmplitude > 2.0
        || !isValid(settings_.scalarProfile)) {
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
    double worldDetail = 0.0;
    if (settings_.worldDetailAmplitude != 0.0) {
        const double tertiaryWorldPhase = tau * (
            (1.31 * settings_.worldFrequencyX + 0.47 * settings_.worldFrequencyY)
                * input.world.x
            + (-0.59 * settings_.worldFrequencyX + 1.17 * settings_.worldFrequencyY)
                * input.world.y
            + 0.43);
        const double quaternaryWorldPhase = tau * (
            (-1.73 * settings_.worldFrequencyX + 0.29 * settings_.worldFrequencyY)
                * input.world.x
            + (0.41 * settings_.worldFrequencyX + 1.53 * settings_.worldFrequencyY)
                * input.world.y
            + 0.71);
        worldDetail = (
            std::sin(worldPhase)
            + 0.63 * std::cos(secondaryWorldPhase)
            + 0.41 * std::sin(tertiaryWorldPhase)
            + 0.28 * std::cos(quaternaryWorldPhase)) / 2.32;
    }
    double worldGrain = 0.0;
    if (settings_.worldGrainAmplitude != 0.0) {
        worldGrain = (
            std::sin(5.3 * worldPhase + 0.8 * std::sin(1.7 * secondaryWorldPhase))
            + 0.5 * std::cos(7.1 * secondaryWorldPhase - 0.35 * worldPhase)
            + 0.25 * std::sin(13.7 * worldPhase + 0.6 * secondaryWorldPhase))
            / 1.75;
    }
    const TileVariationDescriptor tile = describeTile(input.tileSeed);
    double edgeStructure = 0.0;
    if (settings_.edgeStructureAmplitude != 0.0) {
        edgeStructure = settings_.edgeStructureAmplitude
            * edgeConnectedInk(input.parameter, input.edgeColors, tile);
    }
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
    const double value = evaluateBase(warpedParameter)
        + settings_.tileVariationAmplitude
            * window
            * tileVariation(input.parameter, tile)
        + settings_.worldModulationAmplitude * std::sin(worldPhase)
        + settings_.worldDetailAmplitude * worldDetail
        + settings_.worldGrainAmplitude * worldGrain
        + edgeStructure;
    if (settings_.scalarProfile == ScalarProfile::Cells) {
        return std::clamp(
            cellularField(input.world, settings_.cellularScale)
                + 0.12 * std::tanh(value),
            -1.0,
            1.0);
    }
    return applyScalarProfile(value, settings_.scalarProfile);
}

double HybridTorusGenerator::edgeConnectedInk(
    const math::Vec2 parameter,
    const std::array<std::uint32_t, 4>& edgeColors,
    const TileVariationDescriptor& descriptor) noexcept {
    constexpr std::size_t south = 0;
    constexpr std::size_t north = 1;
    constexpr std::size_t west = 2;
    constexpr std::size_t east = 3;
    const std::array<math::Vec2, 4> endpoints{{
        {edgeInkCenter(edgeColors[south]), 0.0},
        {edgeInkCenter(edgeColors[north]), 1.0},
        {0.0, edgeInkCenter(edgeColors[west])},
        {1.0, edgeInkCenter(edgeColors[east])},
    }};
    const std::array<math::Vec2, 4> inwardDirections{{
        {0.0, 1.0},
        {0.0, -1.0},
        {1.0, 0.0},
        {-1.0, 0.0},
    }};
    const std::array<double, 4> widths{{
        edgeInkWidth(edgeColors[south]),
        edgeInkWidth(edgeColors[north]),
        edgeInkWidth(edgeColors[west]),
        edgeInkWidth(edgeColors[east]),
    }};
    std::array<std::size_t, 4> pairs{};
    switch ((descriptor.frequencyU[0] + descriptor.frequencyV[1]) % 3) {
    case 0: pairs = {south, north, west, east}; break;
    case 1: pairs = {south, west, north, east}; break;
    default: pairs = {south, east, north, west}; break;
    }
    const auto controlPoint = [&endpoints, &inwardDirections](
                                  const std::size_t endpoint,
                                  const double reach) noexcept {
        return math::Vec2{
            endpoints[endpoint].x + reach * inwardDirections[endpoint].x,
            endpoints[endpoint].y + reach * inwardDirections[endpoint].y,
        };
    };
    const auto boundaryStroke = [](const std::uint32_t color,
                                   const double tangent,
                                   const double inward) noexcept {
        const double reach = 1.0 - smoothUnit(inward / 0.20);
        return reach * softBump(
            tangent,
            edgeInkCenter(color),
            edgeInkWidth(color));
    };
    const double firstStartReach = 0.25 + 0.06 * std::sin(descriptor.phase[0]);
    const double firstEndReach = 0.25 + 0.06 * std::cos(descriptor.phase[1]);
    const double secondStartReach = 0.25 + 0.06 * std::sin(descriptor.phase[2]);
    const double secondEndReach = 0.25 + 0.06 * std::cos(descriptor.phase[3]);
    const double boundaryInk
        = boundaryStroke(edgeColors[south], parameter.x, parameter.y)
        + boundaryStroke(edgeColors[north], parameter.x, 1.0 - parameter.y)
        + boundaryStroke(edgeColors[west], parameter.y, parameter.x)
        + boundaryStroke(edgeColors[east], parameter.y, 1.0 - parameter.x);
    const double interiorInk = boundaryWindow(parameter) * (
        cubicInk(
            parameter,
            endpoints[pairs[0]],
            controlPoint(pairs[0], firstStartReach),
            controlPoint(pairs[1], firstEndReach),
            endpoints[pairs[1]],
            widths[pairs[0]],
            widths[pairs[1]])
        + cubicInk(
            parameter,
            endpoints[pairs[2]],
            controlPoint(pairs[2], secondStartReach),
            controlPoint(pairs[3], secondEndReach),
            endpoints[pairs[3]],
            widths[pairs[2]],
            widths[pairs[3]]));
    const double coverage = smoothUnit(
        std::clamp(boundaryInk + interiorInk, 0.0, 1.0));
    // 共享边颜色决定入口与线宽；端点法向固定，跨 tile 后路径保持顺滑。
    return 0.58 - 1.48 * coverage;
}

double HybridTorusGenerator::applyScalarProfile(
    const double value,
    const ScalarProfile profile) noexcept {
    const double bounded = std::clamp(value, -1.0, 1.0);
    switch (profile) {
    case ScalarProfile::Ridges:
        return 1.0 - 2.0 * std::abs(bounded);
    case ScalarProfile::Cells:
        return value;
    case ScalarProfile::Natural:
        return value;
    }
    return value;
}

double HybridTorusGenerator::cellularField(
    const math::Vec2 world,
    const double scale) noexcept {
    const math::Vec2 point{world.x * scale, world.y * scale};
    const int originX = static_cast<int>(std::floor(point.x));
    const int originY = static_cast<int>(std::floor(point.y));
    double nearest = 1.0e30;
    double secondNearest = 1.0e30;
    int nearestCellX = originX;
    int nearestCellY = originY;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const int cellX = originX + offsetX;
            const int cellY = originY + offsetY;
            const double featureX = static_cast<double>(cellX) + 0.12
                + 0.76 * unitFloat24(hashCell(cellX, cellY, 0x68bc21ebU));
            const double featureY = static_cast<double>(cellY) + 0.12
                + 0.76 * unitFloat24(hashCell(cellX, cellY, 0x02e5be93U));
            const double dx = featureX - point.x;
            const double dy = featureY - point.y;
            const double distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < nearest) {
                secondNearest = nearest;
                nearest = distanceSquared;
                nearestCellX = cellX;
                nearestCellY = cellY;
            } else if (distanceSquared < secondNearest) {
                secondNearest = distanceSquared;
            }
        }
    }
    const double separation = std::sqrt(secondNearest) - std::sqrt(nearest);
    const double transition = std::clamp((separation - 0.018) / 0.12, 0.0, 1.0);
    const double edgeBlend = transition * transition * (3.0 - 2.0 * transition);
    const double regionValue = -0.70 + 1.40 * unitFloat24(
        hashCell(nearestCellX, nearestCellY, 0xa511e9b3U));
    // 胞元身份给区域分配不同色阶；在中轴边界统一收束到深色，避免硬跳变。
    return -0.95 + edgeBlend * (regionValue + 0.95);
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
