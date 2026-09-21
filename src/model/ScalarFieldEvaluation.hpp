#pragma once

#include "math/Vec2.hpp"

namespace qrp::model {

struct ScalarFieldEvaluation {
    double value = 0.0;
    math::Vec2 gradient;
};

} // namespace qrp::model
