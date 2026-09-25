#include "ReusableTileStudy.hpp"
#include "TileStudyExport.hpp"

namespace qrp::study {
namespace {
using model::PhaseCompatibleQrpTiles;

void writeSources(const PhaseCompatibleQrpTiles& tiles, std::ostream& out) {
    const auto& p = tiles.parameters().qrp;
    const auto certificate = tiles.bandCertificate();
    out << "{\"q\":" << p.resonanceCount << ",\"frequency\":" << p.spatialFrequency
        << ",\"bias\":" << p.directionalBias << ",\"angle_radians\":" << p.orientationRadians
        << ",\"phase_a\":" << p.globalPhase.x << ",\"phase_b\":" << p.globalPhase.y
        << ",\"phase_harmonic_order\":" << p.phaseHarmonicOrder
        << ",\"common_phase\":" << p.commonPhase << ",\"cross_mix\":" << p.crossMix << ",\"vertices\":[";
    const auto& vertices = tiles.parameters().vertexOffsets;
    const auto& phases = tiles.parameters().vertexPhaseOffsets;
    out << '[' << vertices[0].x << ',' << vertices[0].y << "],[" << vertices[1].x << ',' << vertices[1].y
        << "]],\"vertex_phase_offsets\":[[" << phases[0].x << ',' << phases[0].y << "],["
        << phases[1].x << ',' << phases[1].y << "]],\"modes\":[";
    bool first = true;
    for (const auto& mode : tiles.modes()) {
        out << (first ? "" : ",") << "{\"wave\":[" << mode.wave.x << ',' << mode.wave.y
            << "],\"winding\":[" << mode.winding.x << ',' << mode.winding.y
            << "],\"closure\":[" << mode.closure.x << ',' << mode.closure.y
            << "],\"amplitude\":" << mode.amplitude << ",\"phase\":" << mode.phase
            << ",\"vertex_shift\":" << mode.vertexShift << '}';
        first = false;
    }
    out << "],\"band_certificate\":{\"dominant_weight\":" << certificate.dominantWeight
        << ",\"remainder_weight\":" << certificate.remainderWeight
        << ",\"axis\":[" << certificate.axis.x << ',' << certificate.axis.y
        << "],\"minimum_phase_slope\":" << certificate.minimumPhaseSlope
        << ",\"signed_core_bands\":" << (certificate.signedCoreBands ? "true" : "false") << "}}";
}
void exportPhaseStudy(const TileStudyRun& run, const std::filesystem::path& directory) {
    TileStudyOutput output(run, directory, "qrp-phase-compatible-tiles-v1");
    for (const auto& recipe : run.cases) {
        auto parameters = phaseParameters(recipe.parent, run.sourceSpan);
        // 内部状态也是配方输入；执行层不根据实验名称引入额外参数。
        parameters.vertexPhaseOffsets = run.parentPhaseOffsets;
        const PhaseCompatibleQrpTiles first(parameters);
        std::optional<PhaseCompatibleQrpTiles> second;
        if (recipe.child) second.emplace(phaseParameters(*recipe.child, run.sourceSpan));
        output.writeCase(recipe, first, second, {"phase", run.includeReferences, true}, writeSources);
    }
    output.finish();
}
} // namespace

void runReusableTileStudy(const std::vector<TileStudyRun>& runs, const std::filesystem::path& directory) {
    for (const auto& run : runs) exportPhaseStudy(run, directory / run.subdirectory);
}

} // namespace qrp::study
