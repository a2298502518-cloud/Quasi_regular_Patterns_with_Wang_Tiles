#include "render/opengl/GpuPatternRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace qrp::render::opengl {
namespace {

constexpr std::size_t maximumEdgeColors = 16;
constexpr std::size_t maximumFourierModes = 16;
constexpr std::size_t maximumColorStops = 8;

struct alignas(16) GpuTile {
    std::array<std::uint32_t, 4> edges{};
    std::array<float, 4> phaseA{};
    std::array<float, 4> phaseB{};
    std::array<std::int32_t, 4> frequencyU{};
    std::array<std::int32_t, 4> frequencyV{};
};

static_assert(sizeof(GpuTile) == 80);

[[nodiscard]] float narrow(const double value) {
    if (!std::isfinite(value)
        || value < -static_cast<double>(std::numeric_limits<float>::max())
        || value > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument("GPU scene contains a non-finite or out-of-range value.");
    }
    return static_cast<float>(value);
}

} // namespace

GpuPatternRenderer::GpuPatternRenderer(const std::filesystem::path& shaderDirectory)
    : program_(shaderDirectory / "fullscreen.vert", shaderDirectory / "wang_pattern.frag") {
    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &tileBuffer_);
    glGenTextures(1, &gradientTexture_);
}

GpuPatternRenderer::~GpuPatternRenderer() {
    if (gradientTexture_ != 0) {
        glDeleteTextures(1, &gradientTexture_);
    }
    if (tileBuffer_ != 0) {
        glDeleteBuffers(1, &tileBuffer_);
    }
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
    }
}

GLint GpuPatternRenderer::uniformLocation(const char* name) const {
    const GLint location = glGetUniformLocation(program_.id(), name);
    if (location < 0) {
        throw std::runtime_error(std::string("Required shader uniform is missing: ") + name);
    }
    return location;
}

