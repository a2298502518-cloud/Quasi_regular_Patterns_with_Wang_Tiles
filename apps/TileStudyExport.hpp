#pragma once

#include "TileStudyRecipes.hpp"
#include "export/PngWriter.hpp"
#include "model/EndpointWangTiles.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace qrp::study {

// 一轮输出的布局、目录和清单由同一对象拥有，调用方只提供案例及通道。
// 热路径保留模板求值，不引入逐像素虚调用或 std::function 分派。
class TileStudyOutput final {
public:
    TileStudyOutput(const TileStudyRun& run, const std::filesystem::path& directory, const char* modelName);

    void writeCase(const TileCase& recipe, const model::PhaseCompatibleQrpTiles& first,
        const std::optional<model::PhaseCompatibleQrpTiles>& second, bool reference);

    // 成功完成所有案例后调用一次。
    void finish();

private:
    using Vec2 = math::Vec2;
    using Image = render::Image;
    struct Layout {
        std::size_t size;
        std::uint64_t seed;
        std::size_t markedX, markedY;
        std::vector<std::uint32_t> ids;
    };

    [[nodiscard]] static Layout makeLayout(std::size_t size, std::uint64_t seed,
        std::size_t markedX, std::size_t markedY);
    [[nodiscard]] static std::array<render::Rgb8, 5> coverageColors();
    [[nodiscard]] static Image assemble(const std::vector<Image>& library, const Layout& placement);
    static void writeLayout(const Layout& grid, std::ostream& out);
    void writeHeader(const TileStudyRun& run, const char* modelName);
    void beginCase(const TileCase& recipe, double error);

    template <typename Field>
    [[nodiscard]] static Image fill(const std::size_t extent, const std::size_t pixelsPerTile, const Field& field) {
        Image result(extent,extent);
        const auto colors = coverageColors();
        // 每像素固定 2×2 覆盖采样；点式填色不查询邻居，也不做几何距离描边。
        for (std::size_t y = 0; y < extent; ++y) for (std::size_t x = 0; x < extent; ++x) {
            std::size_t covered = 0;
            for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx) {
                const Vec2 p{(static_cast<double>(x) + (sx+0.5)*0.5) / pixelsPerTile,
                    (static_cast<double>(extent-y) - (sy+0.5)*0.5) / pixelsPerTile};
                covered += field(p).value < 0.0;
            }
            result.pixel(x,y) = colors[covered];
        }
        return result;
    }

    template <typename Field>
    [[nodiscard]] static double seamError(const Layout& grid, const Field& field) {
        double error = 0.0;
        const std::size_t size = grid.size;
        const auto compare = [&](const auto a, const auto b) {
            error = std::max({error,std::abs(a.value-b.value),std::abs(a.gradient.x-b.gradient.x),std::abs(a.gradient.y-b.gradient.y)});
        };
        for (std::size_t y = 0; y < size; ++y) for (std::size_t x = 0; x < size; ++x) {
            const auto id = grid.ids[y*size+x];
            for (const double t : {0.0,0.03,0.13,0.5,0.87,0.97,1.0}) {
                if (x+1 < size) compare(field(id,{1,t}),field(grid.ids[y*size+x+1],{0,t}));
                if (y+1 < size) compare(field(id,{t,1}),field(grid.ids[(y+1)*size+x],{t,0}));
            }
        }
        return error;
    }

    std::filesystem::path directory_;
    std::size_t pixels_;
    Layout a_, b_;
    std::ofstream manifest_;
    bool firstCase_ = true;
};

} // namespace qrp::study
