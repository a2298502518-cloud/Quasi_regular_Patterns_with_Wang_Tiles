#pragma once

#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "generators/TorusGenerator.hpp"

#include <array>

namespace qrp::generators {

enum class ScalarProfile : int {
    Natural = 0,
    Ridges = 1,
    Cells = 2,
};

[[nodiscard]] bool isValid(ScalarProfile profile) noexcept;
[[nodiscard]] const char* scalarProfileName(ScalarProfile profile) noexcept;

struct HybridGeneratorSettings {
    double fourierWeight = 0.85;
    double noiseWeight = 0.30;
    double tileVariationAmplitude = 0.22;
    double tileDomainWarpAmplitude = 0.12;
    double worldModulationAmplitude = 0.12;
    double worldDomainWarpAmplitude = 0.08;
    double worldFrequencyX = 0.137;
    double worldFrequencyY = 0.091;
    double worldDetailAmplitude = 0.0;
    ScalarProfile scalarProfile = ScalarProfile::Natural;

    [[nodiscard]] bool operator==(const HybridGeneratorSettings&) const noexcept = default;
};

struct TileVariationDescriptor {
    double firstDomainPhase = 0.0;
    double secondDomainPhase = 0.0;
    std::array<int, 4> frequencyU{};
    std::array<int, 4> frequencyV{};
    std::array<double, 4> phase{};
};

class HybridTorusGenerator final : public TorusGenerator {
public:
    HybridTorusGenerator(
        TorusFourier fourier,
        PeriodicGradientNoise noise,
        HybridGeneratorSettings settings = {});

    [[nodiscard]] const HybridGeneratorSettings& settings() const noexcept;
    [[nodiscard]] const TorusFourier& fourier() const noexcept;
    [[nodiscard]] const PeriodicGradientNoise& noise() const noexcept;
    [[nodiscard]] static TileVariationDescriptor describeTile(
        std::uint64_t tileSeed) noexcept;
    [[nodiscard]] double evaluateBase(math::Vec2 parameter) const noexcept;
    [[nodiscard]] double evaluate(const GeneratorInput& input) const noexcept override;

private:
    [[nodiscard]] static double applyScalarProfile(
        double value,
        ScalarProfile profile) noexcept;
    [[nodiscard]] static double tileVariation(
        math::Vec2 parameter,
        const TileVariationDescriptor& descriptor) noexcept;
    [[nodiscard]] static math::Vec2 tileDomainOffset(
        math::Vec2 parameter,
        const TileVariationDescriptor& descriptor) noexcept;

    TorusFourier fourier_;
    PeriodicGradientNoise noise_;
    HybridGeneratorSettings settings_;
};

} // namespace qrp::generators
