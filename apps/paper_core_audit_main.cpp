#include "color/GradientPalette.hpp"
#include "export/PngWriter.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "math/EdgeFunction.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "render/CpuReferenceRenderer.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr std::size_t kGridWidth = 10;
constexpr std::size_t kGridHeight = 10;
constexpr std::uint32_t kPixelsPerTile = 72;
constexpr std::uint64_t kGridSeed = 0xdecafbad98765432ULL;
constexpr double kEdgeStrength = 0.08;

struct ShiftDifference {
    double horizontal = 0.0;
    double vertical = 0.0;
};

struct AuditCase {
    std::string id;
    qrp::render::CpuRenderResult render;
    qrp::render::SeamMetrics seams;
    qrp::model::PaletteSafetyReport safety;
    ShiftDifference oneTileShift;
    std::size_t uniqueEdgeSignatures = 0;
};

[[nodiscard]] qrp::generators::HybridTorusGenerator createPaperCoreGenerator() {
    qrp::generators::HybridGeneratorSettings settings;
    settings.fourierWeight = 1.0;
    settings.noiseWeight = 0.0;
    settings.tileVariationAmplitude = 0.0;
    settings.tileDomainWarpAmplitude = 0.0;
    settings.worldModulationAmplitude = 0.0;
    settings.worldDomainWarpAmplitude = 0.0;
    settings.worldFrequencyX = 0.0;
    settings.worldFrequencyY = 0.0;
    return qrp::generators::HybridTorusGenerator(
        qrp::generators::TorusFourier::createQuasiRegular(),
        qrp::generators::PeriodicGradientNoise(
            qrp::generators::PeriodicNoiseSettings{}),
        settings);
}

[[nodiscard]] qrp::model::EdgePalette createIdentityEdgePalette() {
    using qrp::math::EdgeFunction;
    using qrp::math::EdgeParameters;
    return qrp::model::EdgePalette({
        EdgeFunction(EdgeParameters{0.0, 0.0}),
        EdgeFunction(EdgeParameters{0.0, 0.0}),
        EdgeFunction(EdgeParameters{0.0, 0.0}),
        EdgeFunction(EdgeParameters{0.0, 0.0}),
        EdgeFunction(EdgeParameters{0.0, 0.0}),
    });
}

[[nodiscard]] qrp::model::EdgePalette createUniformEdgePalette() {
    using qrp::math::EdgeFunction;
    using qrp::math::EdgeParameters;
    return qrp::model::EdgePalette({
        EdgeFunction(EdgeParameters{kEdgeStrength, 0.0}),
        EdgeFunction(EdgeParameters{kEdgeStrength, 0.0}),
        EdgeFunction(EdgeParameters{kEdgeStrength, 0.0}),
        EdgeFunction(EdgeParameters{kEdgeStrength, 0.0}),
        EdgeFunction(EdgeParameters{kEdgeStrength, 0.0}),
    });
}

[[nodiscard]] qrp::model::EdgePalette createWangEdgePalette() {
    using qrp::math::EdgeFunction;
    using qrp::math::EdgeParameters;
    return qrp::model::EdgePalette({
        EdgeFunction(EdgeParameters{0.0, 0.0}),
        EdgeFunction(EdgeParameters{+kEdgeStrength, 0.0}),
        EdgeFunction(EdgeParameters{-kEdgeStrength, 0.0}),
        EdgeFunction(EdgeParameters{0.0, +0.5 * kEdgeStrength}),
        EdgeFunction(EdgeParameters{0.0, -0.5 * kEdgeStrength}),
    });
}

[[nodiscard]] double meanShiftDifference(
    const qrp::render::Image& image,
    const std::size_t shiftX,
    const std::size_t shiftY) {
    if (shiftX >= image.width() || shiftY >= image.height()) {
        throw std::invalid_argument("Image shift must leave a non-empty overlap.");
    }

    const std::size_t comparedWidth = image.width() - shiftX;
    const std::size_t comparedHeight = image.height() - shiftY;
    long double difference = 0.0;
    for (std::size_t y = 0; y < comparedHeight; ++y) {
        for (std::size_t x = 0; x < comparedWidth; ++x) {
            const auto first = image.pixel(x, y);
            const auto second = image.pixel(x + shiftX, y + shiftY);
            difference += std::abs(
                static_cast<int>(first.red) - static_cast<int>(second.red));
            difference += std::abs(
                static_cast<int>(first.green) - static_cast<int>(second.green));
            difference += std::abs(
                static_cast<int>(first.blue) - static_cast<int>(second.blue));
        }
    }

    const long double channelCount = 3.0L
        * static_cast<long double>(comparedWidth)
        * static_cast<long double>(comparedHeight);
    return static_cast<double>(difference / (255.0L * channelCount));
}

