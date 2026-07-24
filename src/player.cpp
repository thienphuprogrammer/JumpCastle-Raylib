#include "jumpcastle/player.hpp"

#include "jumpcastle/game_config.hpp"

#include <algorithm>
#include <cmath>

namespace jumpcastle {

Vec2 charged_jump_velocity(
    const float hold_time,
    const float horizontal_input) noexcept {
    const float raw_charge = std::clamp(
        (hold_time - config::minimum_charge_seconds) /
            (config::maximum_charge_seconds - config::minimum_charge_seconds),
        0.0F,
        1.0F);
    const float charge = raw_charge * raw_charge * (3.0F - 2.0F * raw_charge);
    const float direction = std::clamp(horizontal_input, -1.0F, 1.0F);
    return {
        direction * (4.5F + charge * 3.5F),
        -config::jump_strength * (0.55F + charge * 0.45F),
    };
}

void simulate_ground_movement(
    PlayerState& player,
    const PlayerInput input,
    const float delta) noexcept {
    if (input.jump_released) {
        const float horizontal_input =
            (input.right ? 1.0F : 0.0F) - (input.left ? 1.0F : 0.0F);
        player.velocity = charged_jump_velocity(player.jump_hold_time, horizontal_input);
    }

    if (input.jump_down) {
        player.jump_hold_time = std::min(
            player.jump_hold_time + delta,
            config::maximum_charge_seconds);
        player.mode = PlayerMode::charging;
        return;
    }

    player.jump_hold_time = 0.0F;

    if (input.right) {
        player.velocity.x += config::movement_acceleration * delta;
        player.facing_right = true;
    }
    if (input.left) {
        player.velocity.x -= config::movement_acceleration * delta;
        player.facing_right = false;
    }
    if (input.left_pressed || input.right_pressed) {
        player.animation_time = 0.0F;
    }
}

void integrate_player(PlayerState& player, const float delta) noexcept {
    const float speed = length(player.velocity);
    if (speed > config::maximum_speed) {
        player.velocity = normalized(player.velocity) * config::maximum_speed;
    }

    const Vec2 displacement = player.velocity * delta;
    player.position.x += displacement.x;
    player.position.y += displacement.y;
}

namespace {

// Records the deterministically selected supporting contact so downstream
// systems (surface-normal movement, solver replay) read a stable support frame.
void apply_ground_contact(PlayerState& player, const Contact& contact) noexcept {
    player.on_ground = true;
    player.ground_normal = contact.normal;
    player.ground_point = contact.point;
    player.ground_collider_id = contact.collider_id;
    player.ground_piece_index = contact.piece_index;
}

// Restores the flat default support frame when the player leaves the ground.
void clear_ground_contact(PlayerState& player) noexcept {
    player.on_ground = false;
    player.ground_normal = {0.0F, -1.0F};
    player.ground_point = {};
    player.ground_collider_id = -1;
    player.ground_piece_index = 0;
}

// Applies input, mode transitions and gravity for one tick, updating velocity
// and clamping to the maximum speed. Returns true when the tick is fully
// resolved and no movement integration should follow (still charging on ground).
bool advance_before_resolution(
    PlayerState& player,
    const PlayerInput input,
    const float fixed_delta) noexcept {
    player.animation_time += fixed_delta;

    if (player.mode == PlayerMode::grounded) {
        player.on_ground = true;
        player.velocity = {};
        if (input.jump_down) {
            player.mode = PlayerMode::charging;
            player.jump_hold_time = std::min(
                fixed_delta,
                config::maximum_charge_seconds);
        } else {
            const float direction =
                (input.right ? 1.0F : 0.0F) - (input.left ? 1.0F : 0.0F);
            player.velocity.x = direction * config::movement_acceleration * fixed_delta;
            if (direction != 0.0F) {
                player.facing_right = direction > 0.0F;
            }
        }
    } else if (player.mode == PlayerMode::charging) {
        player.on_ground = true;
        player.velocity = {};
        if (input.right != input.left) {
            player.facing_right = input.right;
        }
        if (input.jump_down) {
            player.jump_hold_time = std::min(
                player.jump_hold_time + fixed_delta,
                config::maximum_charge_seconds);
            return true;
        }
        if (input.jump_released) {
            const float direction =
                (input.right ? 1.0F : 0.0F) - (input.left ? 1.0F : 0.0F);
            player.velocity = charged_jump_velocity(player.jump_hold_time, direction);
            player.jump_hold_time = 0.0F;
            player.mode = PlayerMode::airborne;
            player.on_ground = false;
        } else {
            return true;
        }
    } else {
        player.on_ground = false;
        player.jump_hold_time = 0.0F;
        player.velocity.y += config::gravity * fixed_delta;
    }

    const float speed = length(player.velocity);
    if (speed > config::maximum_speed) {
        player.velocity = normalized(player.velocity) * config::maximum_speed;
    }
    return false;
}

}  // namespace

ResolveResult step_player(
    PlayerState& player,
    const CollisionWorld& world,
    const PlayerInput input,
    const float fixed_delta) noexcept {
    if (advance_before_resolution(player, input, fixed_delta)) {
        return {true, false};
    }

    const Vec2 previous_position = player.position;
    player.position = player.position + player.velocity * fixed_delta;
    ResolveResult result = world.resolve(previous_position, player);

    if (result.ground_contact) {
        apply_ground_contact(player, *result.ground_contact);
    } else if (player.velocity.y >= 0.0F) {
        // SAT reports penetration, not flush contact; probe just below the feet
        // so a resting player keeps a deterministic support instead of
        // flickering airborne each tick.
        const Vec2 half = config::player_half_size;
        const Aabb feet{
            {player.position.x - half.x, player.position.y + half.y - 0.02F},
            {player.position.x + half.x, player.position.y + half.y + 0.08F}};
        if (const std::optional<Contact> support = world.support_contact(feet)) {
            result.on_ground = true;
            result.ground_contact = support;
            apply_ground_contact(player, *support);
        }
    }

    if (player.on_ground) {
        player.mode = player.jump_hold_time > 0.0F
            ? PlayerMode::charging
            : PlayerMode::grounded;
        player.velocity.y = 0.0F;
    } else {
        clear_ground_contact(player);
        if (player.mode != PlayerMode::charging) {
            player.mode = PlayerMode::airborne;
        }
    }
    return result;
}

}  // namespace jumpcastle
