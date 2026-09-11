#include "atlas/WangTextureAtlasBuilder.hpp"

#include "atlas/MinimumErrorCut.hpp"
#include "atlas/WangQuiltGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace qrp::atlas {
namespace {

struct QuiltedTile {
    render::Image image;
    WangTextureCutCosts cutCosts;
    std::optional<render::Image> cutPathImage;
};

struct BoundaryCorrectionStats {
    std::size_t correctedPixelCount = 0;
    std::uint8_t maximumChannelCorrection = 0;
    double meanChannelCorrection = 0.0;
};

struct EdgeInwardStats {
    std::uint8_t maximumChannelDifference = 0;
    double meanChannelDifference = 0.0;
};

struct BoundarySamples {
    std::array<std::vector<render::Rgb8>, 2> northSouth;
    std::array<std::vector<render::Rgb8>, 2> westEast;
    render::Rgb8 fixedCorner;
};

[[nodiscard]] std::uint8_t channelDifference(
    const std::uint8_t first,
    const std::uint8_t second) noexcept {
    return static_cast<std::uint8_t>(
        first > second ? first - second : second - first);
}

[[nodiscard]] std::uint64_t checkedAdd(
    const std::uint64_t first,
    const std::uint64_t second) {
    if (first > std::numeric_limits<std::uint64_t>::max() - second) {
        throw std::overflow_error("Wang texture atlas cut cost overflowed uint64_t.");
    }
    return first + second;
}

[[nodiscard]] std::size_t checkedMultiply(
    const std::size_t first,
    const std::size_t second,
    const char* const description) {
    if (first != 0
        && second > std::numeric_limits<std::size_t>::max() / first) {
        throw std::length_error(description);
    }
    return first * second;
}

[[nodiscard]] std::uint64_t squaredDifference(
    const render::Rgb8 first,
    const render::Rgb8 second) noexcept {
    const auto square = [](const int value) {
        return static_cast<std::uint64_t>(value * value);
    };
    return square(static_cast<int>(first.red) - static_cast<int>(second.red))
        + square(static_cast<int>(first.green) - static_cast<int>(second.green))
        + square(static_cast<int>(first.blue) - static_cast<int>(second.blue));
}

void copyPatch(
    const render::Image& patch,
    const std::size_t destinationX,
    const std::size_t destinationY,
    render::Image& destination) {
    for (std::size_t y = 0; y < patch.height(); ++y) {
        for (std::size_t x = 0; x < patch.width(); ++x) {
            destination.pixel(destinationX + x, destinationY + y) = patch.pixel(x, y);
        }
    }
}

[[nodiscard]] std::vector<std::uint64_t> verticalOverlapCosts(
    const render::Image& existing,
    const render::Image& patch,
    const std::size_t destinationX,
    const std::size_t destinationY,
    const std::size_t overlapPixels) {
    const std::size_t costCount = checkedMultiply(
        overlapPixels,
        patch.height(),
        "Vertical Wang quilt overlap dimensions overflow size_t.");
    std::vector<std::uint64_t> costs(costCount);
    for (std::size_t y = 0; y < patch.height(); ++y) {
        for (std::size_t x = 0; x < overlapPixels; ++x) {
            costs[y * overlapPixels + x] = squaredDifference(
                existing.pixel(destinationX + x, destinationY + y),
                patch.pixel(x, y));
        }
    }
    return costs;
}

[[nodiscard]] std::vector<std::uint64_t> horizontalOverlapCosts(
    const render::Image& existing,
    const render::Image& patch,
    const std::size_t destinationX,
    const std::size_t destinationY,
    const std::size_t overlapPixels) {
    const std::size_t costCount = checkedMultiply(
        patch.width(),
        overlapPixels,
        "Horizontal Wang quilt overlap dimensions overflow size_t.");
    std::vector<std::uint64_t> costs(costCount);
    for (std::size_t y = 0; y < overlapPixels; ++y) {
        for (std::size_t x = 0; x < patch.width(); ++x) {
            costs[y * patch.width() + x] = squaredDifference(
                existing.pixel(destinationX + x, destinationY + y),
                patch.pixel(x, y));
        }
    }
    return costs;
}

void placeEastPatch(
    const render::Image& patch,
    const std::size_t stride,
    const std::size_t overlapPixels,
    const MinimumErrorCut& cut,
    render::Image& canvas) {
    for (std::size_t y = 0; y < patch.height(); ++y) {
        for (std::size_t x = 0; x < patch.width(); ++x) {
            if (x >= overlapPixels || x >= cut.offsets[y]) {
                canvas.pixel(stride + x, y) = patch.pixel(x, y);
            }
        }
    }
}

void placeWestPatch(
    const render::Image& patch,
    const std::size_t stride,
    const std::size_t overlapPixels,
    const MinimumErrorCut& cut,
    render::Image& canvas) {
    for (std::size_t y = 0; y < patch.height(); ++y) {
        for (std::size_t x = 0; x < patch.width(); ++x) {
            if (y >= overlapPixels || y >= cut.offsets[x]) {
                canvas.pixel(x, stride + y) = patch.pixel(x, y);
            }
        }
    }
}

void placeSouthPatch(
    const render::Image& patch,
    const std::size_t stride,
    const std::size_t overlapPixels,
    const MinimumErrorCut& topCut,
    const MinimumErrorCut& leftCut,
    render::Image& canvas) {
    for (std::size_t y = 0; y < patch.height(); ++y) {
        for (std::size_t x = 0; x < patch.width(); ++x) {
            const bool inTopOverlap = y < overlapPixels;
            const bool inLeftOverlap = x < overlapPixels;
            const bool passesTopCut = !inTopOverlap || y >= topCut.offsets[x];
            const bool passesLeftCut = !inLeftOverlap || x >= leftCut.offsets[y];
            if (passesTopCut && passesLeftCut) {
                canvas.pixel(stride + x, stride + y) = patch.pixel(x, y);
            }
        }
    }
}

void markPixel(
    render::Image& image,
    const std::size_t x,
    const std::size_t y,
    const render::Rgb8 color) {
    if (x < image.width() && y < image.height()) {
        image.pixel(x, y) = color;
    }
}

void overlayCuts(
    const std::size_t stride,
    const MinimumErrorCut& eastCut,
    const MinimumErrorCut& westCut,
    const MinimumErrorCut& southTopCut,
    const MinimumErrorCut& southLeftCut,
    render::Image& image) {
    constexpr render::Rgb8 eastColor{255, 68, 170};
    constexpr render::Rgb8 westColor{80, 235, 255};
    constexpr render::Rgb8 southTopColor{255, 226, 70};
    constexpr render::Rgb8 southLeftColor{153, 255, 90};
    for (std::size_t y = 0; y < eastCut.offsets.size(); ++y) {
        markPixel(image, stride + eastCut.offsets[y], y, eastColor);
    }
    for (std::size_t x = 0; x < westCut.offsets.size(); ++x) {
        markPixel(image, x, stride + westCut.offsets[x], westColor);
    }
    for (std::size_t x = 0; x < southTopCut.offsets.size(); ++x) {
        markPixel(
            image,
            stride + x,
            stride + southTopCut.offsets[x],
            southTopColor);
    }
    for (std::size_t y = 0; y < southLeftCut.offsets.size(); ++y) {
        markPixel(
            image,
            stride + southLeftCut.offsets[y],
            stride + y,
            southLeftColor);
    }
}

[[nodiscard]] double boundedSampleCoordinate(
    const double coordinate,
    const std::size_t extent) {
    const double maximum = static_cast<double>(extent - 1);
    const double tolerance = 64.0
        * std::numeric_limits<double>::epsilon()
        * std::max(1.0, maximum);
    if (!std::isfinite(coordinate)
        || coordinate < -tolerance
        || coordinate > maximum + tolerance) {
        throw std::logic_error(
            "Wang quilt sampling coordinate leaves its source image.");
    }
    return std::clamp(coordinate, 0.0, maximum);
}

[[nodiscard]] render::Rgb8 sampleBilinear(
    const render::Image& image,
    const double x,
    const double y) {
    const double clampedX = boundedSampleCoordinate(x, image.width());
    const double clampedY = boundedSampleCoordinate(y, image.height());
    const std::size_t x0 = static_cast<std::size_t>(std::floor(clampedX));
    const std::size_t y0 = static_cast<std::size_t>(std::floor(clampedY));
    const std::size_t x1 = x0 == image.width() - 1 ? x0 : x0 + 1;
    const std::size_t y1 = y0 == image.height() - 1 ? y0 : y0 + 1;
    const double tx = clampedX - static_cast<double>(x0);
    const double ty = clampedY - static_cast<double>(y0);

    const auto channel = [&](const std::uint8_t render::Rgb8::* member) {
        const double top = std::lerp(
            static_cast<double>(image.pixel(x0, y0).*member),
            static_cast<double>(image.pixel(x1, y0).*member),
            tx);
        const double bottom = std::lerp(
            static_cast<double>(image.pixel(x0, y1).*member),
            static_cast<double>(image.pixel(x1, y1).*member),
            tx);
        return static_cast<std::uint8_t>(std::lround(std::clamp(
            std::lerp(top, bottom, ty),
            0.0,
            255.0)));
    };
    return {
        channel(&render::Rgb8::red),
        channel(&render::Rgb8::green),
        channel(&render::Rgb8::blue),
    };
}

[[nodiscard]] render::Image rotateAndCrop(
    const render::Image& composite,
    const WangQuiltGeometry& geometry) {
    if (composite.width() != geometry.compositeSize()
        || composite.height() != geometry.compositeSize()) {
        throw std::invalid_argument(
            "Wang quilt composite dimensions do not match the crop geometry.");
    }
    const std::size_t resolution = geometry.outputResolutionPixels();
    render::Image output(resolution, resolution);
    for (std::size_t y = 0; y < resolution; ++y) {
        for (std::size_t x = 0; x < resolution; ++x) {
            const QuiltSamplePosition source = geometry.compositePosition(x, y);
            output.pixel(x, y) = sampleBilinear(composite, source.x, source.y);
        }
    }
    return output;
}

[[nodiscard]] std::vector<render::Rgb8> sampleNorthSouthBoundary(
    const render::Image& patch,
    const WangQuiltGeometry& geometry) {
    const std::size_t resolution = geometry.outputResolutionPixels();
    std::vector<render::Rgb8> boundary(resolution);
    for (std::size_t index = 0; index < resolution; ++index) {
        const QuiltSamplePosition source =
            geometry.northSouthBoundaryPosition(index);
        boundary[index] = sampleBilinear(patch, source.x, source.y);
    }
    return boundary;
}

[[nodiscard]] std::vector<render::Rgb8> sampleWestEastBoundary(
    const render::Image& patch,
    const WangQuiltGeometry& geometry) {
    const std::size_t resolution = geometry.outputResolutionPixels();
    std::vector<render::Rgb8> boundary(resolution);
    for (std::size_t index = 0; index < resolution; ++index) {
        const QuiltSamplePosition source =
            geometry.westEastBoundaryPosition(index);
        boundary[index] = sampleBilinear(patch, source.x, source.y);
    }
    return boundary;
}

[[nodiscard]] BoundarySamples createBoundarySamples(
    const WangEdgeSampleBank& samples,
    const WangQuiltGeometry& geometry,
    const render::Rgb8 cornerColor) {
    BoundarySamples boundaries;
    for (std::size_t label = 0; label < 2; ++label) {
        boundaries.northSouth[label] = sampleNorthSouthBoundary(
            samples.northSouth[label],
            geometry);
        boundaries.westEast[label] = sampleWestEastBoundary(
            samples.westEast[label],
            geometry);
    }
    boundaries.fixedCorner = cornerColor;
    return boundaries;
}

[[nodiscard]] BoundaryCorrectionStats enforceAuthoritativeBoundaries(
    const WangEdgeSignature edges,
    const BoundarySamples& boundaries,
    render::Image& image) {
    const std::size_t last = image.width() - 1;
    BoundaryCorrectionStats stats;
    std::uint64_t totalChannelCorrection = 0;
    std::size_t boundaryPixelCount = 0;
    const auto apply = [&](const std::size_t x,
                           const std::size_t y,
                           const render::Rgb8 authoritative) {
        const render::Rgb8 before = image.pixel(x, y);
        const std::uint8_t red = channelDifference(before.red, authoritative.red);
        const std::uint8_t green = channelDifference(before.green, authoritative.green);
        const std::uint8_t blue = channelDifference(before.blue, authoritative.blue);
        const std::uint8_t maximum = std::max({red, green, blue});
        if (maximum != 0) {
            ++stats.correctedPixelCount;
        }
        stats.maximumChannelCorrection = std::max(
            stats.maximumChannelCorrection,
            maximum);
        totalChannelCorrection = checkedAdd(totalChannelCorrection, red);
        totalChannelCorrection = checkedAdd(totalChannelCorrection, green);
        totalChannelCorrection = checkedAdd(totalChannelCorrection, blue);
        ++boundaryPixelCount;
        image.pixel(x, y) = authoritative;
    };

    for (std::size_t index = 1; index < last; ++index) {
        apply(index, 0, boundaries.northSouth.at(edges.north).at(index));
        apply(index, last, boundaries.northSouth.at(edges.south).at(index));
        apply(0, index, boundaries.westEast.at(edges.west).at(index));
        apply(last, index, boundaries.westEast.at(edges.east).at(index));
    }
    apply(0, 0, boundaries.fixedCorner);
    apply(last, 0, boundaries.fixedCorner);
    apply(0, last, boundaries.fixedCorner);
    apply(last, last, boundaries.fixedCorner);

    stats.meanChannelCorrection = static_cast<double>(totalChannelCorrection)
        / (static_cast<double>(boundaryPixelCount) * 3.0);
    return stats;
}

[[nodiscard]] EdgeInwardStats measureEdgeInwardDifference(
    const render::Image& image) {
    const std::size_t last = image.width() - 1;
    std::uint64_t totalChannelDifference = 0;
    std::size_t pairCount = 0;
    EdgeInwardStats stats;
    const auto accumulate = [&](const render::Rgb8 edge, const render::Rgb8 inward) {
        const std::uint8_t red = channelDifference(edge.red, inward.red);
        const std::uint8_t green = channelDifference(edge.green, inward.green);
        const std::uint8_t blue = channelDifference(edge.blue, inward.blue);
        stats.maximumChannelDifference = std::max(
            stats.maximumChannelDifference,
            std::max({red, green, blue}));
        totalChannelDifference = checkedAdd(totalChannelDifference, red);
        totalChannelDifference = checkedAdd(totalChannelDifference, green);
        totalChannelDifference = checkedAdd(totalChannelDifference, blue);
        ++pairCount;
    };
    for (std::size_t index = 1; index < last; ++index) {
        accumulate(image.pixel(index, 0), image.pixel(index, 1));
        accumulate(image.pixel(index, last), image.pixel(index, last - 1));
        accumulate(image.pixel(0, index), image.pixel(1, index));
        accumulate(image.pixel(last, index), image.pixel(last - 1, index));
    }
    stats.meanChannelDifference = static_cast<double>(totalChannelDifference)
        / (static_cast<double>(pairCount) * 3.0);
    return stats;
}

[[nodiscard]] std::array<MinimumErrorCutWaypoint, 2> cropSeamWaypoints(
    const WangQuiltGeometry& geometry) {
    // The inverse 45-degree crop meets every overlap strip at its midpoint.
    // Pinning each cut there and again at the common quilt center keeps the
    // four seams inside the cropped tile and gives them one shared junction.
    const std::size_t midpoint = geometry.overlapPixels() / 2;
    const std::size_t centerIndex = geometry.patchSize()
        - (geometry.overlapPixels() - midpoint);
    return {{
        {midpoint, midpoint},
        {centerIndex, midpoint},
    }};
}

[[nodiscard]] QuiltedTile quiltTile(
    const render::Image& north,
    const render::Image& east,
    const render::Image& south,
    const render::Image& west,
    const WangQuiltGeometry& geometry,
    const bool includeDebugImage) {
    const std::size_t patchSize = geometry.patchSize();
    const std::size_t stride = geometry.stridePixels();
    const std::size_t overlapPixels = geometry.overlapPixels();
    const std::size_t compositeSize = geometry.compositeSize();
    const auto seamWaypoints = cropSeamWaypoints(geometry);
    render::Image composite(compositeSize, compositeSize);
    copyPatch(north, 0, 0, composite);

    const auto eastCosts = verticalOverlapCosts(
        composite,
        east,
        stride,
        0,
        overlapPixels);
    const auto eastCut = findVerticalMinimumErrorCut(
        eastCosts,
        overlapPixels,
        patchSize,
        seamWaypoints);
    placeEastPatch(east, stride, overlapPixels, eastCut, composite);

    const auto westCosts = horizontalOverlapCosts(
        composite,
        west,
        0,
        stride,
        overlapPixels);
    const auto westCut = findHorizontalMinimumErrorCut(
        westCosts,
        patchSize,
        overlapPixels,
        seamWaypoints);
    placeWestPatch(west, stride, overlapPixels, westCut, composite);

    const auto southTopCosts = horizontalOverlapCosts(
        composite,
        south,
        stride,
        stride,
        overlapPixels);
    const auto southLeftCosts = verticalOverlapCosts(
        composite,
        south,
        stride,
        stride,
        overlapPixels);
    const auto southTopCut = findHorizontalMinimumErrorCut(
        southTopCosts,
        patchSize,
        overlapPixels,
        seamWaypoints);
    const auto southLeftCut = findVerticalMinimumErrorCut(
        southLeftCosts,
        overlapPixels,
        patchSize,
        seamWaypoints);
    placeSouthPatch(
        south,
        stride,
        overlapPixels,
        southTopCut,
        southLeftCut,
        composite);

    std::optional<render::Image> cutPathImage;
    if (includeDebugImage) {
        cutPathImage.emplace(composite);
        overlayCuts(
            stride,
            eastCut,
            westCut,
            southTopCut,
            southLeftCut,
            *cutPathImage);
    }
    return {
        rotateAndCrop(composite, geometry),
        WangTextureCutCosts{
            eastCut.totalCost,
            westCut.totalCost,
            southTopCut.totalCost,
            southLeftCut.totalCost,
        },
        std::move(cutPathImage),
    };
}

void validateSamples(
    const WangEdgeSampleBank& samples) {
    const std::size_t patchSize = samples.northSouth.front().width();
    if (patchSize < 4
        || samples.northSouth.front().height() != patchSize) {
        throw std::invalid_argument(
            "Wang edge samples must be square and at least four pixels wide.");
    }
    const auto validate = [patchSize](const render::Image& image) {
        if (image.width() != patchSize || image.height() != patchSize) {
            throw std::invalid_argument(
                "Every Wang edge sample must have the same square dimensions.");
        }
    };
    for (const auto& image : samples.northSouth) {
        validate(image);
    }
    for (const auto& image : samples.westEast) {
        validate(image);
    }
}

[[nodiscard]] std::uint64_t sumCutCosts(const WangTextureCutCosts& costs) {
    std::uint64_t sum = 0;
    sum = checkedAdd(sum, costs.eastPlacementVertical);
    sum = checkedAdd(sum, costs.westPlacementHorizontal);
    sum = checkedAdd(sum, costs.southPlacementTop);
    sum = checkedAdd(sum, costs.southPlacementLeft);
    return sum;
}

[[nodiscard]] std::string tileName(const WangEdgeSignature edges) {
    return "quilted_S" + std::to_string(edges.south)
        + "_N" + std::to_string(edges.north)
        + "_W" + std::to_string(edges.west)
        + "_E" + std::to_string(edges.east);
}

enum class SignatureSet {
    MinimalEvenParityEight,
    CompleteCartesianSixteen,
};

[[nodiscard]] WangTextureAtlasBuildResult buildAtlas(
    const WangEdgeSampleBank& samples,
    const WangTextureAtlasBuildOptions& options,
    const SignatureSet signatureSet) {
    validateSamples(samples);
    const std::size_t patchSize = samples.northSouth.front().width();
    const WangQuiltGeometry geometry(
        patchSize,
        options.overlapPixels,
        options.outputResolutionPixels);
    const BoundarySamples boundaries = createBoundarySamples(
        samples,
        geometry,
        options.cornerColor);

    std::vector<WangImageTile> tiles;
    std::vector<WangTextureTileBuildMetrics> metrics;
    std::vector<WangTextureTileDebugImage> debugImages;
    const std::size_t expectedTileCount =
        signatureSet == SignatureSet::MinimalEvenParityEight ? 8 : 16;
    tiles.reserve(expectedTileCount);
    metrics.reserve(expectedTileCount);
    if (options.includeDebugImages) {
        debugImages.reserve(expectedTileCount);
    }

    std::uint64_t independentCutCostSum = 0;
    for (std::uint32_t south = 0; south < 2; ++south) {
        for (std::uint32_t north = 0; north < 2; ++north) {
            for (std::uint32_t west = 0; west < 2; ++west) {
                for (std::uint32_t east = 0; east < 2; ++east) {
                    if (signatureSet == SignatureSet::MinimalEvenParityEight
                        && (south ^ north ^ west ^ east) != 0U) {
                        continue;
                    }
                    const auto edges =
                        WangEdgeSignature::fromNorthEastSouthWest(
                            north,
                            east,
                            south,
                            west);
                    auto quilted = quiltTile(
                        samples.northSouth.at(north),
                        samples.westEast.at(east),
                        samples.northSouth.at(south),
                        samples.westEast.at(west),
                        geometry,
                        options.includeDebugImages);
                    const BoundaryCorrectionStats correction =
                        enforceAuthoritativeBoundaries(
                            edges,
                            boundaries,
                            quilted.image);
                    const EdgeInwardStats inward =
                        measureEdgeInwardDifference(quilted.image);
                    independentCutCostSum = checkedAdd(
                        independentCutCostSum,
                        sumCutCosts(quilted.cutCosts));
                    metrics.push_back({
                        edges,
                        quilted.cutCosts,
                        correction.correctedPixelCount,
                        correction.maximumChannelCorrection,
                        correction.meanChannelCorrection,
                        inward.maximumChannelDifference,
                        inward.meanChannelDifference,
                    });
                    if (options.includeDebugImages) {
                        if (!quilted.cutPathImage.has_value()) {
                            throw std::logic_error(
                                "Requested Wang quilt debug image was not generated.");
                        }
                        debugImages.push_back({
                            edges,
                            std::move(*quilted.cutPathImage),
                        });
                    }
                    tiles.push_back({
                        edges,
                        std::move(quilted.image),
                        tileName(edges),
                    });
                }
            }
        }
    }

    constexpr std::size_t kIndependentCutsPerTile = 4;
    if (tiles.size() != expectedTileCount
        || metrics.size() != expectedTileCount
        || (options.includeDebugImages
            ? debugImages.size() != expectedTileCount
            : !debugImages.empty())) {
        throw std::logic_error(
            "Wang texture atlas construction violated its signature-set cardinality.");
    }
    const std::size_t independentCutCount = checkedMultiply(
        metrics.size(),
        kIndependentCutsPerTile,
        "Wang texture atlas cut count overflows size_t.");
    const std::size_t independentCutPixelCount = checkedMultiply(
        independentCutCount,
        patchSize,
        "Wang texture atlas cut pixel count overflows size_t.");

    WangTextureAtlasBuildReport report;
    report.patchSize = patchSize;
    report.overlapPixels = geometry.overlapPixels();
    report.stridePixels = geometry.stridePixels();
    report.outputResolutionPixels = geometry.outputResolutionPixels();
    report.independentCutCount = independentCutCount;
    report.independentCutPixelCount = independentCutPixelCount;
    report.sumOfIndependentCutCosts = independentCutCostSum;
    report.tiles = std::move(metrics);
    return {
        WangTileAtlas(2, std::move(tiles)),
        std::move(report),
        std::move(debugImages),
    };
}

} // namespace

WangTextureAtlasBuildResult WangTextureAtlasBuilder::buildMinimalEight(
    const WangEdgeSampleBank& samples,
    const WangTextureAtlasBuildOptions& options) {
    return buildAtlas(
        samples,
        options,
        SignatureSet::MinimalEvenParityEight);
}

WangTextureAtlasBuildResult WangTextureAtlasBuilder::buildCompleteSixteen(
    const WangEdgeSampleBank& samples,
    const WangTextureAtlasBuildOptions& options) {
    return buildAtlas(
        samples,
        options,
        SignatureSet::CompleteCartesianSixteen);
}

} // namespace qrp::atlas
