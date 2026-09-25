#pragma once

#include "model/PhaseCompatibleQrpTiles.hpp"
#include <iosfwd>

namespace qrp::study {
// Single serialized spectral definition for C++ baselines and Python research inputs.
void writeQrpSource(const model::PhaseCompatibleQrpTiles& tiles, std::ostream& out);
}
