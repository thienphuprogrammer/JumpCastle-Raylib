#pragma once

#include "jumpcastle/math.hpp"

namespace jumpcastle::config {

inline constexpr int tilemap_width = 16;
inline constexpr int tilemap_height = 12;
inline constexpr int tile_pixels = 16;
inline constexpr int viewport_tiles_width = 28;
inline constexpr int viewport_tiles_height = 36;
inline constexpr int view_width = viewport_tiles_width * tile_pixels;
inline constexpr int view_height = viewport_tiles_height * tile_pixels;
inline constexpr Vec2 player_half_size{0.3F, 0.4F};
inline constexpr float fixed_delta = 1.0F / 120.0F;
inline constexpr float minimum_charge_seconds = 0.12F;
inline constexpr float maximum_charge_seconds = 0.85F;
inline constexpr float gravity = 30.0F;
inline constexpr float movement_acceleration = 200.0F;
inline constexpr float jump_strength = 15.0F;
inline constexpr float horizontal_bounce = 0.45F;
inline constexpr float maximum_speed = 25.0F;
}  // namespace jumpcastle::config
