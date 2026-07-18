#include "jumpcastle/collision.hpp"

#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player.hpp"
#include "test_world_factory.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("overlapped tiles include both crossed boundaries") {
    const TileRange range = overlapped_tiles({2.5F, 3.5F}, {0.6F, 0.6F});

    CHECK(range.start_x == 1);
    CHECK(range.end_x == 3);
    CHECK(range.start_y == 2);
    CHECK(range.end_y == 4);
}

TEST_CASE("swept world collision cannot tunnel through one tile ceiling") {
    const WorldMap world = test::world_with_ceiling();
    PlayerState player{
        .position = {4.5F, 3.6F},
        .velocity = {0.0F, -600.0F},
        .mode = PlayerMode::airborne,
    };

    resolve_world_collision(world, {4.5F, 8.6F}, player);

    CHECK(player.position.y == Catch::Approx(6.0F + config::player_half_size.y));
    CHECK(player.velocity.y == 0.0F);
}
