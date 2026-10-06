#pragma once
#include "qrp/Field.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace qrp {
using Color = std::array<unsigned char, 3>;
struct Palette {
    std::vector<double> levels;
    std::vector<Color> colors;
    bool operator==(const Palette&) const = default;
};
struct Image {
    int width{}, height{};
    std::vector<unsigned char> rgb; // RGB8、sRGB、图像首行在上。
};
Palette loadPalette(const std::filesystem::path& defaults);
int rasterSide(int span, int pixelsPerUnit);
Image renderWang(const Atlas& atlas, const Layout& layout, const Palette& palette, int pixelsPerUnit);
Image renderSource(const Source& source, const Palette& palette, int span, int pixelsPerUnit);
void savePng(const Image& image, const std::filesystem::path& path);
nlohmann::json parametersJson(const Parameters& parameters);
nlohmann::json atlasJson(const Atlas& atlas);
Parameters parametersFromJson(const nlohmann::json& json);
}
