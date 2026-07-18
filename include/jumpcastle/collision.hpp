#pragma once

#include "jumpcastle/math.hpp"

namespace jumpcastle {

struct TileRange {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
};

[[nodiscard]] TileRange overlapped_tiles(Vec2 center, Vec2 half_size) noexcept;

}  // namespace jumpcastle
