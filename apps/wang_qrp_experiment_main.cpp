#include "ReusableTileStudy.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void usage() {
    std::cout << "Usage: qrp_wang_qrp_experiment --study phase|organization|retiling [directory] [pixels/tile]\n"
              << "       qrp_wang_qrp_experiment --help\n"
              << "Current path: QRP phase-compatible fixed tiles -> legal Wang layouts.\n"
              << "Historical global/source studies: enable QRP_BUILD_LEGACY_EXPERIMENTS, then use qrp_legacy_experiment.\n";
}
} // namespace

int main(const int argc, char** argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
            usage();
            return 0;
        }
        std::string study;
        int directoryIndex = 2;
        const std::string_view mode(argv[1]);
        if (mode == "--study" && argc > 2) {
            study = argv[2];
            directoryIndex = 3;
        } else if (mode == "--phase-tile-study") study = "phase";
        else if (mode == "--organization-tile-study") study = "organization";
        else if (mode == "--retiling-study") study = "retiling";
        else throw std::invalid_argument("Unknown command; use --help. Historical studies moved to qrp_legacy_experiment.");
        if (argc > directoryIndex + 2) throw std::invalid_argument("Studies accept directory and pixels/tile only.");
        std::optional<std::size_t> pixels;
        if (argc > directoryIndex + 1) {
            const std::string text(argv[directoryIndex + 1]);
            std::size_t consumed = 0;
            const int value = std::stoi(text, &consumed);
            if (consumed != text.size() || value <= 0) throw std::invalid_argument("Expected a positive integer pixel count.");
            pixels = static_cast<std::size_t>(value);
        }
        const auto runs = qrp::study::tileStudyRuns(study, pixels);
        const std::filesystem::path directory = argc > directoryIndex ? argv[directoryIndex]
            : "output/qrp-" + study + (study == "retiling" ? "-study" : "-tile-study");
        qrp::study::runReusableTileStudy(runs, directory);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Wang-QRP experiment failed: " << error.what() << '\n';
        return 1;
    }
}
