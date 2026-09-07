#include "ui/PatternEditor.hpp"

#include "color/GradientPalette.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace qrp::ui {
namespace {

void configureStyle() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 10.0F;
    style.ChildRounding = 7.0F;
    style.FrameRounding = 5.0F;
    style.GrabRounding = 5.0F;
    style.WindowPadding = {14.0F, 14.0F};
    style.FramePadding = {8.0F, 5.0F};
    style.ItemSpacing = {8.0F, 7.0F};
    style.Colors[ImGuiCol_WindowBg] = {0.035F, 0.047F, 0.075F, 0.96F};
    style.Colors[ImGuiCol_Header] = {0.05F, 0.40F, 0.42F, 0.72F};
    style.Colors[ImGuiCol_HeaderHovered] = {0.08F, 0.55F, 0.56F, 0.85F};
    style.Colors[ImGuiCol_Button] = {0.08F, 0.42F, 0.43F, 0.78F};
    style.Colors[ImGuiCol_ButtonHovered] = {0.11F, 0.58F, 0.57F, 0.90F};
    style.Colors[ImGuiCol_SliderGrab] = {0.91F, 0.64F, 0.24F, 1.0F};
    style.Colors[ImGuiCol_CheckMark] = {0.95F, 0.70F, 0.30F, 1.0F};
}

void sliderDouble(
    const char* label,
    double& value,
    const double minimum,
    const double maximum,
    const char* format = "%.3f") {
    ImGui::SliderScalar(
        label,
        ImGuiDataType_Double,
        &value,
        &minimum,
        &maximum,
        format);
}

void drawGenerator(generators::HybridGeneratorSettings& settings) {
    sliderDouble("Fourier weight", settings.fourierWeight, 0.0, 1.5);
    sliderDouble("Noise weight", settings.noiseWeight, 0.0, 1.5);
    sliderDouble("Tile variation", settings.tileVariationAmplitude, 0.0, 0.8);
    sliderDouble("Tile domain warp", settings.tileDomainWarpAmplitude, 0.0, 0.5);
    sliderDouble("World modulation", settings.worldModulationAmplitude, 0.0, 0.8);
    sliderDouble("World domain warp", settings.worldDomainWarpAmplitude, 0.0, 1.25);
    sliderDouble("World frequency X", settings.worldFrequencyX, -0.5, 0.5, "%.4f");
    sliderDouble("World frequency Y", settings.worldFrequencyY, -0.5, 0.5, "%.4f");
}

void drawEdgePalette(std::vector<math::EdgeParameters>& edges) {
    ImGui::TextDisabled("All color combinations are checked only when Apply is pressed.");
    for (std::size_t index = 0; index < edges.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        char label[32]{};
        std::snprintf(label, sizeof(label), "Edge color %zu", index + 1);
        if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            sliderDouble("Epsilon", edges[index].epsilon, -0.40, 0.40, "%.4f");
            sliderDouble("Delta", edges[index].delta, -0.40, 0.40, "%.4f");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void drawColorRamp(
    std::vector<color::ColorStop>& stops,
    color::ToneSettings& tone) {
    sliderDouble("Center", tone.center, -1.0, 1.0);
    sliderDouble("Contrast", tone.contrast, 0.2, 5.0);
    sliderDouble("Band frequency", tone.bandFrequency, 0.0, 16.0, "%.2f");
    sliderDouble("Band strength", tone.bandStrength, 0.0, 0.45);

    for (std::size_t index = 0; index < stops.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        ImGui::SeparatorText(("Stop " + std::to_string(index + 1)).c_str());
        if (index > 0 && index + 1 < stops.size()) {
            const double minimum = stops[index - 1].position + 0.001;
            const double maximum = stops[index + 1].position - 0.001;
            sliderDouble("Position", stops[index].position, minimum, maximum, "%.3f");
        } else {
            ImGui::TextDisabled("Position %.2f (ramp endpoint)", stops[index].position);
        }

        const color::Color3 srgb = color::srgbFromLinear(stops[index].color);
        std::array<float, 3> editable{
            static_cast<float>(srgb.red),
            static_cast<float>(srgb.green),
            static_cast<float>(srgb.blue),
        };
        if (ImGui::ColorEdit3("Color", editable.data(), ImGuiColorEditFlags_Float)) {
            stops[index].color = color::linearFromSrgb({
                editable[0], editable[1], editable[2]});
        }
        ImGui::PopID();
    }
}

void drawMaterial(color::MaterialSettings& material) {
    ImGui::TextDisabled("Derivative effects fade to zero near every Wang boundary.");
    sliderDouble("Contour frequency", material.contourFrequency, 0.0, 24.0, "%.1f");
    sliderDouble("Contour strength", material.contourStrength, 0.0, 0.35);
    sliderDouble("Contour width", material.contourWidth, 0.01, 0.25);
    sliderDouble("Relief strength", material.reliefStrength, 0.0, 0.65);
}

} // namespace

EditorRuntime::EditorRuntime(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    configureStyle();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // 面板布局由代码给出；不在项目根目录产生机器本地的 imgui.ini。
    io.IniFilename = nullptr;
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        throw std::runtime_error("Dear ImGui GLFW backend initialization failed.");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 430 core")) {
        ImGui_ImplGlfw_Shutdown();
        throw std::runtime_error("Dear ImGui OpenGL backend initialization failed.");
    }
}

