#include "jumpcastle/player.hpp"

#include <algorithm>
#include <cmath>

namespace jumpcastle {

Vec2 charged_jump_velocity(
    const float hold_time,
    const float horizontal_input) noexcept {
    const float jump_scale = std::clamp(hold_time * 2.6F, 1.1F, 2.0F) / 2.0F;
    const float horizontal_strength = 0.75F - jump_scale * 0.5F;
    const float direction = std::clamp(horizontal_input, -1.0F, 1.0F);
    const Vec2 jump_direction = normalized({
        direction * horizontal_strength,
        -1.0F,
    });

    return jump_direction * (jump_scale * config::jump_strength);
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
        player.jump_hold_time += delta;
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

void update_player(
    PlayerState& player,
    const Tilemap& tilemap,
    const float tilemap_offset_y,
    const PlayerInput input,
    const float delta) noexcept {
    player.velocity.y += config::gravity * delta;
    player.on_ground = collides_with_tilemap(
        tilemap,
        tilemap_offset_y,
        {player.position.x, player.position.y + config::player_half_size.y},
        {0.1F, 0.05F});

    if (player.on_ground) {
        player.velocity.x = 0.0F;
        simulate_ground_movement(player, input, delta);
    } else {
        player.jump_hold_time = 0.0F;
    }

    player.animation_time += delta;
    integrate_player(player, delta);
}

}  // namespace jumpcastle
