#include "QrpSourceExport.hpp"
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace qrp;
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: qrp_export_sources [phase_a phase_b [harmonic_order [offset_x offset_y]]]" << std::endl;
        return 0;
    }
    if (argc != 1 && argc != 3 && argc != 4 && argc != 6) {
        std::cerr << "Expected 0, 2, 3 or 5 numeric arguments; use --help." << std::endl;
        return 1;
    }
    try {
        std::cout << std::setprecision(17) << "[";
        bool first = true;
        for (const auto q : {5U, 8U, 12U}) {
            model::PhaseCompatibleQrpTileParameters p;
            p.qrp.resonanceCount = q;
            p.qrp.spatialFrequency = 2.0;
            if (argc > 2) p.qrp.globalPhase = {std::stod(argv[1]),std::stod(argv[2])};
            if (argc > 3) p.qrp.phaseHarmonicOrder = std::stoul(argv[3]);
            p.sourceSpan = 16.0;
            p.vertexOffsets = {{{-8.0,-8.0},{-7.3,-7.9}}};
            if (argc > 5) {
                const math::Vec2 patch{std::stod(argv[4]),std::stod(argv[5])};
                p.vertexOffsets = {{patch,patch}};
            }
            const model::PhaseCompatibleQrpTiles tiles(p);
            std::cout << (first ? "" : ",") << "{\"id\":\"design_q" << q
                << "\",\"relation\":\"direct\",\"channels\":[";
            study::writeQrpSource(tiles, std::cout);
            std::cout << "],\"cpp_oracle\":[";
            bool sampleFirst = true;
            for (const auto id : {0U,6U,15U}) {
                for (const math::Vec2 xy : {math::Vec2{.173,.621},math::Vec2{.37,.81}}) {
                    const auto closed = tiles.evaluate(id,xy);
                    const auto raw = tiles.source(xy,p.vertexOffsets[0]);
                    std::cout << (sampleFirst ? "" : ",") << "{\"tile\":" << id
                        << ",\"xy\":[" << xy.x << ',' << xy.y << "],\"raw\":[" << raw.value
                        << ',' << raw.gradient.x << ',' << raw.gradient.y << "],\"closed\":["
                        << closed.value << ',' << closed.gradient.x << ',' << closed.gradient.y << "]}";
                    sampleFirst = false;
                }
            }
            std::cout << "]}";
            first = false;
        }
        std::cout << "]\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Source export failed: " << error.what() << std::endl;
        return 1;
    }
}
