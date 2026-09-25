#pragma once

#include "model/PhaseCompatibleQrpTiles.hpp"
#include "model/QrpChannelComposition.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace qrp::study {

struct ChannelSpec { double frequency; double bias; double angle = 0.0; };

struct TileCase {
    const char* name;
    const char* label;
    ChannelSpec parent;
    std::optional<ChannelSpec> child;
    model::QrpChannelRelation relation;
    bool featured = true;
    const char* parentStyle = "";
    const char* childStyle = "";
};

// 实验配方只描述输入、窗口和输出组织；不负责采样、铺砌或 PNG 写出。
struct TileStudyRun {
    std::string subdirectory;
    std::string experiment;
    std::size_t size;
    std::size_t pixels;
    double sourceSpan;
    bool includeReferences = true;
    std::string parentStateFamily;
    double closureErrorBudget = 0.0;
    double closureError = 0.0;
    std::vector<TileCase> cases;
    std::array<math::Vec2, 2> parentPhaseOffsets{};
};

[[nodiscard]] std::span<const TileCase> basicTileCases();
[[nodiscard]] model::PhaseCompatibleQrpTileParameters phaseParameters(
    const ChannelSpec& channel, double sourceSpan);
[[nodiscard]] std::vector<TileStudyRun> tileStudyRuns(
    const std::string& study, std::optional<std::size_t> pixels);

} // namespace qrp::study
