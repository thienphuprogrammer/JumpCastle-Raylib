#include "jumpcastle/collision_world.hpp"

#include "jumpcastle/convex.hpp"
#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

namespace {

ConvexPolygon make_polygon(std::vector<Vec2> pts, const ColliderType type) {
    return {pts, outward_edge_normals(pts), polygon_aabb(pts), type};
}

ScreenMap floor_screen() {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.polygons.push_back(make_polygon({{0, 14}, {16, 14}, {16, 15}, {0, 15}}, ColliderType::solid));
    return s;
}

}  // namespace

TEST_CASE("falling player lands on the floor and is grounded") {
    const CollisionWorld world = CollisionWorld::from_screens({floor_screen()}, 15);
    PlayerState player;
    const Vec2 prev{8.0F, 13.0F};
    player.position = {8.0F, 13.9F};
    player.velocity = {0.0F, 10.0F};
    const ResolveResult result = world.resolve(prev, player);
    REQUIRE(result.on_ground);
    REQUIRE(player.velocity.y == Approx(0.0F).margin(1e-3));
    REQUIRE(player.position.y + config::player_half_size.y == Approx(14.0F).margin(1e-2));
}

TEST_CASE("hazard overlap is reported without positional resolve") {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.polygons.push_back(make_polygon({{7, 13}, {9, 13}, {8, 12}}, ColliderType::hazard));
    const CollisionWorld world = CollisionWorld::from_screens({s}, 15);
    PlayerState player;
    player.position = {8.0F, 12.6F};
    const ResolveResult result = world.resolve({8.0F, 12.0F}, player);
    REQUIRE(result.hit_hazard);
}

TEST_CASE("one-way platform blocks a fall from above but not a rise from below") {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.polygons.push_back(make_polygon({{4, 10}, {12, 10}, {12, 10.4F}, {4, 10.4F}}, ColliderType::oneway));
    const CollisionWorld world = CollisionWorld::from_screens({s}, 15);

    SECTION("falling from above lands") {
        PlayerState player;
        player.position = {8.0F, 9.7F};
        player.velocity = {0.0F, 8.0F};
        const ResolveResult result = world.resolve({8.0F, 9.2F}, player);
        REQUIRE(result.on_ground);
    }

    SECTION("rising from below passes through") {
        PlayerState player;
        player.position = {8.0F, 10.2F};
        player.velocity = {0.0F, -8.0F};
        const ResolveResult result = world.resolve({8.0F, 10.8F}, player);
        REQUIRE_FALSE(result.on_ground);
    }
}
