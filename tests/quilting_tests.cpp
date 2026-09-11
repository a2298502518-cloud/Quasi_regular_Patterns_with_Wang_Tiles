#include "atlas/AtlasRenderer.hpp"
#include "atlas/MinimumErrorCut.hpp"
#include "atlas/WangAtlasTiling.hpp"
#include "atlas/WangQuiltGeometry.hpp"
#include "atlas/WangTextureAtlasBuilder.hpp"
#include "atlas/WangTextureSampleOptimizer.hpp"
#include "render/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kPatchSize = 12;
constexpr std::size_t kOverlapPixels = 3;
constexpr std::size_t kOutputResolution = 17;
constexpr qrp::render::Rgb8 kCornerColor{7, 9, 11};

struct TestCase {
    std::string_view name;
    void (*function)();
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Function>
void requireThrows(Function&& function, const std::string& message) {
    bool threwExpected = false;
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        threwExpected = true;
    } catch (...) {
    }
    require(threwExpected, message);
}

[[nodiscard]] bool equal(
    const qrp::render::Rgb8 first,
    const qrp::render::Rgb8 second) noexcept {
    return first.red == second.red
        && first.green == second.green
        && first.blue == second.blue;
}

[[nodiscard]] bool imagesEqual(
    const qrp::render::Image& first,
    const qrp::render::Image& second) {
    if (first.width() != second.width() || first.height() != second.height()) {
        return false;
    }
    for (std::size_t index = 0; index < first.pixels().size(); ++index) {
        if (!equal(first.pixels()[index], second.pixels()[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool imageBlockEqual(
    const qrp::render::Image& first,
    const qrp::render::Image& second,
    const std::size_t originX,
    const std::size_t originY,
    const std::size_t width,
    const std::size_t height) {
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            if (!equal(
                    first.pixel(originX + x, originY + y),
                    second.pixel(originX + x, originY + y))) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] qrp::render::Image solidImage(
    const std::size_t size,
    const qrp::render::Rgb8 color) {
    qrp::render::Image image(size, size);
    for (std::size_t y = 0; y < size; ++y) {
        for (std::size_t x = 0; x < size; ++x) {
            image.pixel(x, y) = color;
        }
    }
    return image;
}

[[nodiscard]] qrp::render::Image asymmetricImage(
    const std::size_t size,
    const std::uint32_t sampleId) {
    qrp::render::Image image(size, size);
    for (std::size_t y = 0; y < size; ++y) {
        for (std::size_t x = 0; x < size; ++x) {
            image.pixel(x, y) = {
                static_cast<std::uint8_t>(
                    (17U + 47U * sampleId + 11U * x + 3U * y) % 251U),
                static_cast<std::uint8_t>(
                    (29U + 31U * sampleId + 5U * x + 13U * y) % 251U),
                static_cast<std::uint8_t>(
                    (43U + 19U * sampleId + 7U * x + 17U * y) % 251U),
            };
        }
    }
    return image;
}

[[nodiscard]] qrp::render::Image asymmetricSource(
    const std::size_t width,
    const std::size_t height) {
    qrp::render::Image image(width, height);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            image.pixel(x, y) = {
                static_cast<std::uint8_t>((9U + 17U * x + 31U * y) % 251U),
                static_cast<std::uint8_t>((21U + 29U * x + 7U * y) % 251U),
                static_cast<std::uint8_t>((37U + 11U * x + 23U * y) % 251U),
            };
        }
    }
    return image;
}

[[nodiscard]] qrp::atlas::WangEdgeSampleBank solidSampleBank(
    const std::size_t size,
    const qrp::render::Rgb8 color) {
    return {
        std::array<qrp::render::Image, 2>{
            solidImage(size, color),
            solidImage(size, color),
        },
        std::array<qrp::render::Image, 2>{
            solidImage(size, color),
            solidImage(size, color),
        },
    };
}

[[nodiscard]] qrp::atlas::WangEdgeSampleBank distinctSolidSampleBank(
    const std::size_t size) {
    return {
        std::array<qrp::render::Image, 2>{
            solidImage(size, {20, 30, 40}),
            solidImage(size, {80, 90, 100}),
        },
        std::array<qrp::render::Image, 2>{
            solidImage(size, {140, 150, 160}),
            solidImage(size, {200, 210, 220}),
        },
    };
}

[[nodiscard]] qrp::atlas::WangEdgeSampleBank asymmetricSampleBank(
    const std::size_t size = kPatchSize) {
    return {
        std::array<qrp::render::Image, 2>{
            asymmetricImage(size, 0),
            asymmetricImage(size, 1),
        },
        std::array<qrp::render::Image, 2>{
            asymmetricImage(size, 2),
            asymmetricImage(size, 3),
        },
    };
}

[[nodiscard]] bool sampleBanksEqual(
    const qrp::atlas::WangEdgeSampleBank& first,
    const qrp::atlas::WangEdgeSampleBank& second) {
    for (std::size_t label = 0; label < 2; ++label) {
        if (!imagesEqual(first.northSouth[label], second.northSouth[label])
            || !imagesEqual(first.westEast[label], second.westEast[label])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::array<std::size_t, 8> originKey(
    const qrp::atlas::WangTextureSampleOrigins& origins) noexcept {
    return {
        origins.northSouth[0].y,
        origins.northSouth[0].x,
        origins.northSouth[1].y,
        origins.northSouth[1].x,
        origins.westEast[0].y,
        origins.westEast[0].x,
        origins.westEast[1].y,
        origins.westEast[1].x,
    };
}

[[nodiscard]] std::array<qrp::atlas::WangTextureSampleOrigin, 4> flatOrigins(
    const qrp::atlas::WangTextureSampleOrigins& origins) noexcept {
    return {
        origins.northSouth[0],
        origins.northSouth[1],
        origins.westEast[0],
        origins.westEast[1],
    };
}

void requireCropMatches(
    const qrp::render::Image& source,
    const qrp::render::Image& sample,
    const qrp::atlas::WangTextureSampleOrigin origin,
    const std::size_t patchSize,
    const std::string& message) {
    require(
        sample.width() == patchSize && sample.height() == patchSize,
        message + ": crop dimensions are incorrect");
    require(
        origin.x + patchSize <= source.width()
            && origin.y + patchSize <= source.height(),
        message + ": crop origin leaves the source image");
    for (std::size_t y = 0; y < patchSize; ++y) {
        for (std::size_t x = 0; x < patchSize; ++x) {
            require(
                equal(sample.pixel(x, y), source.pixel(origin.x + x, origin.y + y)),
                message + ": crop pixels do not trace to the reported origin");
        }
    }
}

[[nodiscard]] qrp::atlas::WangTextureAtlasBuildOptions buildOptions(
    const bool includeDebugImages = false) {
    qrp::atlas::WangTextureAtlasBuildOptions options;
    options.overlapPixels = kOverlapPixels;
    options.outputResolutionPixels = kOutputResolution;
    options.cornerColor = kCornerColor;
    options.includeDebugImages = includeDebugImages;
    return options;
}

[[nodiscard]] std::uint64_t bruteForceVerticalMinimum(
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height,
    const std::span<const qrp::atlas::MinimumErrorCutWaypoint> waypoints = {}) {
    std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
    const auto admits = [waypoints](
                            const std::size_t pathIndex,
                            const std::size_t offset) {
        for (const auto waypoint : waypoints) {
            if (waypoint.pathIndex == pathIndex && waypoint.offset != offset) {
                return false;
            }
        }
        return true;
    };
    const auto visit = [&](auto&& self,
                           const std::size_t y,
                           const std::size_t x,
                           const std::uint64_t accumulated) -> void {
        if (!admits(y, x)) {
            return;
        }
        const std::uint64_t next = accumulated + costs[y * width + x];
        if (y + 1 == height) {
            best = std::min(best, next);
            return;
        }
        const std::size_t first = x == 0 ? 0 : x - 1;
        const std::size_t last = std::min(width - 1, x + 1);
        for (std::size_t nextX = first; nextX <= last; ++nextX) {
            self(self, y + 1, nextX, next);
        }
    };
    for (std::size_t startX = 0; startX < width; ++startX) {
        visit(visit, 0, startX, 0);
    }
    return best;
}

[[nodiscard]] std::uint64_t bruteForceHorizontalMinimum(
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height,
    const std::span<const qrp::atlas::MinimumErrorCutWaypoint> waypoints = {}) {
    std::vector<std::uint64_t> transposed(width * height);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            transposed[x * height + y] = costs[y * width + x];
        }
    }
    return bruteForceVerticalMinimum(transposed, height, width, waypoints);
}

void validateVerticalCut(
    const qrp::atlas::MinimumErrorCut& cut,
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height) {
    require(cut.offsets.size() == height, "vertical cut must contain one x per row");
    std::uint64_t pathCost = 0;
    for (std::size_t y = 0; y < height; ++y) {
        require(cut.offsets[y] < width, "vertical cut x must stay inside overlap");
        if (y > 0) {
            const std::size_t first = cut.offsets[y - 1];
            const std::size_t second = cut.offsets[y];
            require(
                first > second ? first - second <= 1 : second - first <= 1,
                "vertical cut must be 8-connected");
        }
        pathCost += costs[y * width + cut.offsets[y]];
    }
    require(pathCost == cut.totalCost, "vertical cut cost must equal its path sum");
}

void validateHorizontalCut(
    const qrp::atlas::MinimumErrorCut& cut,
    const std::span<const std::uint64_t> costs,
    const std::size_t width,
    const std::size_t height) {
    require(cut.offsets.size() == width, "horizontal cut must contain one y per column");
    std::uint64_t pathCost = 0;
    for (std::size_t x = 0; x < width; ++x) {
        require(cut.offsets[x] < height, "horizontal cut y must stay inside overlap");
        if (x > 0) {
            const std::size_t first = cut.offsets[x - 1];
            const std::size_t second = cut.offsets[x];
            require(
                first > second ? first - second <= 1 : second - first <= 1,
                "horizontal cut must be 8-connected");
        }
        pathCost += costs[cut.offsets[x] * width + x];
    }
    require(pathCost == cut.totalCost, "horizontal cut cost must equal its path sum");
}

[[nodiscard]] qrp::render::Rgb8 sampleBilinearForTest(
    const qrp::render::Image& image,
    const double x,
    const double y) {
    const double clampedX = std::clamp(
        x,
        0.0,
        static_cast<double>(image.width() - 1));
    const double clampedY = std::clamp(
        y,
        0.0,
        static_cast<double>(image.height() - 1));
    const std::size_t x0 = static_cast<std::size_t>(std::floor(clampedX));
    const std::size_t y0 = static_cast<std::size_t>(std::floor(clampedY));
    const std::size_t x1 = std::min(image.width() - 1, x0 + 1);
    const std::size_t y1 = std::min(image.height() - 1, y0 + 1);
    const double tx = clampedX - static_cast<double>(x0);
    const double ty = clampedY - static_cast<double>(y0);
    const auto channel = [&](const std::uint8_t qrp::render::Rgb8::* member) {
        const double top = (1.0 - tx) * image.pixel(x0, y0).*member
            + tx * image.pixel(x1, y0).*member;
        const double bottom = (1.0 - tx) * image.pixel(x0, y1).*member
            + tx * image.pixel(x1, y1).*member;
        return static_cast<std::uint8_t>(std::lround(std::clamp(
            (1.0 - ty) * top + ty * bottom,
            0.0,
            255.0)));
    };
    return {
        channel(&qrp::render::Rgb8::red),
        channel(&qrp::render::Rgb8::green),
        channel(&qrp::render::Rgb8::blue),
    };
}

[[nodiscard]] bool approximatelyEqual(
    const double first,
    const double second,
    const double tolerance = 1.0e-12) noexcept {
    return std::abs(first - second) <= tolerance;
}

[[nodiscard]] std::array<std::uint32_t, 4> signatureKey(
    const qrp::atlas::WangEdgeSignature edges) noexcept {
    return {edges.south, edges.north, edges.west, edges.east};
}

[[nodiscard]] std::uint64_t cutCostSum(
    const qrp::atlas::WangTextureCutCosts& costs) noexcept {
    return costs.eastPlacementVertical
        + costs.westPlacementHorizontal
        + costs.southPlacementTop
        + costs.southPlacementLeft;
}

[[nodiscard]] const qrp::atlas::WangTextureTileBuildMetrics& metricFor(
    const qrp::atlas::WangTextureAtlasBuildReport& report,
    const qrp::atlas::WangEdgeSignature edges) {
    const auto found = std::find_if(
        report.tiles.begin(),
        report.tiles.end(),
        [edges](const qrp::atlas::WangTextureTileBuildMetrics& metric) {
            return metric.edges == edges;
        });
    if (found == report.tiles.end()) {
        throw std::runtime_error("build report is missing a Wang tile signature");
    }
    return *found;
}

void testMinimumErrorCutAgainstBruteForceOracle() {
    constexpr std::size_t width = 3;
    constexpr std::size_t height = 4;
    constexpr std::size_t entryCount = width * height;
    constexpr std::uint32_t matrixCount = 1U << entryCount;
    std::vector<std::uint64_t> costs(entryCount);
    for (std::uint32_t mask = 0; mask < matrixCount; ++mask) {
        for (std::size_t index = 0; index < entryCount; ++index) {
            costs[index] = (mask >> index) & 1U;
        }
        const auto vertical = qrp::atlas::findVerticalMinimumErrorCut(
            costs,
            width,
            height);
        validateVerticalCut(vertical, costs, width, height);
        require(
            vertical.totalCost
                == bruteForceVerticalMinimum(costs, width, height),
            "vertical DP must match the independent exhaustive oracle");

        const auto horizontal = qrp::atlas::findHorizontalMinimumErrorCut(
            costs,
            width,
            height);
        validateHorizontalCut(horizontal, costs, width, height);
        require(
            horizontal.totalCost
                == bruteForceHorizontalMinimum(costs, width, height),
            "horizontal DP must match the independent exhaustive oracle");
    }

    const std::vector<std::uint64_t> uniqueCosts{
        0, 9, 9,
        9, 0, 9,
        9, 9, 0,
        9, 9, 0,
    };
    const auto unique = qrp::atlas::findVerticalMinimumErrorCut(
        uniqueCosts,
        width,
        height);
    require(
        unique.offsets == std::vector<std::size_t>({0, 1, 2, 2}),
        "the hand-authored diagonal valley must produce its unique seam");
    require(unique.totalCost == 0, "the hand-authored seam must have zero cost");
}

void testMinimumErrorCutTieBreakAndFailures() {
    constexpr std::size_t width = 5;
    constexpr std::size_t height = 4;
    const std::vector<std::uint64_t> zeros(width * height, 0);
    const auto vertical = qrp::atlas::findVerticalMinimumErrorCut(
        zeros,
        width,
        height);
    require(
        vertical.offsets == std::vector<std::size_t>(height, 2),
        "flat vertical overlap must choose the stable centered straight cut");
    const auto repeated = qrp::atlas::findVerticalMinimumErrorCut(
        zeros,
        width,
        height);
    require(
        vertical.offsets == repeated.offsets
            && vertical.totalCost == repeated.totalCost,
        "minimum-error cuts must be stateless and deterministic");

    const auto horizontal = qrp::atlas::findHorizontalMinimumErrorCut(
        zeros,
        width,
        height);
    require(
        horizontal.offsets == std::vector<std::size_t>(width, 1),
        "flat horizontal overlap must choose the lower centered straight cut");

    const std::vector<std::uint64_t> empty;
    requireThrows<std::invalid_argument>(
        [&empty]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(empty, 0, 1));
        },
        "zero-width cost grids must be rejected");
    requireThrows<std::invalid_argument>(
        [&empty]() {
            static_cast<void>(qrp::atlas::findHorizontalMinimumErrorCut(empty, 1, 0));
        },
        "zero-height cost grids must be rejected");
    requireThrows<std::invalid_argument>(
        []() {
            const std::vector<std::uint64_t> wrongSize(5, 0);
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                wrongSize,
                2,
                3));
        },
        "cost grids with mismatched storage must be rejected");
    requireThrows<std::length_error>(
        [&empty]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                empty,
                std::numeric_limits<std::size_t>::max(),
                2));
        },
        "cost grid dimension multiplication must detect overflow");

    const std::vector<std::uint64_t> overflowCosts{
        std::numeric_limits<std::uint64_t>::max(),
        1,
    };
    requireThrows<std::overflow_error>(
        [&overflowCosts]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                overflowCosts,
                1,
                2));
        },
        "vertical accumulated cut cost must detect uint64 overflow");
    requireThrows<std::overflow_error>(
        [&overflowCosts]() {
            static_cast<void>(qrp::atlas::findHorizontalMinimumErrorCut(
                overflowCosts,
                2,
                1));
        },
        "horizontal accumulated cut cost must detect uint64 overflow");
}

