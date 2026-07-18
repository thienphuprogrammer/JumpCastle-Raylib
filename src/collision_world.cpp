#include "jumpcastle/collision_world.hpp"

#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player.hpp"
#include "jumpcastle/sat.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace jumpcastle {
namespace {

constexpr float kSubStep = 0.2F;      // max distance per collision sub-step (tiles)
constexpr float kGroundNormalY = -0.5F;  // normal.y below this counts as a floor (y-down)

Aabb player_box(const Vec2 center) noexcept {
    return {center - config::player_half_size, center + config::player_half_size};
}

}  // namespace

CollisionWorld CollisionWorld::from_screens(
    const std::vector<ScreenMap>& screens,
    const int screen_height) {
    CollisionWorld world;
    world.screen_height_ = screen_height > 0 ? screen_height : 1;

    int max_index = 0;
    for (const ScreenMap& screen : screens) {
        max_index = std::max(max_index, screen.index);
    }
    world.by_screen_.resize(static_cast<std::size_t>(max_index) + 1);

    for (const ScreenMap& screen : screens) {
        const float offset = static_cast<float>(screen.index * world.screen_height_);
        std::vector<ConvexPolygon>& target =
            world.by_screen_[static_cast<std::size_t>(screen.index)];
        for (ConvexPolygon polygon : screen.polygons) {
            for (Vec2& point : polygon.points) { point.y += offset; }
            polygon.aabb.min.y += offset;
            polygon.aabb.max.y += offset;
            target.push_back(std::move(polygon));
        }
    }
    return world;
}

const std::vector<ConvexPolygon>* CollisionWorld::screen_polygons(
    const int screen_index) const noexcept {
    if (screen_index < 0 || screen_index >= static_cast<int>(by_screen_.size())) {
        return nullptr;
    }
    return &by_screen_[static_cast<std::size_t>(screen_index)];
}

bool CollisionWorld::overlaps_blocking(const Aabb& box) const noexcept {
    const Vec2 center = aabb_center(box);
    const int screen_index =
        static_cast<int>(std::floor(center.y / static_cast<float>(screen_height_)));
    for (int k = screen_index - 1; k <= screen_index + 1; ++k) {
        const std::vector<ConvexPolygon>* polygons = screen_polygons(k);
        if (polygons == nullptr) { continue; }
        for (const ConvexPolygon& polygon : *polygons) {
            if (polygon.type == ColliderType::hazard) { continue; }
            if (aabb_vs_convex(box, polygon.points, polygon.edge_normals).overlapping) {
                return true;
            }
        }
    }
    return false;
}

ResolveResult CollisionWorld::resolve(const Vec2 previous_position, PlayerState& player) const {
    const Vec2 target = player.position;
    player.position = previous_position;

    ResolveResult result{};

    const Vec2 delta = target - previous_position;
    const int steps = std::max(1, static_cast<int>(std::ceil(length(delta) / kSubStep)));
    const Vec2 step = delta * (1.0F / static_cast<float>(steps));

    for (int s = 0; s < steps; ++s) {
        const Vec2 step_start = player.position;
        player.position = player.position + step;

        const int screen_index =
            static_cast<int>(std::floor(player.position.y / static_cast<float>(screen_height_)));
        for (int k = screen_index - 1; k <= screen_index + 1; ++k) {
            const std::vector<ConvexPolygon>* polygons = screen_polygons(k);
            if (polygons == nullptr) { continue; }

            for (const ConvexPolygon& polygon : *polygons) {
                const Aabb box = player_box(player.position);
                const Mtv mtv = aabb_vs_convex(box, polygon.points, polygon.edge_normals);
                if (!mtv.overlapping) { continue; }

                if (polygon.type == ColliderType::hazard) {
                    result.hit_hazard = true;
                    continue;
                }

                if (polygon.type == ColliderType::oneway) {
                    const bool landing = mtv.normal.y < kGroundNormalY;
                    const float previous_bottom = step_start.y + config::player_half_size.y;
                    if (!landing || previous_bottom > polygon.aabb.min.y) {
                        continue;  // only block a fall from above
                    }
                }

                player.position = player.position + mtv.normal * mtv.depth;
                const float velocity_along_normal = dot(player.velocity, mtv.normal);
                if (velocity_along_normal < 0.0F) {
                    // A vertical wall (mostly-horizontal normal) rebounds an
                    // airborne player Jump King-style: reverse the into-wall
                    // velocity and keep a fraction of it, leaving the vertical
                    // fall untouched so the player loses horizontal control.
                    // Floors, ceilings and grounded contact simply slide
                    // (restitution 0 makes the formula the plain slide).
                    const bool is_wall =
                        std::abs(mtv.normal.x) > std::abs(mtv.normal.y);
                    const float restitution =
                        (is_wall && player.mode == PlayerMode::airborne)
                            ? config::wall_bounce
                            : 0.0F;
                    player.velocity = player.velocity -
                        mtv.normal * (velocity_along_normal * (1.0F + restitution));
                }
                if (mtv.normal.y < kGroundNormalY) { result.on_ground = true; }
            }
        }
    }

    return result;
}

}  // namespace jumpcastle
