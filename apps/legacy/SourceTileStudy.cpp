#include "SourceTileStudy.hpp"
#include "TileStudyExport.hpp"
#include "model/ReusableQrpTiles.hpp"

#include <stdexcept>

namespace {
using namespace qrp::study;
using qrp::model::ReusableQrpTiles;
using qrp::model::QrpSourceSelection;

void writeSources(const ReusableQrpTiles& tiles, std::ostream& out) {
    const auto writeArray = [&](const auto& values) {
        out << '[';
        for (std::size_t i = 0; i < values.size(); ++i) out << (i ? "," : "") << '[' << values[i].x << ',' << values[i].y << ']';
        out << ']';
    };
    const auto& s = tiles.sources();
    const auto& p = tiles.parameters().qrp;
    const auto& f = tiles.fit();
    out << "{\"q\":" << p.resonanceCount << ",\"frequency\":" << p.spatialFrequency
        << ",\"bias\":" << p.directionalBias << ",\"angle_radians\":" << p.orientationRadians
        << ",\"phase_a\":" << p.globalPhase.x << ",\"phase_b\":" << p.globalPhase.y
        << ",\"common_phase\":" << p.commonPhase << ",\"cross_mix\":" << p.crossMix
        << ",\"vertices\":";
    writeArray(s.vertices);
    out << ",\"vertical_edges\":"; writeArray(s.verticalEdges);
    out << ",\"horizontal_edges\":"; writeArray(s.horizontalEdges);
    out << ",\"interiors\":"; writeArray(s.interiors);
    out << ",\"fit\":{\"edge_fixed\":" << f.edgeFixed << ",\"edge_selected\":" << f.edgeSelected
        << ",\"interior_fixed\":" << f.interiorFixed << ",\"interior_selected\":" << f.interiorSelected << "}}";
}

} // namespace

int runSourceTileStudy(const int argc, char** argv) {
    if (argc > 4) throw std::invalid_argument("Source study accepts directory and pixels/tile.");
    const std::filesystem::path directory = argc > 2 ? argv[2] : "output/qrp-tile-study";
    std::size_t pixels = 96;
    if (argc > 3) {
        const std::string argument(argv[3]);
        std::size_t consumed = 0;
        const int value = std::stoi(argument, &consumed);
        if (consumed != argument.size() || value < 64 || value > 192) {
            throw std::invalid_argument("Tile pixels must be in [64,192].");
        }
        pixels = static_cast<std::size_t>(value);
    }
    const TileStudyRun run{"", "sources", 8, pixels, 4.0, true, "", 0.0, 0.0, {}};
    TileStudyOutput output(run, directory, "qrp-edge-source-tiles-v1");
    for (const auto& recipe : basicTileCases()) {
        for (const auto policy : {QrpSourceSelection::Fixed, QrpSourceSelection::BoundaryMatched}) {
            qrp::model::ReusableQrpTileParameters p;
            p.sourceSpan = run.sourceSpan;
            p.qrp = phaseParameters(recipe.parent, run.sourceSpan).qrp;
            p.selection = policy;
            const ReusableQrpTiles first(p);
            std::optional<ReusableQrpTiles> second;
            if (recipe.child) {
                p.qrp = phaseParameters(*recipe.child, run.sourceSpan).qrp;
                second.emplace(p);
            }
            const bool fixed = policy == QrpSourceSelection::Fixed;
            output.writeCase(recipe, first, second, {fixed ? "fixed" : "matched", fixed, false}, writeSources);
        }
    }
    output.finish(",\"transition_width\":0.2,\"source_search\":{\"integer_offset_min\":-6,"
        "\"integer_offset_max\":6,\"gradient_weight\":0.1}");
    return 0;
}