void testMinimumErrorCutWaypoints() {
    constexpr std::size_t width = 5;
    constexpr std::size_t height = 7;
    std::vector<std::uint64_t> costs(width * height);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            costs[y * width + x] = (17 * x + 11 * y + 7 * x * y + 3) % 19;
        }
    }
    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 3> waypoints{{
        {1, 4},
        {3, 2},
        {6, 3},
    }};
    const auto vertical = qrp::atlas::findVerticalMinimumErrorCut(
        costs,
        width,
        height,
        waypoints);
    validateVerticalCut(vertical, costs, width, height);
    for (const auto waypoint : waypoints) {
        require(
            vertical.offsets[waypoint.pathIndex] == waypoint.offset,
            "waypoint-constrained vertical cuts must pass through every anchor");
    }
    require(
        vertical.totalCost
            == bruteForceVerticalMinimum(costs, width, height, waypoints),
        "waypoint-constrained DP must remain globally optimal across all segments");

    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 3> reversedWaypoints{{
        waypoints[2],
        waypoints[1],
        waypoints[0],
    }};
    const auto reversed = qrp::atlas::findVerticalMinimumErrorCut(
        costs,
        width,
        height,
        reversedWaypoints);
    require(
        reversed.offsets == vertical.offsets
            && reversed.totalCost == vertical.totalCost,
        "waypoint input order must not alter path-index semantics");

    std::vector<std::uint64_t> transposed(width * height);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            transposed[x * height + y] = costs[y * width + x];
        }
    }
    const auto horizontal = qrp::atlas::findHorizontalMinimumErrorCut(
        transposed,
        height,
        width,
        waypoints);
    validateHorizontalCut(horizontal, transposed, height, width);
    require(
        horizontal.offsets == vertical.offsets
            && horizontal.totalCost == vertical.totalCost,
        "horizontal waypoint semantics must equal the transposed vertical problem");
    require(
        horizontal.totalCost == bruteForceHorizontalMinimum(
            transposed,
            height,
            width,
            waypoints),
        "horizontal waypoint cuts must match an independent exhaustive oracle");

    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 2> duplicate{{
        {2, 1},
        {2, 1},
    }};
    const auto duplicated = qrp::atlas::findVerticalMinimumErrorCut(
        costs,
        width,
        height,
        duplicate);
    require(
        duplicated.offsets[2] == 1,
        "identical duplicate waypoints must preserve the required anchor");

    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 1> pathOutOfRange{{
        {height, 0},
    }};
    requireThrows<std::invalid_argument>(
        [&costs, &pathOutOfRange]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                costs,
                width,
                height,
                pathOutOfRange));
        },
        "waypoint path indices outside the grid must be rejected");
    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 1> offsetOutOfRange{{
        {0, width},
    }};
    requireThrows<std::invalid_argument>(
        [&costs, &offsetOutOfRange]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                costs,
                width,
                height,
                offsetOutOfRange));
        },
        "waypoint offsets outside the overlap must be rejected");
    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 2> conflicting{{
        {2, 1},
        {2, 3},
    }};
    requireThrows<std::invalid_argument>(
        [&costs, &conflicting]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                costs,
                width,
                height,
                conflicting));
        },
        "conflicting waypoints at one path index must be rejected");
    const std::array<qrp::atlas::MinimumErrorCutWaypoint, 2> disconnected{{
        {1, 4},
        {2, 2},
    }};
    requireThrows<std::invalid_argument>(
        [&costs, &disconnected]() {
            static_cast<void>(qrp::atlas::findVerticalMinimumErrorCut(
                costs,
                width,
                height,
                disconnected));
        },
        "waypoints that require no connected path must be rejected");
}

