#include "TileStudyRecipes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace qrp::study {
namespace {
using model::QrpChannelRelation;
constexpr double kQuarterTurn = 0.5 * std::numbers::pi_v<double>;
constexpr double kClosureErrorBudget = 0.25;

const std::array<TileCase, 8> kOrganizationCases{{
    {"parent", "低频父场", {1.15,0}, {}, QrpChannelRelation::Direct, false},
    {"child_rings", "细环形子场", {6.9,0}, {}, QrpChannelRelation::Direct, false},
    {"child_stripes", "条纹子场", {6.9,1.5}, {}, QrpChannelRelation::Direct, false},
    {"stripe_parent", "带状父场", {1.15,1.5}, {}, QrpChannelRelation::Direct, false},
    {"child_cross", "横向子场", {6.9,1.5,kQuarterTurn}, {}, QrpChannelRelation::Direct, false},
    {"nested_rings", "分簇细环形", {1.15,0}, ChannelSpec{6.9,0}, QrpChannelRelation::Nested, true, "parent", "child_rings"},
    {"nested_stripes", "分簇条纹", {1.15,0}, ChannelSpec{6.9,1.5}, QrpChannelRelation::Nested, true, "parent", "child_stripes"},
    {"nested_cross", "带内横向短条", {1.15,1.5}, ChannelSpec{6.9,1.5,kQuarterTurn}, QrpChannelRelation::Nested, true, "stripe_parent", "child_cross"}
}};

double closureError(const std::span<const TileCase> cases, const double span) {
    double maximum = 0.0;
    const auto include = [&](const ChannelSpec& channel) {
        const model::PhaseCompatibleQrpTiles tiles(phaseParameters(channel, span));
        for (const auto& mode : tiles.modes()) {
            maximum = std::max(maximum, std::hypot(mode.closure.x, mode.closure.y)
                / std::hypot(mode.wave.x, mode.wave.y));
        }
    };
    for (const auto& c : cases) {
        include(c.parent);
        if (c.child) include(*c.child);
    }
    return maximum;
}
} // namespace

std::span<const TileCase> basicTileCases() {
    static const std::array<TileCase, 3> cases{{
        {"rings", "环形组织", {3.15,0}, {}, QrpChannelRelation::Direct},
        {"ribbons", "方向波带", {3.15,1.5}, {}, QrpChannelRelation::Direct},
        {"product", "双通道格纹", {3.15,1.5}, ChannelSpec{3.15,1.5,kQuarterTurn}, QrpChannelRelation::Product}
    }};
    return cases;
}

model::PhaseCompatibleQrpTileParameters phaseParameters(const ChannelSpec& channel, const double sourceSpan) {
    model::PhaseCompatibleQrpTileParameters p;
    p.sourceSpan = sourceSpan;
    p.qrp.spatialFrequency = channel.frequency;
    p.qrp.directionalBias = channel.bias;
    p.qrp.orientationRadians = channel.angle;
    return p;
}

std::vector<TileStudyRun> tileStudyRuns(const std::string& study, const std::optional<std::size_t> pixels) {
    if (study != "phase" && study != "organization" && study != "retiling") {
        throw std::invalid_argument("Unknown study. Choose phase, organization or retiling.");
    }
    const std::size_t maximum = study == "retiling" ? 768 : 192;
    if (pixels && (*pixels < 64 || *pixels > maximum)) {
        throw std::invalid_argument("Tile pixels must be in [64," + std::to_string(maximum) + "].");
    }
    const auto sourceCases = study == "phase" ? basicTileCases() : std::span<const TileCase>(kOrganizationCases);
    std::vector<TileCase> cases;
    for (const auto& c : sourceCases) {
        if (study != "retiling" || c.relation == QrpChannelRelation::Nested) cases.push_back(c);
    }
    double span = 4.0;
    if (study != "phase") {
        while (closureError(cases, span) > kClosureErrorBudget && span < 32.0) span *= 2.0;
        if (closureError(cases, span) > kClosureErrorBudget) {
            throw std::runtime_error("The fixed study window cannot meet the spectral closure budget.");
        }
    }
    const auto resolution = pixels.value_or(study == "retiling" ? static_cast<std::size_t>(24*span) : 96);
    TileStudyRun first{"", study, 8, resolution, study == "retiling" ? span : 4.0,
        study != "retiling", study == "retiling" ? "translation" : "", 0.0, 0.0, cases};
    if (study != "phase") {
        first.closureErrorBudget = kClosureErrorBudget;
        first.closureError = closureError(cases, first.sourceSpan);
    }
    auto second = first;
    if (study == "retiling") {
        second.subdirectory = "relative-phase";
        second.parentStateFamily = "relative-phase";
        second.parentPhaseOffsets = {{{-1.0,-0.5},{1.0,0.5}}};
    } else {
        // 相同源视野和像素密度，改变瓦片跨度时同步改变布局尺寸。
        second.subdirectory = study == "phase" ? "larger-tiles" : "resolved-scale";
        second.sourceSpan = study == "phase" ? 8.0 : span;
        second.size = static_cast<std::size_t>(32.0 / second.sourceSpan);
        second.pixels = 8 * resolution / second.size;
        if (study != "phase") second.closureError = closureError(cases, second.sourceSpan);
    }
    return {first, second};
}

} // namespace qrp::study
