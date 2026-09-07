#include "model/EdgePalette.hpp"

#include <utility>

namespace qrp::model {

EdgePalette::EdgePalette(std::vector<math::EdgeFunction> colors)
    : colors_(std::move(colors)) {}

EdgePalette EdgePalette::createDefault() {
    using math::EdgeFunction;
    using math::EdgeParameters;
    return EdgePalette({
        EdgeFunction(EdgeParameters{0.0, 0.0}),
        EdgeFunction(EdgeParameters{0.035, 0.0}),
        EdgeFunction(EdgeParameters{-0.035, 0.0}),
        EdgeFunction(EdgeParameters{0.0, 0.025}),
        EdgeFunction(EdgeParameters{0.0, -0.025}),
    });
}

const std::vector<math::EdgeFunction>& EdgePalette::colors() const noexcept {
    return colors_;
}

PaletteSafetyReport EdgePalette::validateAllCombinations(
    const math::WarpSafetyOptions& options) const {
    PaletteSafetyReport result;
    result.edgeColorCount = colors_.size();
    if (colors_.empty()) {
        result.message = "The edge palette is empty.";
        return result;
    }

    for (std::size_t south = 0; south < colors_.size(); ++south) {
        for (std::size_t north = 0; north < colors_.size(); ++north) {
            for (std::size_t west = 0; west < colors_.size(); ++west) {
                for (std::size_t east = 0; east < colors_.size(); ++east) {
                    const math::CoonsWarp warp(math::CoonsEdges{
                        colors_[south],
                        colors_[north],
                        colors_[west],
                        colors_[east],
                    });
                    const auto safety = warp.validateSafety(options);
                    ++result.combinationCount;
                    if (safety.determinantLowerBound
                        < result.minimumDeterminantLowerBound) {
                        result.minimumDeterminantLowerBound = safety.determinantLowerBound;
                        result.worstCombination = {south, north, west, east};
                    }
                    if (!safety.valid) {
                        result.message = "At least one edge-color combination is unsafe: "
                            + safety.message;
                        return result;
                    }
                }
            }
        }
    }

    result.valid = true;
    result.message = "Every edge-color combination satisfies the safety bounds.";
    return result;
}

} // namespace qrp::model
