#include "color/GradientPalette.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "presets/PatternPreset.hpp"
#include "project/PatternProject.hpp"
#include "render/CpuReferenceRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using qrp::generators::GeneratorInput;
using qrp::generators::HybridGeneratorSettings;
using qrp::generators::HybridTorusGenerator;
using qrp::generators::PeriodicGradientNoise;
using qrp::generators::PeriodicNoiseSettings;
using qrp::generators::TorusFourier;
using qrp::math::Vec2;

struct TestCase {
    std::string_view name;
    void (*function)();
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(
    const double actual,
    const double expected,
    const double tolerance,
    const std::string& label) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            label + ": expected " + std::to_string(expected)
            + ", got " + std::to_string(actual));
    }
}

[[nodiscard]] HybridTorusGenerator createGenerator(
    const HybridGeneratorSettings settings = {}) {
    return HybridTorusGenerator(
        TorusFourier::createQuasiRegular(),
        PeriodicGradientNoise(PeriodicNoiseSettings{}),
        settings);
}

void testTorusGeneratorsArePeriodic() {
    const TorusFourier fourier = TorusFourier::createQuasiRegular();
    const PeriodicGradientNoise noise(PeriodicNoiseSettings{});
    for (int index = 0; index <= 1000; ++index) {
        const double t = static_cast<double>(index) / 1000.0;
        requireNear(
            fourier.evaluate(Vec2{t, 0.0}),
            fourier.evaluate(Vec2{t, 1.0}),
            2.0e-14,
            "Fourier horizontal period");
        requireNear(
            fourier.evaluate(Vec2{0.0, t}),
            fourier.evaluate(Vec2{1.0, t}),
            2.0e-14,
            "Fourier vertical period");
        requireNear(
            noise.evaluate(Vec2{t, 0.0}),
            noise.evaluate(Vec2{t, 1.0}),
            1.0e-14,
            "noise horizontal period");
        requireNear(
            noise.evaluate(Vec2{0.0, t}),
            noise.evaluate(Vec2{1.0, t}),
            1.0e-14,
            "noise vertical period");
    }
}

void testTileVariationVanishesOnBoundary() {
    const auto generator = createGenerator();
    constexpr std::uint64_t firstSeed = 17;
    constexpr std::uint64_t secondSeed = 0xcafef00d12345678ULL;
    for (int index = 0; index <= 400; ++index) {
        const double t = static_cast<double>(index) / 400.0;
        for (const auto parameter : {
            Vec2{t, 0.0}, Vec2{t, 1.0}, Vec2{0.0, t}, Vec2{1.0, t}}) {
            const Vec2 world{3.0 + parameter.x, 5.0 + parameter.y};
            const double first = generator.evaluate(GeneratorInput{parameter, world, firstSeed});
            const double second = generator.evaluate(GeneratorInput{parameter, world, secondSeed});
            requireNear(first, second, 1.0e-15, "boundary variation");
        }
    }

    const Vec2 interior{0.37, 0.58};
    const Vec2 world{3.37, 5.58};
    const double first = generator.evaluate(GeneratorInput{interior, world, firstSeed});
    const double second = generator.evaluate(GeneratorInput{interior, world, secondSeed});
    require(std::abs(first - second) > 1.0e-6, "tile seeds should vary the interior");

    for (int index = 0; index <= 400; ++index) {
        const double t = static_cast<double>(index) / 400.0;
        const Vec2 sharedWorld{8.0 + t, 4.0};
        requireNear(
            generator.evaluate(GeneratorInput{Vec2{t, 0.0}, sharedWorld, firstSeed}),
            generator.evaluate(GeneratorInput{Vec2{t, 1.0}, sharedWorld, secondSeed}),
            2.0e-14,
            "hybrid horizontal period");
    }
}

void testGradientPalettesStayFiniteAndBounded() {
    const std::vector<qrp::color::GradientPalette> palettes{
        qrp::color::GradientPalette::createMidnightGold(),
        qrp::color::GradientPalette::createMineral(),
        qrp::color::GradientPalette::createAurora(),
    };
    for (const auto& palette : palettes) {
        for (int index = -1000; index <= 1000; ++index) {
            const auto color = palette.sample(static_cast<double>(index) / 100.0);
            require(std::isfinite(color.red), "palette red must be finite");
            require(std::isfinite(color.green), "palette green must be finite");
            require(std::isfinite(color.blue), "palette blue must be finite");
            require(color.red >= 0.0 && color.red <= 1.0, "palette red must be bounded");
            require(color.green >= 0.0 && color.green <= 1.0, "palette green must be bounded");
            require(color.blue >= 0.0 && color.blue <= 1.0, "palette blue must be bounded");
        }
    }
}

