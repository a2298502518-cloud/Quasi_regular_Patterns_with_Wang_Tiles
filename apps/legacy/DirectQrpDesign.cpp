#include "DirectQrpDesign.hpp"

#include "export/PngWriter.hpp"
#include "color/InkCoverage.hpp"
#include "model/ParametricWangQrpField.hpp"
#include "model/QrpChannelComposition.hpp"
#include "render/ContourGeometry.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kGridSize = 20;
constexpr std::size_t kGeometrySamplesPerTile = 64;
constexpr std::uint64_t kSeed = 0x4d595df4d0f33173ULL;
using qrp::model::QrpChannelRelation;

struct DesignParameters {
    double frequency = 3.15;
    double phaseA = 0.0;
    double phaseB = 0.0;
    double angle = 0.0;
    double level = 0.0;
    double inset = 0.025;
    double lineWidth = 0.016;
    int drawing = 0;
    bool highRegion = false;
    double commonPhaseDegrees = 0.0;
    double directionalBias = 0.0;
    double crossMix = 0.0;
    QrpChannelRelation relation = QrpChannelRelation::Direct;
    double secondFrequencyRatio = 1.0;
    double secondAngle = 90.0;
    double secondCommonPhaseDegrees = 0.0;
    double secondDirectionalBias = 1.5;
    double parentLevel = 0.0;
    double gateWidth = 0.35;
};

struct DesignImages {
    qrp::render::Image color;
    qrp::render::Image field;
    qrp::render::Image mask;
    std::size_t segments;
    double fieldAnisotropy;
    double tangentAxisDegrees;
};

void validate(const DesignParameters& p) {
    if (!(p.frequency >= 0.8 && p.frequency <= 8.0)
        || !(std::abs(p.phaseA) <= 0.6) || !(std::abs(p.phaseB) <= 0.6)
        || !(std::abs(p.angle) <= 90.0) || !(std::abs(p.level) <= 0.7)
        || !(p.inset >= 0.0 && p.inset <= 0.06)
        || !(p.lineWidth >= 0.01 && p.lineWidth <= 0.06)
        || !(std::abs(p.commonPhaseDegrees) <= 180.0)
        || !(p.directionalBias >= 0.0 && p.directionalBias <= 4.0)
        || !(p.crossMix >= 0.0 && p.crossMix <= 0.5)
        || (p.drawing != 0 && p.drawing != 1)) {
        throw std::invalid_argument("Design parameters are outside the documented A-route study range.");
    }
}

[[nodiscard]] qrp::model::ParametricWangQrpField makeChannel(const DesignParameters& p, const bool second) {
    const double frequency = p.frequency * (second ? p.secondFrequencyRatio : 1.0);
    return qrp::model::ParametricWangQrpField({5, frequency, {p.phaseA, p.phaseB},
        {0.11 * frequency / 3.15, -0.08 * frequency / 3.15}, 0.5, 0.4,
        second ? p.secondDirectionalBias : p.directionalBias,
        (second ? p.secondAngle : p.angle) * std::numbers::pi_v<double> / 180.0,
        second ? 0.0 : p.crossMix,
        (second ? p.secondCommonPhaseDegrees : p.commonPhaseDegrees) * std::numbers::pi_v<double> / 180.0});
}