void testQuiltGeometryProvenanceAndBounds() {
    const qrp::atlas::WangQuiltGeometry geometry(
        kPatchSize,
        kOverlapPixels,
        kOutputResolution);
    require(geometry.patchSize() == 12, "geometry patch size is incorrect");
    require(geometry.overlapPixels() == 3, "geometry overlap is incorrect");
    require(geometry.stridePixels() == 9, "geometry stride must equal P-O");
    require(geometry.compositeSize() == 21, "geometry composite must equal P+(P-O)");
    require(
        geometry.outputResolutionPixels() == kOutputResolution,
        "explicit output resolution must be preserved");

    const double center = 10.0;
    const double stride = 9.0;
    const std::size_t last = kOutputResolution - 1;
    const auto northWest = geometry.compositePosition(0, 0);
    const auto northEast = geometry.compositePosition(last, 0);
    const auto southWest = geometry.compositePosition(0, last);
    const auto southEast = geometry.compositePosition(last, last);
    require(
        approximatelyEqual(northWest.x, center - stride)
            && approximatelyEqual(northWest.y, center),
        "north-west crop vertex has the wrong composite provenance");
    require(
        approximatelyEqual(northEast.x, center)
            && approximatelyEqual(northEast.y, center - stride),
        "north-east crop vertex has the wrong composite provenance");
    require(
        approximatelyEqual(southWest.x, center)
            && approximatelyEqual(southWest.y, center + stride),
        "south-west crop vertex has the wrong composite provenance");
    require(
        approximatelyEqual(southEast.x, center + stride)
            && approximatelyEqual(southEast.y, center),
        "south-east crop vertex has the wrong composite provenance");

    const auto northMid = geometry.compositePosition(last / 2, 0);
    const auto eastMid = geometry.compositePosition(last, last / 2);
    const auto southMid = geometry.compositePosition(last / 2, last);
    const auto westMid = geometry.compositePosition(0, last / 2);
    require(
        approximatelyEqual(northMid.x, 5.5)
            && approximatelyEqual(northMid.y, 5.5),
        "north edge midpoint must come from the N patch center");
    require(
        approximatelyEqual(eastMid.x, 14.5)
            && approximatelyEqual(eastMid.y, 5.5),
        "east edge midpoint must come from the E patch center");
    require(
        approximatelyEqual(southMid.x, 14.5)
            && approximatelyEqual(southMid.y, 14.5),
        "south edge midpoint must come from the S patch center");
    require(
        approximatelyEqual(westMid.x, 5.5)
            && approximatelyEqual(westMid.y, 14.5),
        "west edge midpoint must come from the W patch center");

    const double compositeMaximum = static_cast<double>(geometry.compositeSize() - 1);
    qrp::atlas::QuiltSamplePosition previousNorthSouth;
    qrp::atlas::QuiltSamplePosition previousWestEast;
    for (std::size_t y = 0; y < kOutputResolution; ++y) {
        for (std::size_t x = 0; x < kOutputResolution; ++x) {
            const auto position = geometry.compositePosition(x, y);
            require(
                position.x >= 0.0 && position.x <= compositeMaximum
                    && position.y >= 0.0 && position.y <= compositeMaximum,
                "every rotated crop sample must stay inside the quilt composite");
        }
    }
    for (std::size_t index = 0; index < kOutputResolution; ++index) {
        const auto northSouth = geometry.northSouthBoundaryPosition(index);
        const auto westEast = geometry.westEastBoundaryPosition(index);
        require(
            approximatelyEqual(
                northSouth.x + northSouth.y,
                static_cast<double>(kPatchSize - 1)),
            "N/S boundary provenance must follow the sample anti-diagonal");
        require(
            approximatelyEqual(westEast.x, westEast.y),
            "W/E boundary provenance must follow the sample diagonal");
        require(
            northSouth.x >= 0.0 && northSouth.x <= kPatchSize - 1
                && northSouth.y >= 0.0 && northSouth.y <= kPatchSize - 1
                && westEast.x >= 0.0 && westEast.x <= kPatchSize - 1
                && westEast.y >= 0.0 && westEast.y <= kPatchSize - 1,
            "authoritative edge provenance must stay inside its source patch");
        if (index > 0) {
            require(
                northSouth.x > previousNorthSouth.x
                    && northSouth.y < previousNorthSouth.y,
                "N/S boundary orientation must be left-to-right without reversal");
            require(
                westEast.x > previousWestEast.x
                    && westEast.y > previousWestEast.y,
                "W/E boundary orientation must be north-to-south without reversal");
        }
        previousNorthSouth = northSouth;
        previousWestEast = westEast;
    }

    const qrp::atlas::WangQuiltGeometry automatic(12, 3);
    require(
        automatic.outputResolutionPixels() == 14,
        "automatic resolution must round sqrt(2)*stride and include both endpoints");
    requireThrows<std::out_of_range>(
        [&geometry]() {
            static_cast<void>(geometry.compositePosition(kOutputResolution, 0));
        },
        "crop coordinates outside the output must be rejected");
    requireThrows<std::invalid_argument>(
        []() { qrp::atlas::WangQuiltGeometry invalid(3, 1, 4); },
        "patches smaller than four pixels must be rejected");
    requireThrows<std::invalid_argument>(
        []() { qrp::atlas::WangQuiltGeometry invalid(12, 0, 4); },
        "zero overlap must be rejected");
    requireThrows<std::invalid_argument>(
        []() { qrp::atlas::WangQuiltGeometry invalid(12, 12, 4); },
        "overlap equal to patch size must be rejected");
    requireThrows<std::invalid_argument>(
        []() { qrp::atlas::WangQuiltGeometry invalid(12, 11, 4); },
        "overlap leaving a one-pixel stride must be rejected");
    requireThrows<std::invalid_argument>(
        []() { qrp::atlas::WangQuiltGeometry invalid(12, 3, 3); },
        "output resolutions below four pixels must be rejected");
    requireThrows<std::length_error>(
        []() {
            qrp::atlas::WangQuiltGeometry invalid(
                std::numeric_limits<std::size_t>::max(),
                1,
                4);
        },
        "composite dimension addition must detect overflow");
}

