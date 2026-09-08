#include "color/GradientPalette.hpp"
#include "export/PngWriter.hpp"
#include "export/ProjectMetadataWriter.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "presets/PatternPreset.hpp"
#include "project/PatternProject.hpp"
#include "render/CpuReferenceRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
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

void testScalarProfilesPreserveBoundaryContinuity() {
    constexpr std::array profiles{
        qrp::generators::ScalarProfile::Natural,
        qrp::generators::ScalarProfile::Ridges,
        qrp::generators::ScalarProfile::Cells,
    };
    for (const auto profile : profiles) {
        auto settings = HybridGeneratorSettings{};
        settings.scalarProfile = profile;
        settings.worldDetailAmplitude = 0.9;
        settings.worldGrainAmplitude = 0.25;
        const auto generator = createGenerator(settings);
        for (int index = 0; index <= 256; ++index) {
            const double t = static_cast<double>(index) / 256.0;
            const Vec2 world{6.0 + t, 9.0};
            requireNear(
                generator.evaluate(GeneratorInput{Vec2{t, 0.0}, world, 17}),
                generator.evaluate(GeneratorInput{Vec2{t, 1.0}, world, 91}),
                2.0e-14,
                std::string("profile boundary ")
                    + qrp::generators::scalarProfileName(profile));
        }
    }
}

void testEdgeConnectedInkMatchesAcrossSharedEdges() {
    auto settings = HybridGeneratorSettings{};
    settings.fourierWeight = 0.0;
    settings.noiseWeight = 0.0;
    settings.tileVariationAmplitude = 0.0;
    settings.tileDomainWarpAmplitude = 0.0;
    settings.worldModulationAmplitude = 0.0;
    settings.worldDomainWarpAmplitude = 0.0;
    settings.worldDetailAmplitude = 0.0;
    settings.worldGrainAmplitude = 0.0;
    settings.edgeStructureAmplitude = 1.0;
    const auto generator = createGenerator(settings);

    for (int index = 0; index <= 400; ++index) {
        const double t = static_cast<double>(index) / 400.0;
        const Vec2 verticalWorld{9.0, 4.0 + t};
        requireNear(
            generator.evaluate(GeneratorInput{
                Vec2{1.0, t}, verticalWorld, 17, {1, 2, 3, 7}}),
            generator.evaluate(GeneratorInput{
                Vec2{0.0, t}, verticalWorld, 91, {4, 5, 7, 6}}),
            2.0e-14,
            "edge ink vertical seam");

        const Vec2 horizontalWorld{6.0 + t, 8.0};
        requireNear(
            generator.evaluate(GeneratorInput{
                Vec2{t, 1.0}, horizontalWorld, 23, {8, 11, 9, 10}}),
            generator.evaluate(GeneratorInput{
                Vec2{t, 0.0}, horizontalWorld, 47, {11, 12, 13, 14}}),
            2.0e-14,
            "edge ink horizontal seam");
    }
}

