#include "render/ContourGeometry.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void near(const double a, const double b, const double tolerance) {
    if (!std::isfinite(a) || std::abs(a - b) > tolerance) throw std::runtime_error("Contour geometry mismatch");
}

void lineAndScale() {
    std::vector<double> samples;
    for (int y = 0; y <= 4; ++y) {
        for (int x = 0; x <= 4; ++x) samples.push_back(x * 0.5 - 0.8);
    }
    const qrp::render::ContourGeometry line(4, 0.5, {0, 0}, samples, 0);
    for (auto& sample : samples) sample *= 17.0;
    const qrp::render::ContourGeometry scaled(4, 0.5, {0, 0}, samples, 0);
    for (const double y : {0.25, 1.0, 1.75}) {
        const auto row = line.scanline(y);
        if (!row.insideAtLeft || row.crossings.size() != 1) throw std::runtime_error("Line parity mismatch");
        near(row.crossings[0], 0.8, 1e-14);
        near(line.distance({0.5, y}, 1.0), 0.3, 1e-14);
        near(scaled.distance({0.5, y}, 1.0), 0.3, 1e-14);
    }
}

void saddleAndCircle() {
    // 确切鞍点保留交叉；不同判别符号选择不同连接，不能恒用一条对角线。
    const qrp::render::ContourGeometry saddle(1, 1.0, {0, 0}, {1, -1, -1, 1}, 0);
    near(saddle.distance({0.5, 0.5}, 1.0), 0, 1e-14);
    const qrp::render::ContourGeometry positive(1, 1.0, {0, 0}, {2, -1, -1, 2}, 0);
    const qrp::render::ContourGeometry negative(1, 1.0, {0, 0}, {1, -2, -2, 1}, 0);
    near(positive.distance({0.9, 0.1}, 1), negative.distance({0.1, 0.1}, 1), 1e-14);
    std::vector<double> values;
    for (int y = 0; y <= 128; ++y) {
        for (int x = 0; x <= 128; ++x) {
            const double px = x / 32.0 - 2.0, py = y / 32.0 - 2.0;
            values.push_back(px * px + py * py - 1.0);
        }
    }
    const qrp::render::ContourGeometry circle(128, 1.0 / 32.0, {-2, -2}, std::move(values), 0);
    near(circle.distance({0, 0}, 2), 1.0, 0.0003);
    near(circle.distance({0.6, 0.8}, 1), 0, 0.0003);
    const auto row = circle.scanline(0.0);
    if (row.insideAtLeft || row.crossings.size() != 2) throw std::runtime_error("Circle scanline mismatch");
    near(row.crossings.front(), -1, 1e-14);
    near(row.crossings.back(), 1, 1e-14);
}
} // namespace

int main() {
    try { lineAndScale(); saddleAndCircle(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "Contour distance, parity and saddle checks passed\n";
}
