#pragma once

#include "jumpcastle/tilemap.hpp"

namespace jumpcastle {

struct TileRange {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
};

[[nodiscard]] TileRange overlapped_tiles(Vector2 center, Vector2 half_size) noexcept;

[[nodiscard]] bool collides_with_tilemap(
    const Tilemap& tilemap,
    float tilemap_offset_y,
    Vector2 center,
    Vector2 half_size) noexcept;

void resolve_tilemap_collision(
    const Tilemap& tilemap,
    float tilemap_offset_y,
    Vector2& center,
    Vector2& velocity,
    Vector2 half_size) noexcept;

}  // namespace jumpcastle
