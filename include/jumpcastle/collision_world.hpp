#pragma once

#include "jumpcastle/map_format.hpp"
#include "jumpcastle/math.hpp"

#include <vector>

namespace jumpcastle {

struct PlayerState;

struct ResolveResult {
    bool on_ground{};
    bool hit_hazard{};
};

// World-space convex-polygon collision built from per-screen maps stacked
// vertically. Polygons are stored in world coordinates (screen-local points
// offset by screen_index * screen_height).
class CollisionWorld {
public:
    CollisionWorld() = default;

    [[nodiscard]] static CollisionWorld from_screens(
        const std::vector<ScreenMap>& screens,
        int screen_height);

    // Moves the player from previous_position toward its current position with
    // sub-stepped SAT resolution. Updates player.position/velocity and returns
    // whether the player is grounded and whether a hazard was touched.
    [[nodiscard]] ResolveResult resolve(Vec2 previous_position, PlayerState& player) const;

private:
    [[nodiscard]] const std::vector<ConvexPolygon>* screen_polygons(int screen_index) const noexcept;

    std::vector<std::vector<ConvexPolygon>> by_screen_;
    int screen_height_{1};
};

}  // namespace jumpcastle
