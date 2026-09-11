#include "atlas/WangQuiltGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qrp::atlas {
namespace {

constexpr long double kSqrtTwo = 1.414213562373095048801688724209698L;

[[nodiscard]] double clampDerivedCoordinate(
    const double coordinate,
    const double maximum) {
    const double scale = std::max(1.0, maximum);
    const double tolerance =
        64.0 * std::numeric_limits<double>::epsilon() * scale;
    if (!std::isfinite(coordinate)
        || coordinate < -tolerance
        || coordinate > maximum + tolerance) {
        throw std::logic_error(
            "Derived Wang quilt crop leaves the composite image.");
    }
    return std::clamp(coordinate, 0.0, maximum);
}

[[nodiscard]] std::size_t automaticResolution(const std::size_t stride) {
    const long double continuousIntervalCount =
        kSqrtTwo * static_cast<long double>(stride);
    const long double roundedIntervalCount =
        std::round(continuousIntervalCount);
    const long double maximumResolution = static_cast<long double>(
        std::numeric_limits<std::size_t>::max());
    if (!std::isfinite(continuousIntervalCount)
        || roundedIntervalCount < 1.0L
        || roundedIntervalCount >= maximumResolution) {
        throw std::length_error("Wang quilt output resolution overflows size_t.");
    }
    const std::size_t intervalCount =
        static_cast<std::size_t>(roundedIntervalCount);
    return intervalCount + 1;
}

} // namespace

WangQuiltGeometry::WangQuiltGeometry(
    const std::size_t patchSize,
    const std::size_t overlapPixels,
    const std::size_t outputResolutionPixels)
    : patchSize_(patchSize),
      overlapPixels_(overlapPixels) {
    if (patchSize_ < 4) {
        throw std::invalid_argument("Wang quilt patches need at least four pixels.");
    }
    if (overlapPixels_ == 0 || overlapPixels_ >= patchSize_) {
        throw std::invalid_argument(
            "Wang quilt overlap must be positive and smaller than the patch.");
    }
    stridePixels_ = patchSize_ - overlapPixels_;
    if (stridePixels_ < 2) {
        throw std::invalid_argument("Wang quilt stride needs at least two pixels.");
    }
    if (patchSize_ > std::numeric_limits<std::size_t>::max() - stridePixels_) {
        throw std::length_error("Wang quilt composite size overflows size_t.");
    }
    compositeSize_ = patchSize_ + stridePixels_;
    outputResolutionPixels_ = outputResolutionPixels == 0
        ? automaticResolution(stridePixels_)
        : outputResolutionPixels;
    if (outputResolutionPixels_ < 4) {
        throw std::invalid_argument(
            "Wang quilt output resolution needs at least four pixels.");
    }
}

std::size_t WangQuiltGeometry::patchSize() const noexcept {
    return patchSize_;
}

std::size_t WangQuiltGeometry::overlapPixels() const noexcept {
    return overlapPixels_;
}

std::size_t WangQuiltGeometry::stridePixels() const noexcept {
    return stridePixels_;
}

std::size_t WangQuiltGeometry::compositeSize() const noexcept {
    return compositeSize_;
}

std::size_t WangQuiltGeometry::outputResolutionPixels() const noexcept {
    return outputResolutionPixels_;
}

QuiltSamplePosition WangQuiltGeometry::compositePosition(
    const std::size_t outputX,
    const std::size_t outputY) const {
    if (outputX >= outputResolutionPixels_ || outputY >= outputResolutionPixels_) {
        throw std::out_of_range("Wang quilt output coordinate is outside the crop.");
    }
    const double intervals = static_cast<double>(outputResolutionPixels_ - 1);
    const double center = static_cast<double>(compositeSize_ - 1) * 0.5;
    const double sourceX = center
        + static_cast<double>(stridePixels_)
            * (static_cast<double>(outputX)
                + static_cast<double>(outputY)
                - intervals)
            / intervals;
    const double sourceY = center
        + static_cast<double>(stridePixels_)
            * (static_cast<double>(outputY) - static_cast<double>(outputX))
            / intervals;
    const double maximum = static_cast<double>(compositeSize_ - 1);
    return {
        clampDerivedCoordinate(sourceX, maximum),
        clampDerivedCoordinate(sourceY, maximum),
    };
}

QuiltSamplePosition WangQuiltGeometry::northSouthBoundaryPosition(
    const std::size_t edgeIndex) const {
    const QuiltSamplePosition position = compositePosition(edgeIndex, 0);
    return position;
}

QuiltSamplePosition WangQuiltGeometry::westEastBoundaryPosition(
    const std::size_t edgeIndex) const {
    const QuiltSamplePosition position = compositePosition(0, edgeIndex);
    return {
        position.x,
        position.y - static_cast<double>(stridePixels_),
    };
}

} // namespace qrp::atlas
