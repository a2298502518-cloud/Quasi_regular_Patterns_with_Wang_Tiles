#include "QrpSourceExport.hpp"
#include <ostream>

namespace qrp::study {
using model::PhaseCompatibleQrpTiles;

void writeQrpSource(const PhaseCompatibleQrpTiles& tiles, std::ostream& out) {
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
} // namespace qrp::study
