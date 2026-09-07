#include "render/Image.hpp"

#include <limits>
#include <stdexcept>

namespace qrp::render {

Image::Image(const std::size_t width, const std::size_t height)
    : width_(width),
      height_(height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Image dimensions must be positive.");
    }
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        throw std::length_error("Image dimensions overflow the pixel count.");
    }
    pixels_.resize(width * height);
}

std::size_t Image::width() const noexcept {
    return width_;
}

std::size_t Image::height() const noexcept {
    return height_;
}

Rgb8& Image::pixel(const std::size_t x, const std::size_t y) {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Image pixel coordinates are out of range.");
    }
    return pixels_[y * width_ + x];
}

const Rgb8& Image::pixel(const std::size_t x, const std::size_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Image pixel coordinates are out of range.");
    }
    return pixels_[y * width_ + x];
}

const std::vector<Rgb8>& Image::pixels() const noexcept {
    return pixels_;
}

} // namespace qrp::render
