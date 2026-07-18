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

// A tall solid column spanning x=[10,11], y=[8,14]; its left face has outward
// normal (-1, 0), so a player approaching from the left strikes a vertical wall.
ScreenMap wall_screen() {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.polygons.push_back(make_polygon({{10, 8}, {11, 8}, {11, 14}, {10, 14}}, ColliderType::solid));
    return s;
}

// A solid ceiling slab spanning x=[4,12], y=[8,9]; its bottom face has outward
// normal (0, 1), so a player rising into it bonks a horizontal surface.
ScreenMap ceiling_screen() {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.polygons.push_back(make_polygon({{4, 8}, {12, 8}, {12, 9}, {4, 9}}, ColliderType::solid));
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

TEST_CASE("airborne player rebounds off a vertical wall like Jump King") {
    const CollisionWorld world = CollisionWorld::from_screens({wall_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::airborne;
    player.position = {10.1F, 12.2F};
    player.velocity = {5.0F, 3.0F};
    static_cast<void>(world.resolve({9.4F, 12.0F}, player));

    // Horizontal velocity reverses and keeps 0.8x its magnitude; the downward
    // fall (y) is untouched, so the player loses horizontal control mid-air.
    REQUIRE(player.velocity.x ==
            Approx(-5.0F * config::wall_bounce_restitution(5.0F)).margin(1e-3));
    REQUIRE(player.velocity.y == Approx(3.0F).margin(1e-3));
    REQUIRE(player.position.x + config::player_half_size.x == Approx(10.0F).margin(1e-2));
}

TEST_CASE("landing on a floor does not bounce the player back up") {
    const CollisionWorld world = CollisionWorld::from_screens({floor_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::airborne;
    player.position = {8.0F, 13.9F};
    player.velocity = {0.0F, 10.0F};
    const ResolveResult result = world.resolve({8.0F, 13.0F}, player);

    REQUIRE(result.on_ground);
    REQUIRE(player.velocity.y == Approx(0.0F).margin(1e-3));  // absorbed, not reversed
}

TEST_CASE("bonking a ceiling absorbs upward velocity without a wall rebound") {
    const CollisionWorld world = CollisionWorld::from_screens({ceiling_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::airborne;
    player.position = {8.0F, 9.3F};
    player.velocity = {2.0F, -8.0F};
    static_cast<void>(world.resolve({8.0F, 9.9F}, player));

    // Upward velocity is killed (slide), horizontal is carried through unchanged
    // -- a ceiling is not a wall, so no horizontal reversal.
    REQUIRE(player.velocity.y == Approx(0.0F).margin(1e-3));
    REQUIRE(player.velocity.x == Approx(2.0F).margin(1e-3));
}

TEST_CASE("grounded player does not bounce off a wall it walks into") {
    const CollisionWorld world = CollisionWorld::from_screens({wall_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::grounded;
    player.position = {10.1F, 12.2F};
    player.velocity = {5.0F, 0.0F};
    static_cast<void>(world.resolve({9.4F, 12.2F}, player));

    // Grounded contact slides to a stop instead of rebounding.
    REQUIRE(player.velocity.x == Approx(0.0F).margin(1e-3));
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

TEST_CASE("wall_bounce_restitution scales with impact speed and caps at 1.0") {
    using config::wall_bounce_restitution;

    // Clamps to the minimum at/below the low anchor.
    REQUIRE(wall_bounce_restitution(0.0F) == Approx(config::wall_bounce_min));
    REQUIRE(wall_bounce_restitution(config::wall_bounce_impact_lo) ==
            Approx(config::wall_bounce_min));

    // Clamps to the maximum at/above the high anchor, never exceeding 1.0.
    REQUIRE(wall_bounce_restitution(config::wall_bounce_impact_hi) ==
            Approx(config::wall_bounce_max));
    REQUIRE(wall_bounce_restitution(100.0F) == Approx(config::wall_bounce_max));
    REQUIRE(wall_bounce_restitution(100.0F) <= 1.0F);

    // Monotonic ramp strictly between the anchors.
    const float mid = wall_bounce_restitution(
        0.5F * (config::wall_bounce_impact_lo + config::wall_bounce_impact_hi));
    REQUIRE(mid > config::wall_bounce_min);
    REQUIRE(mid < config::wall_bounce_max);
}