void testSolidAtlasConstruction() {
    constexpr qrp::render::Rgb8 solid{73, 91, 117};
    const auto samples = solidSampleBank(kPatchSize, solid);
    auto options = buildOptions();
    options.cornerColor = solid;
    const auto result = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        options);
    require(result.atlas.tiles().size() == 8, "solid input must still create eight Wang tiles");
    require(result.report.independentCutCount == 32, "eight tiles must create four cuts each");
    require(
        result.report.independentCutPixelCount == 32 * kPatchSize,
        "cut pixel count must include every sample along all 32 paths");
    require(
        result.report.sumOfIndependentCutCosts == 0,
        "identical solid patches must have zero cut cost");
    require(result.cutPathImages.empty(), "debug composites must be opt-in");
    for (const auto& tile : result.atlas.tiles()) {
        for (const auto pixel : tile.image.pixels()) {
            require(equal(pixel, solid), "solid quilting must not leave holes or foreign pixels");
        }
    }
    for (const auto& metric : result.report.tiles) {
        require(cutCostSum(metric.cutCosts) == 0, "every solid-input seam must cost zero");
        require(
            metric.correctedBoundaryPixelCount == 0
                && metric.maximumBoundaryChannelCorrection == 0
                && metric.meanBoundaryChannelCorrection == 0.0,
            "solid authoritative boundaries must require no correction");
        require(
            metric.maximumEdgeInwardChannelDifference == 0
                && metric.meanEdgeInwardChannelDifference == 0.0,
            "solid edges must have no inward discontinuity");
    }
}

