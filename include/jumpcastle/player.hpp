#pragma once

#include "jumpcastle/collision.hpp"
#include "jumpcastle/collision_world.hpp"

namespace jumpcastle {

enum class PlayerMode {
    airborne,
    grounded,
    charging,
};

struct PlayerInput {
    bool left{};
    bool right{};
    bool left_pressed{};
    bool right_pressed{};
    bool jump_down{};
    bool jump_released{};
};

struct PlayerState {
    Vec2 position{8.0F, -6.0F};
    Vec2 velocity{};
    float jump_hold_time{};
    float animation_time{};
    PlayerMode mode{PlayerMode::airborne};
    bool on_ground{};
    bool facing_right{true};
    Vec2 ground_normal{0.0F, -1.0F};
    Vec2 ground_point{};
    int ground_collider_id{-1};
    int ground_piece_index{};
};

[[nodiscard]] Vec2 charged_jump_velocity(
    float hold_time,
    float horizontal_input) noexcept;

void simulate_ground_movement(
    PlayerState& player,
    PlayerInput input,
    float delta) noexcept;

void integrate_player(PlayerState& player, float delta) noexcept;

[[nodiscard]] ResolveResult step_player(
    PlayerState& player,
    const CollisionWorld& world,
    PlayerInput input,
    float fixed_delta) noexcept;

}  // namespace jumpcastle
