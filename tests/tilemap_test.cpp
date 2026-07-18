#include "jumpcastle/tilemap.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("tilemap applies gameplay boundary policies") {
    const auto& map = screen(starting_screen_index);

    CHECK(map.tile_at(0, 0) == Tile::solid);
    CHECK(map.tile_at(-1, 0) == Tile::solid);
    CHECK(map.tile_at(config::tilemap_width, 0) == Tile::solid);
    CHECK(map.tile_at(0, -1) == Tile::empty);
    CHECK(map.tile_at(0, config::tilemap_height) == Tile::empty);
    CHECK(map.solid_for_render_at(-1, 0));
    CHECK(map.solid_for_render_at(0, -1));
}

TEST_CASE("initial position selects the documented starting screen") {
    const Vec2 start = starting_player_position();
    const ScreenSelection selected = select_screen(start.y);

    CHECK(selected.index == starting_screen_index);
    CHECK(selected.tilemap == &screen(starting_screen_index));
    CHECK(selected.vertical_offset == Approx(-12.0F));
}

TEST_CASE("climbing selects the next screen") {
    const ScreenSelection selected = select_screen(-18.0F);

    CHECK(selected.index == starting_screen_index - 1);
    CHECK(selected.vertical_offset == Approx(-24.0F));
}

TEST_CASE("screen selection never returns an invalid index") {
    CHECK(select_screen(10000.0F).index == invalid_screen_index);
    CHECK(select_screen(-10000.0F).index == invalid_screen_index);
}