void testLShapedOverlapOwnership() {
    const auto samples = distinctSolidSampleBank(kPatchSize);
    const auto result = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        buildOptions(true));
    const auto targetEdges =
        qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(0, 0, 1, 1);
    const auto found = std::find_if(
        result.cutPathImages.begin(),
        result.cutPathImages.end(),
        [targetEdges](const qrp::atlas::WangTextureTileDebugImage& debug) {
            return debug.edges == targetEdges;
        });
    require(found != result.cutPathImages.end(), "L-overlap fixture tile is missing");
    const auto& composite = found->image;
    constexpr std::size_t stride = kPatchSize - kOverlapPixels;
    require(
        equal(composite.pixel(2, 2), samples.northSouth[0].pixel(0, 0)),
        "the north-west quilt region must retain the N sample");
    require(
        equal(composite.pixel(stride + 6, 2), samples.westEast[0].pixel(0, 0)),
        "the north-east quilt region must contain the E sample");
    require(
        equal(composite.pixel(2, stride + 6), samples.westEast[1].pixel(0, 0)),
        "the south-west quilt region must contain the W sample");
    require(
        equal(
            composite.pixel(stride + 6, stride + 6),
            samples.northSouth[1].pixel(0, 0)),
        "the non-overlap south-east quilt region must contain the S sample");

    constexpr qrp::render::Rgb8 eastCutColor{255, 68, 170};
    constexpr qrp::render::Rgb8 westCutColor{80, 235, 255};
    constexpr qrp::render::Rgb8 southTopCutColor{255, 226, 70};
    constexpr qrp::render::Rgb8 southLeftCutColor{153, 255, 90};
    const std::size_t missing = kOverlapPixels;
    std::vector<std::size_t> topCuts(kPatchSize, missing);
    std::vector<std::size_t> leftCuts(kPatchSize, missing);
    for (std::size_t x = 0; x < kPatchSize; ++x) {
        for (std::size_t y = 0; y < kOverlapPixels; ++y) {
            if (equal(
                    composite.pixel(stride + x, stride + y),
                    southTopCutColor)) {
                topCuts[x] = y;
            }
        }
    }
    for (std::size_t y = 0; y < kPatchSize; ++y) {
        for (std::size_t x = 0; x < kOverlapPixels; ++x) {
            if (equal(
                    composite.pixel(stride + x, stride + y),
                    southLeftCutColor)) {
                leftCuts[y] = x;
            }
        }
        require(leftCuts[y] != missing, "every S left-cut row must appear in the debug image");
    }

    std::size_t verifiedOldIntersectionPixels = 0;
    std::size_t verifiedNewIntersectionPixels = 0;
    const auto isOverlayColor = [&](const qrp::render::Rgb8 pixel) {
        return equal(pixel, eastCutColor)
            || equal(pixel, westCutColor)
            || equal(pixel, southTopCutColor)
            || equal(pixel, southLeftCutColor);
    };
    const auto southColor = samples.northSouth[1].pixel(0, 0);
    for (std::size_t y = 0; y < kPatchSize; ++y) {
        for (std::size_t x = 0; x < kPatchSize; ++x) {
            if (topCuts[x] == missing) {
                continue;
            }
            const auto actual = composite.pixel(stride + x, stride + y);
            if (isOverlayColor(actual)) {
                continue;
            }
            const bool passesTop = y >= kOverlapPixels || y >= topCuts[x];
            const bool passesLeft = x >= kOverlapPixels || x >= leftCuts[y];
            const bool shouldUseSouth = passesTop && passesLeft;
            require(
                equal(actual, southColor) == shouldUseSouth,
                "the S patch must be selected exactly where both L-overlap cuts pass");
            if (x < kOverlapPixels && y < kOverlapPixels) {
                if (shouldUseSouth) {
                    ++verifiedNewIntersectionPixels;
                } else {
                    ++verifiedOldIntersectionPixels;
                }
            }
        }
    }
    require(
        verifiedOldIntersectionPixels != 0
            && verifiedNewIntersectionPixels != 0,
        "the L-overlap fixture must exercise both old and new ownership in the corner");
}

void testBoundaryDiagnosticsExposeHighContrastCornerConflict() {
    const qrp::atlas::WangEdgeSampleBank samples{
        std::array<qrp::render::Image, 2>{
            solidImage(kPatchSize, {0, 0, 0}),
            solidImage(kPatchSize, {255, 255, 255}),
        },
        std::array<qrp::render::Image, 2>{
            solidImage(kPatchSize, {255, 0, 0}),
            solidImage(kPatchSize, {0, 0, 255}),
        },
    };
    auto options = buildOptions();
    options.cornerColor = {127, 127, 127};
    const auto result = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        options);
    const auto exactCompatibility =
        qrp::atlas::AtlasRenderer::measureAtlasCompatibility(result.atlas);
    require(
        exactCompatibility.mismatchedPixelCount == 0
            && exactCompatibility.maximumChannelDifference == 0,
        "the high-contrast fixture must retain exact authoritative boundaries");

    std::size_t correctedBoundaryPixels = 0;
    std::uint8_t maximumCorrection = 0;
    for (const auto& metric : result.report.tiles) {
        correctedBoundaryPixels += metric.correctedBoundaryPixelCount;
        maximumCorrection = std::max(
            maximumCorrection,
            metric.maximumBoundaryChannelCorrection);
    }
    require(
        correctedBoundaryPixels != 0,
        "boundary diagnostics must record a conflicting high-contrast corner");
    require(
        maximumCorrection >= 120,
        "boundary diagnostics must expose the high-contrast corner correction magnitude");
}

