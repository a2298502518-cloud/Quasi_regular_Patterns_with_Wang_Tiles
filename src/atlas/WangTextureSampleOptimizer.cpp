#include "atlas/WangTextureSampleOptimizer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace qrp::atlas {
namespace {

using FlatOrigins = std::array<WangTextureSampleOrigin, 4>;

class SplitMix64 final {
public:
    explicit SplitMix64(const std::uint64_t seed) noexcept
        : state_(seed) {}

    [[nodiscard]] std::uint64_t next() noexcept {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    [[nodiscard]] std::size_t bounded(const std::size_t bound) noexcept {
        const std::uint64_t unsignedBound = static_cast<std::uint64_t>(bound);
        const std::uint64_t threshold = (0ULL - unsignedBound) % unsignedBound;
        std::uint64_t value = 0;
        do {
            value = next();
        } while (value < threshold);
        return static_cast<std::size_t>(value % unsignedBound);
    }

private:
    std::uint64_t state_ = 0;
};

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

[[nodiscard]] bool sameOrigin(
    const WangTextureSampleOrigin first,
    const WangTextureSampleOrigin second) noexcept {
    return first.x == second.x && first.y == second.y;
}

[[nodiscard]] bool originsAreSeparated(
    const WangTextureSampleOrigin first,
    const WangTextureSampleOrigin second,
    const std::size_t minimumDistance) noexcept {
    if (sameOrigin(first, second)) {
        return false;
    }
    if (minimumDistance == 0) {
        return true;
    }
    const long double deltaX = first.x > second.x
        ? static_cast<long double>(first.x - second.x)
        : static_cast<long double>(second.x - first.x);
    const long double deltaY = first.y > second.y
        ? static_cast<long double>(first.y - second.y)
        : static_cast<long double>(second.y - first.y);
    const long double distance = static_cast<long double>(minimumDistance);
    return deltaX * deltaX + deltaY * deltaY >= distance * distance;
}

[[nodiscard]] bool isCompatible(
    const WangTextureSampleOrigin candidate,
    const FlatOrigins& selected,
    const std::size_t selectedCount,
    const std::size_t minimumDistance) noexcept {
    for (std::size_t index = 0; index < selectedCount; ++index) {
        if (!originsAreSeparated(candidate, selected[index], minimumDistance)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] WangTextureSampleOrigin originAt(
    const std::size_t linearIndex,
    const std::size_t originWidth) noexcept {
    return {
        linearIndex % originWidth,
        linearIndex / originWidth,
    };
}

[[nodiscard]] bool chooseCandidateGroup(
    SplitMix64& random,
    const std::size_t originWidth,
    const std::size_t originCount,
    const std::size_t minimumDistance,
    FlatOrigins& origins) {
    constexpr std::size_t maximumRestarts = 64;
    for (std::size_t restart = 0; restart < maximumRestarts; ++restart) {
        std::size_t selectedCount = 0;
        for (; selectedCount < origins.size(); ++selectedCount) {
            std::size_t compatibleCount = 0;
            for (std::size_t index = 0; index < originCount; ++index) {
                const WangTextureSampleOrigin candidate = originAt(
                    index,
                    originWidth);
                if (isCompatible(
                        candidate,
                        origins,
                        selectedCount,
                        minimumDistance)) {
                    ++compatibleCount;
                }
            }
            if (compatibleCount == 0) {
                break;
            }
            std::size_t selectedRank = random.bounded(compatibleCount);
            for (std::size_t index = 0; index < originCount; ++index) {
                const WangTextureSampleOrigin candidate = originAt(
                    index,
                    originWidth);
                if (!isCompatible(
                        candidate,
                        origins,
                        selectedCount,
                        minimumDistance)) {
                    continue;
                }
                if (selectedRank == 0) {
                    origins[selectedCount] = candidate;
                    break;
                }
                --selectedRank;
            }
        }
        if (selectedCount == origins.size()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] render::Image crop(
    const render::Image& source,
    const WangTextureSampleOrigin origin,
    const std::size_t patchSize) {
    render::Image result(patchSize, patchSize);
    for (std::size_t y = 0; y < patchSize; ++y) {
        for (std::size_t x = 0; x < patchSize; ++x) {
            result.pixel(x, y) = source.pixel(origin.x + x, origin.y + y);
        }
    }
    return result;
}

[[nodiscard]] WangEdgeSampleBank makeSampleBank(
    const render::Image& source,
    const FlatOrigins& origins,
    const std::size_t patchSize) {
    return {
        std::array<render::Image, 2>{
            crop(source, origins[0], patchSize),
            crop(source, origins[1], patchSize),
        },
        std::array<render::Image, 2>{
            crop(source, origins[2], patchSize),
            crop(source, origins[3], patchSize),
        },
    };
}

[[nodiscard]] WangTextureSampleOrigins namedOrigins(
    const FlatOrigins& origins) noexcept {
    return {
        {origins[0], origins[1]},
        {origins[2], origins[3]},
    };
}

[[nodiscard]] FlatOrigins flattenOrigins(
    const WangTextureSampleOrigins& origins) noexcept {
    return {
        origins.northSouth[0],
        origins.northSouth[1],
        origins.westEast[0],
        origins.westEast[1],
    };
}

[[nodiscard]] bool originsLexicographicallyLess(
    const FlatOrigins& first,
    const FlatOrigins& second) noexcept {
    for (std::size_t index = 0; index < first.size(); ++index) {
        if (first[index].y != second[index].y) {
            return first[index].y < second[index].y;
        }
        if (first[index].x != second[index].x) {
            return first[index].x < second[index].x;
        }
    }
    return false;
}

} // namespace

WangTextureSampleOptimizationResult WangTextureSampleOptimizer::optimize(
    const render::Image& source,
    const WangTextureSampleOptimizationOptions& options) {
    if (options.patchSize < 4) {
        throw std::invalid_argument(
            "Wang texture sample optimization needs patches of at least four pixels.");
    }
    if (source.width() < options.patchSize
        || source.height() < options.patchSize) {
        throw std::invalid_argument(
            "The Wang texture source image is smaller than the requested patch.");
    }
    if (options.candidateGroupCount == 0) {
        throw std::invalid_argument(
            "Wang texture sample optimization needs at least one candidate group.");
    }

    const std::size_t originWidth = source.width() - options.patchSize + 1;
    const std::size_t originHeight = source.height() - options.patchSize + 1;
    const std::size_t originCount = checkedMultiply(
        originWidth,
        originHeight,
        "Wang texture crop-origin count overflows size_t.");
    if (originCount < 4) {
        throw std::invalid_argument(
            "The Wang texture source does not contain four distinct crop origins.");
    }

    SplitMix64 random(options.searchSeed);
    WangTextureAtlasBuildOptions evaluationOptions = options.buildOptions;
    evaluationOptions.includeDebugImages = false;

    WangTextureSampleOptimizationReport report;
    WangEdgeSampleBank bestSamples = makeSampleBank(
        source,
        FlatOrigins{},
        options.patchSize);
    bool hasBest = false;
    for (std::size_t candidateIndex = 0;
         candidateIndex < options.candidateGroupCount;
         ++candidateIndex) {
        FlatOrigins origins{};
        if (!chooseCandidateGroup(
                random,
                originWidth,
                originCount,
                options.minimumOriginDistancePixels,
                origins)) {
            throw std::runtime_error(
                "Wang texture sample search exhausted its crop-group generation attempts.");
        }
        WangEdgeSampleBank samples = makeSampleBank(
            source,
            origins,
            options.patchSize);
        const WangTextureAtlasBuildResult build =
            WangTextureAtlasBuilder::buildMinimalEight(
                samples,
                evaluationOptions);
        const std::uint64_t cost = build.report.sumOfIndependentCutCosts;
        if (candidateIndex == 0) {
            report.firstCandidateCutCost = cost;
        }
        if (!hasBest
            || cost < report.bestCutCost
            || (cost == report.bestCutCost
                && originsLexicographicallyLess(
                    origins,
                    flattenOrigins(report.bestOrigins)))) {
            report.bestCutCost = cost;
            report.bestOrigins = namedOrigins(origins);
            bestSamples = std::move(samples);
            hasBest = true;
        }
        ++report.evaluatedCandidateGroupCount;
    }

    return {
        std::move(bestSamples),
        report,
    };
}

} // namespace qrp::atlas
