#pragma once

#include "jumpcastle/map_format.hpp"
#include "jumpcastle/math.hpp"

#include <optional>
#include <vector>

namespace jumpcastle {

struct PlayerState;

/// Outcome of one collision-world movement resolve.
///
/// `ground_contact` is populated only when `on_ground` is true and identifies
/// the best supporting contact after deterministic ranking.
///
/// @code
/// const ResolveResult result = world.resolve(previous_position, player);
/// if (result.hit_hazard) { respawn_player(); }
/// @endcode
struct ResolveResult {
    /// True when an upward-facing blocking contact supported the player.
    bool on_ground{};
    /// True when any sub-step contacted a hazard.
    bool hit_hazard{};
    /// Best support by normal, depth, collider id, then convex-piece index.
    std::optional<Contact> ground_contact{};
};

/// Immutable world-space shape collection built from vertically stacked maps.
///
/// Runtime geometry is stored in world coordinates. Read-only queries are safe
/// concurrently; `resolve` additionally requires a distinct player per call.
class CollisionWorld {
public:
    /// Creates an empty collision world with a one-tile screen-height fallback.
    ///
    /// @code
    /// CollisionWorld empty_world;
    /// @endcode
    CollisionWorld() = default;

    /// Builds exact runtime shape colliders from authored screen-local geometry.
    ///
    /// Concave polygons are decomposed here, while circles and capsules retain
    /// their authored geometry. The returned world owns all converted data and
    /// can be queried concurrently after construction.
    ///
    /// @code
    /// const CollisionWorld world = CollisionWorld::from_screens(screens, 36);
    /// @endcode
    [[nodiscard]] static CollisionWorld from_screens(
        const std::vector<ScreenMap>& screens,
        int screen_height);

    /// Moves the player with sub-stepped exact shape resolution.
    ///
    /// Updates `player.position` and `player.velocity`; the returned contact
    /// identifies the deterministically selected supporting collider. Concurrent
    /// calls are safe when each call receives a distinct `PlayerState`.
    ///
    /// @code
    /// ResolveResult result = world.resolve(previous_position, player);
    /// if (result.ground_contact) { player.on_ground = true; }
    /// @endcode
    [[nodiscard]] ResolveResult resolve(Vec2 previous_position, PlayerState& player) const;

    /// Returns whether `box` contacts a blocking runtime collider.
    ///
    /// Hazards and invalid curved one-way colliders are ignored. This read-only
    /// query is safe to call concurrently after world construction.
    ///
    /// @code
    /// const bool supported = world.overlaps_blocking(player_bounds);
    /// @endcode
    [[nodiscard]] bool overlaps_blocking(const Aabb& box) const noexcept;

    /// Returns the deterministically selected supporting contact under `box`.
    ///
    /// Uses the same neighboring-screen broad phase and narrow-phase dispatch as
    /// `resolve`, ignoring hazards and accepting only walkable contacts whose
    /// `normal.y <= -0.5F`. Candidates are ranked by upward normal, then depth,
    /// then `(collider_id, piece_index)`. Returns `std::nullopt` when nothing
    /// supports the box. This read-only query is safe to call concurrently.
    ///
    /// @code
    /// if (const auto contact = world.support_contact(feet)) {
    ///     player.ground_normal = contact->normal;
    /// }
    /// @endcode
    [[nodiscard]] std::optional<Contact> support_contact(Aabb box) const noexcept;

    /// Returns world-space colliders for a screen, or `nullptr` out of range.
    /// The pointer remains valid until the world is destroyed or assigned.
    ///
    /// @code
    /// if (const auto* colliders = world.colliders_for_screen(2)) {
    ///     for (const WorldCollider& collider : *colliders) { /* inspect */ }
    /// }
    /// @endcode
    [[nodiscard]] const std::vector<WorldCollider>* colliders_for_screen(
        int screen_index) const noexcept;

    /// Returns the temporary read-only polygon adapter for renderer/solver use.
    ///
    /// Curved geometry is intentionally absent and Task 5 removes this API.
    ///
    /// @code
    /// const auto* render_polygons = world.polygons_for_screen(camera_screen);
    /// @endcode
    [[nodiscard]] const std::vector<ConvexPolygon>* polygons_for_screen(
        int screen_index) const noexcept {
        if (screen_index < 0 ||
            screen_index >= static_cast<int>(polygon_adapter_by_screen_.size())) {
            return nullptr;
        }
        return &polygon_adapter_by_screen_[static_cast<std::size_t>(screen_index)];
    }

private:
    std::vector<std::vector<WorldCollider>> by_screen_;
    std::vector<std::vector<ConvexPolygon>> polygon_adapter_by_screen_;
    int screen_height_{1};
};

}  // namespace jumpcastle
