#pragma once

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

[[nodiscard]] bool collides_with_world(
    const WorldMap& world,
    Vec2 center,
    Vec2 half_size) noexcept;

void resolve_world_collision(
    const WorldMap& world,
    Vec2 previous_position,
    PlayerState& player) noexcept;

}  // namespace jumpcastle