void testAsymmetricAtlasParityProvenanceAndCompatibility() {
    const auto samples = asymmetricSampleBank();
    const auto samplesBefore = samples;
    const auto result = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        buildOptions());
    require(sampleBanksEqual(samples, samplesBefore), "atlas construction must not mutate source samples");

    const auto& atlas = result.atlas;
    const auto coverage = atlas.coverage();
    require(atlas.edgeLabelCount() == 2, "minimal quilted atlas must have two edge labels");
    require(atlas.tileSize() == kOutputResolution, "atlas output resolution is incorrect");
    require(atlas.tiles().size() == 8, "minimal quilted atlas must contain eight tiles");
    require(coverage.signatureCount == 8, "all quilted signatures must be unique");
    require(coverage.expectedSignatureCount == 16, "two labels imply sixteen Cartesian signatures");
    require(!coverage.complete, "minimal even-parity set is intentionally not Cartesian-complete");
    require(result.report.patchSize == kPatchSize, "report patch size is incorrect");
    require(result.report.overlapPixels == kOverlapPixels, "report overlap is incorrect");
    require(result.report.stridePixels == kPatchSize - kOverlapPixels, "report stride is incorrect");
    require(
        result.report.outputResolutionPixels == kOutputResolution,
        "report output resolution is incorrect");
    require(result.report.independentCutCount == 32, "report must name all 32 independent cuts");
    require(
        result.report.independentCutPixelCount == 32 * kPatchSize,
        "report cut pixel count is incorrect");
    require(result.report.tiles.size() == 8, "report needs one metrics record per tile");
    require(result.cutPathImages.empty(), "default atlas construction must not retain debug images");

    std::set<std::array<std::uint32_t, 4>> signatures;
    std::uint64_t independentlySummedCost = 0;
    for (const auto& tile : atlas.tiles()) {
        require(
            (tile.edges.south ^ tile.edges.north
                ^ tile.edges.west ^ tile.edges.east) == 0U,
            "every quilted tile must satisfy the even-parity rule");
        require(signatures.insert(signatureKey(tile.edges)).second, "quilted signatures must be unique");
        require(atlas.select(tile.edges).edges == tile.edges, "atlas lookup must preserve signatures");
        const auto& metric = metricFor(result.report, tile.edges);
        independentlySummedCost += cutCostSum(metric.cutCosts);
        require(
            metric.correctedBoundaryPixelCount <= 4 * kOutputResolution - 4,
            "boundary correction count exceeds the unique boundary pixel count");
        require(
            std::isfinite(metric.meanBoundaryChannelCorrection)
                && std::isfinite(metric.meanEdgeInwardChannelDifference),
            "boundary diagnostics must be finite");
    }
    require(
        independentlySummedCost == result.report.sumOfIndependentCutCosts,
        "report total must equal the named independent cut costs");

    std::size_t eastWestPairs = 0;
    std::size_t northSouthPairs = 0;
    for (const auto& first : atlas.tiles()) {
        for (const auto& second : atlas.tiles()) {
            eastWestPairs += first.edges.east == second.edges.west;
            northSouthPairs += first.edges.north == second.edges.south;
        }
    }
    require(eastWestPairs == 32, "minimal atlas must have 32 compatible E/W ordered pairs");
    require(northSouthPairs == 32, "minimal atlas must have 32 compatible N/S ordered pairs");
    const auto compatibility = qrp::atlas::AtlasRenderer::measureAtlasCompatibility(atlas);
    require(
        compatibility.sampleCount == 64 * kOutputResolution,
        "atlas audit must sample every pixel of all 64 compatible ordered edges");
    require(
        compatibility.mismatchedPixelCount == 0
            && compatibility.maximumChannelDifference == 0,
        "all authoritative compatible boundaries must be bit-exact");

    const qrp::atlas::WangQuiltGeometry geometry(
        kPatchSize,
        kOverlapPixels,
        kOutputResolution);
    const std::size_t last = kOutputResolution - 1;
    for (const auto& tile : atlas.tiles()) {
        require(
            equal(tile.image.pixel(0, 0), kCornerColor)
                && equal(tile.image.pixel(last, 0), kCornerColor)
                && equal(tile.image.pixel(0, last), kCornerColor)
                && equal(tile.image.pixel(last, last), kCornerColor),
            "the explicit common corner constraint must hold at all four corners");
        for (std::size_t index = 1; index < last; ++index) {
            const auto northSouth = geometry.northSouthBoundaryPosition(index);
            const auto westEast = geometry.westEastBoundaryPosition(index);
            require(
                equal(
                    tile.image.pixel(index, 0),
                    sampleBilinearForTest(
                        samples.northSouth[tile.edges.north],
                        northSouth.x,
                        northSouth.y)),
                "north boundary must preserve its label sample and orientation");
            require(
                equal(
                    tile.image.pixel(index, last),
                    sampleBilinearForTest(
                        samples.northSouth[tile.edges.south],
                        northSouth.x,
                        northSouth.y)),
                "south boundary must preserve its label sample and orientation");
            require(
                equal(
                    tile.image.pixel(0, index),
                    sampleBilinearForTest(
                        samples.westEast[tile.edges.west],
                        westEast.x,
                        westEast.y)),
                "west boundary must preserve its label sample and orientation");
            require(
                equal(
                    tile.image.pixel(last, index),
                    sampleBilinearForTest(
                        samples.westEast[tile.edges.east],
                        westEast.x,
                        westEast.y)),
                "east boundary must preserve its label sample and orientation");
        }
    }

    bool northSouthLabelsDiffer = false;
    bool westEastLabelsDiffer = false;
    bool northSouthIsAsymmetric = false;
    bool westEastIsAsymmetric = false;
    for (std::size_t index = 1; index < last; ++index) {
        const auto nsForward = geometry.northSouthBoundaryPosition(index);
        const auto nsReverse = geometry.northSouthBoundaryPosition(last - index);
        const auto weForward = geometry.westEastBoundaryPosition(index);
        const auto weReverse = geometry.westEastBoundaryPosition(last - index);
        northSouthLabelsDiffer = northSouthLabelsDiffer || !equal(
            sampleBilinearForTest(samples.northSouth[0], nsForward.x, nsForward.y),
            sampleBilinearForTest(samples.northSouth[1], nsForward.x, nsForward.y));
        westEastLabelsDiffer = westEastLabelsDiffer || !equal(
            sampleBilinearForTest(samples.westEast[0], weForward.x, weForward.y),
            sampleBilinearForTest(samples.westEast[1], weForward.x, weForward.y));
        northSouthIsAsymmetric = northSouthIsAsymmetric || !equal(
            sampleBilinearForTest(samples.northSouth[0], nsForward.x, nsForward.y),
            sampleBilinearForTest(samples.northSouth[0], nsReverse.x, nsReverse.y));
        westEastIsAsymmetric = westEastIsAsymmetric || !equal(
            sampleBilinearForTest(samples.westEast[0], weForward.x, weForward.y),
            sampleBilinearForTest(samples.westEast[0], weReverse.x, weReverse.y));
    }
    require(northSouthLabelsDiffer, "N/S label samples must be distinguishable in the test fixture");
    require(westEastLabelsDiffer, "W/E label samples must be distinguishable in the test fixture");
    require(northSouthIsAsymmetric, "N/S fixture must detect a reversed boundary");
    require(westEastIsAsymmetric, "W/E fixture must detect a reversed boundary");

    std::size_t legalMacroBlocks = 0;
    for (const auto& southWest : atlas.tiles()) {
        for (const auto& southEast : atlas.tiles()) {
            if (southWest.edges.east != southEast.edges.west) {
                continue;
            }
            for (const auto& northWest : atlas.tiles()) {
                if (southWest.edges.north != northWest.edges.south) {
                    continue;
                }
                for (const auto& northEast : atlas.tiles()) {
                    if (northWest.edges.east != northEast.edges.west
                        || southEast.edges.north != northEast.edges.south) {
                        continue;
                    }
                    ++legalMacroBlocks;
                    const auto centerColor = southWest.image.pixel(last, 0);
                    require(
                        equal(centerColor, southEast.image.pixel(0, 0))
                            && equal(centerColor, northWest.image.pixel(last, last))
                            && equal(centerColor, northEast.image.pixel(0, last)),
                        "all four corners of every legal 2x2 macro-block must agree");
                }
            }
        }
    }
    require(legalMacroBlocks != 0, "the corner audit must enumerate legal 2x2 macro-blocks");
}

