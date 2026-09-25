#include "TileStudyExport.hpp"
#include "color/InkCoverage.hpp"

#include <iomanip>
#include <random>
#include <stdexcept>

namespace qrp::study {
using render::Image;

TileStudyOutput::TileStudyOutput(const TileStudyRun& run, const std::filesystem::path& directory,
    const char* modelName)
    : directory_(directory), pixels_(run.pixels),
      a_(makeLayout(run.size, 20260922, run.size < 4 ? 0 : 1, run.size < 4 ? 0 : 2)),
      b_(makeLayout(run.size, 20260923, run.size < 4 ? run.size-1 : run.size-3,
          run.size < 4 ? run.size-1 : run.size-4)) {
    std::filesystem::create_directories(directory_);
    manifest_.exceptions(std::ios::badbit | std::ios::failbit);
    manifest_.open(directory_ / "manifest.json");
    writeHeader(run, modelName);
}

TileStudyOutput::Layout TileStudyOutput::makeLayout(const std::size_t size, const std::uint64_t seed, const std::size_t markedX, const std::size_t markedY) {
    std::mt19937_64 random(seed);
    std::vector<std::uint32_t> vertices((size+1)*(size+1));
    for (auto& state : vertices) state = static_cast<std::uint32_t>(random() & 1U);
    // 同一个类型 6 在两个不同位置出现；修改共享顶点而不是强行覆盖单块标签。
    for (std::size_t y = 0; y < 2; ++y) for (std::size_t x = 0; x < 2; ++x) {
        vertices[(markedY+y)*(size+1)+markedX+x] = (6U >> (x+2*y)) & 1U;
    }
    Layout result{size, seed, markedX, markedY, std::vector<std::uint32_t>(size*size)};
    for (std::size_t y = 0; y < size; ++y) for (std::size_t x = 0; x < size; ++x) {
        const auto sw = vertices[y*(size+1)+x], se = vertices[y*(size+1)+x+1];
        const auto nw = vertices[(y+1)*(size+1)+x], ne = vertices[(y+1)*(size+1)+x+1];
        const auto id = sw + 2*se + 4*nw + 8*ne;
        result.ids[y*size+x] = id;
        const auto edges = model::EndpointWangTiles::labels(id);
        if ((x > 0 && model::EndpointWangTiles::labels(result.ids[y*size+x-1]).east != edges.west)
            || (y > 0 && model::EndpointWangTiles::labels(result.ids[(y-1)*size+x]).north != edges.south)) {
            throw std::logic_error("Endpoint-coded Wang layout is not legal.");
        }
    }
    return result;
}

std::array<render::Rgb8, 5> TileStudyOutput::coverageColors() {
    std::array<qrp::render::Rgb8, 5> result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const double coverage = static_cast<double>(i) / 4.0;
        result[i] = color::inkCoverage(coverage);
    }
    return result;
}

Image TileStudyOutput::assemble(const std::vector<Image>& library, const Layout& placement) {
    const std::size_t pixels = library.front().width();
    const std::size_t size = placement.size;
    Image result(size*pixels,size*pixels);
    // 铺砌层只有 ID 查找和逐像素复制，没有场求值、全局相位或接缝后处理。
    for (std::size_t y = 0; y < size; ++y) for (std::size_t x = 0; x < size; ++x) {
        const auto& tile = library[placement.ids[y*size+x]];
        for (std::size_t py = 0; py < pixels; ++py) for (std::size_t px = 0; px < pixels; ++px) {
            result.pixel(x*pixels+px,(size-1-y)*pixels+py) = tile.pixel(px,py);
        }
    }
    return result;
}

void TileStudyOutput::writeLayout(const Layout& grid, std::ostream& out) {
    out << "{\"seed\":" << grid.seed << ",\"marked\":[" << grid.markedX << ',' << grid.markedY << "],\"ids\":[";
    for (std::size_t i = 0; i < grid.ids.size(); ++i) out << (i ? "," : "") << grid.ids[i];
    out << "]}";
}
// 共同的输出协议；案例名称、标题和父子关系来自配方，不在图板中复制。
void TileStudyOutput::writeHeader(const TileStudyRun& run, const char* modelName) {
    manifest_ << std::setprecision(17) << "{\"schema\":2,\"study\":\"reusable-tiles\",\"model\":\""
        << modelName << "\",\"experiment\":\"" << run.experiment
        << "\",\"include_references\":" << (run.includeReferences ? "true" : "false")
        << ",\"grid_size\":" << run.size << ",\"tile_count\":16,\"pixels_per_tile\":" << run.pixels
        << ",\"source_span\":" << run.sourceSpan << ",\"supersampling\":2,\"level\":0,"
        << "\"ink\":[" << int(color::kInk.red) << ',' << int(color::kInk.green) << ',' << int(color::kInk.blue)
        << "],\"paper\":[" << int(color::kPaper.red) << ',' << int(color::kPaper.green) << ',' << int(color::kPaper.blue)
        << "],\"layout_row_order\":\"bottom-up\",";
    if (run.closureErrorBudget > 0.0) {
        manifest_ << "\"closure_error_budget\":" << run.closureErrorBudget
            << ",\"max_relative_closure_error\":" << run.closureError << ',';
    }
    if (!run.parentStateFamily.empty()) manifest_ << "\"parent_state_family\":\"" << run.parentStateFamily << "\",";
    manifest_ << "\"layouts\":{\"A\":";
    writeLayout(a_, manifest_);
    manifest_ << ",\"B\":";
    writeLayout(b_, manifest_);
    manifest_ << "},\"tile_edges\":[";
    for (std::uint32_t id = 0; id < model::EndpointWangTiles::count; ++id) {
        const auto e = model::EndpointWangTiles::labels(id);
        manifest_ << (id ? "," : "") << '[' << e.south << ',' << e.north << ',' << e.west << ',' << e.east << ']';
    }
    manifest_ << "],\"cases\":[\n";
}

void TileStudyOutput::beginCase(const TileCase& recipe, const TileCaseOutput& options, const double error) {
    const std::string style = recipe.name;
    const auto& selection = options.selection;
    const std::string id = selection + '_' + style;
    const model::QrpChannelCompositionParameters compositionParameters{recipe.relation};
    if (!firstCase_) manifest_ << ",\n";
    firstCase_ = false;
    manifest_ << "{\"id\":\"" << id << "\",\"style\":\"" << style << "\",\"selection\":\"" << selection
        << "\",\"relation\":\"" << relationName(compositionParameters.relation) << '"'
        << ",\"parent_level\":" << compositionParameters.parentLevel
        << ",\"gate_width\":" << compositionParameters.transitionWidth;
    manifest_ << ",\"label\":\"" << recipe.label << "\",\"featured\":"
        << (recipe.featured ? "true" : "false")
        << ",\"parent_style\":\"" << recipe.parentStyle << "\",\"child_style\":\"" << recipe.childStyle << '"';
    if (options.periodicControl) manifest_ << ",\"periodic_control\":\"repeat_0.png\"";
    manifest_ << ",\"max_seam_error\":" << error << ",\"channels\":[";
}

void TileStudyOutput::finish(const std::string_view additionalRootFields) {
    manifest_ << "\n]" << additionalRootFields << "}\n";
    manifest_.close();
}

} // namespace qrp::study
