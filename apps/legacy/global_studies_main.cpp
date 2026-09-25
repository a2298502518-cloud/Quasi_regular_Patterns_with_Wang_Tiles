#include "color/GradientPalette.hpp"
#include "DirectQrpDesign.hpp"
#include "SourceTileStudy.hpp"
#include "export/PngWriter.hpp"
#include "model/HierarchicalQrpComposition.hpp"
#include "model/OrderedQrpField.hpp"
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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <span>
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

constexpr Vec2 kWangPhase{1.15, -0.80};
constexpr double kWeightCenter = 0.5;
constexpr double kWeightRadius = 0.4;

struct ParameterCase {
    std::string_view id;
    Vec2 globalPhase;
    std::uint32_t resonanceCount = 7;
    double spatialFrequency = 3.15;
    double mediumStrength = 0.19;
    double fineStrength = 0.055;
    double directionalBias = 0.0;
    double orientationDegrees = 0.0;
    double crossMix = 0.0;
    Vec2 wangPhase = kWangPhase;
};

struct RenderOptions {
    std::size_t pixelsPerTile = kPixelsPerTile;
    bool useWangWeight = true;
    bool recordFinalValues = false;
};

struct RenderedCase {
    qrp::render::Image coarseImage;
    qrp::render::Image finalImage;
    std::vector<double> coarseValues;
    std::vector<double> finalValues;
};

constexpr std::array<ParameterCase, 4> kCases{{
    {"P0_zero_phase", {0.0, 0.0}},
    {"P1_axis_phase", {1.65, 0.0}},
    {"P2_oblique_phase", {3.00, 0.85}},
    {"P3_opposed_phase", {2.10, -2.45}},
}};

// Wang 配置始终固定；每一行只探索一种 QRP 控制，以 S03 为共同参考。
constexpr std::array<ParameterCase, 16> kStyleCases{{
    {"S01_phase_zero", {0.0, 0.0}},
    {"S02_phase_axis", {1.65, 0.0}},
    {"S03_reference", {3.0, 0.85}},
    {"S04_phase_opposed", {2.10, -2.45}},
    {"S05_q5", {3.0, 0.85}, 5},
    {"S06_q9", {3.0, 0.85}, 9},
    {"S07_q11", {3.0, 0.85}, 11},
    {"S08_q15", {3.0, 0.85}, 15},
    {"S09_frequency_1_4", {3.0, 0.85}, 7, 1.4},
    {"S10_frequency_2_2", {3.0, 0.85}, 7, 2.2},
    {"S11_frequency_4_8", {3.0, 0.85}, 7, 4.8},
    {"S12_frequency_6_3", {3.0, 0.85}, 7, 6.3},
    {"S13_no_detail", {3.0, 0.85}, 7, 3.15, 0.0, 0.0},
    {"S14_medium_detail", {3.0, 0.85}, 7, 3.15, 0.32, 0.0},
    {"S15_fine_detail", {3.0, 0.85}, 7, 3.15, 0.0, 0.32},
    {"S16_mixed_detail", {3.0, 0.85}, 7, 3.15, 0.25, 0.10},
}};