void testCompleteCartesianAtlas() {
    const auto samples = asymmetricSampleBank();
    const auto minimal =
        qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
            samples,
            buildOptions());
    const auto result =
        qrp::atlas::WangTextureAtlasBuilder::buildCompleteSixteen(
            samples,
            buildOptions());
    const auto coverage = result.atlas.coverage();
    require(coverage.complete, "complete atlas must cover every binary edge signature");
    require(
        coverage.tileCount == 16
            && coverage.signatureCount == 16
            && coverage.expectedSignatureCount == 16,
        "complete binary atlas must contain sixteen unique signatures");
    require(
        result.report.independentCutCount == 64
            && result.report.independentCutPixelCount == 64 * kPatchSize,
        "complete atlas report must contain four cuts per signature");
    for (std::size_t first = 0; first < result.atlas.tiles().size(); ++first) {
        for (std::size_t second = first + 1;
             second < result.atlas.tiles().size();
             ++second) {
            require(
                !imagesEqual(
                    result.atlas.tiles()[first].image,
                    result.atlas.tiles()[second].image),
                "complete atlas signatures must produce distinct tile images");
        }
    }
    for (const auto& minimalTile : minimal.atlas.tiles()) {
        require(
            imagesEqual(
                minimalTile.image,
                result.atlas.select(minimalTile.edges).image),
            "complete atlas must leave the eight shared signature images unchanged");
    }

    for (std::uint32_t north = 0; north < 2; ++north) {
        for (std::uint32_t east = 0; east < 2; ++east) {
            for (std::uint32_t south = 0; south < 2; ++south) {
                for (std::uint32_t west = 0; west < 2; ++west) {
                    const auto edges =
                        qrp::atlas::WangEdgeSignature::fromNorthEastSouthWest(
                            north,
                            east,
                            south,
                            west);
                    require(
                        result.atlas.select(edges).edges == edges,
                        "complete atlas lookup must preserve every signature");
                }
            }
        }
    }
    for (std::uint32_t south = 0; south < 2; ++south) {
        for (std::uint32_t west = 0; west < 2; ++west) {
            std::size_t candidateCount = 0;
            for (const auto& tile : result.atlas.tiles()) {
                candidateCount += tile.edges.south == south
                    && tile.edges.west == west;
            }
            require(
                candidateCount == 4,
                "complete atlas must offer four candidates for every south/west pair");
        }
    }

    const auto compatibility =
        qrp::atlas::AtlasRenderer::measureAtlasCompatibility(result.atlas);
    require(
        compatibility.sampleCount == 256 * kOutputResolution,
        "complete atlas audit must sample all compatible ordered boundaries");
    require(
        compatibility.mismatchedPixelCount == 0
            && compatibility.maximumChannelDifference == 0,
        "complete atlas boundaries must remain bit-exact");

    const qrp::atlas::WangAtlasTiling tiling(
        11,
        7,
        result.atlas,
        0x6a09e667f3bcc909ULL);
    const auto renderedSeams = qrp::atlas::AtlasRenderer::measureSeams(
        tiling,
        result.atlas);
    require(tiling.hasValidAdjacency(), "complete atlas tiling must satisfy Wang adjacency");
    require(
        renderedSeams.mismatchedPixelCount == 0
            && renderedSeams.maximumChannelDifference == 0,
        "complete atlas tiling must render with bit-exact boundaries");
}

void testAtlasDeterminismAndDebugOptIn() {
    const auto samples = asymmetricSampleBank();
    const auto first = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        buildOptions());
    const auto second = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        buildOptions());
    require(
        first.atlas.tiles().size() == second.atlas.tiles().size(),
        "repeat builds must contain the same number of tiles");
    for (std::size_t index = 0; index < first.atlas.tiles().size(); ++index) {
        require(
            first.atlas.tiles()[index].edges == second.atlas.tiles()[index].edges,
            "repeat builds must preserve signature order");
        require(
            imagesEqual(
                first.atlas.tiles()[index].image,
                second.atlas.tiles()[index].image),
            "repeat builds must be bit-identical");
        const auto& firstMetric = metricFor(
            first.report,
            first.atlas.tiles()[index].edges);
        const auto& secondMetric = metricFor(
            second.report,
            second.atlas.tiles()[index].edges);
        require(
            cutCostSum(firstMetric.cutCosts) == cutCostSum(secondMetric.cutCosts),
            "repeat builds must report identical cut costs");
    }
    require(
        first.report.sumOfIndependentCutCosts
            == second.report.sumOfIndependentCutCosts,
        "repeat build reports must be deterministic");

    const auto withDebug = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        buildOptions(true));
    require(withDebug.cutPathImages.size() == 8, "debug opt-in must return all eight composites");
    const std::size_t compositeSize = kPatchSize + (kPatchSize - kOverlapPixels);
    std::set<std::array<std::uint32_t, 4>> debugSignatures;
    for (const auto& debug : withDebug.cutPathImages) {
        require(
            debug.image.width() == compositeSize
                && debug.image.height() == compositeSize,
            "debug images must preserve the pre-rotation quilt dimensions");
        require(
            debugSignatures.insert(signatureKey(debug.edges)).second,
            "debug images need one unique record per tile signature");
    }
}

void testAtlasSourceCausality() {
    const auto baselineSamples = asymmetricSampleBank();
    auto changedSamples = baselineSamples;
    for (std::size_t y = 0; y < kPatchSize; ++y) {
        for (std::size_t x = 0; x < kPatchSize; ++x) {
            auto& pixel = changedSamples.northSouth[0].pixel(x, y);
            pixel.red ^= 0xffU;
            pixel.green ^= 0x55U;
            pixel.blue ^= 0xaaU;
        }
    }

    const auto baseline = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        baselineSamples,
        buildOptions());
    const auto changed = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        changedSamples,
        buildOptions());
    std::size_t dependentChanged = 0;
    std::size_t independentUnchanged = 0;
    for (std::size_t index = 0; index < baseline.atlas.tiles().size(); ++index) {
        const auto& before = baseline.atlas.tiles()[index];
        const auto& after = changed.atlas.tiles()[index];
        require(before.edges == after.edges, "source edits must not change atlas topology or ordering");
        const bool dependsOnChangedSample = before.edges.north == 0
            || before.edges.south == 0;
        if (dependsOnChangedSample) {
            require(
                !imagesEqual(before.image, after.image),
                "every tile using the changed N/S sample must change");
            ++dependentChanged;
        } else {
            require(
                imagesEqual(before.image, after.image),
                "tiles not using the changed sample must remain bit-identical");
            ++independentUnchanged;
        }
    }
    require(dependentChanged == 6, "an N/S label sample must influence exactly six S8 tiles");
    require(independentUnchanged == 2, "exactly two S8 tiles must be independent of one N/S label");

    const auto changedCompatibility =
        qrp::atlas::AtlasRenderer::measureAtlasCompatibility(changed.atlas);
    require(
        changedCompatibility.sampleCount == 64 * kOutputResolution
            && changedCompatibility.mismatchedPixelCount == 0
            && changedCompatibility.maximumChannelDifference == 0,
        "a source edit must preserve the regenerated atlas boundary contract");

    constexpr std::size_t gridWidth = 8;
    constexpr std::size_t gridHeight = 6;
    constexpr std::uint64_t seed = 0x4ec32a197bd80561ULL;
    const qrp::atlas::WangAtlasTiling baselineGrid(
        gridWidth,
        gridHeight,
        baseline.atlas,
        seed);
    const qrp::atlas::WangAtlasTiling changedGrid(
        gridWidth,
        gridHeight,
        changed.atlas,
        seed);
    require(
        baselineGrid.tiles() == changedGrid.tiles(),
        "atlas pixel edits must not alter the seeded Wang signature layout");
    const auto baselineImage = qrp::atlas::AtlasRenderer::render(
        baselineGrid,
        baseline.atlas);
    const auto changedImage = qrp::atlas::AtlasRenderer::render(
        changedGrid,
        changed.atlas);
    const std::size_t tileSize = baseline.atlas.tileSize();
    for (std::size_t y = 0; y < gridHeight; ++y) {
        for (std::size_t x = 0; x < gridWidth; ++x) {
            const bool dependsOnChangedSample = baselineGrid.tile(x, y).north == 0
                || baselineGrid.tile(x, y).south == 0;
            const std::size_t imageTileY = gridHeight - 1 - y;
            const bool blockEqual = imageBlockEqual(
                baselineImage,
                changedImage,
                x * tileSize,
                imageTileY * tileSize,
                tileSize,
                tileSize);
            require(
                dependsOnChangedSample ? !blockEqual : blockEqual,
                "rendered source differences must stay inside dependent tile cells");
        }
    }
}

