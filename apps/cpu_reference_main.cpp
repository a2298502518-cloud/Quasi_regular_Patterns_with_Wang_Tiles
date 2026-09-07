#include "color/GradientPalette.hpp"
#include "export/PpmWriter.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "render/CpuReferenceRenderer.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct BaselinePreset {
    std::string_view name;
    std::size_t gridWidth = 10;
    std::size_t gridHeight = 10;
    std::uint32_t pixelsPerTile = 72;
    std::uint64_t gridSeed = 0;
    qrp::generators::HybridGeneratorSettings generator;
    qrp::color::GradientPalette palette;
};

void renderPreset(
    const BaselinePreset& preset,
    const qrp::model::EdgePalette& edgePalette,
    const std::filesystem::path& outputDirectory) {
    const qrp::model::WangGrid grid(
        preset.gridWidth,
        preset.gridHeight,
        static_cast<std::uint32_t>(edgePalette.colors().size()),
        preset.gridSeed);
    const qrp::generators::HybridTorusGenerator generator(
        qrp::generators::TorusFourier::createQuasiRegular(),
        qrp::generators::PeriodicGradientNoise(qrp::generators::PeriodicNoiseSettings{}),
        preset.generator);

    const auto start = std::chrono::steady_clock::now();
    const auto render = qrp::render::CpuReferenceRenderer::render(
        grid,
        edgePalette,
        generator,
        preset.palette,
        qrp::render::CpuRenderSettings{preset.pixelsPerTile, {}});
    const auto seams = qrp::render::CpuReferenceRenderer::measureSeams(
        grid,
        edgePalette,
        generator,
        preset.palette,
        513);
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    const std::filesystem::path path = outputDirectory
        / (std::string(preset.name) + ".ppm");
    qrp::exporting::writePpm(render.image, path);
    if (preset.gridWidth > 1 || preset.gridHeight > 1) {
        const auto heatmap = qrp::render::CpuReferenceRenderer::renderSeamHeatmap(
            grid,
            edgePalette,
            generator,
            preset.palette,
            qrp::render::SeamHeatmapSettings{preset.pixelsPerTile, 2, 1.0e14});
        qrp::exporting::writePpm(
            heatmap,
            outputDirectory / (std::string(preset.name) + "_seams_x1e14.ppm"));
    }

    std::cout << preset.name << ": "
              << render.image.width() << 'x' << render.image.height()
              << ", render=" << std::fixed << std::setprecision(3) << elapsed << " s"
              << ", inverse_failures=" << render.diagnostics.inverseFailureCount
              << ", max_inverse_residual=" << std::scientific
              << render.diagnostics.maximumInverseResidual
              << ", min_det=" << render.diagnostics.minimumDeterminant
              << ", max_scalar_seam=" << seams.maximumScalarDifference
              << ", max_color_seam=" << seams.maximumColorDifference
              << ", max_8bit_seam="
              << static_cast<unsigned int>(seams.maximumQuantizedChannelDifference)
              << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/cpu");
        std::filesystem::create_directories(outputDirectory);

        const auto edgePalette = qrp::model::EdgePalette::createDefault();
        const std::vector<BaselinePreset> presets{
            {
                "single_qrp",
                1,
                1,
                384,
                0x1199aacc55ee7711ULL,
                {1.10, 0.08, 0.08, 0.04, 0.04, 0.03, 0.137, 0.091},
                qrp::color::GradientPalette::createMidnightGold(),
            },
            {
                "neighbors_2x2",
                2,
                2,
                192,
                0x90ab12cd34ef5678ULL,
                {0.90, 0.25, 0.18, 0.09, 0.08, 0.24, 0.137, 0.091},
                qrp::color::GradientPalette::createAurora(),
            },
            {
                "tiling_qrp_10x10",
                10,
                10,
                72,
                0xdecafbad98765432ULL,
                {1.05, 0.15, 0.16, 0.10, 0.12, 0.28, 0.137, 0.091},
                qrp::color::GradientPalette::createMidnightGold(),
            },
            {
                "tiling_mineral_10x10",
                10,
                10,
                72,
                0xa17e5c4962bd308fULL,
                {0.34, 1.18, 0.20, 0.11, 0.16, 0.34, 0.083, -0.119},
                qrp::color::GradientPalette::createMineral(),
            },
            {
                "tiling_aurora_10x10",
                10,
                10,
                72,
                0x6712e4ad09bc53f8ULL,
                {0.70, 0.68, 0.18, 0.10, 0.18, 0.32, -0.101, 0.073},
                qrp::color::GradientPalette::createAurora(),
            },
        };

        for (const auto& preset : presets) {
            renderPreset(preset, edgePalette, outputDirectory);
        }
    } catch (const std::exception& error) {
        std::cerr << "CPU reference generation failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
