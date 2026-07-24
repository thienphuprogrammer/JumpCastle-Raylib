#include "jumpcastle/map_validation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace jumpcastle;

TEST_CASE("validator rejects duplicate ids and out-of-bounds geometry") {
    ScreenMap map;
    map.index = 0;
    map.width = 28;
    map.height = 36;
    map.colliders = {
        {.id = 3, .geometry = CircleGeometry{{27.5F, 10.0F}, 1.0F}},
        {.id = 3, .geometry = PolygonGeometry{{{2, 2}, {4, 2}, {4, 4}}}},
    };
    CHECK_THROWS_WITH(
        validate_screen_map(map, {.atlas_columns = 24, .atlas_rows = 8}),
        Catch::Matchers::ContainsSubstring("duplicate collider id 3"));
}

TEST_CASE("validator rejects a GID outside the atlas") {
    ScreenMap map;
    map.width = 1;
    map.height = 1;
    map.tiles.terrain = {.columns = 1, .rows = 1, .gids = {193}};
    CHECK_THROWS_WITH(
        validate_screen_map(map, {.atlas_columns = 24, .atlas_rows = 8}),
        Catch::Matchers::ContainsSubstring("terrain GID 193 exceeds atlas tile count 192"));
}
