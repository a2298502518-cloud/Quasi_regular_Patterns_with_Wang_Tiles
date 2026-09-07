#include "math/CoonsWarp.hpp"
#include "math/EdgeFunction.hpp"
#include "model/EdgePalette.hpp"
#include "model/WangGrid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using qrp::math::CoonsEdges;
using qrp::math::CoonsWarp;
using qrp::math::EdgeFunction;
using qrp::math::EdgeParameters;
using qrp::math::Vec2;

struct TestCase {
    std::string_view name;
    void (*function)();
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(
    const double actual,
    const double expected,
    const double tolerance,
    const std::string& label) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            label + ": expected " + std::to_string(expected)
            + ", got " + std::to_string(actual));
    }
}

[[nodiscard]] CoonsWarp representativeWarp() {
    return CoonsWarp(CoonsEdges{
        EdgeFunction(EdgeParameters{0.035, 0.0}),
        EdgeFunction(EdgeParameters{-0.02, 0.01}),
        EdgeFunction(EdgeParameters{0.0, 0.025}),
        EdgeFunction(EdgeParameters{0.015, -0.02}),
    });
}

void testEdgeEndpointsAndDerivative() {
    const EdgeFunction edge(EdgeParameters{0.04, -0.015});
    requireNear(edge.evaluate(0.0), 0.0, 1.0e-15, "edge start");
    requireNear(edge.evaluate(1.0), 1.0, 1.0e-15, "edge end");
    require(edge.hasPositiveDerivative(0.5), "safe edge should have derivative margin");

    constexpr double step = 1.0e-6;
    for (int index = 1; index < 100; ++index) {
        const double parameter = static_cast<double>(index) / 100.0;
        const double finiteDifference = (
            edge.evaluate(parameter + step) - edge.evaluate(parameter - step))
            / (2.0 * step);
        requireNear(
            edge.derivative(parameter),
            finiteDifference,
            2.0e-9,
            "edge derivative");
    }

    const EdgeFunction unsafe(EdgeParameters{0.4, 0.0});
    require(!unsafe.hasPositiveDerivative(), "unsafe edge must be rejected");
}

void testScalarInverse() {
    const EdgeFunction edge(EdgeParameters{-0.035, 0.02});
    for (int index = 0; index <= 100; ++index) {
        const double expected = static_cast<double>(index) / 100.0;
        const double value = edge.evaluate(expected);
        const auto result = edge.inverse(value);
        require(result.converged, "scalar inverse should converge");
        requireNear(result.parameter, expected, 2.0e-12, "scalar inverse parameter");
        require(result.residual <= 1.0e-12, "scalar inverse residual should be small");
    }

    const auto outside = edge.inverse(1.01);
    require(!outside.converged, "out-of-domain scalar inverse must fail");
    require(
        outside.failure == qrp::math::ScalarInverseFailure::TargetOutsideDomain,
        "scalar inverse should report an out-of-domain target");
}

void testIdentityWarpAndBoundaries() {
    const EdgeFunction identity;
    const CoonsWarp warp(CoonsEdges{identity, identity, identity, identity});
    const auto safety = warp.validateSafety();
    require(safety.valid, "identity warp must be safe");
    requireNear(safety.determinantLowerBound, 1.0, 1.0e-15, "identity det bound");

    for (int y = 0; y <= 10; ++y) {
        for (int x = 0; x <= 10; ++x) {
            const Vec2 parameter{
                static_cast<double>(x) / 10.0,
                static_cast<double>(y) / 10.0,
            };
            const Vec2 mapped = warp.map(parameter);
            requireNear(mapped.x, parameter.x, 1.0e-15, "identity x");
            requireNear(mapped.y, parameter.y, 1.0e-15, "identity y");
            requireNear(warp.determinant(parameter), 1.0, 1.0e-15, "identity det");
        }
    }

    const CoonsWarp deformed = representativeWarp();
    const auto& edges = deformed.edges();
    for (int index = 0; index <= 100; ++index) {
        const double t = static_cast<double>(index) / 100.0;
        const Vec2 south = deformed.map(Vec2{t, 0.0});
        const Vec2 north = deformed.map(Vec2{t, 1.0});
        const Vec2 west = deformed.map(Vec2{0.0, t});
        const Vec2 east = deformed.map(Vec2{1.0, t});
        requireNear(south.x, edges.south.evaluate(t), 1.0e-15, "south edge");
        requireNear(south.y, 0.0, 1.0e-15, "south y");
        requireNear(north.x, edges.north.evaluate(t), 1.0e-15, "north edge");
        requireNear(north.y, 1.0, 1.0e-15, "north y");
        requireNear(west.x, 0.0, 1.0e-15, "west x");
        requireNear(west.y, edges.west.evaluate(t), 1.0e-15, "west edge");
        requireNear(east.x, 1.0, 1.0e-15, "east x");
        requireNear(east.y, edges.east.evaluate(t), 1.0e-15, "east edge");
    }
}