void GpuPatternRenderer::uploadScene(
    const model::WangGrid& grid,
    const model::EdgePalette& edgePalette,
    const generators::HybridTorusGenerator& generator,
    const color::GradientPalette& colorPalette,
    const color::MaterialSettings& material) {
    if (!grid.hasValidAdjacency() || grid.colorCount() != edgePalette.colors().size()) {
        throw std::invalid_argument("Cannot upload an invalid Wang scene.");
    }
    if (!edgePalette.validateAllCombinations().valid) {
        throw std::invalid_argument("Cannot upload an unsafe edge palette.");
    }
    if (!color::isValid(material)) {
        throw std::invalid_argument("Cannot upload invalid material settings.");
    }
    if (grid.width() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())
        || grid.height() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())
        || grid.tiles().size()
            > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(GpuTile)) {
        throw std::length_error("Wang grid exceeds OpenGL index or buffer limits.");
    }
    if (edgePalette.colors().size() > maximumEdgeColors
        || generator.fourier().modes().size() > maximumFourierModes
        || colorPalette.stops().size() > maximumColorStops) {
        throw std::length_error("GPU scene exceeds the shader's fixed array capacity.");
    }

    std::vector<GpuTile> gpuTiles;
    gpuTiles.reserve(grid.tiles().size());
    for (const auto& tile : grid.tiles()) {
        const auto descriptor = generators::HybridTorusGenerator::describeTile(tile.seed);
        GpuTile gpuTile;
        gpuTile.edges = {tile.south, tile.north, tile.west, tile.east};
        gpuTile.phaseA = {
            narrow(descriptor.firstDomainPhase),
            narrow(descriptor.secondDomainPhase),
            narrow(descriptor.phase[0]),
            narrow(descriptor.phase[1]),
        };
        gpuTile.phaseB = {
            narrow(descriptor.phase[2]),
            narrow(descriptor.phase[3]),
            0.0F,
            0.0F,
        };
        for (std::size_t mode = 0; mode < descriptor.phase.size(); ++mode) {
            gpuTile.frequencyU[mode] = descriptor.frequencyU[mode];
            gpuTile.frequencyV[mode] = descriptor.frequencyV[mode];
        }
        gpuTiles.push_back(gpuTile);
    }

    program_.use();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, tileBuffer_);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(gpuTiles.size() * sizeof(GpuTile)),
        gpuTiles.data(),
        GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tileBuffer_);

    std::array<float, maximumEdgeColors * 2> edgeParameters{};
    for (std::size_t index = 0; index < edgePalette.colors().size(); ++index) {
        const auto parameters = edgePalette.colors()[index].parameters();
        edgeParameters[index * 2] = narrow(parameters.epsilon);
        edgeParameters[index * 2 + 1] = narrow(parameters.delta);
    }
    glUniform1i(uniformLocation("u_edgeCount"), static_cast<GLint>(edgePalette.colors().size()));
    glUniform2fv(
        uniformLocation("u_edgeParameters"),
        static_cast<GLsizei>(edgePalette.colors().size()),
        edgeParameters.data());

    const auto& modes = generator.fourier().modes();
    std::array<GLint, maximumFourierModes * 2> frequencies{};
    std::array<float, maximumFourierModes> amplitudes{};
    std::array<float, maximumFourierModes> phases{};
    double fourierNormalizer = 0.0;
    for (std::size_t index = 0; index < modes.size(); ++index) {
        frequencies[index * 2] = modes[index].frequencyU;
        frequencies[index * 2 + 1] = modes[index].frequencyV;
        amplitudes[index] = narrow(modes[index].amplitude);
        phases[index] = narrow(modes[index].phase);
        fourierNormalizer += std::abs(modes[index].amplitude);
    }
    glUniform1i(uniformLocation("u_fourierCount"), static_cast<GLint>(modes.size()));
    glUniform2iv(
        uniformLocation("u_fourierFrequency"),
        static_cast<GLsizei>(modes.size()),
        frequencies.data());
    glUniform1fv(
        uniformLocation("u_fourierAmplitude"),
        static_cast<GLsizei>(modes.size()),
        amplitudes.data());
    glUniform1fv(
        uniformLocation("u_fourierPhase"),
        static_cast<GLsizei>(modes.size()),
        phases.data());
    glUniform1f(uniformLocation("u_fourierNormalizer"), narrow(fourierNormalizer));

    const auto& noiseSettings = generator.noise().settings();
    const std::uint32_t maximumNoiseFrequency = noiseSettings.baseFrequency
        << (noiseSettings.octaves - 1U);
    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (maximumNoiseFrequency > static_cast<std::uint32_t>(maximumTextureSize)) {
        throw std::length_error("Periodic noise exceeds the GPU texture-size limit.");
    }
    std::vector<float> gradients(
        static_cast<std::size_t>(maximumNoiseFrequency)
            * maximumNoiseFrequency * 2U);
    for (std::uint32_t y = 0; y < maximumNoiseFrequency; ++y) {
        for (std::uint32_t x = 0; x < maximumNoiseFrequency; ++x) {
            const auto gradient = generator.noise().gradientAt(x, y);
            const std::size_t offset = (static_cast<std::size_t>(y)
                * maximumNoiseFrequency + x) * 2U;
            gradients[offset] = narrow(gradient.x);
            gradients[offset + 1] = narrow(gradient.y);
        }
    }
    double noiseNormalizer = 0.0;
    double amplitude = 1.0;
    for (std::uint32_t octave = 0; octave < noiseSettings.octaves; ++octave) {
        noiseNormalizer += amplitude;
        amplitude *= noiseSettings.persistence;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gradientTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RG32F,
        static_cast<GLsizei>(maximumNoiseFrequency),
        static_cast<GLsizei>(maximumNoiseFrequency),
        0,
        GL_RG,
        GL_FLOAT,
        gradients.data());
    glUniform1i(uniformLocation("u_gradientTexture"), 0);
    glUniform4f(
        uniformLocation("u_noiseSettings"),
        static_cast<float>(noiseSettings.baseFrequency),
        static_cast<float>(noiseSettings.octaves),
        narrow(noiseSettings.persistence),
        narrow(noiseNormalizer));

    const auto& stops = colorPalette.stops();
    std::array<float, maximumColorStops> positions{};
    std::array<float, maximumColorStops * 3> colors{};
    for (std::size_t index = 0; index < stops.size(); ++index) {
        positions[index] = narrow(stops[index].position);
        colors[index * 3] = narrow(stops[index].color.red);
        colors[index * 3 + 1] = narrow(stops[index].color.green);
        colors[index * 3 + 2] = narrow(stops[index].color.blue);
    }
    glUniform1i(uniformLocation("u_colorStopCount"), static_cast<GLint>(stops.size()));
    glUniform1fv(
        uniformLocation("u_colorStopPosition"),
        static_cast<GLsizei>(stops.size()),
        positions.data());
    glUniform3fv(
        uniformLocation("u_colorStopValue"),
        static_cast<GLsizei>(stops.size()),
        colors.data());
    const auto& tone = colorPalette.tone();
    glUniform4f(
        uniformLocation("u_toneSettings"),
        narrow(tone.center),
        narrow(tone.contrast),
        narrow(tone.bandFrequency),
        narrow(tone.bandStrength));
    glUniform2f(
        uniformLocation("u_posterizeSettings"),
        static_cast<float>(tone.posterizeLevels),
        narrow(tone.posterizeSoftness));
    material_ = material;

    const auto& settings = generator.settings();
    glUniform4f(
        uniformLocation("u_generatorWeights"),
        narrow(settings.fourierWeight),
        narrow(settings.noiseWeight),
        narrow(settings.tileVariationAmplitude),
        narrow(settings.tileDomainWarpAmplitude));
    glUniform4f(
        uniformLocation("u_worldSettings"),
        narrow(settings.worldModulationAmplitude),
        narrow(settings.worldDomainWarpAmplitude),
        narrow(settings.worldFrequencyX),
        narrow(settings.worldFrequencyY));
    glUniform1i(
        uniformLocation("u_scalarProfile"),
        static_cast<GLint>(settings.scalarProfile));
    glUniform1f(
        uniformLocation("u_worldDetailAmplitude"),
        narrow(settings.worldDetailAmplitude));
    glUniform2f(
        uniformLocation("u_styleStructureSettings"),
        narrow(settings.worldGrainAmplitude),
        narrow(settings.cellularScale));
    glUniform2i(
        uniformLocation("u_gridSize"),
        static_cast<GLint>(grid.width()),
        static_cast<GLint>(grid.height()));
    sceneUploaded_ = true;
}