[[nodiscard]] qrp::model::WangContentWeightEvaluation designWeight(
    const qrp::math::Vec2 position, const qrp::model::WangGrid& grid,
    const qrp::model::WangContentWeight& weights) {
    const int x = static_cast<int>(std::floor(position.x));
    const int y = static_cast<int>(std::floor(position.y));
    qrp::model::WangTile tile;
    if (x >= 0 && x < 20 && y >= 0 && y < 20) {
        tile = grid.tile(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
    } else {
        // 外扩一圈合法标签，保留原 20×20 内部完全不变。仅用来正确裁切边缘的偏移轮廓。
        // 外圈相互共享的边统一取标签 0，与内部相接的边复制原边标签。
        if (x == -1 && y >= 0 && y < 20) tile.east = grid.tile(0, static_cast<std::size_t>(y)).west;
        if (x == 20 && y >= 0 && y < 20) tile.west = grid.tile(19, static_cast<std::size_t>(y)).east;
        if (y == -1 && x >= 0 && x < 20) tile.north = grid.tile(static_cast<std::size_t>(x), 0).south;
        if (y == 20 && x >= 0 && x < 20) tile.south = grid.tile(static_cast<std::size_t>(x), 19).north;
    }
    return weights.evaluate(tile, {position.x - x, position.y - y});
}

[[nodiscard]] DesignImages renderDesign(const DesignParameters& p, const std::size_t pixelsPerTile) {
    validate(p);
    const qrp::model::WangGrid grid(kGridSize, kGridSize, 5, kSeed);
    const qrp::model::WangContentWeight weights({5, 0.5, 0.1, 0.4});
    // 保持上轮 A 路线在 κ=3.15 的值；其他频率按比例缩放内部耦合，避免低频轮廓被扰动主导。
    const auto field = makeChannel(p, false);
    const auto secondField = makeChannel(p, true);
    const qrp::model::QrpChannelComposition composition({p.relation, p.parentLevel, p.gateWidth});
    constexpr std::size_t cells = (kGridSize + 2) * kGeometrySamplesPerTile;
    constexpr double spacing = 1.0 / kGeometrySamplesPerTile;
    std::vector<double> values((cells + 1) * (cells + 1));
    double jxx = 0.0, jxy = 0.0, jyy = 0.0;
    for (std::size_t y = 0; y <= cells; ++y) {
        for (std::size_t x = 0; x <= cells; ++x) {
            const qrp::math::Vec2 point{-1.0 + spacing * static_cast<double>(x), -1.0 + spacing * static_cast<double>(y)};
            const auto weight = designWeight(point, grid, weights);
            const auto first = field.evaluate(point, weight);
            const auto sample = p.relation == QrpChannelRelation::Direct ? first
                : composition.evaluate(first, secondField.evaluate(point, weight));
            values[y * (cells + 1) + x] = sample.value;
            // 仅在成图窗口统计解析梯度方向，不把边缘外扩区或色带当作结构。
            if (x >= kGeometrySamplesPerTile && x < (kGridSize + 1) * kGeometrySamplesPerTile
                && y >= kGeometrySamplesPerTile && y < (kGridSize + 1) * kGeometrySamplesPerTile) {
                jxx += sample.gradient.x * sample.gradient.x;
                jxy += sample.gradient.x * sample.gradient.y;
                jyy += sample.gradient.y * sample.gradient.y;
            }
        }
    }
    const qrp::render::ContourGeometry geometry(cells, spacing, {-1.0, -1.0}, std::move(values), p.level);
    const std::size_t extent = kGridSize * pixelsPerTile;
    const double anisotropy = (jxx + jyy) > 0.0 ? std::hypot(jxx - jyy, 2.0 * jxy) / (jxx + jyy) : 0.0;
    const double tangentAxis = std::fmod(0.5 * std::atan2(2.0 * jxy, jxx - jyy)
        * 180.0 / std::numbers::pi_v<double> + 270.0, 180.0);
    DesignImages result{qrp::render::Image(extent, extent), qrp::render::Image(extent, extent),
        qrp::render::Image(extent, extent), geometry.segmentCount(), anisotropy, tangentAxis};
    const double pixelSize = 1.0 / static_cast<double>(pixelsPerTile);
    const double characteristicLength = 2.0 * std::numbers::pi_v<double> / p.frequency;
    const double inset = p.inset * characteristicLength;
    const double halfWidth = 0.5 * p.lineWidth * characteristicLength;
    const double searchRadius = inset + (p.drawing == 1 ? halfWidth : 0.0) + pixelSize;
    for (std::size_t y = 0; y < extent; ++y) {
        const double worldY = 20.0 - (static_cast<double>(y) + 0.5) * pixelSize;
        const auto row = geometry.scanline(worldY);
        bool inside = p.highRegion ? !row.insideAtLeft : row.insideAtLeft;
        std::size_t crossing = 0;
        for (std::size_t x = 0; x < extent; ++x) {
            const qrp::math::Vec2 point{(static_cast<double>(x) + 0.5) * pixelSize, worldY};
            while (crossing < row.crossings.size() && row.crossings[crossing] <= point.x) {
                inside = !inside;
                ++crossing;
            }
            const double distance = (inside ? 1.0 : -1.0) * geometry.distance(point, searchRadius);
            const double margin = p.drawing == 0 ? distance - inset : halfWidth - std::abs(distance - inset);
            const double coverage = std::clamp(0.5 + margin / pixelSize, 0.0, 1.0);
            result.color.pixel(x, y) = qrp::color::inkCoverage(coverage);
            const auto gray = static_cast<std::uint8_t>(std::lround(255.0
                * std::clamp(0.5 + 0.5 * geometry.sample(point), 0.0, 1.0)));
            result.field.pixel(x, y) = {gray, gray, gray};
            const auto mask = static_cast<std::uint8_t>(std::lround(255.0 * (1.0 - coverage)));
            result.mask.pixel(x, y) = {mask, mask, mask};
        }
    }
    return result;
}

void save(const DesignImages& images, const std::filesystem::path& directory, const std::string& id,
    const bool composedStudy = false) {
    qrp::exporting::writePng(images.color, directory / (id + "_color.png"));
    qrp::exporting::writePng(images.field, directory / (id + (composedStudy ? "_field_gray.png" : "_coarse_gray.png")));
    qrp::exporting::writePng(images.mask, directory / (id + "_mask.png"));
}

[[nodiscard]] double number(const char* arg) {
    std::size_t consumed = 0;
    const std::string text(arg);
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value)) throw std::invalid_argument("Expected finite numeric design argument.");
    return value;
}

