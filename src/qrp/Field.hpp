#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace qrp {
struct Vec2 { double x{}, y{}; bool operator==(const Vec2&) const = default; };
struct Jet { double value{}, dx{}, dy{}; };
enum class Model { Basic, CubicDirections };
struct Parameters {
    double q{7}, frequency{12};
    Model model{Model::Basic};
    Vec2 shift{};
    bool operator==(const Parameters&) const = default;
};
struct Region {
    double radius{};
    Vec2 bounds{};
    std::array<double, 4> transform{1, 0, 0, 1};
    double peakRadius{}, peakContrast{};
    bool automatic{};
};

class Source {
public:
    explicit Source(const Parameters& parameters);
    Jet jet(const Vec2& point) const;
    const Parameters& parameters() const { return parameters_; }
    const std::vector<Vec2>& directions() const { return directions_; }
    const std::vector<Vec2>& waves() const { return waves_; }
private:
    Parameters parameters_;
    std::vector<Vec2> directions_, waves_;
};
Region selectRegion(const Parameters& parameters, std::optional<double> explicitRadius = {});

class PhaseLattice {
public:
    explicit PhaseLattice(const Source& source);
    std::vector<std::int64_t> integerState(const Vec2& center) const;
    std::vector<double> roundingBounds() const;
    const std::vector<std::vector<std::int64_t>>& relations() const { return matrix_; }
private:
    std::vector<Vec2> waves_;
    std::vector<std::vector<std::int64_t>> matrix_;
    std::vector<std::vector<double>> orthogonal_, upper_;
};

struct Layout {
    int span{};
    std::vector<int> tiles; // y 向上，x 为快索引；不是 PNG 行顺序。
    int at(int x, int y) const { return tiles[static_cast<std::size_t>(y)*span+x]; }
};
struct Geometry {
    int width{}, height{}, sufficientSquare{};
    double relativeBound{};
};
class Atlas {
public:
    Atlas(const Parameters& parameters, const Region& region);
    Jet field(int tile, const Vec2& local) const;
    Layout plan(int span, std::uint32_t seed, bool uniform = false) const;
    int mismatches(const Layout& layout) const;
    int tileCount() const { return 16*geometry_.width*geometry_.height; }
    const Source& source() const { return source_; }
    const Region& region() const { return region_; }
    const Geometry& geometry() const { return geometry_; }
    const PhaseLattice& lattice() const { return lattice_; }
    const std::array<Vec2, 2>& shifts() const { return shifts_; }
    const std::array<std::vector<std::int64_t>, 2>& increments() const { return increments_; }
    const std::vector<std::array<int, 4>>& edges() const { return edges_; }
private:
    Source source_;
    Region region_;
    PhaseLattice lattice_;
    Geometry geometry_;
    Vec2 halfCore_{};
    std::array<Vec2, 2> shifts_{};
    std::array<std::vector<std::int64_t>, 2> increments_;
    std::array<std::vector<std::array<double, 4>>, 16> offsets_;
    std::vector<std::array<int, 4>> edges_;
};
}