void testMeasuredSeams() {
    const auto edgePalette = qrp::model::EdgePalette::createDefault();
    const qrp::model::WangGrid grid(7, 5, 5, 0xdecafbad98765432ULL);
    const auto generator = createGenerator();
    const auto colorPalette = qrp::color::GradientPalette::createMidnightGold();
    const auto metrics = qrp::render::CpuReferenceRenderer::measureSeams(
        grid,
        edgePalette,
        generator,
        colorPalette,
        257);
    const std::size_t seamCount = (grid.width() - 1) * grid.height()
        + grid.width() * (grid.height() - 1);
    require(metrics.sampleCount == seamCount * 257, "seam sample count must cover every edge");
    require(metrics.inverseFailureCount == 0, "seam sampling must not fail inversion");
    require(metrics.maximumScalarDifference <= 1.0e-12, "scalar seam error exceeds target");
    require(metrics.maximumColorDifference <= 1.0e-12, "color seam error exceeds target");
    require(metrics.maximumQuantizedChannelDifference == 0, "8-bit seam must be exact");
}

void testCpuReferenceRender() {
    const auto edgePalette = qrp::model::EdgePalette::createDefault();
    const qrp::model::WangGrid grid(3, 2, 5, 1234);
    const auto generator = createGenerator();
    const auto colorPalette = qrp::color::GradientPalette::createMineral();
    const auto result = qrp::render::CpuReferenceRenderer::render(
        grid,
        edgePalette,
        generator,
        colorPalette,
        qrp::render::CpuRenderSettings{24, {}});
    require(result.image.width() == 72, "render width must match grid and tile size");
    require(result.image.height() == 48, "render height must match grid and tile size");
    require(result.diagnostics.inverseFailureCount == 0, "reference render must have no failures");
    require(
        result.diagnostics.maximumInverseResidual <= 1.0e-11,
        "reference render residual exceeds target");
    require(result.diagnostics.minimumDeterminant > 0.4, "render determinant margin is too small");
    require(result.diagnostics.maximumInverseIterations <= 8, "render inversion is unexpectedly slow");

    const auto firstPixel = result.image.pixel(0, 0);
    bool observedDifferentPixel = false;
    for (const auto pixel : result.image.pixels()) {
        if (pixel.red != firstPixel.red
            || pixel.green != firstPixel.green
            || pixel.blue != firstPixel.blue) {
            observedDifferentPixel = true;
            break;
        }
    }
    require(observedDifferentPixel, "reference render must not be a flat image");

    const auto heatmap = qrp::render::CpuReferenceRenderer::renderSeamHeatmap(
        grid,
        edgePalette,
        generator,
        colorPalette,
        qrp::render::SeamHeatmapSettings{24, 2, 1.0e14});
    require(heatmap.width() == 72, "seam heatmap width must match the render");
    require(heatmap.height() == 48, "seam heatmap height must match the render");
    require(
        heatmap.pixel(24, 12).blue > heatmap.pixel(12, 12).blue,
        "seam heatmap must identify internal grid edges");
}

void testProjectDraftCommitTransaction() {
    const auto presets = qrp::presets::createBaselinePresets();
    qrp::project::PatternProject project(
        qrp::project::configurationFromPreset(presets.at(2)));
    const auto firstRevision = project.revision();
    const auto firstSeed = project.scene().grid.seed();

    project.draft().gridSeed = firstSeed + 1;
    require(project.isDirty(), "editing draft must mark the project dirty");
    require(project.revision() == firstRevision, "editing draft must not increment revision");
    require(project.scene().grid.seed() == firstSeed, "editing draft must not rebuild scene");

    const auto success = project.applyDraft();
    require(success.applied, "valid draft must commit");
    require(project.revision() == firstRevision + 1, "successful Apply must increment revision");
    require(project.scene().grid.seed() == firstSeed + 1, "successful Apply must rebuild scene");

    const auto committed = project.committed();
    project.draft().edgeColors.front().epsilon = 4.0;
    const auto failed = project.applyDraft();
    require(!failed.applied, "unsafe edge draft must be rejected");
    require(project.revision() == firstRevision + 1, "failed Apply must preserve revision");
    require(project.committed() == committed, "failed Apply must preserve committed parameters");
    require(project.scene().grid.seed() == firstSeed + 1, "failed Apply must preserve scene");

    project.resetDraft();
    require(!project.isDirty(), "Reset Draft must restore committed parameters");
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"torus generator periodicity", testTorusGeneratorsArePeriodic},
        {"tile variation boundary", testTileVariationVanishesOnBoundary},
        {"gradient palette range", testGradientPalettesStayFiniteAndBounded},
        {"measured seams", testMeasuredSeams},
        {"CPU reference render", testCpuReferenceRender},
        {"project draft/commit transaction", testProjectDraftCommitTransaction},
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << tests.size() << " test(s) passed.\n";
    return EXIT_SUCCESS;
}
