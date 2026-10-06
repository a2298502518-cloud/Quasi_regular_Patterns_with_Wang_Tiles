#include "qrp/Field.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
double error(const qrp::Jet& a,const qrp::Jet& b) { return std::max({std::abs(a.value-b.value),std::abs(a.dx-b.dx),std::abs(a.dy-b.dy)}); }
int main() {
    try {
        for (const auto& p : {qrp::Parameters{5.5,12,qrp::Model::Basic,{}},qrp::Parameters{4.8,12,qrp::Model::CubicDirections,{}},qrp::Parameters{7,12,qrp::Model::Basic,{.3,.2}}}) {
            const qrp::Atlas atlas(p,qrp::selectRegion(p,6));
            const auto a=atlas.plan(12,11),large=atlas.plan(24,11);
            require(atlas.mismatches(a)==0,"边标签不合法");
            require(atlas.geometry().relativeBound<=.5,"解析相位界错误");
            for (int y=0; y<12; ++y) for (int x=0; x<12; ++x) require(a.at(x,y)==large.at(x,y),"扩幅改变原布局");
            std::map<std::pair<int,int>,qrp::Jet> traces;
            for (int tile=0; tile<atlas.tileCount(); ++tile) {
                for (int side=0; side<4; ++side) for (int s=0; s<=16; ++s) {
                    const double t=s/16.;
                    const qrp::Vec2 point=side==0 ? qrp::Vec2{t,0} : side==1 ? qrp::Vec2{t,1} : side==2 ? qrp::Vec2{0,t} : qrp::Vec2{1,t};
                    const auto key=std::pair(atlas.edges()[tile][side],s);
                    const auto value=atlas.field(tile,point);
                    if (traces.contains(key)) require(error(value,traces.at(key))<1e-9,"完整目录边迹不一致");
                    else traces.emplace(key,value);
                }
            }
            for (int color=0; color<2; ++color) for (double dx : {-.25,0.,.25}) for (double dy : {-.25,0.,.25}) {
                const auto center=atlas.shifts()[color];
                const double x=center.x+dx,y=center.y+dy;
                const int w=atlas.geometry().width,h=atlas.geometry().height;
                const double lx=x-std::floor(x/w)*w,ly=y-std::floor(y/h)*h;
                const int i=std::min(static_cast<int>(lx),w-1),j=std::min(static_cast<int>(ly),h-1);
                require(error(atlas.field(15*color*w*h+j*w+i,{lx-i,ly-j}),atlas.source().jet({dx,dy}))<1e-9,"保护核心与原源不同");
            }
            constexpr double step=1e-6;
            const auto value=atlas.field(0,{.371,.613});
            require(std::abs(value.dx-(atlas.field(0,{.371+step,.613}).value-atlas.field(0,{.371-step,.613}).value)/(2*step))<1e-7,"x 梯度错误");
            require(std::abs(value.dy-(atlas.field(0,{.371,.613+step}).value-atlas.field(0,{.371,.613-step}).value)/(2*step))<1e-7,"y 梯度错误");
        }
        std::cout<<"Current native source, seams, protected core and extension passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