constexpr std::array<ParameterCase, 16> kDirectionalCases{{
    {"D01_reference", {3.0, 0.85}},
    {"D02_bias_1", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 1.0},
    {"D03_bias_3", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0},
    {"D04_bias_8", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 8.0},
    {"D05_angle_15", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 15.0},
    {"D06_angle_30", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 30.0},
    {"D07_angle_60", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 60.0},
    {"D08_angle_90", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 90.0},
    {"D09_sparse", {3.0, 0.85}, 7, 1.6, 0.19, 0.055, 3.0},
    {"D10_dense", {3.0, 0.85}, 7, 5.0, 0.19, 0.055, 3.0},
    {"D11_smooth", {3.0, 0.85}, 7, 3.15, 0.0, 0.0, 3.0},
    {"D12_phase", {0.0, 0.0}, 7, 3.15, 0.19, 0.055, 3.0},
    {"D13_cross_15", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 0.0, 0.15},
    {"D14_cross_35", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 0.0, 0.35},
    {"D15_cross_50", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 0.0, 0.50},
    {"D16_cross_rotated", {3.0, 0.85}, 7, 3.15, 0.19, 0.055, 3.0, 45.0, 0.50},
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
    const Vec2 wangPhase = {},
    const double directionalBias = 0.0,
    const double orientationDegrees = 0.0,
    const double crossMix = 0.0) {
    return ParametricWangQrpField(ParametricWangQrpParameters{
        q,
        frequency,
        globalPhase,
        wangPhase,
        kWeightCenter,
        kWeightRadius,
        directionalBias,
        orientationDegrees * std::numbers::pi_v<double> / 180.0,
        crossMix,
    });
}

template<class CoarseEvaluator, class FinalEvaluator>
[[nodiscard]] RenderedCase renderField(
    const CoarseEvaluator& coarseEvaluator,
    const FinalEvaluator& finalEvaluator,
    const qrp::model::WangGrid& grid,
    const qrp::model::WangContentWeight& weightField,
    const qrp::color::GradientPalette& palette,
    const RenderOptions options) {
    const std::size_t extent = kGridSize * options.pixelsPerTile;
    const double pixelsPerTile = static_cast<double>(options.pixelsPerTile);
    RenderedCase result{
        qrp::render::Image(extent, extent),
        qrp::render::Image(extent, extent),
        std::vector<double>(extent * extent),
        std::vector<double>(options.recordFinalValues ? extent * extent : 0),
    };
    const qrp::model::WangContentWeightEvaluation neutral{0.5, {}};

    for (std::size_t imageY = 0; imageY < extent; ++imageY) {
        const double worldY = static_cast<double>(extent - imageY) / pixelsPerTile
            - 0.5 / pixelsPerTile;
        const std::size_t tileY = std::min(
            static_cast<std::size_t>(worldY), kGridSize - 1);
        for (std::size_t imageX = 0; imageX < extent; ++imageX) {
            const double worldX = (static_cast<double>(imageX) + 0.5) / pixelsPerTile;
            const std::size_t tileX = std::min(
                static_cast<std::size_t>(worldX), kGridSize - 1);
            const Vec2 position{worldX, worldY};
            const Vec2 local{
                worldX - static_cast<double>(tileX),
                worldY - static_cast<double>(tileY),
            };
            const auto weight = options.useWangWeight
                ? weightField.evaluate(grid.tile(tileX, tileY), local)
                : neutral;
            const ScalarFieldEvaluation coarseSample = coarseEvaluator(position, weight);
            const ScalarFieldEvaluation finalSample = finalEvaluator(position, coarseSample);
            const std::size_t index = imageY * extent + imageX;
            result.coarseValues[index] = coarseSample.value;
            if (options.recordFinalValues) {
                result.finalValues[index] = finalSample.value;
            }
            result.coarseImage.pixel(imageX, imageY) = quantize(
                palette.sample(coarseSample.value));
            result.finalImage.pixel(imageX, imageY) = quantize(
                palette.sample(finalSample.value));
        }
    }
    return result;
}

[[nodiscard]] RenderedCase renderCase(
    const ParameterCase& parameterCase,
    const qrp::model::WangGrid& grid,
    const qrp::model::WangContentWeight& weightField,
    const ParametricWangQrpField& medium,
    const ParametricWangQrpField& fine,
    const qrp::color::GradientPalette& palette,
    const RenderOptions options = {}) {
    const auto coarse = makeField(parameterCase.resonanceCount,
        parameterCase.spatialFrequency, parameterCase.globalPhase, parameterCase.wangPhase,
        parameterCase.directionalBias, parameterCase.orientationDegrees, parameterCase.crossMix);
    const qrp::model::HierarchicalQrpComposition hierarchy(
        {parameterCase.mediumStrength, parameterCase.fineStrength});
    return renderField(
        [&coarse](const Vec2 position, const qrp::model::WangContentWeightEvaluation weight) {
            return coarse.evaluate(position, weight);
        },
        [&](const Vec2 position, const ScalarFieldEvaluation sample) {
            return hierarchy.evaluate(sample,
                medium.evaluate(position, {0.5, {}}), fine.evaluate(position, {0.5, {}}));
        }, grid, weightField, palette, options);
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

[[nodiscard]] qrp::render::Image grayscaleImage(
    const std::vector<double>& values,
    const std::size_t extent) {
    qrp::render::Image image(extent, extent);
    for (std::size_t y = 0; y < extent; ++y) {
        for (std::size_t x = 0; x < extent; ++x) {
            // 固定标量 [-1,1] 到 sRGB 灰度码值 [0,255]，不做逐图归一化或色带调制。
            const auto gray = static_cast<std::uint8_t>(std::lround(
                255.0 * std::clamp(0.5 + 0.5 * values[y * extent + x], 0.0, 1.0)));
            image.pixel(x, y) = {gray, gray, gray};
        }
    }
    return image;
}

void writeStatistics(const std::vector<double>& values, std::ostream& out) {
    const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
    long double sum = 0.0;
    long double squaredSum = 0.0;
    for (const double value : values) {
        sum += value;
        squaredSum += value * value;
    }
    const long double mean = sum / values.size();
    const double variance = static_cast<double>(squaredSum / values.size() - mean * mean);
    out << "{\"min\":" << *minimum << ",\"max\":" << *maximum
        << ",\"mean\":" << static_cast<double>(mean)
        << ",\"standard_deviation\":" << std::sqrt(std::max(0.0, variance)) << '}';
}

void writeFieldParameters(const ParametricWangQrpField& field, std::ostream& out) {
    const auto& parameters = field.parameters();
    out << "{\"q\":" << parameters.resonanceCount
        << ",\"frequency\":" << parameters.spatialFrequency
        << ",\"phase\":[" << parameters.globalPhase.x << ',' << parameters.globalPhase.y
        << "],\"wang_phase\":[" << parameters.wangPhase.x << ',' << parameters.wangPhase.y << "]}";
}

void writeSampleImages(
    const ParameterCase& parameters,
    const RenderedCase& rendered,
    const std::filesystem::path& directory) {
    const std::string id(parameters.id);
    const auto extent = rendered.finalImage.width();
    qrp::exporting::writePng(rendered.finalImage, directory / (id + "_color.png"));
    qrp::exporting::writePng(grayscaleImage(rendered.coarseValues, extent), directory / (id + "_coarse_gray.png"));
    qrp::exporting::writePng(grayscaleImage(rendered.finalValues, extent), directory / (id + "_final_gray.png"));
}

[[nodiscard]] double readNumber(const char* argument) {
    const std::string text(argument);
    std::size_t consumed = 0;
    const double number = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(number)) {
        throw std::invalid_argument("Render arguments must be finite numbers.");
    }
    return number;
}

[[nodiscard]] ParameterCase readRenderCase(const int argc, char** argv) {
    if (argc != 13) {
        throw std::invalid_argument("--render requires directory, pixels/tile, q, frequency, A, B, medium, fine, bias, angle, cross.");
    }
    const double q = readNumber(argv[4]);
    if (q < 5.0 || q > 31.0 || q != std::floor(q) || static_cast<int>(q) % 2 == 0) {
        throw std::invalid_argument("Interactive q must be an odd integer in [5,31].");
    }
    return {"render", {readNumber(argv[6]), readNumber(argv[7])},
        static_cast<std::uint32_t>(q), readNumber(argv[5]), readNumber(argv[8]),
        readNumber(argv[9]), readNumber(argv[10]), readNumber(argv[11]), readNumber(argv[12])};
}

void runStyleStudy(
    const std::filesystem::path& outputDirectory,
    const std::span<const ParameterCase> cases,
    const bool directional,
    const qrp::model::WangGrid& grid,
    const qrp::model::WangContentWeight& weightField,
    const ParametricWangQrpField& medium,
    const ParametricWangQrpField& fine,
    const qrp::color::GradientPalette& palette) {
    constexpr std::size_t pixelsPerTile = 32;
    std::ofstream manifest;
    manifest.exceptions(std::ios::failbit | std::ios::badbit);
    manifest.open(outputDirectory / "manifest.json");
    const auto& weightParameters = weightField.parameters();
    manifest << std::setprecision(12)
        << "{\n  \"schema\":1,\n  \"study\":\"" << (directional ? "directional" : "original")
        << "\",\n  \"reference\":\"" << (directional ? "D01_reference" : "S03_reference") << "\",\n"
        << "  \"grid\":{\"width\":" << grid.width() << ",\"height\":" << grid.height()
        << ",\"pixels_per_tile\":" << pixelsPerTile << ",\"seed\":\"0x"
        << std::hex << grid.seed() << std::dec << "\"},\n"
        << "  \"wang_fixed\":{\"labels\":" << weightParameters.labelCount
        << ",\"center\":" << weightParameters.neutralWeight
        << ",\"rho\":" << weightParameters.edgeValueAmplitude
        << ",\"sigma\":" << weightParameters.transverseDerivativeAmplitude
        << ",\"radius\":" << kWeightRadius << ",\"phase_coupling\":["
        << kWangPhase.x << ',' << kWangPhase.y << "]},\n"
        << "  \"medium_fixed\":";
    writeFieldParameters(medium, manifest);
    manifest << ",\n  \"fine_fixed\":";
    writeFieldParameters(fine, manifest);
    manifest << ",\n"
        << "  \"sampling\":\"pixel centers; x east, y north; PNG top row north\",\n"
        << "  \"grayscale\":\"round(255*(0.5+0.5*scalar)); fixed [-1,1]; no normalization\",\n"
        << "  \"palette\":\"GradientPalette::createMidnightGold\",\n"
        << "  \"cases\":[\n";
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto& parameters = cases[index];
        const auto rendered = renderCase(parameters, grid, weightField, medium, fine,
            palette, {.pixelsPerTile = pixelsPerTile, .recordFinalValues = true});
        const std::string id(parameters.id);
        writeSampleImages(parameters, rendered, outputDirectory);
        manifest << "    {\"id\":\"" << id << "\",\"q\":" << parameters.resonanceCount
            << ",\"frequency\":" << parameters.spatialFrequency
            << ",\"phase\":[" << parameters.globalPhase.x << ',' << parameters.globalPhase.y
            << "],\"detail\":[" << parameters.mediumStrength << ',' << parameters.fineStrength
            << "],\"directional_bias\":" << parameters.directionalBias
            << ",\"orientation_degrees\":" << parameters.orientationDegrees
            << ",\"cross_mix\":" << parameters.crossMix
            << ",\"coarse_statistics\":";
        writeStatistics(rendered.coarseValues, manifest);
        manifest << ",\"final_statistics\":";
        writeStatistics(rendered.finalValues, manifest);
        manifest << '}' << (index + 1 < cases.size() ? ",\n" : "\n");
        std::cout << "Style study " << index + 1 << '/' << cases.size()
                  << ": " << id << '\n';
    }
    manifest << "  ]\n}\n";
    std::cout << "Wrote fixed-Wang QRP samples to " << outputDirectory.string() << '\n';
}

