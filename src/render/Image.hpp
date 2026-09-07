#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace qrp::render {

struct Rgb8 {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

class Image final {
public:
    Image(std::size_t width, std::size_t height);

    [[nodiscard]] std::size_t width() const noexcept;
    [[nodiscard]] std::size_t height() const noexcept;
    [[nodiscard]] Rgb8& pixel(std::size_t x, std::size_t y);
    [[nodiscard]] const Rgb8& pixel(std::size_t x, std::size_t y) const;
    [[nodiscard]] const std::vector<Rgb8>& pixels() const noexcept;

private:
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    std::vector<Rgb8> pixels_;
};

} // namespace qrp::render
