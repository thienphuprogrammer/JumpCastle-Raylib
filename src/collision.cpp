#include "jumpcastle/collision.hpp"

#include <cmath>

namespace jumpcastle {

TileRange overlapped_tiles(const Vec2 center, const Vec2 half_size) noexcept {
    return {
        static_cast<int>(std::floor(center.x - half_size.x)),
        static_cast<int>(std::floor(center.y - half_size.y)),
        static_cast<int>(std::floor(center.x + half_size.x)),
        static_cast<int>(std::floor(center.y + half_size.y)),
    };
}

}  // namespace jumpcastle