void writeDirectOrderRecord(
    const ParameterCase& parameters,
    const std::string_view paletteName,
    const double level,
    const RenderedCase& rendered,
    std::ostream& out) {
    out << "{\"id\":\"" << parameters.id << "\",\"model\":\"direct-interference\",\"q\":"
        << parameters.resonanceCount << ",\"frequency\":" << parameters.spatialFrequency
        << ",\"phase\":[" << parameters.globalPhase.x << ',' << parameters.globalPhase.y
        << "],\"wang_phase\":[" << parameters.wangPhase.x << ',' << parameters.wangPhase.y
        << "],\"bias\":" << parameters.directionalBias << ",\"angle\":" << parameters.orientationDegrees
        << ",\"cross\":" << parameters.crossMix << ",\"detail\":["
        << parameters.mediumStrength << ',' << parameters.fineStrength
        << "],\"palette\":\"" << paletteName << "\",\"level\":" << level << ",\"statistics\":";
    writeStatistics(rendered.finalValues, out);
    out << "},\n";
}

void runOrderStudy(
    const std::filesystem::path& directory,
    const std::size_t pixelsPerTile,
    const qrp::model::WangGrid& grid,
    const qrp::model::WangContentWeight& weightField,
    const ParametricWangQrpField& medium,
    const ParametricWangQrpField& fine) {
    const auto gold = qrp::color::GradientPalette::createMidnightGold();
    const auto noBands = qrp::color::GradientPalette::createMidnightGold(false);
    const auto ink = qrp::color::GradientPalette::createInkCream();
    const RenderOptions options{.pixelsPerTile = pixelsPerTile, .recordFinalValues = true};
    std::ofstream manifest;
    manifest.exceptions(std::ios::failbit | std::ios::badbit);
    manifest.open(directory / "manifest.json");
    manifest << std::setprecision(12)
        << "{\"schema\":1,\"study\":\"order\",\"grid_size\":20,\"seed\":\"0x4d595df4d0f33173\","
        << "\"pixels_per_tile\":" << pixelsPerTile
        << ",\"wang_fixed\":{\"labels\":5,\"center\":0.5,\"rho\":0.1,\"sigma\":0.4},"
        << "\"sampling\":\"pixel centers; x east, y north; fixed [-1,1] gray\",\"cases\":[\n";
    // 每行依次只去掉独立细节、去掉周期色带、换成简明双色，先定位原模型的问题。
    constexpr std::array<std::string_view, 8> ablationIds{
        "A01_cluster_original", "A02_cluster_no_detail", "A03_cluster_no_bands", "A04_cluster_ink",
        "A05_flow_original", "A06_flow_no_detail", "A07_flow_no_bands", "A08_flow_ink"};
    for (std::size_t index = 0; index < ablationIds.size(); ++index) {
        const std::size_t stage = index % 4;
        ParameterCase parameters{ablationIds[index], {3.0, 0.85}};
        parameters.directionalBias = index < 4 ? 0.0 : 3.0;
        if (stage > 0) {
            parameters.mediumStrength = 0.0;
            parameters.fineStrength = 0.0;
        }
        const auto& palette = stage < 2 ? gold : stage == 2 ? noBands : ink;
        const auto rendered = renderCase(parameters, grid, weightField, medium, fine, palette, options);
        writeSampleImages(parameters, rendered, directory);
        writeDirectOrderRecord(parameters,
            stage < 2 ? "gold-bands" : stage == 2 ? "gold-monotone" : "ink-cream",
            stage == 3 ? 0.2 : 0.0, rendered, manifest);
        std::cout << "Order study: " << parameters.id << '\n';
    }
    struct Candidate {
        std::string_view id;
        qrp::model::OrderedQrpParameters parameters;
    };
    // 保守候选先保留直接 QRP 造型，分别检查相位、内部耦合与方向数。
    const std::array<ParameterCase, 4> directProbes{{
        {"A09_zero_phase", {0.0, 0.0}, 7, 3.15, 0.0, 0.0},
        {"A10_weak_coupling", {0.0, 0.0}, 7, 3.15, 0.0, 0.0, 0.0, 0.0, 0.0, {0.11, -0.08}},
        {"A11_q5", {0.0, 0.0}, 5, 3.15, 0.0, 0.0, 0.0, 0.0, 0.0, {0.11, -0.08}},
        {"A12_q5_phase", {0.5, 0.15}, 5, 3.15, 0.0, 0.0, 0.0, 0.0, 0.0, {0.11, -0.08}},
    }};
    for (const auto& parameters : directProbes) {
        const auto rendered = renderCase(parameters, grid, weightField, medium, fine, ink, options);
        writeSampleImages(parameters, rendered, directory);
        writeDirectOrderRecord(parameters, "ink-cream", 0.2, rendered, manifest);
        std::cout << "Order study: " << parameters.id << '\n';
    }
    const std::array<std::string_view, 3> profileIds{
        "A13_q5_zero_level", "A14_q5_negative_level", "A15_q5_contour"};
    const std::array<qrp::color::GradientPalette, 3> profiles{
        qrp::color::GradientPalette::createInkCream(0.0),
        qrp::color::GradientPalette::createInkCream(-0.15),
        qrp::color::GradientPalette::createContourInk()};
    for (std::size_t index = 0; index < profileIds.size(); ++index) {
        auto parameters = directProbes[2];
        parameters.id = profileIds[index];
        const auto rendered = renderCase(parameters, grid, weightField, medium, fine, profiles[index], options);
        writeSampleImages(parameters, rendered, directory);
        writeDirectOrderRecord(parameters, index == 2 ? "contour-ink" : "ink-cream",
            index == 1 ? -0.15 : 0.0, rendered, manifest);
        std::cout << "Order study: " << parameters.id << '\n';
    }
    using qrp::model::OrderedMotif;
    constexpr double radians = std::numbers::pi_v<double> / 180.0;
    const std::array<Candidate, 6> candidates{{
        {"O01_periodic_spots", {7, 3.15, {3.0, 0.85}, 0.0, 0.0, 1.0, OrderedMotif::Spots}},
        {"O02_varied_spots", {7, 3.15, {3.0, 0.85}, 0.0, 0.8, 1.0, OrderedMotif::Spots}},
        {"O03_periodic_ribbons", {7, 3.15, {3.0, 0.85}, 0.0, 0.0, 1.0, OrderedMotif::Ribbons}},
        {"O04_varied_ribbons", {7, 3.15, {3.0, 0.85}, 0.0, 0.8, 1.0, OrderedMotif::Ribbons}},
        {"O05_elongated", {7, 3.15, {3.0, 0.85}, 30.0 * radians, 0.8, 0.45, OrderedMotif::Spots}},
        {"O06_diagonal", {7, 2.2, {3.0, 0.85}, 45.0 * radians, 1.0, 1.0, OrderedMotif::Spots}},
    }};
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        const auto& parameters = candidate.parameters;
        const qrp::model::OrderedQrpField field(parameters);
        const auto rendered = renderField(
            [&field](const Vec2 position, const qrp::model::WangContentWeightEvaluation weight) {
                return field.evaluate(position, weight);
            },
            [](const Vec2, const ScalarFieldEvaluation sample) { return sample; },
            grid, weightField, ink, options);
        writeSampleImages(ParameterCase{candidate.id, {}}, rendered, directory);
        manifest << "{\"id\":\"" << candidate.id << "\",\"model\":\"ordered-qrp-v1\",\"q\":"
            << parameters.resonanceCount << ",\"frequency\":" << parameters.spatialFrequency
            << ",\"phase\":[" << parameters.globalPhase.x << ',' << parameters.globalPhase.y
            << "],\"angle\":" << parameters.orientationRadians / radians
            << ",\"variation\":" << parameters.variation << ",\"aspect\":" << parameters.aspect
            << ",\"motif\":\"" << (parameters.motif == OrderedMotif::Spots ? "spots" : "ribbons")
            << "\",\"modulation_frequency_ratio\":0.28,\"wang_phase_per_frequency\":[0.035,-0.025],"
            << "\"second_phase_offset\":[1.3,-0.9],\"detail\":[0,0],\"palette\":\"ink-cream\",\"level\":0.2,\"statistics\":";
        writeStatistics(rendered.finalValues, manifest);
        manifest << '}' << (index + 1 < candidates.size() ? ",\n" : "\n");
        std::cout << "Order study: " << candidate.id << '\n';
    }
    manifest << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--tile-study") return runSourceTileStudy(argc, argv);
        if (argc > 1 && (std::string_view(argv[1]) == "--design-render"
            || std::string_view(argv[1]) == "--design-study"
            || std::string_view(argv[1]) == "--phase-study"
            || std::string_view(argv[1]) == "--control-study"
            || std::string_view(argv[1]) == "--diversity-study")) {
            return runDirectQrpDesign(argc, argv);
        }
        if (argc > 1 && std::string_view(argv[1]) == "--help") {
            std::cout << "Usage: qrp_legacy_experiment [output-directory]\n"
                      << "       qrp_legacy_experiment --style-study [output-directory]\n"
                      << "       qrp_legacy_experiment --direction-study [output-directory]\n"
                      << "       qrp_legacy_experiment --order-study [output-directory] [pixels/tile]\n"
                      << "       qrp_legacy_experiment --design-study [output-directory] [pixels/tile]\n"
                      << "       qrp_legacy_experiment --phase-study [output-directory] [pixels/tile]\n"
                      << "       qrp_legacy_experiment --control-study [output-directory] [pixels/tile]\n"
                      << "       qrp_legacy_experiment --diversity-study [output-directory] [pixels/tile]\n"
                      << "       qrp_legacy_experiment --tile-study [output-directory] [pixels/tile]\n"
                      << "       qrp_legacy_experiment --design-render directory pixels/tile frequency A B angle level inset line_width drawing\n"
                      << "       qrp_legacy_experiment --render directory pixels/tile q frequency A B medium fine bias angle cross\n";
            return 0;
        }
        const bool styleStudy = argc > 1 && std::string_view(argv[1]) == "--style-study";
        const bool directionalStudy = argc > 1 && std::string_view(argv[1]) == "--direction-study";
        const bool orderStudy = argc > 1 && std::string_view(argv[1]) == "--order-study";
        const bool singleRender = argc > 1 && std::string_view(argv[1]) == "--render";
        const auto renderParameters = singleRender ? readRenderCase(argc, argv) : ParameterCase{};
        const int directoryIndex = styleStudy || directionalStudy || orderStudy || singleRender ? 2 : 1;
        const std::filesystem::path outputDirectory = argc > directoryIndex
            ? std::filesystem::path(argv[directoryIndex])
            : std::filesystem::path(orderStudy ? "output/qrp-order-study"
                : directionalStudy ? "output/qrp-direction-study"
                : styleStudy ? "output/qrp-style-study" : "output/wang-qrp-experiment");
        std::filesystem::create_directories(outputDirectory);

        const qrp::model::WangGrid grid(kGridSize, kGridSize, 5, kGridSeed);
        const qrp::model::WangContentWeight weightField({5, 0.5, 0.10, 0.40});
        const auto medium = makeField(9, 7.4, {0.55, -0.30});
        const auto fine = makeField(11, 12.8, {-0.25, 0.45});
        const auto palette = qrp::color::GradientPalette::createMidnightGold();

        if (orderStudy) {
            const double resolution = argc > 3 ? readNumber(argv[3]) : 32.0;
            if (resolution < 16.0 || resolution > 160.0 || resolution != std::floor(resolution)) {
                throw std::invalid_argument("Study pixels/tile must be an integer in [16,160].");
            }
            runOrderStudy(outputDirectory, static_cast<std::size_t>(resolution),
                grid, weightField, medium, fine);
            return 0;
        }

        if (singleRender) {
            const double resolution = readNumber(argv[3]);
            if (resolution < 16.0 || resolution > 160.0 || resolution != std::floor(resolution)) {
                throw std::invalid_argument("Render pixels/tile must be an integer in [16,160].");
            }
            const auto rendered = renderCase(renderParameters, grid, weightField, medium, fine,
                palette, {.pixelsPerTile = static_cast<std::size_t>(resolution), .recordFinalValues = true});
            writeSampleImages(renderParameters, rendered, outputDirectory);
            return 0;
        }

        if (styleStudy || directionalStudy) {
            const auto cases = directionalStudy ? std::span<const ParameterCase>(kDirectionalCases)
                : std::span<const ParameterCase>(kStyleCases);
            runStyleStudy(outputDirectory, cases, directionalStudy, grid, weightField, medium, fine, palette);
            return 0;
        }

        std::array<RenderedCase, 4> rendered{
            renderCase(kCases[0], grid, weightField, medium, fine, palette),
            renderCase(kCases[1], grid, weightField, medium, fine, palette),
            renderCase(kCases[2], grid, weightField, medium, fine, palette),
            renderCase(kCases[3], grid, weightField, medium, fine, palette),
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
            kCases[2], grid, weightField, medium, fine, palette, {.useWangWeight = false});
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
