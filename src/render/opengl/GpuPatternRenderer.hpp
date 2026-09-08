#pragma once

#include "color/GradientPalette.hpp"
#include "color/MaterialSettings.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "render/Image.hpp"
#include "render/opengl/ShaderProgram.hpp"

#include <filesystem>
#include <array>
#include <vector>

namespace qrp::render::opengl {

enum class DebugView : int {
    Pattern = 0,
    Jacobian = 1,
    NewtonResidual = 2,
    TileEdges = 3,
};

struct Camera2D {
    double originX = 0.0;
    double originY = 0.0;
    double pixelsPerTile = 72.0;
};

struct GpuValidationBuffers {
    int width = 0;
    int height = 0;
    // OpenGL 原点位于左下；每个元素依次保存 uv/scalar/detJ 与 linear RGB/residual。
    std::vector<std::array<float, 4>> diagnostics;
    std::vector<std::array<float, 4>> linearColorAndResidual;
};

class GpuPatternRenderer final {
public:
    explicit GpuPatternRenderer(const std::filesystem::path& shaderDirectory);
    ~GpuPatternRenderer();

    GpuPatternRenderer(const GpuPatternRenderer&) = delete;
    GpuPatternRenderer& operator=(const GpuPatternRenderer&) = delete;

    void uploadScene(
        const model::WangGrid& grid,
        const model::EdgePalette& edgePalette,
        const generators::HybridTorusGenerator& generator,
        const color::GradientPalette& colorPalette,
        const color::MaterialSettings& material);
    void draw(
        int framebufferWidth,
        int framebufferHeight,
        const Camera2D& camera,
        DebugView debugView,
        bool enableMaterial = true) const;
    // 离屏导出复用 draw()；这里仅负责目标纹理和 OpenGL/图像坐标系转换。
    [[nodiscard]] render::Image renderImage(
        int width,
        int height,
        const Camera2D& camera,
        DebugView debugView = DebugView::Pattern,
        bool enableMaterial = true) const;
    [[nodiscard]] GpuValidationBuffers renderValidationBuffers(
        int width,
        int height,
        const Camera2D& camera) const;

private:
    [[nodiscard]] GLint uniformLocation(const char* name) const;

    ShaderProgram program_;
    GLuint vertexArray_ = 0;
    GLuint tileBuffer_ = 0;
    GLuint gradientTexture_ = 0;
    color::MaterialSettings material_;
    bool sceneUploaded_ = false;
};

} // namespace qrp::render::opengl