void testGradientPalettesStayFiniteAndBounded() {
    const std::vector<qrp::color::GradientPalette> palettes{
        qrp::color::GradientPalette::createMidnightGold(),
        qrp::color::GradientPalette::createMineral(),
        qrp::color::GradientPalette::createAurora(),
        qrp::color::GradientPalette::createGraphicPrimary(),
        qrp::color::GradientPalette::createInkWash(),
        qrp::color::GradientPalette::createFieldCamo(),
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

void testPaletteUsesPerceptualInterpolation() {
    const qrp::color::GradientPalette palette({
        {0.0, {0.0, 0.0, 0.0}},
        {1.0, {1.0, 1.0, 1.0}},
    }, qrp::color::ToneSettings{0.0, 1.0, 0.0, 0.0});
    const auto midpoint = palette.sample(0.0);
    // Oklab 的 L=0.5 转回线性 sRGB 约为 0.125；直接 RGB 插值会错误地产生 0.5。
    requireNear(midpoint.red, 0.125, 2.0e-5, "Oklab midpoint red");
    requireNear(midpoint.green, 0.125, 2.0e-5, "Oklab midpoint green");
    requireNear(midpoint.blue, 0.125, 2.0e-5, "Oklab midpoint blue");
}

void testPosterizedPaletteUsesStablePlateaus() {
    const qrp::color::GradientPalette palette({
        {0.0, {0.0, 0.0, 0.0}},
        {1.0, {1.0, 1.0, 1.0}},
    }, qrp::color::ToneSettings{0.0, 1.0, 0.0, 0.0, 2, 0.10});
    require(
        palette.sample(-2.0) == palette.sample(-0.3),
        "posterization must create a stable lower plateau");
    require(
        palette.sample(2.0) == palette.sample(0.3),
        "posterization must create a stable upper plateau");

    bool rejectedSingleLevel = false;
    try {
        const qrp::color::GradientPalette invalid({
            {0.0, {0.0, 0.0, 0.0}},
            {1.0, {1.0, 1.0, 1.0}},
        }, qrp::color::ToneSettings{0.0, 1.0, 0.0, 0.0, 1, 0.10});
        static_cast<void>(invalid);
    } catch (const std::invalid_argument&) {
        rejectedSingleLevel = true;
    }
    require(rejectedSingleLevel, "a one-level posterization must be rejected");
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

    project.draft().generator.scalarProfile
        = static_cast<qrp::generators::ScalarProfile>(99);
    const auto invalidProfile = project.applyDraft();
    require(!invalidProfile.applied, "unknown scalar profiles must be rejected");
    require(project.committed() == committed, "invalid profiles must preserve committed state");

    project.resetDraft();
    project.draft().generator.cellularScale = 0.0;
    const auto invalidCellularScale = project.applyDraft();
    require(!invalidCellularScale.applied, "zero cellular scale must be rejected");
    require(project.committed() == committed, "invalid cellular scale must preserve committed state");
}

void testMaterialSettingsCommitAtomically() {
    const auto presets = qrp::presets::createBaselinePresets();
    qrp::project::PatternProject project(
        qrp::project::configurationFromPreset(presets.at(3)));
    const auto firstRevision = project.revision();
    project.draft().material.reliefStrength = 0.31;
    require(project.isDirty(), "material edit must remain in draft");
    requireNear(
        project.scene().material.reliefStrength,
        presets.at(3).material.reliefStrength,
        0.0,
        "committed material before Apply");

    const auto success = project.applyDraft();
    require(success.applied, "valid material draft must commit");
    require(project.revision() == firstRevision + 1, "material Apply must increment revision");
    requireNear(project.scene().material.reliefStrength, 0.31, 0.0, "committed material");

    const auto committed = project.committed();
    project.draft().material.contourStrength = std::numeric_limits<double>::quiet_NaN();
    const auto failed = project.applyDraft();
    require(!failed.applied, "non-finite material draft must be rejected");
    require(project.committed() == committed, "failed material Apply must preserve committed state");
    require(project.revision() == firstRevision + 1, "failed material Apply must preserve revision");
}

void testPngEncodingCarriesSrgbMetadata() {
    qrp::render::Image image(2, 2);
    image.pixel(0, 0) = {255, 0, 0};
    image.pixel(1, 0) = {0, 255, 0};
    image.pixel(0, 1) = {0, 0, 255};
    image.pixel(1, 1) = {255, 255, 255};
    const auto encoded = qrp::exporting::encodePng(image);
    constexpr std::array<std::uint8_t, 8> signature{
        137, 80, 78, 71, 13, 10, 26, 10,
    };
    require(
        encoded.size() > signature.size()
            && std::equal(signature.begin(), signature.end(), encoded.begin()),
        "PNG signature must be valid");
    const auto containsChunk = [&encoded](const std::string_view name) {
        return std::search(encoded.begin(), encoded.end(), name.begin(), name.end())
            != encoded.end();
    };
    require(containsChunk("sRGB"), "PNG must declare the sRGB color space");
    require(containsChunk("gAMA"), "PNG must carry the matching sRGB gamma chunk");
}

void testProjectMetadataIsCompleteAndStable() {
    const auto presets = qrp::presets::createBaselinePresets();
    const auto configuration = qrp::project::configurationFromPreset(presets.at(2));
    const auto json = qrp::exporting::serializeProjectMetadata(
        configuration,
        7,
        qrp::exporting::ExportView{2880, 2880, 0.0, 0.0, 288.0, true});
    require(json.find("\"schemaVersion\": 1") != std::string::npos, "metadata needs a schema");
    require(json.find("\"projectRevision\": 7") != std::string::npos, "metadata needs revision");
    require(json.find("\"width\": 2880") != std::string::npos, "metadata needs width");
    require(json.find("\"interpolation\": \"Oklab\"") != std::string::npos, "metadata needs color semantics");
    require(json.find("\"seedHex\": \"0x") != std::string::npos, "metadata needs an exact seed");
    require(json.find("\"model\": \"hybrid_torus_v4\"") != std::string::npos, "metadata needs generator semantics");
    require(json.find("\"scalarProfile\": \"natural\"") != std::string::npos, "metadata needs scalar profile");
    require(json.find("\"worldDetailAmplitude\"") != std::string::npos, "metadata needs world detail");
    require(json.find("\"worldGrainAmplitude\"") != std::string::npos, "metadata needs world grain");
    require(json.find("\"cellularScale\"") != std::string::npos, "metadata needs cellular scale");
    require(json.find("\"edgeStructureAmplitude\"") != std::string::npos, "metadata needs edge structure");
    require(json.find("\"posterizeLevels\"") != std::string::npos, "metadata needs posterization");
    require(json.find("\"fourierModes\"") != std::string::npos, "metadata needs Fourier basis data");
    require(json.find("\"periodicNoise\"") != std::string::npos, "metadata needs noise basis data");
    require(json.find("\"material\"") != std::string::npos, "metadata needs material settings");
}

void testStylePresetsChangeStructureNotOnlyColor() {
    const auto presets = qrp::presets::createBaselinePresets();
    require(presets.size() == 8, "the preset suite must contain eight baselines");
    const auto& screenprint = presets.at(5);
    const auto& ink = presets.at(6);
    const auto& cells = presets.at(7);
    require(
        screenprint.generator.scalarProfile == qrp::generators::ScalarProfile::Ridges,
        "graphic screenprint must use the ridged scalar profile");
    require(
        screenprint.palette.tone().posterizeLevels == 4,
        "graphic screenprint must use a finite four-color treatment");
    require(
        ink.generator.edgeStructureAmplitude
            > 5.0 * (ink.generator.noiseWeight + ink.generator.fourierWeight),
        "ink wash must be structurally Wang-edge-dominant");
    require(
        ink.generator.worldGrainAmplitude > 0.0,
        "ink wash must carry cross-tile world grain");
    require(
        ink.generator.edgeStructureAmplitude > 0.0,
        "ink wash must expose its Wang-edge structure");
    require(
        cells.generator.scalarProfile == qrp::generators::ScalarProfile::Cells,
        "cellular camo must use the cellular scalar profile");
    require(
        screenprint.material.contourStrength == 0.0
            && ink.material.contourStrength == 0.0
            && cells.material.contourStrength == 0.0,
        "new styles must not inherit the original contour language");
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"torus generator periodicity", testTorusGeneratorsArePeriodic},
        {"tile variation boundary", testTileVariationVanishesOnBoundary},
        {"scalar profile boundaries", testScalarProfilesPreserveBoundaryContinuity},
        {"edge ink shared boundaries", testEdgeConnectedInkMatchesAcrossSharedEdges},
        {"gradient palette range", testGradientPalettesStayFiniteAndBounded},
        {"perceptual palette interpolation", testPaletteUsesPerceptualInterpolation},
        {"posterized palette plateaus", testPosterizedPaletteUsesStablePlateaus},
        {"measured seams", testMeasuredSeams},
        {"CPU reference render", testCpuReferenceRender},
        {"project draft/commit transaction", testProjectDraftCommitTransaction},
        {"material settings transaction", testMaterialSettingsCommitAtomically},
        {"PNG sRGB metadata", testPngEncodingCarriesSrgbMetadata},
        {"project export metadata", testProjectMetadataIsCompleteAndStable},
        {"structurally distinct style presets", testStylePresetsChangeStructureNotOnlyColor},
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
