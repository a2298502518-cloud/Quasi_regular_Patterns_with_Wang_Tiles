#include "render/Raster.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace {
const std::string project=QRP_PROJECT_DIR;
const std::filesystem::path root(std::u8string(project.begin(),project.end()));
using Clock=std::chrono::steady_clock;
struct Settings {
    qrp::Parameters source;
    std::optional<double> radius;
    int span{24}, pixels{48}, seed{11};
    bool uniform{};
    qrp::Palette palette;
    bool operator==(const Settings&) const = default;
};
struct Result {
    Settings settings;
    std::shared_ptr<const qrp::Atlas> atlas;
    qrp::Image wang, source;
    double prepareSeconds{}, renderSeconds{};
};
std::shared_ptr<Result> generate(const Settings& s, const std::shared_ptr<const Result>& previous) {
    const auto start=Clock::now();
    auto result=std::make_shared<Result>(); result->settings=s;
    if (s.span<1 || s.pixels<1 || s.seed<0) throw std::invalid_argument("画幅、分辨率须为正数，布局种子须非负。");
    qrp::rasterSide(s.span,s.pixels);
    if (previous && previous->settings.source==s.source && previous->settings.radius==s.radius) result->atlas=previous->atlas;
    else result->atlas=std::make_shared<qrp::Atlas>(s.source,qrp::selectRegion(s.source,s.radius));
    const auto prepared=Clock::now();
    const auto layout=result->atlas->plan(s.span,static_cast<std::uint32_t>(s.seed),s.uniform);
    if (result->atlas->mismatches(layout)) throw std::runtime_error("布局边色不相容。");
    result->wang=qrp::renderWang(*result->atlas,layout,s.palette,s.pixels);
    result->source=qrp::renderSource(result->atlas->source(),s.palette,s.span,s.pixels);
    result->prepareSeconds=std::chrono::duration<double>(prepared-start).count();
    result->renderSeconds=std::chrono::duration<double>(Clock::now()-prepared).count();
    return result;
}
std::filesystem::path saveResult(const Result& result) {
    // 每次导出新建独立目录，既有论文交付和其他预览不被覆盖。
    const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto directory=root/"output/native-workbench"/std::to_string(stamp);
    while (std::filesystem::exists(directory)) directory+=L"-new";
    qrp::savePng(result.wang,directory/"wang.png"); qrp::savePng(result.source,directory/"source.png");
    auto report=qrp::atlasJson(*result.atlas);
    report["extent"]=result.settings.span; report["pixels_per_unit"]=result.settings.pixels;
    report["seed"]=result.settings.seed; report["uniform"]=result.settings.uniform;
    report["render"]={{"kind","bands"},{"levels",result.settings.palette.levels},{"colors",result.settings.palette.colors}};
    report["prepare_seconds"]=result.prepareSeconds; report["render_seconds"]=result.renderSeconds;
    std::ofstream output(directory/"parameters.json"); output<<report.dump(2);
    if (!output) throw std::runtime_error("导出参数写入失败。");
    return directory;
}
struct Texture {
    GLuint id{};
    ~Texture() { if (id) glDeleteTextures(1,&id); }
    void upload(const qrp::Image& image) {
        GLint limit; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
        if (image.width>limit || image.height>limit) throw std::runtime_error("预览图超过显卡纹理尺寸上限；请降低预览分辨率，高清可独立导出。");
        if (!id) glGenTextures(1,&id);
        glBindTexture(GL_TEXTURE_2D,id); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,image.width,image.height,0,GL_RGB,GL_UNSIGNED_BYTE,image.rgb.data());
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL 纹理上传失败。");
    }
};
class Workbench {
public:
    Workbench() {
        draft_.palette=qrp::loadPalette(root/"tools/qrp_wang_defaults.json");
        std::ifstream input(root/"tools/qrp_wang_defaults.json"); input>>defaults_;
        for (const auto& value : defaults_.at("cases")) if (value.at("name")=="q7") draft_.source=qrp::parametersFromJson(value);
        startPreview();
    }
    void poll() {
        if (!job_.valid() || job_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        try {
            auto finished=job_.get();
            if (exporting_) status_="导出完成："+finished.directory.filename().string();
            else {
                Texture nextWang,nextSource;
                nextWang.upload(finished.result->wang); nextSource.upload(finished.result->source);
                std::swap(wang_.id,nextWang.id); std::swap(source_.id,nextSource.id);
                current_=std::move(finished.result); status_="已生成：C++ 公式求值 / OpenGL 显示";
            }
            error_.clear();
        } catch (const std::exception& e) { error_=e.what(); status_="本次操作失败；上次结果保留"; }
    }
    bool busy() const { return job_.valid(); }
    bool ready() const { return static_cast<bool>(current_); }
    bool failed() const { return !error_.empty(); }
    void savePreview() const { if (current_) saveResult(*current_); }
    void draw() {
        const auto viewport=ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos); ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("QRP x Wang",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("QRP × WANG  /  原生纹样操作台");
        ImGui::SameLine(); ImGui::TextDisabled("C++ 数学核心 · OpenGL · 不调用 Python");
        ImGui::Separator();
        ImGui::BeginChild("controls",ImVec2(340,0),ImGuiChildFlags_Borders); controls(); ImGui::EndChild();
        ImGui::SameLine(); ImGui::BeginChild("canvas",ImVec2(0,0)); canvas(); ImGui::EndChild();
        ImGui::End();
    }
private:
    struct Job { std::shared_ptr<Result> result; std::filesystem::path directory; };
    Settings draft_;
    nlohmann::json defaults_;
    std::shared_ptr<const Result> current_;
    std::future<Job> job_;
    bool exporting_{},showGrid_{},sourceView_{};
    int exportPixels_{192};
    float zoom_{1}; ImVec2 pan_{};
    Texture wang_,source_;
    std::string status_="正在生成默认 Q7…",error_;
    void startPreview() {
        const auto settings=draft_; const auto previous=current_;
        exporting_=false; error_.clear(); status_="正在计算；可继续编辑草稿";
        job_=std::async(std::launch::async,[settings,previous] { return Job{generate(settings,previous),{}}; });
    }
    void startExport(bool highResolution) {
        const auto previous=current_; auto settings=previous->settings;
        if (highResolution) settings.pixels=exportPixels_;
        exporting_=true; error_.clear(); status_="正在导出已应用参数；草稿不进入导出";
        job_=std::async(std::launch::async,[settings,previous,highResolution] {
            auto result=highResolution ? generate(settings,previous) : std::make_shared<Result>(*previous);
            return Job{result,saveResult(*result)};
        });
    }
    void controls() {
        ImGui::TextUnformatted("源参数（QRP）");
        ImGui::InputDouble("Q",&draft_.source.q,.1,1,"%.6g");
        int model=draft_.source.model==qrp::Model::Basic ? 0 : 1;
        if (ImGui::Combo("方向函数族",&model,"基础方向\0三次方向\0")) draft_.source.model=model==0 ? qrp::Model::Basic : qrp::Model::CubicDirections;
        ImGui::InputDouble("频率 λ",&draft_.source.frequency,.5,2,"%.6g");
        ImGui::InputDouble("源偏移 X",&draft_.source.shift.x,.1,1,"%.6g");
        ImGui::InputDouble("源偏移 Y",&draft_.source.shift.y,.1,1,"%.6g");
        bool automatic=!draft_.radius.has_value();
        if (ImGui::Checkbox("自动选取保护区域",&automatic)) draft_.radius=automatic ? std::optional<double>{} : std::optional<double>{6.};
        if (draft_.radius) ImGui::InputDouble("源区域半径",&*draft_.radius,.25,1,"%.6g");
        ImGui::TextWrapped("自动选区是启发式探针，不保证所有 Q 都能识别母题。失败时可手动指定源区域。");
        if (ImGui::CollapsingHeader("已有代表输入")) {
            for (const auto& value : defaults_.at("cases")) {
                const auto name=value.at("name").get<std::string>();
                if (ImGui::Button(name.c_str())) { draft_.source=qrp::parametersFromJson(value); draft_.radius.reset(); }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        ImGui::Separator(); ImGui::TextUnformatted("画幅与显示");
        ImGui::InputInt("单位 tile 画幅",&draft_.span);
        ImGui::InputInt("预览 px / 单位",&draft_.pixels);
        ImGui::TextDisabled("%d × %d 单位；不是母题数量",draft_.span,draft_.span);
        if (ImGui::CollapsingHeader("铺砌对照（非风格参数）")) {
            ImGui::InputInt("布局种子",&draft_.seed);
            ImGui::Checkbox("全同状态周期对照",&draft_.uniform);
        }
        if (ImGui::CollapsingHeader("高度阈值与配色")) {
            for (std::size_t i=0; i<draft_.palette.levels.size(); ++i) {
                ImGui::PushID(static_cast<int>(i)); ImGui::InputDouble("阈值",&draft_.palette.levels[i],.01,.1,"%.3f"); ImGui::PopID();
            }
            for (std::size_t i=0; i<draft_.palette.colors.size(); ++i) {
                auto& c=draft_.palette.colors[i]; float color[3]={c[0]/255.F,c[1]/255.F,c[2]/255.F};
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::ColorEdit3("分色",color)) for (int j=0; j<3; ++j) c[j]=static_cast<unsigned char>(std::clamp(std::lround(color[j]*255),0L,255L));
                ImGui::PopID();
            }
        }
        ImGui::Separator();
        ImGui::BeginDisabled(busy());
        if (ImGui::Button("应用参数 / 生成",ImVec2(-1,0))) startPreview();
        ImGui::EndDisabled();
        if (current_ && !(draft_==current_->settings)) ImGui::TextWrapped("有未应用草稿。画面和导出仍使用上次已应用参数。");
        ImGui::InputInt("高清 px / 单位",&exportPixels_);
        ImGui::BeginDisabled(busy() || !current_);
        if (ImGui::Button("导出高清 PNG")) startExport(true);
        ImGui::SameLine(); if (ImGui::Button("保存预览")) startExport(false);
        ImGui::EndDisabled();
        if (current_) {
            const auto& g=current_->atlas->geometry();
            ImGui::Separator();
            ImGui::Text("自动宏单元：%d × %d",g.width,g.height);
            ImGui::Text("完整单位角色：%d",current_->atlas->tileCount());
            ImGui::Text("解析相对波矢界：%.4f",g.relativeBound);
            ImGui::Text("准备 %.3fs / 出图 %.3fs",current_->prepareSeconds,current_->renderSeconds);
        }
        ImGui::TextWrapped("%s",status_.c_str());
        if (!error_.empty()) { ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.45F,.35F,1)); ImGui::TextWrapped("%s",error_.c_str()); ImGui::PopStyleColor(); }
    }
    void canvas() {
        ImGui::Checkbox("纯 QRP 对照（同尺度）",&sourceView_); ImGui::SameLine(); ImGui::Checkbox("单位 tile 网格",&showGrid_);
        ImGui::SameLine(); if (ImGui::Button("适应窗口")) { zoom_=1; pan_={}; }
        ImGui::TextDisabled("滚轮缩放 · 左键拖动画布 · 网格不进入导出");
        if (!current_) { ImGui::TextUnformatted("等待首张纹样…"); return; }
        const auto available=ImGui::GetContentRegionAvail(), origin=ImGui::GetCursorScreenPos();
        const float side=std::min(available.x,available.y)*zoom_;
        const ImVec2 top{origin.x+(available.x-side)/2+pan_.x,origin.y+(available.y-side)/2+pan_.y};
        const ImVec2 bottom{top.x+side,top.y+side};
        ImGui::InvisibleButton("texture canvas",available,ImGuiButtonFlags_MouseButtonLeft);
        const auto& io=ImGui::GetIO();
        if (ImGui::IsItemHovered()) {
            if (io.MouseWheel!=0) {
                const float next=std::clamp(zoom_*std::pow(1.15F,io.MouseWheel),.2F,12.F), ratio=next/zoom_;
                const ImVec2 center{origin.x+available.x/2,origin.y+available.y/2};
                pan_={(pan_.x-(io.MousePos.x-center.x))*ratio+(io.MousePos.x-center.x),(pan_.y-(io.MousePos.y-center.y))*ratio+(io.MousePos.y-center.y)};
                zoom_=next;
            }
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { pan_.x+=io.MouseDelta.x; pan_.y+=io.MouseDelta.y; }
        auto* draw=ImGui::GetWindowDrawList();
        draw->PushClipRect(origin,ImVec2(origin.x+available.x,origin.y+available.y),true);
        draw->AddImage(static_cast<ImTextureID>(sourceView_ ? source_.id : wang_.id),top,bottom);
        if (showGrid_ && !sourceView_) for (int i=0; i<=current_->settings.span; ++i) {
            const float p=side*i/current_->settings.span;
            draw->AddLine({top.x+p,top.y},{top.x+p,bottom.y},IM_COL32(255,255,255,85));
            draw->AddLine({top.x,top.y+p},{bottom.x,top.y+p},IM_COL32(255,255,255,85));
        }
        draw->PopClipRect();
    }
};
}

