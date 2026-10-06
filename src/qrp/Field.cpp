#include "qrp/Field.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <tuple>

namespace qrp {
namespace {
constexpr double pi = std::numbers::pi;
double dot(const Vec2& a, const Vec2& b) { return a.x*b.x+a.y*b.y; }
double norm(const Vec2& a) { return std::hypot(a.x, a.y); }
using Polynomial = std::vector<std::int64_t>;
std::pair<Polynomial, Polynomial> divide(Polynomial value, const Polynomial& divisor) {
    Polynomial quotient(std::max(1, static_cast<int>(value.size())-static_cast<int>(divisor.size())+1));
    for (int high=static_cast<int>(value.size())-1; high>=static_cast<int>(divisor.size())-1; --high) {
        const int index=high-static_cast<int>(divisor.size())+1;
        quotient[index]=value[high];
        for (std::size_t j=0; j<divisor.size(); ++j) value[index+j]-=quotient[index]*divisor[j];
    }
    value.resize(std::min(value.size(), divisor.size()-1));
    return {quotient, value};
}
Polynomial cyclotomic(int n, std::map<int, Polynomial>& cache) {
    if (cache.contains(n)) return cache.at(n);
    Polynomial result(n+1); result.front()=-1; result.back()=1;
    for (int d=1; d<n; ++d) if (n%d==0) result=divide(result, cyclotomic(d, cache)).first;
    cache[n]=result;
    return result;
}
// 只需要判断精确二进制浮点分数的分子是否超过阈值，不将非整数 Q 擅自取整。
std::uint64_t rationalNumerator(double q, std::uint64_t threshold) {
    const auto bits=std::bit_cast<std::uint64_t>(q);
    auto numerator=(bits&((1ULL<<52)-1))|(1ULL<<52);
    int exponent=static_cast<int>((bits>>52)&0x7ff)-1023-52;
    while (exponent<0 && numerator%2==0) { numerator/=2; ++exponent; }
    if (exponent>0) {
        if (exponent>=63 || numerator>(threshold>>exponent)) return threshold+1;
        numerator<<=exponent;
    }
    return numerator;
}
std::array<double, 2> transition(double x, double core, double length) {
    const double gap=length-2*core, t=std::clamp((x-core)/gap, 0., 1.);
    return {t*t*t*(10-15*t+6*t*t), 30*t*t*(1-t)*(1-t)/gap};
}
std::uint32_t nodeState(int x, int y, std::uint32_t seed) {
    auto n=(static_cast<std::uint32_t>(x)*0x9e3779b1u)^(static_cast<std::uint32_t>(y)*0x85ebca77u)^seed;
    n=(n^(n>>16))*0x7feb352du; n=(n^(n>>15))*0x846ca68bu;
    return (n^(n>>16))&1u;
}
}

Source::Source(const Parameters& p) : parameters_(p) {
    if (!std::isfinite(p.q) || p.q<1 || !std::isfinite(p.frequency) || p.frequency<=0 ||
        !std::isfinite(p.shift.x) || !std::isfinite(p.shift.y)) throw std::invalid_argument("需要有限 Q>=1、正频率及有限相位偏移。");
    // 这是数值矩阵的内存预算，不是理论的 Q 取值白名单。
    if (std::floor(p.q)>2048) throw std::length_error("当前桌面数值矩阵预算为 2048 项；理论参数域不受此预算限制。");
    for (int m=1; m<=static_cast<int>(p.q); ++m) {
        const double angle=2*pi*m/p.q;
        Vec2 v{std::cos(angle), std::sin(angle)};
        if (p.model==Model::CubicDirections) { v.x=v.x*v.x*v.x; v.y=v.y*v.y*v.y; }
        directions_.push_back(v); waves_.push_back({p.frequency*v.x, p.frequency*v.y});
    }
}
Jet Source::jet(const Vec2& p) const {
    Jet result;
    for (std::size_t m=0; m<waves_.size(); ++m) {
        const double phase=dot(waves_[m], p)+dot(directions_[m], parameters_.shift), derivative=-std::sin(phase);
        result.value+=std::cos(phase); result.dx+=derivative*waves_[m].x; result.dy+=derivative*waves_[m].y;
    }
    const double count=static_cast<double>(waves_.size());
    return {result.value/count, result.dx/count, result.dy/count};
}

Region selectRegion(const Parameters& parameters, std::optional<double> radius) {
    Region region;
    if (radius) {
        if (!std::isfinite(*radius) || *radius<=0) throw std::invalid_argument("显式源区域半径必须为有限正数。");
        region.radius=*radius; region.bounds={*radius/parameters.frequency, *radius/parameters.frequency};
        return region;
    }
    auto normalized=parameters; normalized.frequency=1;
    const Source source(normalized);
    // 选区沿用研究探针的 +/-shift 平均；最终源仍只使用 +shift。
    auto probe=[&](const Vec2& p) {
        Jet result;
        for (const auto& v : source.directions()) {
            const double angle=dot(v, p), factor=std::cos(dot(v, parameters.shift)), d=-std::sin(angle)*factor;
            result.value+=std::cos(angle)*factor; result.dx+=d*v.x; result.dy+=d*v.y;
        }
        const double n=static_cast<double>(source.waves().size());
        return Jet{result.value/n, result.dx/n, result.dy/n};
    };
    constexpr double eps=1e-4;
    const auto x=probe({eps,0}), y=probe({0,eps});
    const double a=-x.dx/eps, c=-y.dy/eps, b=-(x.dy+y.dx)/(2*eps);
    const double delta=std::hypot(a-c, 2*b), low=(a+c-delta)/2, high=(a+c+delta)/2;
    if (low<=1e-6) throw std::runtime_error("中心不是非退化二维峰，请改用显式源区域半径。");
    const double mean=(low+high)/2, fl=std::sqrt(mean/low), fh=std::sqrt(mean/high);
    double tx=1, ty=1, txy=0;
    if (delta>1e-15) {
        const double slope=(fh-fl)/delta;
        tx=fl+slope*(a-low); ty=fl+slope*(c-low); txy=slope*b;
    }
    region.transform={tx,txy,txy,ty};
    std::array<Vec2,512> directions;
    for (int i=0; i<512; ++i) {
        const double angle=2*pi*i/512, cx=std::cos(angle), cy=std::sin(angle);
        directions[i]={cx*tx+cy*txy, cx*txy+cy*ty};
    }
    std::array<double,1025> contrast;
    for (int r=0; r<1025; ++r) {
        double lo=1, hi=-1;
        for (const auto& v : directions) {
            const double value=probe({v.x*r/32.,v.y*r/32.}).value;
            lo=std::min(lo,value); hi=std::max(hi,value);
        }
        contrast[r]=hi-lo;
    }
    int peak=0;
    for (int r=1; r<1024; ++r) if (contrast[r]>contrast[r-1] && contrast[r]>=contrast[r+1] && contrast[r]>.35) { peak=r; break; }
    if (!peak) throw std::runtime_error("探针搜索范围内未找到方向调制圈，请使用显式源区域。");
    region.automatic=true; region.peakRadius=peak/32.; region.peakContrast=contrast[peak];
    region.radius=region.peakRadius+pi/std::sqrt(low+high);
    region.bounds={region.radius*std::hypot(tx,txy)/parameters.frequency, region.radius*std::hypot(txy,ty)/parameters.frequency};
    return region;
}

PhaseLattice::PhaseLattice(const Source& source) : waves_(source.waves()) {
    const int n=static_cast<int>(waves_.size());
    const auto threshold=2ULL*n*n, numerator=rationalNumerator(source.parameters().q, threshold);
    Polynomial polynomial;
    if (source.parameters().model==Model::Basic && numerator<=threshold) {
        std::map<int,Polynomial> cache; polynomial=cyclotomic(static_cast<int>(numerator), cache);
    }
    const int rank=polynomial.empty() ? n : std::min(n,static_cast<int>(polynomial.size())-1);
    matrix_.assign(n, std::vector<std::int64_t>(rank));
    for (int i=0; i<n; ++i) {
        if (rank==n) matrix_[i][i]=1;
        else {
            Polynomial monomial(i+1); monomial.back()=1;
            const auto remainder=divide(monomial,polynomial).second;
            std::copy(remainder.begin(),remainder.end(),matrix_[i].begin());
        }
    }
    double reconstructionError=0;
    for (int i=0; i<n; ++i) {
        Vec2 reconstructed{};
        for (int j=0; j<rank; ++j) { reconstructed.x+=static_cast<double>(matrix_[i][j])*waves_[j].x; reconstructed.y+=static_cast<double>(matrix_[i][j])*waves_[j].y; }
        reconstructionError=std::max({reconstructionError,std::abs(reconstructed.x-waves_[i].x),std::abs(reconstructed.y-waves_[i].y)});
    }
    if (reconstructionError>=1e-10) throw std::runtime_error("当前浮点波矢不能在参考容差内重构认证频率关系。");
    // 与 NumPy/LAPACK 相同的 Householder 取号；取支不能随意换成另一种整数格算法。
    std::vector<std::vector<double>> work(n,std::vector<double>(rank));
    for (int i=0; i<n; ++i) for (int j=0; j<rank; ++j) work[i][j]=static_cast<double>(matrix_[i][j]);
    std::vector<double> tau(rank);
    for (int k=0; k<rank; ++k) {
        double tail=0;
        for (int i=k+1; i<n; ++i) tail=std::hypot(tail,work[i][k]);
        if (tail==0) continue;
        const double alpha=work[k][k], beta=-std::copysign(std::hypot(alpha,tail),alpha);
        tau[k]=(beta-alpha)/beta;
        for (int i=k+1; i<n; ++i) work[i][k]/=alpha-beta;
        work[k][k]=beta;
        for (int j=k+1; j<rank; ++j) {
            double sum=work[k][j];
            for (int i=k+1; i<n; ++i) sum+=work[i][k]*work[i][j];
            sum*=tau[k]; work[k][j]-=sum;
            for (int i=k+1; i<n; ++i) work[i][j]-=work[i][k]*sum;
        }
    }
    upper_.assign(rank,std::vector<double>(rank));
    for (int i=0; i<rank; ++i) for (int j=i; j<rank; ++j) upper_[i][j]=work[i][j];
    orthogonal_.assign(n,std::vector<double>(rank));
    for (int i=0; i<rank; ++i) orthogonal_[i][i]=1;
    for (int k=rank-1; k>=0; --k) for (int j=0; j<rank; ++j) {
        double sum=orthogonal_[k][j];
        for (int i=k+1; i<n; ++i) sum+=work[i][k]*orthogonal_[i][j];
        sum*=tau[k]; orthogonal_[k][j]-=sum;
        for (int i=k+1; i<n; ++i) orthogonal_[i][j]-=work[i][k]*sum;
    }
}
std::vector<std::int64_t> PhaseLattice::integerState(const Vec2& center) const {
    const int rank=static_cast<int>(upper_.size()), n=static_cast<int>(waves_.size());
    std::vector<double> projected(rank);
    for (int j=0; j<rank; ++j) for (int i=0; i<n; ++i) projected[j]+=dot(center,waves_[i])/(2*pi)*orthogonal_[i][j];
    std::vector<std::int64_t> state(rank), result(n);
    for (int j=rank-1; j>=0; --j) {
        double value=projected[j];
        for (int k=j+1; k<rank; ++k) value-=static_cast<double>(state[k])*upper_[j][k];
        const double rounded=std::nearbyint(value/upper_[j][j]);
        const double lower=static_cast<double>(std::numeric_limits<std::int64_t>::min());
        if (!std::isfinite(rounded) || rounded<lower || rounded>=-lower) throw std::length_error("相位整数提升超出 64 位数值范围。");
        state[j]=static_cast<std::int64_t>(rounded);
    }
    for (int i=0; i<n; ++i) for (int j=0; j<rank; ++j) result[i]+=matrix_[i][j]*state[j];
    return result;
}
std::vector<double> PhaseLattice::roundingBounds() const {
    std::vector<double> result(waves_.size());
    for (std::size_t i=0; i<result.size(); ++i) for (std::size_t j=0; j<upper_.size(); ++j) result[i]+=pi*std::abs(orthogonal_[i][j]*upper_[j][j]);
    return result;
}

Atlas::Atlas(const Parameters& p, const Region& region) : source_(p), region_(region), lattice_(source_) {
    const auto& waves=source_.waves(); const std::size_t n=waves.size();
    const double distance=region.radius/p.frequency/2;
    const Vec2 displacement{distance/std::sqrt(3.),distance*std::sqrt(2.)/std::sqrt(3.)};
    shifts_={Vec2{-displacement.x,-displacement.y},displacement};
    halfCore_={region.bounds.x+displacement.x,region.bounds.y+displacement.y};
    const auto lift=lattice_.integerState({2*displacement.x,2*displacement.y});
    std::array<std::vector<double>,2> states{std::vector<double>(n),std::vector<double>(n)};
    std::vector<double> range(n);
    for (std::size_t m=0; m<n; ++m) {
        states[0][m]=-dot(shifts_[0],waves[m]); states[1][m]=2*pi*static_cast<double>(lift[m])-dot(shifts_[1],waves[m]);
        range[m]=std::abs(states[1][m]-states[0][m]);
    }
    const auto rounding=lattice_.roundingBounds();
    double maximum=0;
    for (std::size_t m=0; m<n; ++m) maximum=std::max(maximum,(rounding[m]+range[m])/norm(waves[m]));
    const double sufficient=std::ceil(2*std::max(halfCore_.x,halfCore_.y)+1.875*std::sqrt(2.)*maximum/.5)+1;
    if (!std::isfinite(sufficient) || sufficient>10000) throw std::length_error("连接尺寸超出当前交互枚举预算，请检查频率或保护区大小。");
    geometry_.sufficientSquare=static_cast<int>(sufficient);
    const int minW=static_cast<int>(std::floor(2*halfCore_.x))+1, minH=static_cast<int>(std::floor(2*halfCore_.y))+1;
    std::vector<std::vector<std::int64_t>> xStates(geometry_.sufficientSquare*geometry_.sufficientSquare/minH+1);
    std::vector<std::vector<std::int64_t>> yStates(geometry_.sufficientSquare*geometry_.sufficientSquare/minW+1);
    for (int area=minW*minH; area<=geometry_.sufficientSquare*geometry_.sufficientSquare; ++area) {
        bool found=false;
        for (int w=minW; w<=area/minH; ++w) {
            if (area%w) continue;
            const int h=area/w;
            if (xStates[w].empty()) xStates[w]=lattice_.integerState({static_cast<double>(w),0});
            if (yStates[h].empty()) yStates[h]=lattice_.integerState({0,static_cast<double>(h)});
            double bound=0;
            for (std::size_t m=0; m<n; ++m) {
                const double rx=2*pi*static_cast<double>(xStates[w][m])-w*waves[m].x, ry=2*pi*static_cast<double>(yStates[h][m])-h*waves[m].y;
                bound=std::max(bound,1.875*std::hypot((std::abs(rx)+range[m])/(w-2*halfCore_.x),(std::abs(ry)+range[m])/(h-2*halfCore_.y))/norm(waves[m]));
            }
            if (bound<=.5 && (!found || std::tuple(bound,w,h)<std::tuple(geometry_.relativeBound,geometry_.width,geometry_.height))) {
                found=true; geometry_.width=w; geometry_.height=h; geometry_.relativeBound=bound;
            }
        }
        if (found) break;
    }
    if (geometry_.width==0) throw std::runtime_error("未找到满足解析波矢界的连接尺寸。");
    increments_={lattice_.integerState({static_cast<double>(geometry_.width),0}),lattice_.integerState({0,static_cast<double>(geometry_.height)})};
    for (int code=0; code<16; ++code) {
        offsets_[code].resize(n);
        for (std::size_t m=0; m<n; ++m) for (int corner=0; corner<4; ++corner) {
            const double rx=2*pi*static_cast<double>(increments_[0][m])-geometry_.width*waves[m].x;
            const double ry=2*pi*static_cast<double>(increments_[1][m])-geometry_.height*waves[m].y;
            offsets_[code][m][corner]=(corner%2)*rx+(corner/2)*ry+states[(code>>corner)&1][m];
        }
    }
    std::map<std::array<int,5>,int> labels;
    auto label=[&](std::array<int,5> key) { return labels.try_emplace(key,static_cast<int>(labels.size())).first->second; };
    for (int code=0; code<16; ++code) for (int j=0; j<geometry_.height; ++j) for (int i=0; i<geometry_.width; ++i) {
        const int sw=code&1,se=(code>>1)&1,nw=(code>>2)&1,ne=(code>>3)&1;
        edges_.push_back({label(j==0 ? std::array<int,5>{0,sw,se,i,0} : std::array<int,5>{2,code,i,j,0}),
                          label(j==geometry_.height-1 ? std::array<int,5>{0,nw,ne,i,0} : std::array<int,5>{2,code,i,j+1,0}),
                          label(i==0 ? std::array<int,5>{1,sw,nw,j,0} : std::array<int,5>{3,code,i,j,0}),
                          label(i==geometry_.width-1 ? std::array<int,5>{1,se,ne,j,0} : std::array<int,5>{3,code,i+1,j,0})});
    }
}
Jet Atlas::field(int tile, const Vec2& local) const {
    const int area=geometry_.width*geometry_.height, code=tile/area, role=tile%area;
    const Vec2 p{local.x+role%geometry_.width,local.y+role/geometry_.width};
    const auto [a,da]=transition(p.x,halfCore_.x,geometry_.width);
    const auto [b,db]=transition(p.y,halfCore_.y,geometry_.height);
    const std::array<double,4> weights{(1-a)*(1-b),a*(1-b),(1-a)*b,a*b};
    const std::array<double,4> dx{-da*(1-b),da*(1-b),-da*b,da*b}, dy{-(1-a)*db,-a*db,(1-a)*db,a*db};
    Jet result;
    for (std::size_t m=0; m<source_.waves().size(); ++m) {
        double phase=dot(p,source_.waves()[m])+dot(source_.directions()[m],source_.parameters().shift);
        double gx=source_.waves()[m].x, gy=source_.waves()[m].y;
        for (int c=0; c<4; ++c) { const double offset=offsets_[code][m][c]; phase+=weights[c]*offset; gx+=dx[c]*offset; gy+=dy[c]*offset; }
        const double d=-std::sin(phase); result.value+=std::cos(phase); result.dx+=d*gx; result.dy+=d*gy;
    }
    const double count=static_cast<double>(source_.waves().size());
    return {result.value/count,result.dx/count,result.dy/count};
}
Layout Atlas::plan(int span, std::uint32_t seed, bool uniform) const {
    if (span<1) throw std::invalid_argument("画幅至少为一个单位 tile。");
    Layout result{span,std::vector<int>(static_cast<std::size_t>(span)*span)};
    const int w=geometry_.width,h=geometry_.height;
    for (int row=0; row*h<span; ++row) for (int col=0; col*w<span; ++col) {
        int code=0;
        for (int b=0; b<4; ++b) if (!uniform) code|=static_cast<int>(nodeState(col+b%2,row+b/2,seed))<<b;
        for (int j=0; j<h && row*h+j<span; ++j) for (int i=0; i<w && col*w+i<span; ++i) result.tiles[static_cast<std::size_t>(row*h+j)*span+col*w+i]=code*w*h+j*w+i;
    }
    return result;
}
int Atlas::mismatches(const Layout& layout) const {
    int count=0;
    for (int y=0; y<layout.span; ++y) for (int x=0; x<layout.span; ++x) {
        const auto& e=edges_[layout.at(x,y)];
        if (x+1<layout.span) count+=e[3]!=edges_[layout.at(x+1,y)][2];
        if (y+1<layout.span) count+=e[1]!=edges_[layout.at(x,y+1)][0];
    }
    return count;
}
}
