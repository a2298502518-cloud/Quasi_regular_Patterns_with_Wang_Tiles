#pragma once

#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "generators/TorusGenerator.hpp"

namespace qrp::generators {

struct HybridGeneratorSettings {
    double fourierWeight = 0.85;
    double noiseWeight = 0.30;
    double tileVariationAmplitude = 0.22;
    double tileDomainWarpAmplitude = 0.12;
    double worldModulationAmplitude = 0.12;
    double worldDomainWarpAmplitude = 0.08;
    double worldFrequencyX = 0.137;
    double worldFrequencyY = 0.091;
};

class HybridTorusGenerator final : public TorusGenerator {
public:
    HybridTorusGenerator(
        TorusFourier fourier,
        PeriodicGradientNoise noise,
        HybridGeneratorSettings settings = {});

    [[nodiscard]] const HybridGeneratorSettings& settings() const noexcept;
    [[nodiscard]] double evaluateBase(math::Vec2 parameter) const noexcept;
    [[nodiscard]] double evaluate(const GeneratorInput& input) const noexcept override;

private:
    [[nodiscard]] static double tileVariation(
        math::Vec2 parameter,
        std::uint64_t tileSeed) noexcept;
    [[nodiscard]] static math::Vec2 tileDomainOffset(
        math::Vec2 parameter,
        std::uint64_t tileSeed) noexcept;

    TorusFourier fourier_;
    PeriodicGradientNoise noise_;
    HybridGeneratorSettings settings_;
};

} // namespace qrp::generators
