#include "export/PpmWriter.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "generators/PeriodicGradientNoise.hpp"
#include "generators/TorusFourier.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "presets/PatternPreset.hpp"
#include "project/PatternProject.hpp"
#include "render/CpuReferenceRenderer.hpp"
#include "render/opengl/GpuPatternRenderer.hpp"
#include "ui/PatternEditor.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef QRP_SHADER_DIR
#error QRP_SHADER_DIR must name the runtime shader directory.
#endif

namespace {

using qrp::render::opengl::Camera2D;
using qrp::render::opengl::DebugView;

struct CommandLineOptions {
    bool capture = false;
    bool captureUi = false;
    bool validate = false;
    std::filesystem::path capturePath = "output/gpu/gpu_capture.ppm";
    std::size_t presetIndex = 2;
    DebugView debugView = DebugView::Pattern;
    int captureWidth = 0;
    int captureHeight = 0;
    bool cameraOverride = false;
    Camera2D camera;
    int benchmarkFrames = 0;
};

struct AppState {
    Camera2D camera;
    DebugView debugView = DebugView::Pattern;
    std::size_t requestedPreset = std::numeric_limits<std::size_t>::max();
    bool resetRequested = false;
    bool dragging = false;
    double previousCursorX = 0.0;
    double previousCursorY = 0.0;
};

class GlfwRuntime final {
public:
    GlfwRuntime() {
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("GLFW initialization failed.");
        }
    }

    ~GlfwRuntime() {
        glfwTerminate();
    }

    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;
};

struct WindowDeleter {
    void operator()(GLFWwindow* window) const noexcept {
        glfwDestroyWindow(window);
    }
};

using WindowPointer = std::unique_ptr<GLFWwindow, WindowDeleter>;

[[nodiscard]] CommandLineOptions parseCommandLine(const int argc, char** argv) {
    CommandLineOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--capture") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--capture requires an output path.");
            }
            options.capture = true;
            options.capturePath = argv[++index];
        } else if (argument == "--capture-ui") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--capture-ui requires an output path.");
            }
            options.capture = true;
            options.captureUi = true;
            options.capturePath = argv[++index];
        } else if (argument == "--validate") {
            options.capture = true;
            options.validate = true;
        } else if (argument == "--preset") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--preset requires an index from 1 to 5.");
            }
            const int humanIndex = std::stoi(argv[++index]);
            if (humanIndex < 1 || humanIndex > 5) {
                throw std::out_of_range("Preset index must be from 1 to 5.");
            }
            options.presetIndex = static_cast<std::size_t>(humanIndex - 1);
        } else if (argument == "--debug") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--debug requires pattern, jacobian, residual, or edges.");
            }
            const std::string_view value(argv[++index]);
            if (value == "pattern") {
                options.debugView = DebugView::Pattern;
            } else if (value == "jacobian") {
                options.debugView = DebugView::Jacobian;
            } else if (value == "residual") {
                options.debugView = DebugView::NewtonResidual;
            } else if (value == "edges") {
                options.debugView = DebugView::TileEdges;
            } else {
                throw std::invalid_argument("Unknown debug view: " + std::string(value));
            }
        } else if (argument == "--size") {
            if (index + 2 >= argc) {
                throw std::invalid_argument("--size requires width and height.");
            }
            options.captureWidth = std::stoi(argv[++index]);
            options.captureHeight = std::stoi(argv[++index]);
            if (options.captureWidth <= 0 || options.captureHeight <= 0) {
                throw std::invalid_argument("Capture dimensions must be positive.");
            }
        } else if (argument == "--camera") {
            if (index + 3 >= argc) {
                throw std::invalid_argument("--camera requires originX, originY, and pixelsPerTile.");
            }
            options.cameraOverride = true;
            options.camera.originX = std::stod(argv[++index]);
            options.camera.originY = std::stod(argv[++index]);
            options.camera.pixelsPerTile = std::stod(argv[++index]);
            if (!std::isfinite(options.camera.originX)
                || !std::isfinite(options.camera.originY)
                || !std::isfinite(options.camera.pixelsPerTile)
                || options.camera.pixelsPerTile <= 0.0) {
                throw std::invalid_argument("Capture camera values must be finite and positive.");
            }
        } else if (argument == "--benchmark") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--benchmark requires a positive frame count.");
            }
            options.benchmarkFrames = std::stoi(argv[++index]);
            if (options.benchmarkFrames <= 0) {
                throw std::invalid_argument("Benchmark frame count must be positive.");
            }
            options.capture = true;
        } else {
            throw std::invalid_argument("Unknown command-line argument: " + std::string(argument));
        }
    }
    return options;
}

