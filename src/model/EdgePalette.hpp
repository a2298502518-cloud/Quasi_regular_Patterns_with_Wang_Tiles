#pragma once

#include "math/CoonsWarp.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace qrp::model {

struct PaletteSafetyReport {
    bool valid = false;
    std::size_t edgeColorCount = 0;
    std::size_t combinationCount = 0;
    double minimumDeterminantLowerBound = std::numeric_limits<double>::infinity();
    std::array<std::size_t, 4> worstCombination{};
    std::string message;
};

class EdgePalette final {
public:
    explicit EdgePalette(std::vector<math::EdgeFunction> colors);

    [[nodiscard]] static EdgePalette createDefault();
    [[nodiscard]] const std::vector<math::EdgeFunction>& colors() const noexcept;
    [[nodiscard]] PaletteSafetyReport validateAllCombinations(
        const math::WarpSafetyOptions& options = {}) const;

private:
    std::vector<math::EdgeFunction> colors_;
};

} // namespace qrp::model