void writeParameters(const DesignParameters& p, std::ostream& out) {
    out << "{\"frequency\":" << p.frequency << ",\"phase_a\":" << p.phaseA << ",\"phase_b\":" << p.phaseB
        << ",\"angle\":" << p.angle << ",\"level\":" << p.level << ",\"inset\":" << p.inset
        << ",\"line_width\":" << p.lineWidth << ",\"drawing\":" << p.drawing
        << ",\"region\":\"" << (p.highRegion ? "above" : "below")
        << "\",\"common_phase_degrees\":" << p.commonPhaseDegrees
        << ",\"directional_bias\":" << p.directionalBias << ",\"cross_mix\":" << p.crossMix
        << ",\"relation\":\"" << relationName(p.relation) << "\",\"second_frequency_ratio\":" << p.secondFrequencyRatio
        << ",\"second_angle\":" << p.secondAngle << ",\"second_common_phase_degrees\":" << p.secondCommonPhaseDegrees
        << ",\"second_directional_bias\":" << p.secondDirectionalBias
        << ",\"parent_level\":" << p.parentLevel << ",\"gate_width\":" << p.gateWidth << '}';
}

} // namespace

int runDirectQrpDesign(const int argc, char** argv) {
    const bool phaseStudy = std::string_view(argv[1]) == "--phase-study";
    const bool controlStudy = std::string_view(argv[1]) == "--control-study";
    const bool diversityStudy = std::string_view(argv[1]) == "--diversity-study";
    const bool study = phaseStudy || controlStudy || diversityStudy || std::string_view(argv[1]) == "--design-study";
    if (!study && argc != 12) throw std::invalid_argument(
        "--design-render requires directory, pixels/tile, frequency, A, B, angle, level, inset, line_width, drawing.");
    const std::filesystem::path directory = argc > 2 ? argv[2]
        : (diversityStudy ? "output/qrp-diversity-study"
            : (phaseStudy ? "output/qrp-phase-study" : (controlStudy ? "output/qrp-control-study" : "output/qrp-design-study")));
    const double resolution = argc > 3 ? number(argv[3]) : 32.0;
    if (resolution < 16 || resolution > 160 || resolution != std::floor(resolution)) {
        throw std::invalid_argument("Design pixels/tile must be an integer in [16,160].");
    }
    const auto pixels = static_cast<std::size_t>(resolution);
    std::filesystem::create_directories(directory);
    if (!study) {
        const double drawing = number(argv[11]);
        if (drawing != 0.0 && drawing != 1.0) throw std::invalid_argument("Drawing must be 0 (fill) or 1 (outline).");
        const DesignParameters parameters{number(argv[4]), number(argv[5]), number(argv[6]), number(argv[7]),
            number(argv[8]), number(argv[9]), number(argv[10]), static_cast<int>(drawing)};
        const auto images = renderDesign(parameters, pixels);
        save(images, directory, "render");
        std::cout << "A-route contour segments: " << images.segments << '\n';
        return 0;
    }
    struct Case { const char* id; DesignParameters p; };
    std::vector<Case> cases{
        {"T01_no_inset", {3.15, 0, 0, 0, 0, 0}},
        {"T02_inset", {}},
        {"T03_outline", {3.15, 0, 0, 0, 0, 0, 0.016, 1}},
        {"T04_inset_outline", {3.15, 0, 0, 0, 0, 0.025, 0.016, 1}},
        {"T05_sparse", {1.8}},
        {"T06_dense", {4.8}},
        {"T07_phase_positive", {3.15, 0.6, 0.6}},
        {"T08_phase_opposed", {3.15, 0.6, -0.6}},
        {"T09_phase_negative", {3.15, -0.6, -0.6}},
        {"T10_level_negative", {3.15, 0, 0, 0, -0.12}},
        {"T11_level_positive", {3.15, 0, 0, 0, 0.12}},
        {"T12_rotated", {3.15, 0.3, -0.2, 30, 0, 0.025, 0.016, 1}},
        {"U01_flower", {3.15, 0, 0, 0, -0.08, 0.025, 0.024, 0}},
        {"U02_open", {2.4, 0, 0, 0, -0.12, 0.035, 0.024, 0}},
        {"U03_line", {3.15, 0, 0, 0, -0.08, 0.025, 0.024, 1}},
        {"U04_slanted", {3.15, 0.3, -0.2, 30, -0.08, 0.025, 0.024, 1}},
        // 同一个 QRP 场的区域选择对照，不用收缩或描边改善观感。
        {"V01_low_02", {3.15, 0, 0, 0, -0.2, 0}},
        {"V02_low_035", {3.15, 0, 0, 0, -0.35, 0}},
        {"V03_low_05", {3.15, 0, 0, 0, -0.5, 0}},
        {"V04_high_0", {3.15, 0, 0, 0, 0, 0, 0.016, 0, true}},
        {"V05_high_02", {3.15, 0, 0, 0, 0.2, 0, 0.016, 0, true}},
        {"V06_high_035", {3.15, 0, 0, 0, 0.35, 0, 0.016, 0, true}},
        {"V07_high_05", {3.15, 0, 0, 0, 0.5, 0, 0.016, 0, true}},
        {"V08_high_065", {3.15, 0, 0, 0, 0.65, 0, 0.016, 0, true}},
    };
    if (phaseStudy) {
        cases = {
            {"M01_reference", {3.15, 0, 0, 0, 0, 0}},
            {"M02_phase_a", {3.15, 0.3, 0, 0, 0, 0}},
            {"M03_phase_b", {3.15, 0, 0.3, 0, 0, 0}},
            {"M04_phase_ab", {3.15, 0.3, 0.3, 0, 0, 0}},
            {"M05_common_30", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 30}},
            {"M06_common_60", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 60}},
            {"M07_common_90", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 90}},
            {"M08_common_120", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 120}},
        };
    }
    if (controlStudy) {
        cases = {
            {"N01_reference", {3.15, 0, 0, 0, 0, 0}},
            {"N02_bias_05", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 0, 0.5}},
            {"N03_bias_10", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 0, 1.0}},
            {"N04_bias_15", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 0, 1.5}},
            {"N05_phase_60", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 60, 0}},
            {"N06_bias_05_phase_60", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 60, 0.5}},
            {"N07_bias_10_phase_60", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 60, 1.0}},
            {"N08_bias_15_phase_60", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 60, 1.5}},
            {"N09_rotated", {3.15, 0, 0, 30, 0, 0, 0.016, 0, false, 0, 1.0}},
            {"N10_cross_025", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 0, 1.5, 0.25}},
            {"N11_cross_05", {3.15, 0, 0, 0, 0, 0, 0.016, 0, false, 0, 1.5, 0.5}},
            {"N12_reference_outline", {3.15, 0, 0, 0, 0, 0, 0.024, 1}},
        };
    }
    if (diversityStudy) {
        DesignParameters reference;
        reference.inset = 0.0;
        auto ribbon = reference;
        ribbon.directionalBias = 1.5;
        auto product = ribbon;
        product.relation = QrpChannelRelation::Product;
        auto network = product;
        network.drawing = 1;
        network.lineWidth = 0.035;
        auto cells = product;
        cells.relation = QrpChannelRelation::JointEnergy;
        cells.level = 0.65;
        auto cellsOpen = cells;
        cellsOpen.level = 0.5;
        auto nested = reference;
        nested.frequency = 1.15;
        nested.relation = QrpChannelRelation::Nested;
        nested.secondFrequencyRatio = 3.5;
        nested.secondAngle = 0.0;
        nested.secondDirectionalBias = 0.0;
        auto nestedShift = nested;
        nestedShift.secondCommonPhaseDegrees = 60.0;
        auto parent = nested;
        parent.relation = QrpChannelRelation::Direct;
        auto child = reference;
        child.frequency = nested.frequency * nested.secondFrequencyRatio;
        // 验证父子尺度分离与子结构类型，而不是继续扫描同一个环瓣场的相位。
        auto fineClusters = nested;
        fineClusters.secondFrequencyRatio = 6.0;
        auto stripedClusters = fineClusters;
        stripedClusters.secondDirectionalBias = 1.5;
        auto stripeChild = ribbon;
        stripeChild.frequency = stripedClusters.frequency * stripedClusters.secondFrequencyRatio;
        cases = {
            {"X01_rings", reference}, {"X02_ribbons", ribbon},
            {"X03_product_fill", product}, {"X04_product_network", network},
            {"X05_energy_cells", cells}, {"X06_energy_open", cellsOpen},
            {"X07_nested", nested}, {"X08_nested_phase", nestedShift},
            {"X09_parent", parent}, {"X10_child", child},
            {"X11_fine_clusters", fineClusters}, {"X12_striped_clusters", stripedClusters},
            {"X13_stripe_child", stripeChild},
        };
    }
    std::ofstream manifest;
    manifest.exceptions(std::ios::badbit | std::ios::failbit);
    manifest.open(directory / "manifest.json");
    const char* studyName = diversityStudy ? "diversity" : (controlStudy ? "control" : (phaseStudy ? "phase" : "design"));
    const char* modelName = diversityStudy ? "qrp-channel-relations-v1"
        : (controlStudy ? "direct-qrp-control-v1" : (phaseStudy ? "direct-qrp-phase-v1" : "direct-qrp-design-v1"));
    manifest << std::setprecision(12) << "{\"study\":\"" << studyName
        << "\",\"model\":\"" << modelName << "\",\"q\":5,"
        << "\"geometry_samples_per_tile\":64,\"geometry_domain\":[-1,21],\"output_domain\":[0,20],"
        << "\"seed\":\"0x4d595df4d0f33173\",\"wang\":{\"labels\":5,\"center\":0.5,\"rho\":0.1,\"sigma\":0.4},"
        << "\"internal_coupling\":\"(0.11,-0.08)*each_channel_frequency/3.15\",\"pixels_per_tile\":" << pixels << ",\"cases\":[\n";
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto images = renderDesign(cases[i].p, pixels);
        save(images, directory, cases[i].id, diversityStudy);
        manifest << "{\"id\":\"" << cases[i].id << "\",\"parameters\":";
        writeParameters(cases[i].p, manifest);
        manifest << ",\"segments\":" << images.segments
            << ",\"field_anisotropy\":" << images.fieldAnisotropy
            << ",\"tangent_axis_degrees\":" << images.tangentAxisDegrees
            << '}' << (i + 1 < cases.size() ? ",\n" : "\n");
        std::cout << "Design study: " << cases[i].id << " (" << images.segments << " segments)" << std::endl;
    }
    manifest << "]}\n";
    return 0;
}
