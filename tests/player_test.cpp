#include "jumpcastle/player.hpp"

#include "jumpcastle/game_config.hpp"
#include "test_world_factory.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("player steps against a polygon collision world and lands grounded") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 16;
    screen.height = 15;
    const std::vector<Vec2> floor{{0, 14}, {16, 14}, {16, 15}, {0, 15}};
    screen.colliders.push_back({
        .id = 1,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{floor},
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 15);

    PlayerState player;
    player.position = {8.0F, 8.0F};
    player.velocity = {};
    player.mode = PlayerMode::airborne;

    ResolveResult result{};
    for (int tick = 0; tick < 600; ++tick) {
        result = step_player(player, world, PlayerInput{}, config::fixed_delta);
    }

    REQUIRE(result.on_ground);
    REQUIRE(player.on_ground);
    REQUIRE(player.position.y + config::player_half_size.y == Approx(14.0F).margin(2e-2));
}

TEST_CASE("landing persists the selected support frame") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders.push_back({
        .id = 21,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{{{4,20},{12,16},{12,20}}},
        .tag = "slope",
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    PlayerState player{.position = {10.0F, 14.0F}, .velocity = {0.0F, 4.0F}};

    for (int tick = 0; tick < 240 && !player.on_ground; ++tick) {
        step_player(player, world, {}, config::fixed_delta);
    }

    REQUIRE(player.on_ground);
    CHECK(player.ground_collider_id == 21);
    CHECK(length(player.ground_normal) == Approx(1.0F));
    CHECK(player.ground_normal.x < 0.0F);
    CHECK(player.ground_normal.y <= -0.5F);
}

TEST_CASE("grounded player walking off a ledge loses support and falls") {
    // Regression: advance_before_resolution speculatively sets on_ground=true
    // for grounded players. The end-of-step transition must be driven by the
    // post-resolution verdict, not that stale flag, or a player that walks past
    // a platform edge hovers in mid-air forever instead of falling.
    ScreenMap screen;
    screen.index = 0;
    screen.width = 32;
    screen.height = 32;
    screen.colliders.push_back({
        .id = 7,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{{{0, 20}, {12, 20}, {12, 24}, {0, 24}}},
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 32);

    // Land the player on the platform.
    PlayerState player;
    player.position = {6.0F, 8.0F};
    player.velocity = {};
    player.mode = PlayerMode::airborne;
    for (int tick = 0; tick < 600 && !player.on_ground; ++tick) {
        step_player(player, world, PlayerInput{}, config::fixed_delta);
    }
    REQUIRE(player.on_ground);
    const float grounded_y = player.position.y;

    // Hold right and walk past the x=12 edge into the void.
    for (int tick = 0; tick < 600; ++tick) {
        step_player(player, world, PlayerInput{.right = true}, config::fixed_delta);
    }

    CHECK_FALSE(player.on_ground);
    CHECK(player.mode == PlayerMode::airborne);
    CHECK(player.position.y > grounded_y + 1.0F);
}

TEST_CASE("player defaults to a flat deterministic support frame") {
    const PlayerState player{};
    CHECK(player.ground_normal == Vec2{0.0F, -1.0F});
    CHECK(player.ground_collider_id == -1);
    CHECK(player.ground_piece_index == 0);
}

TEST_CASE("player state starts deterministically") {
    const PlayerState player{};

    CHECK(player.position.x == Approx(8.0F));
    CHECK(player.position.y == Approx(-6.0F));
    CHECK(player.velocity.x == 0.0F);
    CHECK(player.velocity.y == 0.0F);
    CHECK(player.jump_hold_time == 0.0F);
    CHECK(player.animation_time == 0.0F);
    CHECK_FALSE(player.on_ground);
    CHECK(player.facing_right);
}

TEST_CASE("ground input changes facing direction") {
    PlayerState player{};
    player.on_ground = true;

    simulate_ground_movement(player, PlayerInput{.left = true}, 0.1F);

    CHECK(player.velocity.x < 0.0F);
    CHECK_FALSE(player.facing_right);
}

TEST_CASE("long jump charge is clamped") {
    const Vec2 short_jump = charged_jump_velocity(0.1F, 0.0F);
    const Vec2 long_jump = charged_jump_velocity(10.0F, 0.0F);

    CHECK(long_jump.y < short_jump.y);
    CHECK(long_jump.y == Approx(-config::jump_strength));
}

TEST_CASE("charged jump follows horizontal input") {
    const Vec2 jump_left = charged_jump_velocity(0.5F, -1.0F);
    const Vec2 jump_right = charged_jump_velocity(0.5F, 1.0F);

    CHECK(jump_left.x < 0.0F);
    CHECK(jump_right.x > 0.0F);
    CHECK(jump_left.y == Approx(jump_right.y));
}

TEST_CASE("surface-relative jump preserves flat-ground behavior") {
    const Vec2 legacy = charged_jump_velocity(0.5F, 1.0F);
    const Vec2 framed = charged_jump_velocity(0.5F, 1.0F, {0.0F, -1.0F});
    CHECK(framed.x == Approx(legacy.x));
    CHECK(framed.y == Approx(legacy.y));
}

TEST_CASE("neutral jump follows slope normal") {
    const Vec2 normal = normalized(Vec2{-1.0F, -1.0F});
    const Vec2 velocity = charged_jump_velocity(0.5F, 0.0F, normal);
    CHECK(normalized(velocity).x == Approx(normal.x).margin(1e-5));
    CHECK(normalized(velocity).y == Approx(normal.y).margin(1e-5));
}

TEST_CASE("right input adds right-facing surface tangent") {
    const Vec2 normal = normalized(Vec2{0.6F, -0.8F});
    const Vec2 tangent = surface_tangent_right(normal);
    const Vec2 neutral = charged_jump_velocity(0.5F, 0.0F, normal);
    const Vec2 right = charged_jump_velocity(0.5F, 1.0F, normal);
    CHECK(dot(right - neutral, tangent) > 0.0F);
}

TEST_CASE("ground movement follows the support tangent") {
    PlayerState player{};
    player.mode = PlayerMode::grounded;
    player.on_ground = true;
    player.ground_normal = normalized(Vec2{-1.0F, -1.0F});
    simulate_ground_movement(player, {.right = true}, 0.1F);
    CHECK(dot(player.velocity, surface_tangent_right(player.ground_normal)) > 0.0F);
    CHECK(std::abs(dot(player.velocity, player.ground_normal)) < 1e-5F);
}

TEST_CASE("simulation caps player speed") {
    PlayerState player{};
    player.velocity = {100.0F, 100.0F};

    integrate_player(player, 1.0F / 60.0F);

    CHECK(std::hypot(player.velocity.x, player.velocity.y) == Approx(config::maximum_speed));
}

TEST_CASE("position integration is stable across equivalent frame splits") {
    PlayerState one_step{};
    PlayerState two_steps{};
    one_step.velocity = {3.0F, -4.0F};
    two_steps.velocity = one_step.velocity;

    integrate_player(one_step, 1.0F / 30.0F);
    integrate_player(two_steps, 1.0F / 60.0F);
    integrate_player(two_steps, 1.0F / 60.0F);

    CHECK(one_step.position.x == Approx(two_steps.position.x));
    CHECK(one_step.position.y == Approx(two_steps.position.y));
}
