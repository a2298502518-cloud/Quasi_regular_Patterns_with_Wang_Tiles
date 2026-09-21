#include "color/GradientPalette.hpp"
#include "export/PngWriter.hpp"
#include "model/HierarchicalQrpComposition.hpp"
#include "model/ParametricWangQrpField.hpp"
#include "model/WangContentWeight.hpp"
#include "model/WangGrid.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kGridSize = 20;
constexpr std::size_t kPixelsPerTile = 80;
constexpr std::size_t kGutter = 10;
constexpr std::uint64_t kGridSeed = 0x4d595df4d0f33173ULL;

using qrp::math::Vec2;
using qrp::model::ParametricWangQrpField;
using qrp::model::ParametricWangQrpParameters;
using qrp::model::ScalarFieldEvaluation;

struct ParameterCase {
    std::string_view id;
    Vec2 globalPhase;
};

struct RenderedCase {
    qrp::render::Image coarseImage;
    qrp::render::Image finalImage;
    std::vector<double> coarseValues;
};

constexpr std::array<ParameterCase, 4> kCases{{
    {"P0_zero_phase", {0.0, 0.0}},
    {"P1_axis_phase", {1.65, 0.0}},
    {"P2_oblique_phase", {3.00, 0.85}},
    {"P3_opposed_phase", {2.10, -2.45}},
}};

[[nodiscard]] qrp::render::Rgb8 quantize(
    const qrp::color::Color3 linearColor) noexcept {
    const auto channel = [](const double value) {
        const double linear = std::clamp(value, 0.0, 1.0);
        const double srgb = linear <= 0.0031308
            ? 12.92 * linear
            : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
        return static_cast<std::uint8_t>(std::lround(255.0 * srgb));
    };
    return {channel(linearColor.red), channel(linearColor.green), channel(linearColor.blue)};
}

[[nodiscard]] ParametricWangQrpField makeField(
    const std::uint32_t q,
    const double frequency,
    const Vec2 globalPhase,
    const Vec2 wangPhase = {}) {
    return ParametricWangQrpField(ParametricWangQrpParameters{
        q,
        frequency,
        globalPhase,
        wangPhase,
        0.5,
        0.4,
    });
}

[[nodiscard]] RenderedCase renderCase(
    const ParameterCase& parameterCase,
    const qrp::model::WangGrid& grid,
    const qrp::model::WangContentWeight& weightField,
    const ParametricWangQrpField& medium,
    const ParametricWangQrpField& fine,
    const qrp::model::HierarchicalQrpComposition& hierarchy,
    const qrp::color::GradientPalette& palette,
    const bool useWangWeight = true) {
    constexpr std::size_t extent = kGridSize * kPixelsPerTile;
    RenderedCase result{
        qrp::render::Image(extent, extent),
        qrp::render::Image(extent, extent),
        std::vector<double>(extent * extent),
    };
    const auto coarse = makeField(7, 3.15, parameterCase.globalPhase, {1.15, -0.80});
    const qrp::model::WangContentWeightEvaluation neutral{0.5, {}};

    for (std::size_t imageY = 0; imageY < extent; ++imageY) {
        const double worldY = static_cast<double>(extent - imageY) / kPixelsPerTile
            - 0.5 / kPixelsPerTile;
        const std::size_t tileY = std::min(
            static_cast<std::size_t>(worldY), kGridSize - 1);
        for (std::size_t imageX = 0; imageX < extent; ++imageX) {
            const double worldX = (static_cast<double>(imageX) + 0.5) / kPixelsPerTile;
            const std::size_t tileX = std::min(
                static_cast<std::size_t>(worldX), kGridSize - 1);
            const Vec2 position{worldX, worldY};
            const Vec2 local{
                worldX - static_cast<double>(tileX),
                worldY - static_cast<double>(tileY),
            };
            const auto weight = useWangWeight
                ? weightField.evaluate(grid.tile(tileX, tileY), local)
                : neutral;
            const ScalarFieldEvaluation coarseSample = coarse.evaluate(position, weight);
            const ScalarFieldEvaluation finalSample = hierarchy.evaluate(
                coarseSample,
                medium.evaluate(position, neutral),
                fine.evaluate(position, neutral));
            const std::size_t index = imageY * extent + imageX;
            result.coarseValues[index] = coarseSample.value;
            result.coarseImage.pixel(imageX, imageY) = quantize(
                palette.sample(coarseSample.value));
            result.finalImage.pixel(imageX, imageY) = quantize(
                palette.sample(finalSample.value));
        }
    }
    return result;
}