void testJacobianAgainstFiniteDifference() {
    const CoonsWarp warp = representativeWarp();
    constexpr double step = 1.0e-6;
    for (int y = 1; y < 10; ++y) {
        for (int x = 1; x < 10; ++x) {
            const Vec2 p{
                static_cast<double>(x) / 10.0,
                static_cast<double>(y) / 10.0,
            };
            const auto matrix = warp.jacobian(p);
            const Vec2 x0 = warp.map(Vec2{p.x - step, p.y});
            const Vec2 x1 = warp.map(Vec2{p.x + step, p.y});
            const Vec2 y0 = warp.map(Vec2{p.x, p.y - step});
            const Vec2 y1 = warp.map(Vec2{p.x, p.y + step});
            requireNear(matrix.dXdu, (x1.x - x0.x) / (2.0 * step), 2.0e-9, "dXdu");
            requireNear(matrix.dYdu, (x1.y - x0.y) / (2.0 * step), 2.0e-9, "dYdu");
            requireNear(matrix.dXdv, (y1.x - y0.x) / (2.0 * step), 2.0e-9, "dXdv");
            requireNear(matrix.dYdv, (y1.y - y0.y) / (2.0 * step), 2.0e-9, "dYdv");
        }
    }
}

void testWarpSafety() {
    const CoonsWarp safe = representativeWarp();
    const auto safeReport = safe.validateSafety();
    require(safeReport.valid, "representative warp must satisfy safety bounds");
    require(
        safe.sampledMinimumDeterminant(129) >= safeReport.determinantLowerBound - 1.0e-12,
        "sampled determinant must respect the analytic lower bound");

    const EdgeFunction positive(EdgeParameters{0.3, 0.0});
    const EdgeFunction negative(EdgeParameters{-0.3, 0.0});
    const CoonsWarp unsafe(CoonsEdges{positive, negative, positive, negative});
    const auto unsafeReport = unsafe.validateSafety();
    require(!unsafeReport.valid, "cross-coupled extreme warp must be rejected");
    require(
        unsafeReport.determinantLowerBound < 0.0,
        "unsafe example should expose a negative conservative bound");
}

void testWarpInverse() {
    const CoonsWarp warp = representativeWarp();
    require(warp.validateSafety().valid, "inverse test requires a safe warp");

    double maximumResidual = 0.0;
    std::uint32_t maximumIterations = 0;
    bool observedBoundarySolve = false;
    for (int y = 0; y <= 32; ++y) {
        for (int x = 0; x <= 32; ++x) {
            const Vec2 expected{
                static_cast<double>(x) / 32.0,
                static_cast<double>(y) / 32.0,
            };
            const Vec2 physical = warp.map(expected);
            const auto result = warp.inverse(physical);
            require(result.converged, "2D inverse should converge on mapped points");
            requireNear(result.parameter.x, expected.x, 2.0e-11, "inverse u");
            requireNear(result.parameter.y, expected.y, 2.0e-11, "inverse v");
            maximumResidual = std::max(maximumResidual, result.residualNorm);
            maximumIterations = std::max(maximumIterations, result.iterations);
            observedBoundarySolve = observedBoundarySolve || result.boundarySolveUsed;
        }
    }
    require(maximumResidual <= 1.0e-11, "maximum inverse residual exceeds target");
    require(maximumIterations <= 8, "representative inverse should converge quickly");
    require(observedBoundarySolve, "grid test must exercise exact boundary inversion");

    const auto outside = warp.inverse(Vec2{-0.1, 0.5});
    require(!outside.converged, "out-of-domain warp inverse must fail");
    require(
        outside.failure == qrp::math::InverseWarpFailure::TargetOutsideDomain,
        "warp inverse should report an out-of-domain target");
}

void testSharedBoundaryInverse() {
    const EdgeFunction shared(EdgeParameters{0.025, -0.015});
    const CoonsWarp lower(CoonsEdges{
        EdgeFunction(EdgeParameters{0.035, 0.0}),
        shared,
        EdgeFunction(EdgeParameters{0.0, 0.025}),
        EdgeFunction(EdgeParameters{-0.02, 0.01}),
    });
    const CoonsWarp upper(CoonsEdges{
        shared,
        EdgeFunction(EdgeParameters{-0.035, 0.0}),
        EdgeFunction(EdgeParameters{0.015, -0.02}),
        EdgeFunction(EdgeParameters{0.0, -0.025}),
    });
    require(lower.validateSafety().valid, "lower shared-edge warp must be safe");
    require(upper.validateSafety().valid, "upper shared-edge warp must be safe");

    for (int index = 0; index <= 200; ++index) {
        const double physicalX = static_cast<double>(index) / 200.0;
        const auto lowerResult = lower.inverse(Vec2{physicalX, 1.0});
        const auto upperResult = upper.inverse(Vec2{physicalX, 0.0});
        require(lowerResult.converged, "lower shared edge inverse must converge");
        require(upperResult.converged, "upper shared edge inverse must converge");
        require(lowerResult.boundarySolveUsed, "lower edge must use scalar inversion");
        require(upperResult.boundarySolveUsed, "upper edge must use scalar inversion");
        requireNear(
            lowerResult.parameter.x,
            upperResult.parameter.x,
            1.0e-13,
            "shared edge parameter");
        requireNear(lowerResult.parameter.y, 1.0, 1.0e-15, "lower edge v");
        requireNear(upperResult.parameter.y, 0.0, 1.0e-15, "upper edge v");
    }
}

