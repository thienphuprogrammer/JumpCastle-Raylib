#pragma once

#include "jumpcastle/math.hpp"

#include <algorithm>
#include <cmath>

namespace jumpcastle::config {

inline constexpr int tilemap_width = 16;
inline constexpr int tilemap_height = 12;
inline constexpr int tile_pixels = 16;
inline constexpr int viewport_tiles_width = 28;
inline constexpr int viewport_tiles_height = 36;
inline constexpr int view_width = viewport_tiles_width * tile_pixels;
inline constexpr int view_height = viewport_tiles_height * tile_pixels;
inline constexpr Vec2 player_half_size{0.3F, 0.4F};
inline constexpr float fixed_delta = 1.0F / 120.0F;
inline constexpr float minimum_charge_seconds = 0.12F;
inline constexpr float maximum_charge_seconds = 0.85F;
inline constexpr float gravity = 30.0F;
inline constexpr float movement_acceleration = 200.0F;
inline constexpr float jump_strength = 15.0F;
inline constexpr float horizontal_bounce = 0.6F;
// Jump King-style wall rebound: fraction of horizontal speed retained (and
// reversed) when an airborne player strikes a vertical wall in the polygon
// collision path. Floors and ceilings do not bounce.
inline constexpr float wall_bounce = 0.8F;

// Charge-proportional (impact-scaled) wall rebound. The restitution fraction
// ramps with impact speed between the anchors below, clamped to
// [wall_bounce_min, wall_bounce_max]. The <= 1.0 ceiling keeps the wall from
// adding kinetic energy (a solver-fairness invariant). This replaces the fixed
// wall_bounce/horizontal_bounce constants at the call sites (migrated in the
// following tasks).
inline constexpr float wall_bounce_min = 0.55F;
inline constexpr float wall_bounce_max = 1.0F;
inline constexpr float wall_bounce_impact_lo = 4.5F;
inline constexpr float wall_bounce_impact_hi = 8.0F;

[[nodiscard]] inline float wall_bounce_restitution(const float impact_speed) noexcept {
    // Smoothstep-eased impact->restitution ramp: normalize the impact speed
    // into [0,1] across the anchors, ease it with the smoothstep S-curve
    // t*t*(3-2t) -- the same shape used for jump charge in player.cpp, so the
    // two controls feel consistent -- then map onto [wall_bounce_min,
    // wall_bounce_max]. Eases in/out, with the most sensitivity mid-charge.
    const float span = wall_bounce_impact_hi - wall_bounce_impact_lo;
    const float t = std::clamp((impact_speed - wall_bounce_impact_lo) / span,
                               0.0F, 1.0F);
    const float eased = t * t * (3.0F - 2.0F * t);
    return std::lerp(wall_bounce_min, wall_bounce_max, eased);
}

inline constexpr float maximum_speed = 25.0F;
}  // namespace jumpcastle::config
