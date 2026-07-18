#include "jumpcastle/player.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using namespace jumpcastle;

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
