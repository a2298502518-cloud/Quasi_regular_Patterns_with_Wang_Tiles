#include "atlas/MinimumErrorCut.hpp"

#include <limits>
#include <stdexcept>
#include <vector>

namespace qrp::atlas {
namespace {

[[nodiscard]] std::size_t validateCostGrid(
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Minimum-error cut dimensions must be positive.");
    }
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        throw std::length_error("Minimum-error cut dimensions overflow size_t.");
    }
    if (costs.size() != width * height) {
        throw std::invalid_argument(
            "Minimum-error cut costs do not match the requested dimensions.");
    }
    return width * height;
}

[[nodiscard]] std::uint64_t checkedAdd(
    const std::uint64_t first,
    const std::uint64_t second) {
    if (first > std::numeric_limits<std::uint64_t>::max() - second) {
        throw std::overflow_error("Minimum-error cut cost overflowed uint64_t.");
    }
    return first + second;
}

} // namespace

MinimumErrorCut findVerticalMinimumErrorCut(
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height,
    const std::span<const MinimumErrorCutWaypoint> waypoints) {
    const std::size_t elementCount = validateCostGrid(costs, width, height);

    // `width` is also an invalid offset sentinel because valid offsets are
    // strictly smaller than the cost-grid width.
    std::vector<std::size_t> fixedOffsetAt(height, width);
    for (const MinimumErrorCutWaypoint waypoint : waypoints) {
        if (waypoint.pathIndex >= height || waypoint.offset >= width) {
            throw std::invalid_argument(
                "Minimum-error cut waypoint is outside the cost grid.");
        }
        std::size_t& fixedOffset = fixedOffsetAt[waypoint.pathIndex];
        if (fixedOffset != width && fixedOffset != waypoint.offset) {
            throw std::invalid_argument(
                "Minimum-error cut has conflicting waypoints at one path index.");
        }
        fixedOffset = waypoint.offset;
    }

    std::vector<std::uint64_t> accumulated(costs.begin(), costs.end());
    std::vector<std::size_t> predecessor(elementCount, 0);
    std::vector<std::uint8_t> reachable(elementCount, 0);
    for (std::size_t x = 0; x < width; ++x) {
        if (fixedOffsetAt[0] == width || fixedOffsetAt[0] == x) {
            reachable[x] = 1;
        }
    }

    for (std::size_t y = 1; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            if (fixedOffsetAt[y] != width && fixedOffsetAt[y] != x) {
                continue;
            }
            const std::size_t firstCandidate = x == 0 ? 0 : x - 1;
            const std::size_t lastCandidate = x == width - 1 ? x : x + 1;
            std::size_t bestX = 0;
            std::uint64_t bestCost = 0;
            bool foundPredecessor = false;
            if (reachable[(y - 1) * width + x] != 0) {
                bestX = x;
                bestCost = accumulated[(y - 1) * width + x];
                foundPredecessor = true;
            }
            for (std::size_t candidate = firstCandidate;; ++candidate) {
                if (reachable[(y - 1) * width + candidate] != 0) {
                    const std::uint64_t candidateCost =
                        accumulated[(y - 1) * width + candidate];
                    if (!foundPredecessor || candidateCost < bestCost) {
                        bestCost = candidateCost;
                        bestX = candidate;
                        foundPredecessor = true;
                    }
                }
                if (candidate == lastCandidate) {
                    break;
                }
            }
            if (!foundPredecessor) {
                continue;
            }
            predecessor[y * width + x] = bestX;
            accumulated[y * width + x] = checkedAdd(
                costs[y * width + x],
                bestCost);
            reachable[y * width + x] = 1;
        }
    }

    std::size_t endX = 0;
    std::uint64_t totalCost = 0;
    std::size_t bestCenterDistance = width - 1;
    bool foundEndpoint = false;
    for (std::size_t x = 0; x < width; ++x) {
        if (reachable[(height - 1) * width + x] == 0) {
            continue;
        }
        const std::uint64_t candidateCost = accumulated[(height - 1) * width + x];
        const std::size_t leftDistance = x;
        const std::size_t rightDistance = width - 1 - x;
        const std::size_t centerDistance = leftDistance > rightDistance
            ? leftDistance - rightDistance
            : rightDistance - leftDistance;
        if (!foundEndpoint
            || candidateCost < totalCost
            || (candidateCost == totalCost
                && centerDistance < bestCenterDistance)) {
            totalCost = candidateCost;
            endX = x;
            bestCenterDistance = centerDistance;
            foundEndpoint = true;
        }
    }
    if (!foundEndpoint) {
        throw std::invalid_argument(
            "Minimum-error cut waypoints do not admit a connected path.");
    }

    MinimumErrorCut result;
    result.offsets.resize(height);
    result.totalCost = totalCost;
    result.offsets[height - 1] = endX;
    for (std::size_t y = height - 1; y > 0; --y) {
        result.offsets[y - 1] = predecessor[y * width + result.offsets[y]];
    }
    return result;
}

MinimumErrorCut findHorizontalMinimumErrorCut(
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height,
    const std::span<const MinimumErrorCutWaypoint> waypoints) {
    const std::size_t elementCount = validateCostGrid(costs, width, height);

    std::vector<std::uint64_t> transposed(elementCount);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            transposed[x * height + y] = costs[y * width + x];
        }
    }
    return findVerticalMinimumErrorCut(transposed, height, width, waypoints);
}

} // namespace qrp::atlas
