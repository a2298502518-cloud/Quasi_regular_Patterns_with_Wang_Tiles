#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace qrp::atlas {

struct MinimumErrorCut {
    std::vector<std::size_t> offsets;
    std::uint64_t totalCost = 0;
};

struct MinimumErrorCutWaypoint {
    std::size_t pathIndex = 0;
    std::size_t offset = 0;
};

[[nodiscard]] MinimumErrorCut findVerticalMinimumErrorCut(
    std::span<const std::uint64_t> costs,
    std::size_t width,
    std::size_t height,
    std::span<const MinimumErrorCutWaypoint> waypoints = {});

[[nodiscard]] MinimumErrorCut findHorizontalMinimumErrorCut(
    std::span<const std::uint64_t> costs,
    std::size_t width,
    std::size_t height,
    std::span<const MinimumErrorCutWaypoint> waypoints = {});

} // namespace qrp::atlas
