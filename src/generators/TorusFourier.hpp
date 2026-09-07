#pragma once

#include "math/CoonsWarp.hpp"

#include <vector>

namespace qrp::generators {

struct FourierMode {
    int frequencyU = 0;
    int frequencyV = 0;
    double amplitude = 1.0;
    double phase = 0.0;
};

class TorusFourier final {
public:
    explicit TorusFourier(std::vector<FourierMode> modes);

    [[nodiscard]] static TorusFourier createQuasiRegular();
    [[nodiscard]] const std::vector<FourierMode>& modes() const noexcept;
    [[nodiscard]] double evaluate(math::Vec2 parameter) const noexcept;

private:
    std::vector<FourierMode> modes_;
    double amplitudeNormalizer_ = 1.0;
};

} // namespace qrp::generators