void testDefaultPalette() {
    const auto palette = qrp::model::EdgePalette::createDefault();
    require(palette.colors().size() == 5, "default palette must contain five colors");
    const auto report = palette.validateAllCombinations();
    require(report.valid, "all default palette combinations must be safe");
    require(report.combinationCount == 625, "five colors must produce 625 combinations");
    require(
        report.minimumDeterminantLowerBound > 0.4,
        "default palette should retain a generous determinant margin");
}

void testDefaultPaletteInverseStress() {
    const auto palette = qrp::model::EdgePalette::createDefault();
    const auto& colors = palette.colors();
    std::size_t testedCombinations = 0;

    for (const auto& south : colors) {
        for (const auto& north : colors) {
            for (const auto& west : colors) {
                for (const auto& east : colors) {
                    const CoonsWarp warp(CoonsEdges{south, north, west, east});
                    const auto safety = warp.validateSafety();
                    require(safety.valid, "default palette stress warp must be safe");
                    require(
                        warp.sampledMinimumDeterminant(17)
                            >= safety.determinantLowerBound - 1.0e-12,
                        "stress warp determinant must respect the analytic bound");

                    for (int y = 0; y <= 6; ++y) {
                        for (int x = 0; x <= 6; ++x) {
                            const Vec2 expected{
                                static_cast<double>(x) / 6.0,
                                static_cast<double>(y) / 6.0,
                            };
                            const auto result = warp.inverse(warp.map(expected));
                            require(result.converged, "stress warp inverse must converge");
                            requireNear(result.parameter.x, expected.x, 2.0e-11, "stress inverse u");
                            requireNear(result.parameter.y, expected.y, 2.0e-11, "stress inverse v");
                        }
                    }
                    ++testedCombinations;
                }
            }
        }
    }

    require(testedCombinations == 625, "stress test must cover all edge combinations");
}

void testWangGridDeterminismAndAdjacency() {
    const qrp::model::WangGrid first(8, 6, 5, 0x123456789abcdef0ULL);
    const qrp::model::WangGrid second(8, 6, 5, 0x123456789abcdef0ULL);
    const qrp::model::WangGrid different(8, 6, 5, 0x123456789abcdef1ULL);
    require(first.hasValidAdjacency(), "generated Wang grid must have valid adjacency");
    require(first.tiles() == second.tiles(), "equal seeds must reproduce the same grid");
    require(first.tiles() != different.tiles(), "different seeds should change the grid");
    require(first.width() == 8 && first.height() == 6, "grid dimensions must be retained");
    require(first.colorCount() == 5, "grid color count must be retained");

    const qrp::model::WangGrid row(17, 1, 3, 7);
    const qrp::model::WangGrid column(1, 17, 3, 7);
    const qrp::model::WangGrid single(1, 1, 1, 7);
    require(row.hasValidAdjacency(), "one-row grid must be valid");
    require(column.hasValidAdjacency(), "one-column grid must be valid");
    require(single.hasValidAdjacency(), "single-tile grid must be valid");

    bool rejectedEmptyGrid = false;
    try {
        [[maybe_unused]] const qrp::model::WangGrid invalid(0, 1, 5, 0);
    } catch (const std::invalid_argument&) {
        rejectedEmptyGrid = true;
    }
    require(rejectedEmptyGrid, "empty grid dimensions must be rejected");
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"edge endpoints and derivative", testEdgeEndpointsAndDerivative},
        {"scalar inverse", testScalarInverse},
        {"identity warp and boundaries", testIdentityWarpAndBoundaries},
        {"Jacobian finite difference", testJacobianAgainstFiniteDifference},
        {"warp safety", testWarpSafety},
        {"warp inverse", testWarpInverse},
        {"shared boundary inverse", testSharedBoundaryInverse},
        {"default palette", testDefaultPalette},
        {"default palette inverse stress", testDefaultPaletteInverseStress},
        {"Wang grid determinism and adjacency", testWangGridDeterminismAndAdjacency},
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