void GpuPatternRenderer::draw(
    const int framebufferWidth,
    const int framebufferHeight,
    const Camera2D& camera,
    const DebugView debugView,
    const bool enableMaterial) const {
    if (!sceneUploaded_) {
        throw std::logic_error("A GPU scene must be uploaded before drawing.");
    }
    if (framebufferWidth <= 0 || framebufferHeight <= 0
        || !std::isfinite(camera.originX)
        || !std::isfinite(camera.originY)
        || !std::isfinite(camera.pixelsPerTile)
        || camera.pixelsPerTile <= 0.0) {
        throw std::invalid_argument("Invalid GPU viewport or camera.");
    }

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    program_.use();
    glUniform2f(
        uniformLocation("u_worldOrigin"),
        narrow(camera.originX),
        narrow(camera.originY));
    glUniform1f(uniformLocation("u_pixelsPerTile"), narrow(camera.pixelsPerTile));
    glUniform1i(uniformLocation("u_debugView"), static_cast<GLint>(debugView));
    glUniform4f(
        uniformLocation("u_materialSettings"),
        enableMaterial ? narrow(material_.contourFrequency) : 0.0F,
        enableMaterial ? narrow(material_.contourStrength) : 0.0F,
        narrow(material_.contourWidth),
        enableMaterial ? narrow(material_.reliefStrength) : 0.0F);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gradientTexture_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tileBuffer_);
    glBindVertexArray(vertexArray_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

render::Image GpuPatternRenderer::renderImage(
    const int width,
    const int height,
    const Camera2D& camera,
    const DebugView debugView,
    const bool enableMaterial) const {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("GPU export dimensions must be positive.");
    }
    GLint maximumTextureSize = 0;
    std::array<GLint, 2> maximumViewport{};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maximumViewport.data());
    if (width > maximumTextureSize || height > maximumTextureSize
        || width > maximumViewport[0] || height > maximumViewport[1]) {
        throw std::length_error("GPU export dimensions exceed the device limit.");
    }

    GLint previousFramebuffer = 0;
    GLint previousPackAlignment = 0;
    std::array<GLint, 4> previousViewport{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glGetIntegerv(GL_VIEWPORT, previousViewport.data());

    GLuint framebuffer = 0;
    GLuint texture = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);
    const auto restore = [&]() noexcept {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
        glViewport(
            previousViewport[0],
            previousViewport[1],
            previousViewport[2],
            previousViewport[3]);
        glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
        if (framebuffer != 0) {
            glDeleteFramebuffers(1, &framebuffer);
        }
    };

    try {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB8,
            width,
            height,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            texture,
            0);
        constexpr GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &drawBuffer);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("GPU export framebuffer is incomplete.");
        }

        draw(width, height, camera, debugView, enableMaterial);
        glFinish();
        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

        render::Image image(
            static_cast<std::size_t>(width),
            static_cast<std::size_t>(height));
        for (int y = 0; y < height; ++y) {
            const int sourceY = height - 1 - y;
            for (int x = 0; x < width; ++x) {
                const std::size_t offset = (
                    static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * 3U;
                image.pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y)) = {
                    pixels[offset], pixels[offset + 1U], pixels[offset + 2U]};
            }
        }
        restore();
        return image;
    } catch (...) {
        restore();
        throw;
    }
}

