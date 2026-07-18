#include "jumpcastle/collision.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

namespace {

Tilemap::Grid empty_grid() {
    Tilemap::Grid grid{};
    for (auto& row : grid) {
        row.fill(Tile::empty);
    }
    return grid;
}

Tilemap test_map_with_floor_at(const int y) {
    auto grid = empty_grid();
    grid.at(static_cast<std::size_t>(y)).fill(Tile::solid);
    return Tilemap{grid};
}

Tilemap test_map_with_wall_at(const int x) {
    auto grid = empty_grid();
    for (auto& row : grid) {
        row.at(static_cast<std::size_t>(x)) = Tile::solid;
    }
    return Tilemap{grid};
}

}  // namespace

TEST_CASE("overlapped tiles include both crossed boundaries") {
    const TileRange range = overlapped_tiles({2.5F, 3.5F}, {0.6F, 0.6F});

    CHECK(range.start_x == 1);
    CHECK(range.end_x == 3);
    CHECK(range.start_y == 2);
    CHECK(range.end_y == 4);
}

TEST_CASE("landing clips the player to the floor and stops downward velocity") {
    const Tilemap map = test_map_with_floor_at(6);
    Vec2 center{4.5F, 5.8F};
    Vec2 velocity{0.0F, 10.0F};

    resolve_tilemap_collision(map, 0.0F, center, velocity, config::player_half_size);

    CHECK(center.y == Approx(5.6F));
    CHECK(velocity.y == Approx(0.0F));
}

TEST_CASE("wall collision reflects horizontal velocity") {
    const Tilemap map = test_map_with_wall_at(6);
    Vec2 center{5.8F, 4.5F};
    Vec2 velocity{10.0F, 0.0F};

    resolve_tilemap_collision(map, 0.0F, center, velocity, config::player_half_size);

    CHECK(center.x == Approx(5.7F));
    CHECK(velocity.x == Approx(-4.5F));
}

TEST_CASE("collision query distinguishes solid and empty regions") {
    const Tilemap map = test_map_with_floor_at(6);

    CHECK(collides_with_tilemap(map, 0.0F, {4.5F, 5.8F}, config::player_half_size));
    CHECK_FALSE(collides_with_tilemap(map, 0.0F, {4.5F, 2.0F}, config::player_half_size));
}