int run(const std::vector<std::string>& args) {
    try {
        bool smoke=false;
        std::optional<std::filesystem::path> capture;
        for (std::size_t i=1; i<args.size(); ++i) {
            const std::string arg=args[i];
            if (arg=="--smoke") smoke=true;
            else if (arg=="--capture" && i+1<args.size()) { const auto value=args[++i]; capture=std::filesystem::path(std::u8string(value.begin(),value.end())); }
            else throw std::invalid_argument("未知桌面参数。");
        }
        glfwSetErrorCallback([](int code,const char* text) { std::cerr<<"GLFW "<<code<<": "<<text<<'\n'; });
        if (!glfwInit()) throw std::runtime_error("GLFW 初始化失败。");
        struct GlfwGuard { ~GlfwGuard() { glfwTerminate(); } } glfwGuard;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE,smoke ? GLFW_FALSE : GLFW_TRUE);
        std::unique_ptr<GLFWwindow,decltype(&glfwDestroyWindow)> window(glfwCreateWindow(1440,960,"QRP x Wang - C++ OpenGL",nullptr,nullptr),glfwDestroyWindow);
        if (!window) throw std::runtime_error("OpenGL 3.3 窗口创建失败。");
        glfwSetWindowSizeLimits(window.get(),1000,640,GLFW_DONT_CARE,GLFW_DONT_CARE);
        glfwMakeContextCurrent(window.get()); glfwSwapInterval(smoke ? 0 : 1);
        IMGUI_CHECKVERSION(); ImGui::CreateContext();
        struct UiGuard {
            bool glfw{}, opengl{};
            ~UiGuard() { if (opengl) ImGui_ImplOpenGL3_Shutdown(); if (glfw) ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); }
        } uiGuard;
        auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
        const char* font="C:/Windows/Fonts/msyh.ttc";
        if (std::filesystem::exists(font)) io.Fonts->AddFontFromFileTTF(font,19,nullptr,io.Fonts->GetGlyphRangesChineseFull());
        else io.Fonts->AddFontDefault();
        ImGui::StyleColorsDark();
        uiGuard.glfw=ImGui_ImplGlfw_InitForOpenGL(window.get(),true);
        if (uiGuard.glfw) uiGuard.opengl=ImGui_ImplOpenGL3_Init("#version 330");
        if (!uiGuard.opengl) throw std::runtime_error("ImGui / OpenGL 后端初始化失败。");
        std::cout<<"OpenGL: "<<glGetString(GL_VERSION)<<" / "<<glGetString(GL_RENDERER)<<'\n';
        Workbench app;
        const auto start=Clock::now();
        int completedFrames=0;
        while (!glfwWindowShouldClose(window.get())) {
            glfwPollEvents(); app.poll();
            if (smoke && app.failed()) throw std::runtime_error("桌面 smoke 生成或上传失败。");
            if (smoke && Clock::now()-start>std::chrono::seconds(60)) throw std::runtime_error("桌面 smoke 超时。");
            int width,height; glfwGetFramebufferSize(window.get(),&width,&height);
            if (width==0 || height==0) { glfwWaitEventsTimeout(.05); continue; }
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame(); app.draw(); ImGui::Render();
            glViewport(0,0,width,height); glClearColor(.06F,.08F,.1F,1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (app.ready() && !app.busy() && ++completedFrames==3 && (smoke || capture)) {
                if (capture) {
                    qrp::Image image{width,height,std::vector<unsigned char>(static_cast<std::size_t>(width)*height*3)};
                    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,image.rgb.data());
                    for (int y=0; y<height/2; ++y) for (int x=0; x<width*3; ++x) std::swap(image.rgb[static_cast<std::size_t>(y)*width*3+x],image.rgb[static_cast<std::size_t>(height-1-y)*width*3+x]);
                    qrp::savePng(image,*capture);
                }
                if (smoke) { app.savePreview(); break; }
            }
            glfwSwapBuffers(window.get());
            if (smoke) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (smoke) std::cout<<"Native formula generation, texture upload, UI and PNG export passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> args;
    for (int i=0; i<argc; ++i) { const auto value=std::filesystem::path(argv[i]).u8string(); args.emplace_back(value.begin(),value.end()); }
    return run(args);
}
#else
int main(int argc,char** argv) { return run(std::vector<std::string>(argv,argv+argc)); }
#endif
