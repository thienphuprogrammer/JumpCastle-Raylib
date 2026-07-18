#pragma once

#include "jumpcastle/tilemap.hpp"

namespace jumpcastle {

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

}  // namespace jumpcastle
