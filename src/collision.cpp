#include "jumpcastle/collision.hpp"

#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player.hpp"

#include <algorithm>
#include <cmath>

namespace jumpcastle {
namespace {

bool overlaps_world_tile(
    const Vec2 center,
    const Vec2 half_size,
    const int x,
    const int y) noexcept {
    return center.x + half_size.x > static_cast<float>(x) &&
        center.x - half_size.x < static_cast<float>(x + 1) &&
        center.y + half_size.y > static_cast<float>(y) &&
        center.y - half_size.y < static_cast<float>(y + 1);
}

bool resolve_world_x(
    const WorldMap& world,
    PlayerState& player,
    const float direction) noexcept {
    const TileRange range = overlapped_tiles(player.position, config::player_half_size);
    for (int y = range.start_y; y <= range.end_y; ++y) {
        for (int x = range.start_x; x <= range.end_x; ++x) {
            if (!world.solid_at(x, y) ||
                !overlaps_world_tile(player.position, config::player_half_size, x, y)) {
                continue;
            }
            if (direction > 0.0F) {
                player.position.x = static_cast<float>(x) - config::player_half_size.x;
                if (player.velocity.x > 0.0F) {
                    player.velocity.x = -player.velocity.x * config::horizontal_bounce;
                }
            } else {
                player.position.x = static_cast<float>(x + 1) + config::player_half_size.x;
                if (player.velocity.x < 0.0F) {
                    player.velocity.x = -player.velocity.x * config::horizontal_bounce;
                }
            }
            return true;
        }
    }
    return false;
}

bool resolve_world_y(
    const WorldMap& world,
    PlayerState& player,
    const float direction) noexcept {
    const TileRange range = overlapped_tiles(player.position, config::player_half_size);
    for (int y = range.start_y; y <= range.end_y; ++y) {
        for (int x = range.start_x; x <= range.end_x; ++x) {
            if (!world.solid_at(x, y) ||
                !overlaps_world_tile(player.position, config::player_half_size, x, y)) {
                continue;
            }
            if (direction > 0.0F) {
                player.position.y = static_cast<float>(y) - config::player_half_size.y;
                player.velocity.y = 0.0F;
                player.mode = PlayerMode::grounded;
                player.on_ground = true;
            } else {
                player.position.y = static_cast<float>(y + 1) + config::player_half_size.y;
                player.velocity.y = 0.0F;
            }
            return true;
        }
    }
    return false;
}

}  // namespace

TileRange overlapped_tiles(const Vec2 center, const Vec2 half_size) noexcept {
    return {
        static_cast<int>(std::floor(center.x - half_size.x)),
        static_cast<int>(std::floor(center.y - half_size.y)),
        static_cast<int>(std::floor(center.x + half_size.x)),
        static_cast<int>(std::floor(center.y + half_size.y)),
    };
}

bool collides_with_world(
    const WorldMap& world,
    const Vec2 center,
    const Vec2 half_size) noexcept {
    const TileRange range = overlapped_tiles(center, half_size);
    for (int y = range.start_y; y <= range.end_y; ++y) {
        for (int x = range.start_x; x <= range.end_x; ++x) {
            if (world.solid_at(x, y) &&
                overlaps_world_tile(center, half_size, x, y)) {
                return true;
            }
        }
    }
    return false;
}

void resolve_world_collision(
    const WorldMap& world,
    const Vec2 previous_position,
    PlayerState& player) noexcept {
    const Vec2 target = player.position;
    player.position = previous_position;
    player.on_ground = false;

    const float delta_x = target.x - previous_position.x;
    const int x_steps = std::max(
        1,
        static_cast<int>(std::ceil(std::abs(delta_x) / 0.2F)));
    const float x_step = delta_x / static_cast<float>(x_steps);
    for (int step = 0; step < x_steps; ++step) {
        player.position.x += x_step;
        if (x_step != 0.0F && resolve_world_x(world, player, x_step)) {
            break;
        }
    }

    const float delta_y = target.y - previous_position.y;
    const int y_steps = std::max(
        1,
        static_cast<int>(std::ceil(std::abs(delta_y) / 0.2F)));
    const float y_step = delta_y / static_cast<float>(y_steps);
    for (int step = 0; step < y_steps; ++step) {
        player.position.y += y_step;
        if (y_step != 0.0F && resolve_world_y(world, player, y_step)) {
            break;
        }
    }
}

}  // namespace jumpcastle