void testMultipleTilingSeedsAndRenderedSeams() {
    const auto samples = asymmetricSampleBank();
    const auto result = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        samples,
        buildOptions());
    constexpr std::size_t width = 13;
    constexpr std::size_t height = 9;
    const std::array<std::uint64_t, 5> seeds{
        0,
        1,
        0x123456789abcdef0ULL,
        0xcafef00d12345678ULL,
        std::numeric_limits<std::uint64_t>::max(),
    };
    std::vector<qrp::atlas::WangEdgeSignature> firstLayout;
    bool foundDifferentLayout = false;
    for (const std::uint64_t seed : seeds) {
        const qrp::atlas::WangAtlasTiling tiling(
            width,
            height,
            result.atlas,
            seed);
        require(tiling.hasValidAdjacency(), "every seeded quilted-atlas tiling must be valid");
        if (firstLayout.empty()) {
            firstLayout = tiling.tiles();
        } else {
            foundDifferentLayout = foundDifferentLayout || tiling.tiles() != firstLayout;
        }
        const auto seams = qrp::atlas::AtlasRenderer::measureSeams(
            tiling,
            result.atlas);
        const std::size_t seamCount = (width - 1) * height
            + width * (height - 1);
        require(
            seams.sampleCount == seamCount * result.atlas.tileSize(),
            "render seam audit must inspect every internal boundary pixel");
        require(
            seams.mismatchedPixelCount == 0
                && seams.maximumChannelDifference == 0,
            "every seeded rendered quilt must preserve bit-exact Wang seams");
        const auto image = qrp::atlas::AtlasRenderer::render(
            tiling,
            result.atlas);
        require(
            image.width() == width * result.atlas.tileSize()
                && image.height() == height * result.atlas.tileSize(),
            "quilted Wang render dimensions are incorrect");
    }
    require(foundDifferentLayout, "multiple placement seeds should explore different legal layouts");
}

void testAtlasBuilderRejectsInvalidInputs() {
    const auto validSamples = asymmetricSampleBank();
    requireThrows<std::invalid_argument>(
        []() {
            const auto tiny = asymmetricSampleBank(3);
            static_cast<void>(
                qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                    tiny,
                    buildOptions()));
        },
        "sample patches smaller than four pixels must be rejected");
    requireThrows<std::invalid_argument>(
        [&validSamples]() {
            auto mismatched = validSamples;
            mismatched.westEast[1] = qrp::render::Image(11, 12);
            static_cast<void>(
                qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                    mismatched,
                    buildOptions()));
        },
        "non-square or differently sized samples must be rejected");
    requireThrows<std::invalid_argument>(
        [&validSamples]() {
            auto options = buildOptions();
            options.overlapPixels = 0;
            static_cast<void>(
                qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                    validSamples,
                    options));
        },
        "zero overlap must be rejected by the builder");
    requireThrows<std::invalid_argument>(
        [&validSamples]() {
            auto options = buildOptions();
            options.overlapPixels = kPatchSize;
            static_cast<void>(
                qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                    validSamples,
                    options));
        },
        "overlap equal to patch size must be rejected by the builder");
    requireThrows<std::invalid_argument>(
        [&validSamples]() {
            auto options = buildOptions();
            options.overlapPixels = kPatchSize - 1;
            static_cast<void>(
                qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                    validSamples,
                    options));
        },
        "builder overlap must leave at least a two-pixel stride");
    requireThrows<std::invalid_argument>(
        [&validSamples]() {
            auto options = buildOptions();
            options.outputResolutionPixels = 3;
            static_cast<void>(
                qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
                    validSamples,
                    options));
        },
        "builder output resolutions below four pixels must be rejected");

    auto automaticOptions = buildOptions();
    automaticOptions.outputResolutionPixels = 0;
    const auto automatic = qrp::atlas::WangTextureAtlasBuilder::buildMinimalEight(
        validSamples,
        automaticOptions);
    require(
        automatic.atlas.tileSize() == 14
            && automatic.report.outputResolutionPixels == 14,
        "builder must report and use the automatic endpoint-inclusive resolution");
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"minimum-error cut exhaustive oracle", testMinimumErrorCutAgainstBruteForceOracle},
        {"minimum-error cut tie-break and failures", testMinimumErrorCutTieBreakAndFailures},
        {"minimum-error cut waypoints", testMinimumErrorCutWaypoints},
        {"quilt geometry provenance and bounds", testQuiltGeometryProvenanceAndBounds},
        {"solid atlas construction", testSolidAtlasConstruction},
        {"L-shaped overlap ownership", testLShapedOverlapOwnership},
        {"high-contrast corner diagnostics", testBoundaryDiagnosticsExposeHighContrastCornerConflict},
        {"asymmetric atlas parity and provenance", testAsymmetricAtlasParityProvenanceAndCompatibility},
        {"complete Cartesian atlas", testCompleteCartesianAtlas},
        {"atlas determinism and debug opt-in", testAtlasDeterminismAndDebugOptIn},
        {"atlas source causality", testAtlasSourceCausality},
        {"multiple tiling seeds and rendered seams", testMultipleTilingSeedsAndRenderedSeams},
        {"atlas builder invalid inputs", testAtlasBuilderRejectsInvalidInputs},
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << tests.size() << " test(s) passed.\n";
    return EXIT_SUCCESS;
}
