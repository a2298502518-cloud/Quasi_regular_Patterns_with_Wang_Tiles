#include "project/PatternProject.hpp"

#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace qrp::project {
namespace {

[[nodiscard]] std::vector<math::EdgeParameters> defaultEdgeParameters() {
    std::vector<math::EdgeParameters> result;
    const auto palette = model::EdgePalette::createDefault();
    for (const auto& edge : palette.colors()) {
        result.push_back(edge.parameters());
    }
    return result;
}

[[nodiscard]] bool validGenerator(
    const generators::HybridGeneratorSettings& settings) noexcept {
    return std::isfinite(settings.fourierWeight)
        && std::isfinite(settings.noiseWeight)
        && std::isfinite(settings.tileVariationAmplitude)
        && std::isfinite(settings.tileDomainWarpAmplitude)
        && std::isfinite(settings.worldModulationAmplitude)
        && std::isfinite(settings.worldDomainWarpAmplitude)
        && std::isfinite(settings.worldFrequencyX)
        && std::isfinite(settings.worldFrequencyY)
        && std::isfinite(settings.worldDetailAmplitude)
        && std::isfinite(settings.worldGrainAmplitude)
        && std::isfinite(settings.cellularScale)
        && settings.cellularScale > 0.0
        && settings.cellularScale <= 4.0
        && generators::isValid(settings.scalarProfile);
}

[[nodiscard]] PatternScene placeholderScene() {
    const auto edgePalette = model::EdgePalette::createDefault();
    return {
        edgePalette,
        model::WangGrid(1, 1, static_cast<std::uint32_t>(edgePalette.colors().size()), 0),
        generators::HybridTorusGenerator(
            generators::TorusFourier::createQuasiRegular(),
            generators::PeriodicGradientNoise{}),
        color::GradientPalette::createMidnightGold(),
        {},
    };
}

} // namespace

PatternConfiguration configurationFromPreset(const presets::PatternPreset& preset) {
    return {
        preset.name,
        preset.gridWidth,
        preset.gridHeight,
        preset.pixelsPerTile,
        preset.gridSeed,
        defaultEdgeParameters(),
        preset.generator,
        preset.palette.stops(),
        preset.palette.tone(),
        preset.material,
    };
}

PatternProject::PatternProject(PatternConfiguration initial)
    : draft_(std::move(initial)),
      committed_(draft_),
      scene_(placeholderScene()) {
    auto [candidate, result] = buildScene(committed_, revision_);
    if (!candidate) {
        throw std::invalid_argument("Invalid initial pattern configuration: " + result.message);
    }
    scene_ = std::move(*candidate);
    lastApplyResult_ = std::move(result);
}

PatternConfiguration& PatternProject::draft() noexcept {
    return draft_;
}

const PatternConfiguration& PatternProject::draft() const noexcept {
    return draft_;
}

const PatternConfiguration& PatternProject::committed() const noexcept {
    return committed_;
}

const PatternScene& PatternProject::scene() const noexcept {
    return scene_;
}

std::uint64_t PatternProject::revision() const noexcept {
    return revision_;
}

bool PatternProject::isDirty() const noexcept {
    return draft_ != committed_;
}

const ApplyResult& PatternProject::lastApplyResult() const noexcept {
    return lastApplyResult_;
}

void PatternProject::loadPresetIntoDraft(const presets::PatternPreset& preset) {
    draft_ = configurationFromPreset(preset);
}

void PatternProject::resetDraft() {
    draft_ = committed_;
    lastApplyResult_.message = "Draft reset to the committed revision.";
}

ApplyResult PatternProject::applyDraft() {
    if (!isDirty()) {
        lastApplyResult_.applied = false;
        lastApplyResult_.revision = revision_;
        lastApplyResult_.message = "Draft already matches the committed revision.";
        return lastApplyResult_;
    }

    auto [candidate, result] = buildScene(draft_, revision_ + 1);
    if (!candidate) {
        lastApplyResult_ = result;
        return lastApplyResult_;
    }

    committed_ = draft_;
    scene_ = std::move(*candidate);
    revision_ = result.revision;
    lastApplyResult_ = result;
    return lastApplyResult_;
}

std::pair<std::optional<PatternScene>, ApplyResult> PatternProject::buildScene(
    const PatternConfiguration& configuration,
    const std::uint64_t revision) {
    ApplyResult result;
    result.revision = revision;

    if (configuration.gridWidth == 0 || configuration.gridHeight == 0
        || configuration.gridWidth > 128 || configuration.gridHeight > 128) {
        result.message = "Grid dimensions must be within 1..128.";
        return {std::nullopt, result};
    }
    if (configuration.pixelsPerTile < 12 || configuration.pixelsPerTile > 720) {
        result.message = "Pixels per tile must be within 12..720.";
        return {std::nullopt, result};
    }
    if (configuration.edgeColors.empty() || configuration.edgeColors.size() > 16) {
        result.message = "The edge palette must contain 1..16 colors.";
        return {std::nullopt, result};
    }
    if (configuration.colorStops.size() < 2 || configuration.colorStops.size() > 8) {
        result.message = "The color ramp must contain 2..8 stops.";
        return {std::nullopt, result};
    }
    if (!validGenerator(configuration.generator)) {
        result.message = "Generator settings must be finite and use a recognized profile.";
        return {std::nullopt, result};
    }
    if (!color::isValid(configuration.material)) {
        result.message = "Material settings are outside their safe ranges.";
        return {std::nullopt, result};
    }

    try {
        std::vector<math::EdgeFunction> edges;
        edges.reserve(configuration.edgeColors.size());
        for (const auto parameters : configuration.edgeColors) {
            edges.emplace_back(parameters);
        }
        model::EdgePalette edgePalette(std::move(edges));
        const auto safety = edgePalette.validateAllCombinations();
        result.minimumDeterminantLowerBound = safety.minimumDeterminantLowerBound;
        if (!safety.valid) {
            result.message = safety.message;
            return {std::nullopt, result};
        }

        PatternScene scene{
            edgePalette,
            model::WangGrid(
                configuration.gridWidth,
                configuration.gridHeight,
                static_cast<std::uint32_t>(edgePalette.colors().size()),
                configuration.gridSeed),
            generators::HybridTorusGenerator(
                generators::TorusFourier::createQuasiRegular(),
                generators::PeriodicGradientNoise{},
                configuration.generator),
            color::GradientPalette(configuration.colorStops, configuration.tone),
            configuration.material,
        };
        result.applied = true;
        result.message = "Committed after validating every edge-color combination.";
        return {std::move(scene), result};
    } catch (const std::exception& error) {
        result.message = error.what();
        return {std::nullopt, result};
    }
}

} // namespace qrp::project