EditorRuntime::~EditorRuntime() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorRuntime::beginFrame() const {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorRuntime::render() const {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

EditorActions PatternEditor::draw(
    project::PatternConfiguration& draft,
    const bool dirty,
    const std::uint64_t revision,
    const project::ApplyResult& lastResult,
    render::opengl::DebugView& debugView,
    const std::vector<presets::PatternPreset>& presets) const {
    EditorActions actions;
    ImGui::SetNextWindowPos({18.0F, 18.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({390.0F, 730.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.96F);
    if (!ImGui::Begin("Pattern Studio", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return actions;
    }

    ImGui::Text("Committed revision %llu", static_cast<unsigned long long>(revision));
    ImGui::SameLine();
    ImGui::TextColored(
        dirty ? ImVec4{0.98F, 0.70F, 0.26F, 1.0F} : ImVec4{0.35F, 0.86F, 0.64F, 1.0F},
        dirty ? "DRAFT CHANGED" : "SYNCHRONIZED");
    ImGui::TextWrapped("%s", lastResult.message.c_str());
    if (lastResult.minimumDeterminantLowerBound > 0.0) {
        ImGui::TextDisabled(
            "Certified determinant lower bound: %.4f",
            lastResult.minimumDeterminantLowerBound);
    }

    ImGui::SeparatorText("Baseline presets");
    for (std::size_t index = 0; index < presets.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Button(presets[index].name.c_str(), {-FLT_MIN, 0.0F})) {
            actions.loadPreset = index;
        }
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Wang grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        int width = static_cast<int>(draft.gridWidth);
        int height = static_cast<int>(draft.gridHeight);
        int pixels = static_cast<int>(draft.pixelsPerTile);
        if (ImGui::SliderInt("Width", &width, 1, 64)) {
            draft.gridWidth = static_cast<std::size_t>(width);
        }
        if (ImGui::SliderInt("Height", &height, 1, 64)) {
            draft.gridHeight = static_cast<std::size_t>(height);
        }
        if (ImGui::SliderInt("Pixels per tile", &pixels, 12, 360)) {
            draft.pixelsPerTile = static_cast<std::uint32_t>(pixels);
        }
        ImGui::InputScalar("Grid seed", ImGuiDataType_U64, &draft.gridSeed);
    }
    if (ImGui::CollapsingHeader("Edge functions")) {
        drawEdgePalette(draft.edgeColors);
    }
    if (ImGui::CollapsingHeader("Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawGenerator(draft.generator);
    }
    if (ImGui::CollapsingHeader("Color and tone")) {
        drawColorRamp(draft.colorStops, draft.tone);
    }
    if (ImGui::CollapsingHeader("Contours and material", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawMaterial(draft.material);
    }
    if (ImGui::CollapsingHeader("Diagnostics")) {
        int selected = static_cast<int>(debugView);
        const char* names[] = {"Pattern", "Jacobian", "Newton residual", "Tile edges"};
        if (ImGui::Combo("View", &selected, names, 4)) {
            debugView = static_cast<render::opengl::DebugView>(selected);
        }
        if (ImGui::Button("Reset camera", {-FLT_MIN, 0.0F})) {
            actions.resetCamera = true;
        }
    }

    ImGui::Separator();
    if (!dirty) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Apply validated draft", {230.0F, 36.0F})) {
        actions.applyDraft = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard", {-FLT_MIN, 36.0F})) {
        actions.discardDraft = true;
    }
    if (!dirty) {
        ImGui::EndDisabled();
    }
    ImGui::TextDisabled("Pan: left drag  |  Zoom: wheel  |  D: debug  |  R: camera");
    ImGui::End();
    return actions;
}

bool wantsMouseInput() noexcept {
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

bool wantsKeyboardInput() noexcept {
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace qrp::ui