[[nodiscard]] ShiftDifference measureOneTileShift(
    const qrp::render::Image& image) {
    return {
        meanShiftDifference(image, kPixelsPerTile, 0),
        meanShiftDifference(image, 0, kPixelsPerTile),
    };
}

[[nodiscard]] std::size_t countUniqueEdgeSignatures(
    const qrp::model::WangGrid& grid) {
    std::set<std::array<std::uint32_t, 4>> signatures;
    for (const auto& tile : grid.tiles()) {
        signatures.insert({tile.south, tile.north, tile.west, tile.east});
    }
    return signatures.size();
}

[[nodiscard]] AuditCase renderCase(
    std::string id,
    const qrp::model::WangGrid& grid,
    const qrp::model::EdgePalette& edgePalette,
    const qrp::generators::HybridTorusGenerator& generator,
    const qrp::color::GradientPalette& colorPalette,
    const std::filesystem::path& outputDirectory) {
    const auto safety = edgePalette.validateAllCombinations();
    if (!safety.valid) {
        throw std::runtime_error(id + " edge palette is unsafe: " + safety.message);
    }

    auto render = qrp::render::CpuReferenceRenderer::render(
        grid,
        edgePalette,
        generator,
        colorPalette,
        qrp::render::CpuRenderSettings{kPixelsPerTile, {}});
    const auto seams = qrp::render::CpuReferenceRenderer::measureSeams(
        grid,
        edgePalette,
        generator,
        colorPalette,
        257);
    const auto shift = measureOneTileShift(render.image);
    qrp::exporting::writePng(render.image, outputDirectory / (id + ".png"));

    return {
        std::move(id),
        std::move(render),
        seams,
        safety,
        shift,
        countUniqueEdgeSignatures(grid),
    };
}

[[nodiscard]] qrp::render::Rgb8 labelColor(const std::uint32_t label) {
    constexpr std::array<qrp::render::Rgb8, 5> colors{{
        {250, 243, 214},
        {255, 101, 132},
        {72, 210, 255},
        {137, 232, 148},
        {255, 190, 78},
    }};
    return colors.at(label % colors.size());
}

void blendPixel(
    qrp::render::Image& image,
    const std::size_t x,
    const std::size_t y,
    const qrp::render::Rgb8 foreground) {
    constexpr int foregroundWeight = 4;
    constexpr int backgroundWeight = 1;
    constexpr int denominator = foregroundWeight + backgroundWeight;
    const auto background = image.pixel(x, y);
    image.pixel(x, y) = {
        static_cast<std::uint8_t>((foregroundWeight * foreground.red
            + backgroundWeight * background.red) / denominator),
        static_cast<std::uint8_t>((foregroundWeight * foreground.green
            + backgroundWeight * background.green) / denominator),
        static_cast<std::uint8_t>((foregroundWeight * foreground.blue
            + backgroundWeight * background.blue) / denominator),
    };
}

void overlayEdgeLabels(
    qrp::render::Image& image,
    const qrp::model::WangGrid& grid) {
    constexpr std::size_t thickness = 3;
    for (std::size_t tileY = 0; tileY < grid.height(); ++tileY) {
        const std::size_t top = (grid.height() - tileY - 1) * kPixelsPerTile;
        const std::size_t bottom = top + kPixelsPerTile - 1;
        for (std::size_t tileX = 0; tileX < grid.width(); ++tileX) {
            const std::size_t left = tileX * kPixelsPerTile;
            const std::size_t right = left + kPixelsPerTile - 1;
            const auto& tile = grid.tile(tileX, tileY);
            for (std::size_t offset = 0; offset < thickness; ++offset) {
                for (std::size_t x = left + thickness; x <= right - thickness; ++x) {
                    blendPixel(image, x, top + offset, labelColor(tile.north));
                    blendPixel(image, x, bottom - offset, labelColor(tile.south));
                }
                for (std::size_t y = top + thickness; y <= bottom - thickness; ++y) {
                    blendPixel(image, left + offset, y, labelColor(tile.west));
                    blendPixel(image, right - offset, y, labelColor(tile.east));
                }
            }
        }
    }
}