void resetCamera(
    const qrp::project::PatternConfiguration& configuration,
    const int framebufferWidth,
    const int framebufferHeight,
    Camera2D& camera) {
    camera.pixelsPerTile = static_cast<double>(configuration.pixelsPerTile);
    camera.originX = 0.5 * (
        static_cast<double>(configuration.gridWidth)
        - static_cast<double>(framebufferWidth) / camera.pixelsPerTile);
    camera.originY = 0.5 * (
        static_cast<double>(configuration.gridHeight)
        - static_cast<double>(framebufferHeight) / camera.pixelsPerTile);
}

void errorCallback(const int code, const char* description) {
    std::cerr << "GLFW error " << code << ": " << description << '\n';
}

void APIENTRY debugCallback(
    GLenum,
    GLenum,
    GLuint,
    GLenum severity,
    GLsizei,
    const GLchar* message,
    const void*) {
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        std::cerr << "OpenGL: " << message << '\n';
    }
}

void keyCallback(GLFWwindow* window, const int key, int, const int action, int) {
    if (action != GLFW_PRESS) {
        return;
    }
    auto& state = *static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (qrp::ui::wantsKeyboardInput()) {
        return;
    } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
        state.requestedPreset = static_cast<std::size_t>(key - GLFW_KEY_1);
    } else if (key == GLFW_KEY_D) {
        const int next = (static_cast<int>(state.debugView) + 1) % 4;
        state.debugView = static_cast<DebugView>(next);
    } else if (key == GLFW_KEY_R) {
        state.resetRequested = true;
    }
}

void mouseButtonCallback(GLFWwindow* window, const int button, const int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    auto& state = *static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (qrp::ui::wantsMouseInput()) {
        state.dragging = false;
        return;
    }
    state.dragging = action == GLFW_PRESS;
    if (state.dragging) {
        glfwGetCursorPos(window, &state.previousCursorX, &state.previousCursorY);
    }
}

void cursorCallback(GLFWwindow* window, const double x, const double y) {
    auto& state = *static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (qrp::ui::wantsMouseInput()) {
        state.dragging = false;
        return;
    }
    if (!state.dragging) {
        return;
    }
    int windowWidth = 1;
    int windowHeight = 1;
    int framebufferWidth = 1;
    int framebufferHeight = 1;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    const double scaleX = static_cast<double>(framebufferWidth) / windowWidth;
    const double scaleY = static_cast<double>(framebufferHeight) / windowHeight;
    state.camera.originX -= (x - state.previousCursorX) * scaleX
        / state.camera.pixelsPerTile;
    state.camera.originY += (y - state.previousCursorY) * scaleY
        / state.camera.pixelsPerTile;
    state.previousCursorX = x;
    state.previousCursorY = y;
}

void scrollCallback(GLFWwindow* window, double, const double yOffset) {
    if (qrp::ui::wantsMouseInput()) {
        return;
    }
    auto& state = *static_cast<AppState*>(glfwGetWindowUserPointer(window));
    int windowWidth = 1;
    int windowHeight = 1;
    int framebufferWidth = 1;
    int framebufferHeight = 1;
    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glfwGetCursorPos(window, &cursorX, &cursorY);
    const double fragmentX = cursorX * static_cast<double>(framebufferWidth) / windowWidth;
    const double fragmentY = static_cast<double>(framebufferHeight)
        - cursorY * static_cast<double>(framebufferHeight) / windowHeight;
    const double worldX = state.camera.originX + fragmentX / state.camera.pixelsPerTile;
    const double worldY = state.camera.originY + fragmentY / state.camera.pixelsPerTile;
    const double zoom = std::pow(1.16, yOffset);
    state.camera.pixelsPerTile = std::clamp(
        state.camera.pixelsPerTile * zoom,
        12.0,
        720.0);
    state.camera.originX = worldX - fragmentX / state.camera.pixelsPerTile;
    state.camera.originY = worldY - fragmentY / state.camera.pixelsPerTile;
}

