#include "render/Raster.hpp"
#include <lodepng.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace qrp {
namespace {
template<class F> void parallel(int count, F&& work) {
    std::atomic<int> next{};
    const unsigned threads=std::min(static_cast<unsigned>(count),std::max(1u,std::thread::hardware_concurrency()));
    std::vector<std::jthread> workers;
    std::exception_ptr failure;
    std::mutex failureMutex;
    for (unsigned t=0; t<threads; ++t) workers.emplace_back([&] {
        try { for (int i=next.fetch_add(1); i<count; i=next.fetch_add(1)) work(i); }
        catch (...) { std::lock_guard lock(failureMutex); if (!failure) failure=std::current_exception(); next.store(count); }
    });
    workers.clear(); // 汇合后才允许调用方读取结果，异常交回应用线程处理。
    if (failure) std::rethrow_exception(failure);
}
struct ColorMap {
    const Palette& palette;
    std::vector<std::array<double,3>> linear;
    explicit ColorMap(const Palette& p) : palette(p) {
        if (p.colors.size()!=p.levels.size()+1 || !std::is_sorted(p.levels.begin(),p.levels.end()) ||
            std::any_of(p.levels.begin(),p.levels.end(),[](double v) { return !std::isfinite(v); })) throw std::invalid_argument("配色阈值必须有限且排序，颜色数须比阈值数多一。");
        for (const auto& c : p.colors) {
            std::array<double,3> rgb;
            for (int i=0; i<3; ++i) { const double v=c[i]/255.; rgb[i]=v<=.04045 ? v/12.92 : std::pow((v+.055)/1.055,2.4); }
            linear.push_back(rgb);
        }
    }
    template<class Evaluate> Color pixel(const Vec2& origin, double step, const Evaluate& evaluate) const {
        std::array<double,3> sum{};
        for (int y=0; y<2; ++y) for (int x=0; x<2; ++x) {
            const double value=evaluate(Vec2{origin.x+(x+.5)*step/2,origin.y-(y+.5)*step/2}).value;
            // 保持 lower_bound 的等号侧；避免 Debug STL 在每个采样注册共享容器迭代器。
            std::size_t low=0,high=palette.levels.size();
            while (low<high) { const auto middle=(low+high)/2; if (palette.levels[middle]<value) low=middle+1; else high=middle; }
            const auto index=low;
            for (int i=0; i<3; ++i) sum[i]+=linear[index][i]/4;
        }
        Color result;
        for (int i=0; i<3; ++i) {
            const double v=sum[i], encoded=v<=.0031308 ? 12.92*v : 1.055*std::pow(v,1/2.4)-.055;
            result[i]=static_cast<unsigned char>(std::clamp(std::floor(encoded*255+.5),0.,255.));
        }
        return result;
    }
};
Image createImage(int span, int pixels) {
    const int side=rasterSide(span,pixels);
    return {side,side,std::vector<unsigned char>(static_cast<std::size_t>(side)*side*3)};
}
}
int rasterSide(int span,int pixels) {
    const auto side=static_cast<std::int64_t>(span)*pixels;
    if (span<1 || pixels<1 || side>16384) throw std::length_error("当前图像单边预算为 16384 像素，请降低画幅或分辨率。");
    return static_cast<int>(side);
}
Palette loadPalette(const std::filesystem::path& defaults) {
    std::ifstream input(defaults);
    if (!input) throw std::runtime_error("无法打开默认参数文件。");
    nlohmann::json document; input>>document;
    const auto& style=document.at("render");
    if (style.at("kind")!="bands" || style.contains("outline_half_width")) throw std::runtime_error("当前原生入口维护固定高度分色，不隐式降级其他显示模式。");
    return {style.at("levels").get<std::vector<double>>(),style.at("colors").get<std::vector<Color>>()};
}
Image renderWang(const Atlas& atlas, const Layout& layout, const Palette& palette, int pixels) {
    Image image=createImage(layout.span,pixels);
    const ColorMap color(palette);
    const std::set<int> unique(layout.tiles.begin(),layout.tiles.end());
    const std::vector<int> ids(unique.begin(),unique.end());
    std::vector<std::vector<unsigned char>> tiles(ids.size());
    std::vector<int> index(atlas.tileCount(),-1);
    // 只缓存本次布局实际用到的像素；没有预生成磁盘图片库或旧类型目录。
    for (std::size_t i=0; i<ids.size(); ++i) index[ids[i]]=static_cast<int>(i);
    parallel(static_cast<int>(ids.size()),[&](int k) {
        auto& tile=tiles[k]; tile.resize(static_cast<std::size_t>(pixels)*pixels*3);
        for (int row=0; row<pixels; ++row) for (int col=0; col<pixels; ++col) {
            const auto rgb=color.pixel({static_cast<double>(col)/pixels,1-static_cast<double>(row)/pixels},1./pixels,
                                      [&](const Vec2& p) { return atlas.field(ids[k],p); });
            std::copy(rgb.begin(),rgb.end(),tile.data()+(static_cast<std::size_t>(row)*pixels+col)*3);
        }
    });
    parallel(layout.span,[&](int row) {
        for (int col=0; col<layout.span; ++col) {
            const auto& tile=tiles[index[layout.at(col,layout.span-1-row)]];
            for (int y=0; y<pixels; ++y) std::copy_n(tile.data()+static_cast<std::size_t>(y)*pixels*3,pixels*3,
                image.rgb.data()+(static_cast<std::size_t>(row*pixels+y)*image.width+col*pixels)*3);
        }
    });
    return image;
}
Image renderSource(const Source& source, const Palette& palette, int span, int pixels) {
    Image image=createImage(span,pixels);
    const ColorMap color(palette);
    parallel(span,[&](int row) {
        for (int col=0; col<image.width; ++col) for (int y=0; y<pixels; ++y) {
            const int pixelRow=row*pixels+y;
            const auto rgb=color.pixel({static_cast<double>(col)/pixels-span/2.,span/2.-static_cast<double>(pixelRow)/pixels},1./pixels,
                                      [&](const Vec2& p) { return source.jet(p); });
            std::copy(rgb.begin(),rgb.end(),image.rgb.data()+(static_cast<std::size_t>(pixelRow)*image.width+col)*3);
        }
    });
    return image;
}
void savePng(const Image& image, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    lodepng::State state;
    state.encoder.auto_convert=0; // 固定 RGB8，避免库按颜色数自动改成索引色 PNG。
    state.info_raw.colortype=LCT_RGB; state.info_raw.bitdepth=8;
    state.info_png.color.colortype=LCT_RGB; state.info_png.color.bitdepth=8;
    state.info_png.srgb_defined=1; state.info_png.srgb_intent=0;
    state.info_png.phys_defined=1; state.info_png.phys_x=11811; state.info_png.phys_y=11811; state.info_png.phys_unit=1;
    std::vector<unsigned char> encoded;
    const auto error=lodepng::encode(encoded,image.rgb,image.width,image.height,state);
    if (error) throw std::runtime_error(lodepng_error_text(error));
    // 宽路径由 filesystem 打开，避免中文项目路径被 PNG 库窄字符文件名破坏。
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(encoded.data()),static_cast<std::streamsize>(encoded.size()));
    if (!output) throw std::runtime_error("PNG 文件写入失败。");
}
nlohmann::json parametersJson(const Parameters& p) {
    return { {"q",p.q},{"model",p.model==Model::Basic ? "basic" : "cubic-directions"}, {"frequency",p.frequency},{"shift",{p.shift.x,p.shift.y}} };
}
Parameters parametersFromJson(const nlohmann::json& value) {
    Parameters p; p.q=value.at("q"); p.frequency=value.at("frequency");
    const std::string model=value.at("model");
    if (model!="basic" && model!="cubic-directions") throw std::invalid_argument("未知线性相位源函数族。");
    p.model=model=="basic" ? Model::Basic : Model::CubicDirections;
    p.shift={value.at("shift").at(0),value.at("shift").at(1)};
    return p;
}
nlohmann::json atlasJson(const Atlas& atlas) {
    const auto& r=atlas.region(); const auto& g=atlas.geometry();
    nlohmann::json samples=nlohmann::json::array();
    for (int tile=0; tile<atlas.tileCount(); tile+=std::max(1,atlas.tileCount()/17)) {
        const auto jet=atlas.field(tile,{.371,.613});
        samples.push_back({{"tile",tile},{"point",{.371,.613}},{"jet",{jet.value,jet.dx,jet.dy}}});
    }
    return {{"parameters",parametersJson(atlas.source().parameters())},
        {"probe",{{"radius",r.radius},{"physical_half_bounds",{r.bounds.x,r.bounds.y}},{"transform",{{r.transform[0],r.transform[1]},{r.transform[2],r.transform[3]}}},{"automatic",r.automatic}}},
        {"cell_world_size",{g.width,g.height}},{"catalogue_size",atlas.tileCount()},
        {"analytic_relative_wave_vector_bound",g.relativeBound},{"proven_sufficient_square_size",g.sufficientSquare},
        {"base_integer_increments",atlas.increments()},{"relation_matrix",atlas.lattice().relations()},
        {"samples",samples},{"connection","relational"},{"implicit_fitted_images",0}};
}
}