[[nodiscard]] double measureSeedDependence(
    const qrp::generators::HybridTorusGenerator& generator) {
    double maximumDifference = 0.0;
    for (int y = 0; y <= 24; ++y) {
        for (int x = 0; x <= 24; ++x) {
            const qrp::math::Vec2 parameter{
                static_cast<double>(x) / 24.0,
                static_cast<double>(y) / 24.0,
            };
            const qrp::math::Vec2 world{3.0 + parameter.x, 7.0 + parameter.y};
            const double first = generator.evaluate({parameter, world, 17});
            const double second = generator.evaluate({
                parameter,
                world,
                0xcafef00d12345678ULL,
            });
            maximumDifference = std::max(maximumDifference, std::abs(first - second));
        }
    }
    return maximumDifference;
}

void requireValidCase(const AuditCase& auditCase) {
    if (auditCase.render.diagnostics.inverseFailureCount != 0
        || auditCase.seams.inverseFailureCount != 0) {
        throw std::runtime_error(auditCase.id + " contains inverse-warp failures.");
    }
    if (auditCase.render.diagnostics.maximumInverseResidual > 1.0e-11) {
        throw std::runtime_error(auditCase.id + " exceeds the inverse residual target.");
    }
    if (auditCase.seams.maximumScalarDifference > 1.0e-12
        || auditCase.seams.maximumColorDifference > 1.0e-12
        || auditCase.seams.maximumQuantizedChannelDifference != 0) {
        throw std::runtime_error(auditCase.id + " exceeds the seam target.");
    }
}

void writeCaseJson(std::ostream& output, const AuditCase& auditCase) {
    output << "    {\n"
           << "      \"id\": \"" << auditCase.id << "\",\n"
           << "      \"rawLabelSignatureCount\": "
           << auditCase.uniqueEdgeSignatures << ",\n"
           << "      \"enumeratedLabelCombinationCount\": "
           << auditCase.safety.combinationCount << ",\n"
           << "      \"minimumDeterminantLowerBound\": "
           << auditCase.safety.minimumDeterminantLowerBound << ",\n"
           << "      \"minimumSampledDeterminant\": "
           << auditCase.render.diagnostics.minimumDeterminant << ",\n"
           << "      \"inverseFailureCount\": "
           << auditCase.render.diagnostics.inverseFailureCount << ",\n"
           << "      \"maximumInverseResidual\": "
           << auditCase.render.diagnostics.maximumInverseResidual << ",\n"
           << "      \"maximumScalarSeamDifference\": "
           << auditCase.seams.maximumScalarDifference << ",\n"
           << "      \"maximumColorSeamDifference\": "
           << auditCase.seams.maximumColorDifference << ",\n"
           << "      \"maximumQuantizedSeamDifference\": "
           << static_cast<unsigned int>(
                  auditCase.seams.maximumQuantizedChannelDifference)
           << ",\n"
           << "      \"meanOneTileShiftDifferenceX\": "
           << auditCase.oneTileShift.horizontal << ",\n"
           << "      \"meanOneTileShiftDifferenceY\": "
           << auditCase.oneTileShift.vertical << "\n"
           << "    }";
}

