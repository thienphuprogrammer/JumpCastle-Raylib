#pragma once

#include "jumpcastle/tilemap.hpp"
#include "jumpcastle/world.hpp"

namespace jumpcastle {

struct PlayerState;

struct TileRange {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
};

[[nodiscard]] TileRange overlapped_tiles(Vec2 center, Vec2 half_size) noexcept;

[[nodiscard]] bool collides_with_tilemap(
    const Tilemap& tilemap,
    float tilemap_offset_y,
    Vec2 center,
    Vec2 half_size) noexcept;

void resolve_tilemap_collision(
    const Tilemap& tilemap,
    float tilemap_offset_y,
    Vec2& center,
    Vec2& velocity,
    Vec2 half_size) noexcept;

[[nodiscard]] bool collides_with_world(
    const WorldMap& world,
    Vec2 center,
    Vec2 half_size) noexcept;

void resolve_world_collision(
    const WorldMap& world,
    Vec2 previous_position,
    PlayerState& player) noexcept;

}  // namespace jumpcastle
