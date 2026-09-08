#include "export/PngWriter.hpp"

#include <lodepng.h>

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace qrp::exporting {
namespace {

[[nodiscard]] std::vector<std::uint8_t> contiguousRgb(const render::Image& image) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(image.pixels().size() * 3U);
    for (const auto pixel : image.pixels()) {
        bytes.push_back(pixel.red);
        bytes.push_back(pixel.green);
        bytes.push_back(pixel.blue);
    }
    return bytes;
}

} // namespace

std::vector<std::uint8_t> encodePng(const render::Image& image) {
    if (image.width() > std::numeric_limits<unsigned>::max()
        || image.height() > std::numeric_limits<unsigned>::max()) {
        throw std::length_error("PNG dimensions exceed the encoder limit.");
    }

    const auto rgb = contiguousRgb(image);
    lodepng::State state;
    state.info_raw.colortype = LCT_RGB;
    state.info_raw.bitdepth = 8;
    state.info_png.color.colortype = LCT_RGB;
    state.info_png.color.bitdepth = 8;
    state.encoder.auto_convert = 0;
    // Shader 输出已经编码为 sRGB；显式 chunk 防止查看器按未知设备色彩解释。
    state.info_png.srgb_defined = 1;
    state.info_png.srgb_intent = 0;
    state.info_png.gama_defined = 1;
    state.info_png.gama_gamma = 45455;
    const unsigned textError = lodepng_add_text(
        &state.info_png,
        "Software",
        "Quasi-regular Patterns with Wang Tiles");
    if (textError != 0) {
        throw std::runtime_error(
            "Failed to create PNG metadata: " + std::string(lodepng_error_text(textError)));
    }

    std::vector<std::uint8_t> encoded;
    const unsigned error = lodepng::encode(
        encoded,
        rgb,
        static_cast<unsigned>(image.width()),
        static_cast<unsigned>(image.height()),
        state);
    if (error != 0) {
        throw std::runtime_error(
            "Failed to encode PNG: " + std::string(lodepng_error_text(error)));
    }
    return encoded;
}

void writePng(const render::Image& image, const std::filesystem::path& path) {
    const auto encoded = encodePng(image);
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open the PNG output path.");
    }
    stream.write(
        reinterpret_cast<const char*>(encoded.data()),
        static_cast<std::streamsize>(encoded.size()));
    if (!stream) {
        throw std::runtime_error("Failed while writing the PNG image.");
    }
}

} // namespace qrp::exporting