[[nodiscard]] qrp::render::Image readFramebuffer(const int width, const int height) {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    qrp::render::Image image(
        static_cast<std::size_t>(width),
        static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(sourceY) * width + x) * 3U;
            image.pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y)) = {
                pixels[offset], pixels[offset + 1], pixels[offset + 2]};
        }
    }
    return image;
}

struct ComparisonMetrics {
    std::uint8_t maximumChannelDifference = 0;
    double meanChannelDifference = 0.0;
    std::size_t pixelsOverOneLsb = 0;
    std::size_t magentaPixelCount = 0;
};

struct IntermediateComparisonMetrics {
    std::size_t cpuInverseFailures = 0;
    std::size_t gpuInvalidPixels = 0;
    double maximumParameterDifference = 0.0;
    double maximumScalarDifference = 0.0;
    double maximumLinearColorDifference = 0.0;
    double maximumDeterminantDifference = 0.0;
    double maximumGpuResidual = 0.0;
};

[[nodiscard]] ComparisonMetrics compareImages(
    const qrp::render::Image& first,
    const qrp::render::Image& second) {
    if (first.width() != second.width() || first.height() != second.height()) {
        throw std::invalid_argument("GPU and CPU comparison images have different dimensions.");
    }
    ComparisonMetrics metrics;
    std::uint64_t totalDifference = 0;
    for (std::size_t index = 0; index < first.pixels().size(); ++index) {
        const auto a = first.pixels()[index];
        const auto b = second.pixels()[index];
        const auto difference = [](const std::uint8_t x, const std::uint8_t y) {
            return static_cast<std::uint8_t>(x > y ? x - y : y - x);
        };
        const std::uint8_t red = difference(a.red, b.red);
        const std::uint8_t green = difference(a.green, b.green);
        const std::uint8_t blue = difference(a.blue, b.blue);
        const std::uint8_t maximum = std::max({red, green, blue});
        metrics.maximumChannelDifference = std::max(metrics.maximumChannelDifference, maximum);
        totalDifference += static_cast<std::uint64_t>(red) + green + blue;
        if (maximum > 1) {
            ++metrics.pixelsOverOneLsb;
        }
        if (a.red == 255 && a.green == 0 && a.blue == 255) {
            ++metrics.magentaPixelCount;
        }
    }
    metrics.meanChannelDifference = static_cast<double>(totalDifference)
        / static_cast<double>(first.pixels().size() * 3U);
    return metrics;
}

