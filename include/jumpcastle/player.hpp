#pragma once

#include "jumpcastle/collision.hpp"

namespace jumpcastle {

struct PlayerInput {
    bool left{};
    bool right{};
    bool left_pressed{};
    bool right_pressed{};
    bool jump_down{};
    bool jump_released{};
};

struct PlayerState {
    Vector2 position{starting_player_position()};
    Vector2 velocity{};
    float jump_hold_time{};
    float animation_time{};
    bool on_ground{};
    bool facing_right{true};
};

[[nodiscard]] Vector2 charged_jump_velocity(
    float hold_time,
    float horizontal_input) noexcept;

void simulate_ground_movement(
    PlayerState& player,
    PlayerInput input,
    float delta) noexcept;

void integrate_player(PlayerState& player, float delta) noexcept;

void update_player(
    PlayerState& player,
    const Tilemap& tilemap,
    float tilemap_offset_y,
    PlayerInput input,
    float delta) noexcept;

}  // namespace jumpcastle
