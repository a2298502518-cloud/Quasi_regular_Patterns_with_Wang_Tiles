#include "ReusableTileStudy.hpp"
#include "TileStudyExport.hpp"

namespace qrp::study {
namespace {
using model::PhaseCompatibleQrpTiles;

void exportPhaseStudy(const TileStudyRun& run, const std::filesystem::path& directory) {
    TileStudyOutput output(run, directory, "qrp-phase-compatible-tiles-v1");
    for (const auto& recipe : run.cases) {
        auto parameters = phaseParameters(recipe.parent, run.sourceSpan);
        // 内部状态也是配方输入；执行层不根据实验名称引入额外参数。
        parameters.vertexPhaseOffsets = run.parentPhaseOffsets;
        const PhaseCompatibleQrpTiles first(parameters);
        std::optional<PhaseCompatibleQrpTiles> second;
        if (recipe.child) second.emplace(phaseParameters(*recipe.child, run.sourceSpan));
        output.writeCase(recipe, first, second, run.includeReferences);
    }
    output.finish();
}
} // namespace

void runReusableTileStudy(const std::vector<TileStudyRun>& runs, const std::filesystem::path& directory) {
    for (const auto& run : runs) exportPhaseStudy(run, directory / run.subdirectory);
}

} // namespace qrp::study
