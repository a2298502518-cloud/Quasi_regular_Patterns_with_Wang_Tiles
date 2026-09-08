#include "export/ProjectMetadataWriter.hpp"

#include "color/GradientPalette.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace qrp::exporting {
namespace {

[[nodiscard]] std::string escapeJson(const std::string_view value) {
    std::ostringstream stream;
    for (const unsigned char character : value) {
        switch (character) {
        case '"': stream << "\\\""; break;
        case '\\': stream << "\\\\"; break;
        case '\b': stream << "\\b"; break;
        case '\f': stream << "\\f"; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default:
            if (character < 0x20U) {
                stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec;
            } else {
                stream << static_cast<char>(character);
            }
        }
    }
    return stream.str();
}

void requireFiniteView(const ExportView& view) {
    if (view.width == 0 || view.height == 0
        || !std::isfinite(view.originX)
        || !std::isfinite(view.originY)
        || !std::isfinite(view.pixelsPerTile)
        || view.pixelsPerTile <= 0.0) {
        throw std::invalid_argument("Export view must be finite and non-empty.");
    }
}

} // namespace

std::string serializeProjectMetadata(
    const project::PatternConfiguration& configuration,
    const std::uint64_t revision,
    const ExportView& view) {
    requireFiniteView(view);
    std::ostringstream stream;
    stream << std::setprecision(17);
    stream << "{\n"
           << "  \"schemaVersion\": 1,\n"
           << "  \"projectRevision\": " << revision << ",\n"
           << "  \"name\": \"" << escapeJson(configuration.name) << "\",\n"
           << "  \"image\": {\"width\": " << view.width
           << ", \"height\": " << view.height
           << ", \"encoding\": \"sRGB\", \"materialEffects\": "
           << (view.materialEffects ? "true" : "false") << "},\n"
           << "  \"camera\": {\"originX\": " << view.originX
           << ", \"originY\": " << view.originY
           << ", \"pixelsPerTile\": " << view.pixelsPerTile << "},\n"
           << "  \"grid\": {\"width\": " << configuration.gridWidth
           << ", \"height\": " << configuration.gridHeight
           << ", \"pixelsPerTile\": " << configuration.pixelsPerTile
           << ", \"seedHex\": \"0x" << std::hex << std::setw(16) << std::setfill('0')
           << configuration.gridSeed << std::dec << std::setfill(' ') << "\"},\n"
           << "  \"edgeColors\": [\n";
    for (std::size_t index = 0; index < configuration.edgeColors.size(); ++index) {
        const auto edge = configuration.edgeColors[index];
        stream << "    {\"epsilon\": " << edge.epsilon
               << ", \"delta\": " << edge.delta << "}"
               << (index + 1 == configuration.edgeColors.size() ? "\n" : ",\n");
    }
    const auto& generator = configuration.generator;
    const auto fourier = generators::TorusFourier::createQuasiRegular();
    const generators::PeriodicGradientNoise noise;
    const auto& noiseSettings = noise.settings();
    stream << "  ],\n"
           << "  \"generator\": {\n"
           << "    \"model\": \"hybrid_torus_v4\",\n"
           << "    \"scalarProfile\": \""
           << generators::scalarProfileName(generator.scalarProfile) << "\",\n"
           << "    \"fourierWeight\": " << generator.fourierWeight << ",\n"
           << "    \"noiseWeight\": " << generator.noiseWeight << ",\n"
           << "    \"tileVariationAmplitude\": " << generator.tileVariationAmplitude << ",\n"
           << "    \"tileDomainWarpAmplitude\": " << generator.tileDomainWarpAmplitude << ",\n"
           << "    \"worldModulationAmplitude\": " << generator.worldModulationAmplitude << ",\n"
           << "    \"worldDomainWarpAmplitude\": " << generator.worldDomainWarpAmplitude << ",\n"
           << "    \"worldFrequencyX\": " << generator.worldFrequencyX << ",\n"
           << "    \"worldFrequencyY\": " << generator.worldFrequencyY << ",\n"
           << "    \"worldDetailAmplitude\": " << generator.worldDetailAmplitude << ",\n"
           << "    \"worldGrainAmplitude\": " << generator.worldGrainAmplitude << ",\n"
           << "    \"cellularScale\": " << generator.cellularScale << ",\n"
           << "    \"edgeStructureAmplitude\": "
           << generator.edgeStructureAmplitude << ",\n"
           << "    \"fourierModes\": [\n";
    for (std::size_t index = 0; index < fourier.modes().size(); ++index) {
        const auto& mode = fourier.modes()[index];
        stream << "      {\"frequencyU\": " << mode.frequencyU
               << ", \"frequencyV\": " << mode.frequencyV
               << ", \"amplitude\": " << mode.amplitude
               << ", \"phase\": " << mode.phase << "}"
               << (index + 1 == fourier.modes().size() ? "\n" : ",\n");
    }
    stream << "    ],\n"
           << "    \"periodicNoise\": {\"baseFrequency\": " << noiseSettings.baseFrequency
           << ", \"octaves\": " << noiseSettings.octaves
           << ", \"persistence\": " << noiseSettings.persistence
           << ", \"seedHex\": \"0x" << std::hex << std::setw(16) << std::setfill('0')
           << noiseSettings.seed << std::dec << std::setfill(' ') << "\"}\n"
           << "  },\n"
           << "  \"color\": {\n"
           << "    \"interpolation\": \"Oklab\",\n"
           << "    \"tone\": {\"center\": " << configuration.tone.center
           << ", \"contrast\": " << configuration.tone.contrast
           << ", \"bandFrequency\": " << configuration.tone.bandFrequency
           << ", \"bandStrength\": " << configuration.tone.bandStrength
           << ", \"posterizeLevels\": " << configuration.tone.posterizeLevels
           << ", \"posterizeSoftness\": " << configuration.tone.posterizeSoftness
           << "},\n"
           << "    \"stops\": [\n";
    for (std::size_t index = 0; index < configuration.colorStops.size(); ++index) {
        const auto& stop = configuration.colorStops[index];
        const auto srgb = color::srgbFromLinear(stop.color);
        stream << "      {\"position\": " << stop.position
               << ", \"linearRgb\": [" << stop.color.red << ", "
               << stop.color.green << ", " << stop.color.blue << "]"
               << ", \"srgb\": [" << srgb.red << ", " << srgb.green
               << ", " << srgb.blue << "]}"
               << (index + 1 == configuration.colorStops.size() ? "\n" : ",\n");
    }
    const auto& material = configuration.material;
    stream << "    ]\n"
           << "  },\n"
           << "  \"material\": {\"contourFrequency\": " << material.contourFrequency
           << ", \"contourStrength\": " << material.contourStrength
           << ", \"contourWidth\": " << material.contourWidth
           << ", \"reliefStrength\": " << material.reliefStrength << "}\n"
           << "}\n";
    return stream.str();
}

void writeProjectMetadata(
    const project::PatternConfiguration& configuration,
    const std::uint64_t revision,
    const ExportView& view,
    const std::filesystem::path& path) {
    const auto text = serializeProjectMetadata(configuration, revision, view);
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open the export metadata path.");
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        throw std::runtime_error("Failed while writing export metadata.");
    }
}

} // namespace qrp::exporting
