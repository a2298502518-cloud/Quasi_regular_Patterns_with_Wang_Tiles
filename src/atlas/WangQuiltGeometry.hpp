#pragma once

#include <cstddef>

namespace qrp::atlas {

struct QuiltSamplePosition {
    double x = 0.0;
    double y = 0.0;
};

class WangQuiltGeometry final {
public:
    WangQuiltGeometry(
        std::size_t patchSize,
        std::size_t overlapPixels,
        std::size_t outputResolutionPixels = 0);

    [[nodiscard]] std::size_t patchSize() const noexcept;
    [[nodiscard]] std::size_t overlapPixels() const noexcept;
    [[nodiscard]] std::size_t stridePixels() const noexcept;
    [[nodiscard]] std::size_t compositeSize() const noexcept;
    [[nodiscard]] std::size_t outputResolutionPixels() const noexcept;

    [[nodiscard]] QuiltSamplePosition compositePosition(
        std::size_t outputX,
        std::size_t outputY) const;
    [[nodiscard]] QuiltSamplePosition northSouthBoundaryPosition(
        std::size_t edgeIndex) const;
    [[nodiscard]] QuiltSamplePosition westEastBoundaryPosition(
        std::size_t edgeIndex) const;

private:
    std::size_t patchSize_ = 0;
    std::size_t overlapPixels_ = 0;
    std::size_t stridePixels_ = 0;
    std::size_t compositeSize_ = 0;
    std::size_t outputResolutionPixels_ = 0;
};

} // namespace qrp::atlas