GpuValidationBuffers GpuPatternRenderer::renderValidationBuffers(
    const int width,
    const int height,
    const Camera2D& camera) const {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("GPU validation dimensions must be positive.");
    }
    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (width > maximumTextureSize || height > maximumTextureSize) {
        throw std::length_error("GPU validation dimensions exceed the texture-size limit.");
    }

    GLuint framebuffer = 0;
    std::array<GLuint, 3> textures{};
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
    try {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        for (std::size_t index = 0; index < textures.size(); ++index) {
            glBindTexture(GL_TEXTURE_2D, textures[index]);
            const GLint internalFormat = index == 0 ? GL_RGBA8 : GL_RGBA32F;
            const GLenum type = index == 0 ? GL_UNSIGNED_BYTE : GL_FLOAT;
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                internalFormat,
                width,
                height,
                0,
                GL_RGBA,
                type,
                nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(index),
                GL_TEXTURE_2D,
                textures[index],
                0);
        }
        constexpr std::array<GLenum, 3> drawBuffers{
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2,
        };
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("GPU validation framebuffer is incomplete.");
        }

        draw(width, height, camera, DebugView::Pattern, false);
        glFinish();
        GpuValidationBuffers result;
        result.width = width;
        result.height = height;
        const std::size_t pixelCount = static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height);
        result.diagnostics.resize(pixelCount);
        result.linearColorAndResidual.resize(pixelCount);
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(
            0,
            0,
            width,
            height,
            GL_RGBA,
            GL_FLOAT,
            result.diagnostics.data());
        glReadBuffer(GL_COLOR_ATTACHMENT2);
        glReadPixels(
            0,
            0,
            width,
            height,
            GL_RGBA,
            GL_FLOAT,
            result.linearColorAndResidual.data());

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        glDeleteFramebuffers(1, &framebuffer);
        return result;
    } catch (...) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        glDeleteFramebuffers(1, &framebuffer);
        throw;
    }
}

} // namespace qrp::render::opengl
