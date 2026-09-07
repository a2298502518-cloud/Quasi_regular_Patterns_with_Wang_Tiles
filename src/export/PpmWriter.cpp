#include "export/PpmWriter.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace qrp::exporting {

void writePpm(const render::Image& image, const std::filesystem::path& path) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open the PPM output path.");
    }

    stream << "P6\n" << image.width() << ' ' << image.height() << "\n255\n";
    std::vector<char> bytes;
    bytes.reserve(image.pixels().size() * 3U);
    for (const auto pixel : image.pixels()) {
        bytes.push_back(static_cast<char>(pixel.red));
        bytes.push_back(static_cast<char>(pixel.green));
        bytes.push_back(static_cast<char>(pixel.blue));
    }
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("Failed while writing the PPM image.");
    }
}

} // namespace qrp::exporting
