#include "jumpcastle/collision.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("overlapped tiles include both crossed boundaries") {
    const TileRange range = overlapped_tiles({2.5F, 3.5F}, {0.6F, 0.6F});

    CHECK(range.start_x == 1);
    CHECK(range.end_x == 3);
    CHECK(range.start_y == 2);
    CHECK(range.end_y == 4);
}