[[nodiscard]] qrp::render::Image makePair(
    const RenderedCase& first,
    const RenderedCase& second) {
    constexpr std::size_t panelExtent = kGridSize * kPixelsPerTile;
    qrp::render::Image pair(2 * panelExtent + kGutter, panelExtent);
    for (std::size_t y = 0; y < panelExtent; ++y) {
        for (std::size_t x = 0; x < pair.width(); ++x) {
            pair.pixel(x, y) = {8, 13, 22};
        }
        for (std::size_t x = 0; x < panelExtent; ++x) {
            pair.pixel(x, y) = first.finalImage.pixel(x, y);
            pair.pixel(panelExtent + kGutter + x, y) = second.finalImage.pixel(x, y);
        }
    }
    return pair;
}

[[nodiscard]] qrp::render::Image makeSheet(
    const std::array<RenderedCase, 4>& cases,
    const bool finalImage) {
    constexpr std::size_t panelExtent = kGridSize * kPixelsPerTile;
    constexpr std::size_t sheetExtent = 2 * panelExtent + kGutter;
    qrp::render::Image sheet(sheetExtent, sheetExtent);
    for (std::size_t y = 0; y < sheetExtent; ++y) {
        for (std::size_t x = 0; x < sheetExtent; ++x) {
            sheet.pixel(x, y) = {8, 13, 22};
        }
    }
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const std::size_t offsetX = (index % 2) * (panelExtent + kGutter);
        const std::size_t offsetY = (index / 2) * (panelExtent + kGutter);
        const auto& source = finalImage ? cases[index].finalImage : cases[index].coarseImage;
        for (std::size_t y = 0; y < panelExtent; ++y) {
            for (std::size_t x = 0; x < panelExtent; ++x) {
                sheet.pixel(offsetX + x, offsetY + y) = source.pixel(x, y);
            }
        }
    }
    return sheet;
}

void printPairwiseDifferences(const std::array<RenderedCase, 4>& cases) {
    for (std::size_t first = 0; first < cases.size(); ++first) {
        for (std::size_t second = first + 1; second < cases.size(); ++second) {
            long double squared = 0.0;
            std::size_t signDisagreements = 0;
            for (std::size_t index = 0; index < cases[first].coarseValues.size(); ++index) {
                const double a = cases[first].coarseValues[index];
                const double b = cases[second].coarseValues[index];
                const double difference = a - b;
                squared += difference * difference;
                signDisagreements += (a < 0.0) != (b < 0.0) ? 1U : 0U;
            }
            const double count = static_cast<double>(cases[first].coarseValues.size());
            std::cout << kCases[first].id << " vs " << kCases[second].id
                      << ": rms=" << std::sqrt(static_cast<double>(squared) / count)
                      << ", sign_disagreement=" << signDisagreements / count << '\n';
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("output/wang-qrp-experiment");
        std::filesystem::create_directories(outputDirectory);

        const qrp::model::WangGrid grid(kGridSize, kGridSize, 5, kGridSeed);
        const qrp::model::WangContentWeight weightField({5, 0.5, 0.10, 0.40});
        const auto medium = makeField(9, 7.4, {0.55, -0.30});
        const auto fine = makeField(11, 12.8, {-0.25, 0.45});
        const qrp::model::HierarchicalQrpComposition hierarchy({0.19, 0.055});
        const auto palette = qrp::color::GradientPalette::createMidnightGold();

        std::array<RenderedCase, 4> rendered{
            renderCase(kCases[0], grid, weightField, medium, fine, hierarchy, palette),
            renderCase(kCases[1], grid, weightField, medium, fine, hierarchy, palette),
            renderCase(kCases[2], grid, weightField, medium, fine, hierarchy, palette),
            renderCase(kCases[3], grid, weightField, medium, fine, hierarchy, palette),
        };
        for (std::size_t index = 0; index < rendered.size(); ++index) {
            qrp::exporting::writePng(
                rendered[index].finalImage,
                outputDirectory / (std::string(kCases[index].id) + ".png"));
        }
        qrp::exporting::writePng(
            makeSheet(rendered, false), outputDirectory / "A_coarse_parameter_family.png");
        qrp::exporting::writePng(
            makeSheet(rendered, true), outputDirectory / "B_layered_parameter_family.png");
        const auto uniformControl = renderCase(
            kCases[2], grid, weightField, medium, fine, hierarchy, palette, false);
        qrp::exporting::writePng(
            makePair(uniformControl, rendered[2]),
            outputDirectory / "C_uniform_vs_wang.png");
        printPairwiseDifferences(rendered);
        std::cout << "Wrote the unified parameter family to "
                  << outputDirectory.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Wang-QRP experiment failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