[[nodiscard]] IntermediateComparisonMetrics compareIntermediateValues(
    const qrp::render::opengl::GpuValidationBuffers& gpu,
    const qrp::model::WangGrid& grid,
    const qrp::model::EdgePalette& edgePalette,
    const qrp::generators::HybridTorusGenerator& generator,
    const qrp::color::GradientPalette& colorPalette,
    const std::uint32_t pixelsPerTile) {
    IntermediateComparisonMetrics metrics;
    std::vector<qrp::math::CoonsWarp> warps;
    warps.reserve(grid.tiles().size());
    for (const auto& tile : grid.tiles()) {
        const auto& colors = edgePalette.colors();
        warps.emplace_back(qrp::math::CoonsEdges{
            colors.at(tile.south),
            colors.at(tile.north),
            colors.at(tile.west),
            colors.at(tile.east),
        });
    }

    const double scale = static_cast<double>(pixelsPerTile);
    for (int y = 0; y < gpu.height; ++y) {
        const double worldY = (static_cast<double>(y) + 0.5) / scale;
        const std::size_t tileY = static_cast<std::size_t>(std::floor(worldY));
        for (int x = 0; x < gpu.width; ++x) {
            const double worldX = (static_cast<double>(x) + 0.5) / scale;
            const std::size_t tileX = static_cast<std::size_t>(std::floor(worldX));
            const std::size_t tileIndex = tileY * grid.width() + tileX;
            const std::size_t pixelIndex = static_cast<std::size_t>(y) * gpu.width + x;
            const auto& gpuValues = gpu.diagnostics[pixelIndex];
            const auto& gpuColor = gpu.linearColorAndResidual[pixelIndex];
            if (gpuColor[0] < -0.5F) {
                ++metrics.gpuInvalidPixels;
                continue;
            }

            const qrp::math::Vec2 physical{
                worldX - static_cast<double>(tileX),
                worldY - static_cast<double>(tileY),
            };
            const auto inverse = warps[tileIndex].inverse(physical);
            if (!inverse.converged) {
                ++metrics.cpuInverseFailures;
                continue;
            }
            metrics.maximumParameterDifference = std::max({
                metrics.maximumParameterDifference,
                std::abs(inverse.parameter.x - gpuValues[0]),
                std::abs(inverse.parameter.y - gpuValues[1]),
            });

            const auto& tile = grid.tiles()[tileIndex];
            const double scalar = generator.evaluate(qrp::generators::GeneratorInput{
                inverse.parameter,
                qrp::math::Vec2{worldX, worldY},
                tile.seed,
            });
            metrics.maximumScalarDifference = std::max(
                metrics.maximumScalarDifference,
                std::abs(scalar - gpuValues[2]));
            const auto color = colorPalette.sample(scalar);
            metrics.maximumLinearColorDifference = std::max({
                metrics.maximumLinearColorDifference,
                std::abs(color.red - gpuColor[0]),
                std::abs(color.green - gpuColor[1]),
                std::abs(color.blue - gpuColor[2]),
            });
            metrics.maximumDeterminantDifference = std::max(
                metrics.maximumDeterminantDifference,
                std::abs(warps[tileIndex].determinant(inverse.parameter) - gpuValues[3]));
            metrics.maximumGpuResidual = std::max(
                metrics.maximumGpuResidual,
                static_cast<double>(gpuColor[3]));
        }
    }
    return metrics;
}

