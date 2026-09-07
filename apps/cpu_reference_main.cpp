#include "color/GradientPalette.hpp"
#include "export/PpmWriter.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "presets/PatternPreset.hpp"
#include "render/CpuReferenceRenderer.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {

void renderPreset(
    const qrp::presets::PatternPreset& preset,
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
        const auto presets = qrp::presets::createBaselinePresets();

        for (const auto& preset : presets) {
            renderPreset(preset, edgePalette, outputDirectory);
        }
    } catch (const std::exception& error) {
        std::cerr << "CPU reference generation failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
