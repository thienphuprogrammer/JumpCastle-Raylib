#include "jumpcastle/collision.hpp"

#include <algorithm>
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

bool collides_with_tilemap(
    const Tilemap& tilemap,
    const float tilemap_offset_y,
    Vec2 center,
    const Vec2 half_size) noexcept {
    center.y -= tilemap_offset_y;
    const TileRange range = overlapped_tiles(center, half_size);

    for (int x = range.start_x; x <= range.end_x; ++x) {
        for (int y = range.start_y; y <= range.end_y; ++y) {
            if (!tilemap.solid_at(x, y)) {
                continue;
            }

            const Vec2 tile_center{0.5F + static_cast<float>(x),
                                      0.5F + static_cast<float>(y)};
            const Vec2 combined_half_size{half_size.x + 0.5F,
                                             half_size.y + 0.5F};
            const Vec2 surface_distance{
                std::abs(center.x - tile_center.x) - combined_half_size.x,
                std::abs(center.y - tile_center.y) - combined_half_size.y,
            };

            if (surface_distance.x <= 0.0F && surface_distance.y <= 0.0F) {
                return true;
            }
        }
    }

    return false;
}

void resolve_tilemap_collision(
    const Tilemap& tilemap,
    const float tilemap_offset_y,
    Vec2& center,
    Vec2& velocity,
    const Vec2 half_size) noexcept {
    center.y -= tilemap_offset_y;
    const TileRange range = overlapped_tiles(center, half_size);

    for (int x = range.start_x; x <= range.end_x; ++x) {
        for (int y = range.start_y; y <= range.end_y; ++y) {
            if (!tilemap.solid_at(x, y)) {
                continue;
            }

            const Vec2 tile_center{0.5F + static_cast<float>(x),
                                      0.5F + static_cast<float>(y)};
            const Vec2 combined_half_size{half_size.x + 0.5F,
                                             half_size.y + 0.5F};
            const Vec2 surface_distance{
                std::abs(center.x - tile_center.x) - combined_half_size.x,
                std::abs(center.y - tile_center.y) - combined_half_size.y,
            };

            if (surface_distance.x > 0.0F || surface_distance.y > 0.0F) {
                continue;
            }

            const int neighbor_x = x + (center.x > tile_center.x ? 1 : -1);
            const int neighbor_y = y + (center.y > tile_center.y ? 1 : -1);
            const bool x_edge_is_open = !tilemap.solid_at(neighbor_x, y);
            const bool y_edge_is_open = !tilemap.solid_at(x, neighbor_y);

            if (!x_edge_is_open && !y_edge_is_open) {
                continue;
            }

            bool resolve_x = x_edge_is_open;
            if (x_edge_is_open && y_edge_is_open) {
                resolve_x = surface_distance.x > surface_distance.y;
            }

            if (resolve_x) {
                if (center.x > tile_center.x) {
                    center.x = tile_center.x + combined_half_size.x;
                    if (velocity.x < 0.0F) {
                        velocity.x = -velocity.x * config::horizontal_bounce;
                    }
                } else {
                    center.x = tile_center.x - combined_half_size.x;
                    if (velocity.x > 0.0F) {
                        velocity.x = -velocity.x * config::horizontal_bounce;
                    }
                }
            } else if (center.y > tile_center.y) {
                center.y = tile_center.y + combined_half_size.y;
                velocity.y = std::max(velocity.y, 0.0F);
            } else {
                center.y = tile_center.y - combined_half_size.y;
                velocity.y = std::min(velocity.y, 0.0F);
            }
        }
    }

    center.y += tilemap_offset_y;
}

}  // namespace jumpcastle