void writeMetrics(
    const std::filesystem::path& path,
    const std::array<const AuditCase*, 3>& cases,
    const double maximumSeedDifference) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to open paper-core metrics output.");
    }

    output << std::setprecision(17)
           << "{\n"
           << "  \"schemaVersion\": 1,\n"
           << "  \"experiment\": \"paper-core-causal-audit\",\n"
           << "  \"equation\": \"I_T(x) = C(G(Phi_T^{-1}(x)))\",\n"
           << "  \"gridWidth\": " << kGridWidth << ",\n"
           << "  \"gridHeight\": " << kGridHeight << ",\n"
           << "  \"pixelsPerTile\": " << kPixelsPerTile << ",\n"
           << "  \"gridSeedHex\": \"0x" << std::hex << kGridSeed
           << std::dec << "\",\n"
           << "  \"edgeStrength\": " << kEdgeStrength << ",\n"
           << "  \"optionalPerturbationsEnabled\": false,\n"
           << "  \"maximumTileSeedDifference\": "
           << maximumSeedDifference << ",\n"
           << "  \"labelOverlay\": {\n"
           << "    \"file\": \"D_wang_edge_labels.png\",\n"
           << "    \"rgbByLabel\": [[250,243,214],[255,101,132],"
              "[72,210,255],[137,232,148],[255,190,78]]\n"
           << "  },\n"
           << "  \"cases\": [\n";
    for (std::size_t index = 0; index < cases.size(); ++index) {
        writeCaseJson(output, *cases[index]);
        output << (index + 1 == cases.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    if (!output) {
        throw std::runtime_error("Unable to write paper-core metrics output.");
    }
}

void printCase(const AuditCase& auditCase) {
    std::cout << auditCase.id
              << ": raw_label_signatures=" << auditCase.uniqueEdgeSignatures
              << ", min_det=" << std::scientific
              << auditCase.render.diagnostics.minimumDeterminant
              << ", max_residual="
              << auditCase.render.diagnostics.maximumInverseResidual
              << ", scalar_seam=" << auditCase.seams.maximumScalarDifference
              << ", color_seam=" << auditCase.seams.maximumColorDifference
              << ", shift_x=" << auditCase.oneTileShift.horizontal
              << ", shift_y=" << auditCase.oneTileShift.vertical
              << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/paper-core-audit");
        std::filesystem::create_directories(outputDirectory);
        std::filesystem::remove(outputDirectory / "metrics.json");

        const auto generator = createPaperCoreGenerator();
        const auto colorPalette = qrp::color::GradientPalette::createMidnightGold();
        const auto identityEdges = createIdentityEdgePalette();
        const auto uniformEdges = createUniformEdgePalette();
        const auto wangEdges = createWangEdgePalette();
        const qrp::model::WangGrid wangGrid(
            kGridWidth, kGridHeight, 5, kGridSeed);

        auto identity = renderCase(
            "A_identity",
            wangGrid,
            identityEdges,
            generator,
            colorPalette,
            outputDirectory);
        auto uniform = renderCase(
            "B_uniform_deformation",
            wangGrid,
            uniformEdges,
            generator,
            colorPalette,
            outputDirectory);
        auto wang = renderCase(
            "C_wang_edge_deformation",
            wangGrid,
            wangEdges,
            generator,
            colorPalette,
            outputDirectory);

        qrp::render::Image labels = wang.render.image;
        overlayEdgeLabels(labels, wangGrid);
        qrp::exporting::writePng(
            labels,
            outputDirectory / "D_wang_edge_labels.png");

        requireValidCase(identity);
        requireValidCase(uniform);
        requireValidCase(wang);
        if (identity.oneTileShift.horizontal != 0.0
            || identity.oneTileShift.vertical != 0.0
            || uniform.oneTileShift.horizontal != 0.0
            || uniform.oneTileShift.vertical != 0.0) {
            throw std::runtime_error(
                "Identity and uniform-label controls must remain exactly one-tile periodic.");
        }
        if (wang.oneTileShift.horizontal <= 1.0e-6
            && wang.oneTileShift.vertical <= 1.0e-6) {
            throw std::runtime_error(
                "The Wang-label case did not measurably depart from one-tile repetition.");
        }

        const double maximumSeedDifference = measureSeedDependence(generator);
        if (maximumSeedDifference != 0.0) {
            throw std::runtime_error(
                "Tile seeds affect the paper-core generator while perturbations are disabled.");
        }
        writeMetrics(
            outputDirectory / "metrics.json",
            {&identity, &uniform, &wang},
            maximumSeedDifference);

        printCase(identity);
        printCase(uniform);
        printCase(wang);
        std::cout << "maximum_tile_seed_difference=" << maximumSeedDifference << '\n'
                  << "Wrote paper-core causal audit to "
                  << outputDirectory.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Paper-core audit failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
