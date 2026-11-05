#pragma once

#include "jumpcastle/collider.hpp"

#include <vector>

namespace jumpcastle {

// A single deterministic launch/landing anchor on a walkable surface.
//
// Example: `SurfaceSample{{4.0F, 8.0F}, {0.0F, -1.0F}, 7, 0, 3}` is sample
// three on collider seven's first convex piece.
struct SurfaceSample {
    Vec2 position{};
    Vec2 normal{0.0F, -1.0F};
    int collider_id{-1};
    int piece_index{};
    int sample_index{};

    friend bool operator==(const SurfaceSample&, const SurfaceSample&) = default;
};

// Returns deterministic launch anchors along the walkable part of `collider`.
//
// Walkable means the local surface normal satisfies `normal.y <= -0.5F`
// (matching the player's ground-contact threshold). Adjacent samples are at
// most `spacing` apart in edge or arc length. `spacing` must be finite and
// positive; otherwise the result is empty. Each result preserves
// `(collider_id, piece_index)` and has a zero-based `sample_index` in returned
// order. Polygon order follows authored edges, circle order follows increasing
// walkable angle, and capsule order is canonical `(position.y, position.x)`.
// The function has no shared state and is safe to call concurrently.
//
// Example:
// `auto samples = sample_walkable_surfaces(world_collider, 0.25F);`
[[nodiscard]] std::vector<SurfaceSample> sample_walkable_surfaces(
    const WorldCollider& collider, float spacing);

}  // namespace jumpcastle
