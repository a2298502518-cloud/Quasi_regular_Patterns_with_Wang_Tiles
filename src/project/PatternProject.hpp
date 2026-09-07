#pragma once

#include "color/GradientPalette.hpp"
#include "generators/HybridTorusGenerator.hpp"
#include "math/EdgeFunction.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"
#include "presets/PatternPreset.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace qrp::project {

struct PatternConfiguration {
    std::string name;
    std::size_t gridWidth = 10;
    std::size_t gridHeight = 10;
    std::uint32_t pixelsPerTile = 72;
    std::uint64_t gridSeed = 0;
    std::vector<math::EdgeParameters> edgeColors;
    generators::HybridGeneratorSettings generator;
    std::vector<color::ColorStop> colorStops;
    color::ToneSettings tone;

    [[nodiscard]] bool operator==(const PatternConfiguration&) const noexcept = default;
};

struct PatternScene {
    model::EdgePalette edgePalette;
    model::WangGrid grid;
    generators::HybridTorusGenerator generator;
    color::GradientPalette colorPalette;
};

struct ApplyResult {
    bool applied = false;
    std::uint64_t revision = 0;
    double minimumDeterminantLowerBound = 0.0;
    std::string message;
};

[[nodiscard]] PatternConfiguration configurationFromPreset(
    const presets::PatternPreset& preset);

// 会话是 draft/committed 的唯一所有者；Apply 先在临时对象中完整校验，成功后再原子替换。
class PatternProject final {
public:
    explicit PatternProject(PatternConfiguration initial);

    [[nodiscard]] PatternConfiguration& draft() noexcept;
    [[nodiscard]] const PatternConfiguration& draft() const noexcept;
    [[nodiscard]] const PatternConfiguration& committed() const noexcept;
    [[nodiscard]] const PatternScene& scene() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] bool isDirty() const noexcept;
    [[nodiscard]] const ApplyResult& lastApplyResult() const noexcept;

    void loadPresetIntoDraft(const presets::PatternPreset& preset);
    void resetDraft();
    [[nodiscard]] ApplyResult applyDraft();

private:
    [[nodiscard]] static std::pair<std::optional<PatternScene>, ApplyResult> buildScene(
        const PatternConfiguration& configuration,
        std::uint64_t revision);

    PatternConfiguration draft_;
    PatternConfiguration committed_;
    PatternScene scene_;
    std::uint64_t revision_ = 1;
    ApplyResult lastApplyResult_;
};

} // namespace qrp::project
