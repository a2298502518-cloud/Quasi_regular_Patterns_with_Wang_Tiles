#pragma once

#include "TileStudyRecipes.hpp"

#include <filesystem>

namespace qrp::study {

// 先烘焙位置无关内容库，再仅以 ID 复制到不同布局。
void runReusableTileStudy(const std::vector<TileStudyRun>& runs, const std::filesystem::path& directory);

} // namespace qrp::study
