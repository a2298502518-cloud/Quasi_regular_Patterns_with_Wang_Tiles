#include "render/Raster.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

int run(const std::vector<std::string>& args) {
    try {
        qrp::Parameters p;
        std::optional<double> radius;
        int span=24,pixels=64;
        std::uint32_t seed=11;
        const std::string project=QRP_PROJECT_DIR;
        const std::filesystem::path root(std::u8string(project.begin(),project.end()));
        std::filesystem::path output=root/"output/qrp-native-run";
        for (std::size_t i=1; i<args.size(); ++i) {
            const std::string option=args[i];
            if (option=="--help") { std::cout<<"qrp_generate --q Q --model basic|cubic-directions --frequency F --shift X Y --core-radius R --extent N --pixels P --seed S --output DIR\n"; return 0; }
            auto value=[&]() -> std::string { if (++i>=args.size()) throw std::invalid_argument("选项缺少值："+option); return args[i]; };
            if (option=="--q") p.q=std::stod(value());
            else if (option=="--frequency") p.frequency=std::stod(value());
            else if (option=="--shift") { p.shift.x=std::stod(value()); p.shift.y=std::stod(value()); }
            else if (option=="--core-radius") radius=std::stod(value());
            else if (option=="--extent") span=std::stoi(value());
            else if (option=="--pixels") pixels=std::stoi(value());
            else if (option=="--seed") seed=static_cast<std::uint32_t>(std::stoul(value()));
            else if (option=="--output") { const auto path=value(); output=std::filesystem::path(std::u8string(path.begin(),path.end())); }
            else if (option=="--model") {
                const auto model=value();
                if (model!="basic" && model!="cubic-directions") throw std::invalid_argument("未知源函数族。");
                p.model=model=="basic" ? qrp::Model::Basic : qrp::Model::CubicDirections;
            } else throw std::invalid_argument("未知选项："+option);
        }
        const auto start=std::chrono::steady_clock::now();
        qrp::rasterSide(span,pixels);
        const qrp::Atlas atlas(p,qrp::selectRegion(p,radius));
        auto report=qrp::atlasJson(atlas);
        const auto prepared=std::chrono::steady_clock::now();
        const auto palette=qrp::loadPalette(root/"tools/qrp_wang_defaults.json");
        const auto a=atlas.plan(span,seed),b=atlas.plan(span,37);
        if (atlas.mismatches(a) || atlas.mismatches(b)) throw std::runtime_error("生成布局边标签不相容。");
        qrp::savePng(qrp::renderWang(atlas,a,palette,pixels),output/"wang-a.png");
        qrp::savePng(qrp::renderWang(atlas,b,palette,pixels),output/"wang-b.png");
        qrp::savePng(qrp::renderSource(atlas.source(),palette,span,pixels),output/"source.png");
        report["extent"]=span; report["pixels_per_unit"]=pixels; report["seed"]=seed;
        report["layout_b_seed"]=37;
        report["layout_a"]=a.tiles; report["layout_b"]=b.tiles; report["edge_mismatches"]=0;
        report["prepare_seconds"]=std::chrono::duration<double>(prepared-start).count();
        report["elapsed_seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::ofstream metadata(output/"parameters.json"); metadata<<report.dump(2);
        if (!metadata) throw std::runtime_error("参数文件写入失败。");
        std::cout<<"Q="<<p.q<<"  "<<atlas.geometry().width<<'x'<<atlas.geometry().height<<" macro  "<<atlas.tileCount()<<" roles  "<<report["elapsed_seconds"]<<" s\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> args;
    for (int i=0; i<argc; ++i) { const auto value=std::filesystem::path(argv[i]).u8string(); args.emplace_back(value.begin(),value.end()); }
    return run(args);
}
#else
int main(int argc,char** argv) { return run(std::vector<std::string>(argv,argv+argc)); }
#endif
