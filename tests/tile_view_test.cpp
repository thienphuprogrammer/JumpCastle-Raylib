#include "jumpcastle/tile_view.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("tile_source_cell maps a plain GID to an atlas cell", "[tile_view]") {
    // firstgid==1, 21-column atlas, 16px tiles.
    const TileCell a = tile_source_cell(1u, 21, 16);   // local 0 -> col0,row0
    CHECK(a.src_x == 0);
    CHECK(a.src_y == 0);
    CHECK_FALSE(a.flip_h);
    CHECK_FALSE(a.flip_v);

    const TileCell b = tile_source_cell(2u, 21, 16);   // local 1 -> col1,row0
    CHECK(b.src_x == 16);
    CHECK(b.src_y == 0);

    const TileCell c = tile_source_cell(22u, 21, 16);  // local 21 -> col0,row1
    CHECK(c.src_x == 0);
    CHECK(c.src_y == 16);
}

TEST_CASE("tile_source_cell strips and reports flip bits", "[tile_view]") {
    const TileCell h = tile_source_cell(0x80000000u | 1u, 21, 16);
    CHECK(h.flip_h);
    CHECK_FALSE(h.flip_v);
    CHECK(h.src_x == 0);   // flip bits do not shift the source cell
    CHECK(h.src_y == 0);

    const TileCell v = tile_source_cell(0x40000000u | 22u, 21, 16);
    CHECK(v.flip_v);
    CHECK_FALSE(v.flip_h);
    CHECK(v.src_x == 0);
    CHECK(v.src_y == 16);
}
