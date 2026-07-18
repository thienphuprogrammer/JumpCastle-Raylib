#pragma once

#include "jumpcastle/assets.hpp"
#include "jumpcastle/player.hpp"

#include <cmath>
#include <cstddef>

namespace jumpcastle {

// The generated King atlas registers every cell the same way: the sprite is
// horizontally centred and its feet sit on the cell floor. Drawing the cell as
// a square this many pixels tall makes the King about two tiles wide and keeps
// its feet on the ground when the destination bottom is aligned to the player's
// feet.
inline constexpr float player_sprite_display =
    3.0F * static_cast<float>(config::tile_pixels);

// Place the King so it is centred on the player horizontally and stands with
// its feet at the player's feet. Because the sprite is centred in its cell, the
// destination is independent of facing, so flipping the source never shifts it.
[[nodiscard]] inline Rectangle player_sprite_destination(
    const Vector2 position,
    const float screen_offset_y) noexcept {
    const float centre_x = position.x * static_cast<float>(config::tile_pixels);
    const float feet_y =
        (position.y + config::player_half_size.y - screen_offset_y) *
        static_cast<float>(config::tile_pixels);
    return {
        centre_x - player_sprite_display * 0.5F,
        feet_y - player_sprite_display,
        player_sprite_display,
        player_sprite_display,
    };
}

[[nodiscard]] inline PlayerAnimation select_animation(
    const PlayerState& player,
    const float respawn_animation_time) noexcept {
    if (respawn_animation_time > 0.0F) return PlayerAnimation::respawn;
    if (player.on_ground) {
        if (player.jump_hold_time > 0.001F) return PlayerAnimation::charge;
        if (std::abs(player.velocity.x) > 0.01F) return PlayerAnimation::run;
        return PlayerAnimation::idle;
    }
    return player.velocity.y < 0.0F ? PlayerAnimation::rise : PlayerAnimation::fall;
}

// Advance through a clip at its frame rate, looping. Guards empty clips so a
// malformed manifest can never divide by zero at draw time.
[[nodiscard]] inline std::size_t animation_frame_index(
    const float animation_time,
    const int fps,
    const std::size_t frame_count) noexcept {
    if (frame_count == 0) return 0;
    const auto step = static_cast<std::size_t>(
        std::floor(animation_time * static_cast<float>(fps)));
    return step % frame_count;
}

}  // namespace jumpcastle
