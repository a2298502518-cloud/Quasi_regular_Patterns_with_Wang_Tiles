#pragma once

#include "project/PatternProject.hpp"
#include "render/opengl/GpuPatternRenderer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct GLFWwindow;

namespace qrp::ui {

struct EditorActions {
    std::optional<std::size_t> loadPreset;
    bool applyDraft = false;
    bool discardDraft = false;
    bool resetCamera = false;
};

// ImGui 生命周期与后端调用集中在这里，业务界面不直接持有窗口或 GPU 场景。
class EditorRuntime final {
public:
    explicit EditorRuntime(GLFWwindow* window);
    ~EditorRuntime();

    EditorRuntime(const EditorRuntime&) = delete;
    EditorRuntime& operator=(const EditorRuntime&) = delete;

    void beginFrame() const;
    void render() const;
};

class PatternEditor final {
public:
    [[nodiscard]] EditorActions draw(
        project::PatternConfiguration& draft,
        bool dirty,
        std::uint64_t revision,
        const project::ApplyResult& lastResult,
        render::opengl::DebugView& debugView,
        const std::vector<presets::PatternPreset>& presets) const;
};

[[nodiscard]] bool wantsMouseInput() noexcept;
[[nodiscard]] bool wantsKeyboardInput() noexcept;

} // namespace qrp::ui