[[nodiscard]] const char* debugViewName(const DebugView view) noexcept {
    switch (view) {
    case DebugView::Pattern:
        return "pattern";
    case DebugView::Jacobian:
        return "jacobian";
    case DebugView::NewtonResidual:
        return "residual";
    case DebugView::TileEdges:
        return "tile edges";
    }
    return "unknown";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CommandLineOptions options = parseCommandLine(argc, argv);
        const auto presets = qrp::presets::createBaselinePresets();
        if (options.presetIndex >= presets.size()) {
            throw std::out_of_range("Requested preset does not exist.");
        }
        const auto& initialPreset = presets[options.presetIndex];
        if (options.validate
            && (options.debugView != DebugView::Pattern
                || options.captureWidth != 0
                || options.cameraOverride
                || options.captureUi)) {
            throw std::invalid_argument(
                "GPU validation requires the default size, camera, and pattern view.");
        }
        const int initialWidth = options.captureWidth > 0
            ? options.captureWidth
            : options.capture
                ? static_cast<int>(initialPreset.gridWidth * initialPreset.pixelsPerTile)
            : 1100;
        const int initialHeight = options.captureHeight > 0
            ? options.captureHeight
            : options.capture
                ? static_cast<int>(initialPreset.gridHeight * initialPreset.pixelsPerTile)
            : 800;

        glfwSetErrorCallback(errorCallback);
        const GlfwRuntime glfw;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, options.capture ? GLFW_FALSE : GLFW_TRUE);
        WindowPointer window(glfwCreateWindow(
            initialWidth,
            initialHeight,
            "Quasi-regular Wang Patterns",
            nullptr,
            nullptr));
        if (!window) {
            throw std::runtime_error("Failed to create an OpenGL 4.3 window.");
        }
        glfwMakeContextCurrent(window.get());
        const int version = gladLoadGL(glfwGetProcAddress);
        if (version == 0) {
            throw std::runtime_error("Failed to load OpenGL functions.");
        }
        std::cout << "OpenGL " << GLAD_VERSION_MAJOR(version) << '.'
                  << GLAD_VERSION_MINOR(version) << ", renderer="
                  << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << '\n';
        if (GLAD_VERSION_MAJOR(version) < 4
            || (GLAD_VERSION_MAJOR(version) == 4 && GLAD_VERSION_MINOR(version) < 3)) {
            throw std::runtime_error("OpenGL 4.3 or newer is required.");
        }
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(debugCallback, nullptr);

        qrp::render::opengl::GpuPatternRenderer renderer(QRP_SHADER_DIR);
        qrp::project::PatternProject project(
            qrp::project::configurationFromPreset(initialPreset));
        AppState state;
        state.debugView = options.debugView;
        glfwSetWindowUserPointer(window.get(), &state);
        glfwSetKeyCallback(window.get(), keyCallback);
        glfwSetMouseButtonCallback(window.get(), mouseButtonCallback);
        glfwSetCursorPosCallback(window.get(), cursorCallback);
        glfwSetScrollCallback(window.get(), scrollCallback);

        auto uploadCommittedScene = [&]() {
            const auto& scene = project.scene();
            renderer.uploadScene(
                scene.grid,
                scene.edgePalette,
                scene.generator,
                scene.colorPalette);
        };
        uploadCommittedScene();
        int framebufferWidth = initialWidth;
        int framebufferHeight = initialHeight;
        glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
        resetCamera(project.committed(), framebufferWidth, framebufferHeight, state.camera);
        if (options.benchmarkFrames > 0) {
            const auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < options.benchmarkFrames; ++frame) {
                renderer.draw(framebufferWidth, framebufferHeight, state.camera, state.debugView);
                glFinish();
            }
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
            std::cout << "Benchmark: frames=" << options.benchmarkFrames
                      << ", average_ms=" << std::fixed << std::setprecision(3)
                      << 1000.0 * elapsed / options.benchmarkFrames
                      << ", fps=" << options.benchmarkFrames / elapsed << '\n';
            return 0;
        }

        if (options.capture) {
            if (options.cameraOverride) {
                state.camera = options.camera;
            } else if (options.captureWidth == 0) {
                state.camera.originX = 0.0;
                state.camera.originY = 0.0;
            }
        }

        if (options.capture) {
            if (options.captureUi) {
                glfwPollEvents();
                qrp::ui::EditorRuntime editorRuntime(window.get());
                const qrp::ui::PatternEditor editor;
                editorRuntime.beginFrame();
                static_cast<void>(editor.draw(
                    project.draft(),
                    project.isDirty(),
                    project.revision(),
                    project.lastApplyResult(),
                    state.debugView,
                    presets));
                renderer.draw(framebufferWidth, framebufferHeight, state.camera, state.debugView);
                editorRuntime.render();
            } else {
                renderer.draw(framebufferWidth, framebufferHeight, state.camera, state.debugView);
            }
            glFinish();
            const auto gpuImage = readFramebuffer(framebufferWidth, framebufferHeight);
            if (!options.capturePath.parent_path().empty()) {
                std::filesystem::create_directories(options.capturePath.parent_path());
            }
            qrp::exporting::writePpm(gpuImage, options.capturePath);

            if (!options.validate) {
                std::cout << "Captured "
                          << (options.captureUi ? "editor" : debugViewName(state.debugView))
                          << " view to " << options.capturePath.string() << '\n';
                return 0;
            }

            const auto& configuration = project.committed();
            const auto& scene = project.scene();
            const auto cpu = qrp::render::CpuReferenceRenderer::render(
                scene.grid,
                scene.edgePalette,
                scene.generator,
                scene.colorPalette,
                qrp::render::CpuRenderSettings{configuration.pixelsPerTile, {}});
            const auto comparison = compareImages(gpuImage, cpu.image);
            const auto gpuValues = renderer.renderValidationBuffers(
                framebufferWidth,
                framebufferHeight,
                state.camera);
            const auto intermediate = compareIntermediateValues(
                gpuValues,
                scene.grid,
                scene.edgePalette,
                scene.generator,
                scene.colorPalette,
                configuration.pixelsPerTile);
            std::cout << "GPU/CPU: max_8bit_difference="
                      << static_cast<unsigned int>(comparison.maximumChannelDifference)
                      << ", mean_8bit_difference=" << std::fixed << std::setprecision(4)
                      << comparison.meanChannelDifference
                      << ", pixels_over_1_lsb=" << comparison.pixelsOverOneLsb
                      << ", invalid_gpu_pixels=" << comparison.magentaPixelCount << '\n';
            std::cout << "GPU/CPU stages: max_parameter=" << std::scientific
                      << intermediate.maximumParameterDifference
                      << ", max_scalar=" << intermediate.maximumScalarDifference
                      << ", max_linear_rgb=" << intermediate.maximumLinearColorDifference
                      << ", max_det=" << intermediate.maximumDeterminantDifference
                      << ", max_gpu_residual=" << intermediate.maximumGpuResidual
                      << ", cpu_inverse_failures=" << intermediate.cpuInverseFailures
                      << ", gpu_invalid_values=" << intermediate.gpuInvalidPixels << '\n';
            if (options.validate
                && (comparison.maximumChannelDifference > 12
                    || comparison.meanChannelDifference > 0.75
                    || comparison.magentaPixelCount != 0
                    || intermediate.cpuInverseFailures != 0
                    || intermediate.gpuInvalidPixels != 0
                    || intermediate.maximumParameterDifference > 1.0e-5
                    || intermediate.maximumScalarDifference > 2.0e-4
                    || intermediate.maximumLinearColorDifference > 1.0e-3
                    || intermediate.maximumDeterminantDifference > 2.0e-4
                    || intermediate.maximumGpuResidual > 2.0e-5)) {
                std::cerr << "GPU/CPU validation exceeded its acceptance threshold.\n";
                return 2;
            }
            return 0;
        }

        glfwSwapInterval(1);
        qrp::ui::EditorRuntime editorRuntime(window.get());
        const qrp::ui::PatternEditor editor;
        auto titleStart = std::chrono::steady_clock::now();
        std::uint64_t titleFrames = 0;
        while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
            glfwPollEvents();
            glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
            if (framebufferWidth == 0 || framebufferHeight == 0) {
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            if (state.requestedPreset < presets.size()) {
                const std::size_t requestedPreset = state.requestedPreset;
                state.requestedPreset = std::numeric_limits<std::size_t>::max();
                project.loadPresetIntoDraft(presets[requestedPreset]);
                const auto result = project.applyDraft();
                if (!result.applied) {
                    throw std::runtime_error("Baseline preset failed validation: " + result.message);
                }
                uploadCommittedScene();
                resetCamera(
                    project.committed(),
                    framebufferWidth,
                    framebufferHeight,
                    state.camera);
            }
            if (state.resetRequested) {
                resetCamera(
                    project.committed(),
                    framebufferWidth,
                    framebufferHeight,
                    state.camera);
                state.resetRequested = false;
            }

            editorRuntime.beginFrame();
            const auto actions = editor.draw(
                project.draft(),
                project.isDirty(),
                project.revision(),
                project.lastApplyResult(),
                state.debugView,
                presets);
            if (actions.loadPreset) {
                project.loadPresetIntoDraft(presets.at(*actions.loadPreset));
            }
            if (actions.discardDraft) {
                project.resetDraft();
            }
            if (actions.applyDraft) {
                const auto result = project.applyDraft();
                if (result.applied) {
                    uploadCommittedScene();
                }
            }
            if (actions.resetCamera) {
                resetCamera(
                    project.committed(),
                    framebufferWidth,
                    framebufferHeight,
                    state.camera);
            }

            renderer.draw(framebufferWidth, framebufferHeight, state.camera, state.debugView);
            editorRuntime.render();
            glfwSwapBuffers(window.get());
            ++titleFrames;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - titleStart).count();
            if (elapsed >= 0.5) {
                const double fps = static_cast<double>(titleFrames) / elapsed;
                const std::string title = "Quasi-regular Wang Patterns | "
                    + project.committed().name + " | " + debugViewName(state.debugView)
                    + " | " + std::to_string(static_cast<int>(std::lround(fps)))
                    + " FPS | draft/Apply editor";
                glfwSetWindowTitle(window.get(), title.c_str());
                titleStart = now;
                titleFrames = 0;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Realtime application failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
