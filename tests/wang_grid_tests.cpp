#include "model/WangGrid.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDeterminismAndAdjacency() {
    const qrp::model::WangGrid first(8, 6, 5, 0x123456789abcdef0ULL);
    const qrp::model::WangGrid same(8, 6, 5, 0x123456789abcdef0ULL);
    const qrp::model::WangGrid different(8, 6, 5, 0x123456789abcdef1ULL);

    require(first.hasValidAdjacency(), "generated Wang grid must have valid adjacency");
    require(first.tiles() == same.tiles(), "equal seeds must reproduce the same grid");
    require(first.tiles() != different.tiles(), "different seeds should change the grid");
    require(first.width() == 8 && first.height() == 6, "grid dimensions must be retained");
    require(first.colorCount() == 5, "grid color count must be retained");
    require(first.seed() == 0x123456789abcdef0ULL, "grid seed must be retained");

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
    try {
        testDeterminismAndAdjacency();
        std::cout << "1/1 Wang grid tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] Wang grid determinism and adjacency: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
