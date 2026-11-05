#pragma once

#include "jumpcastle/collider.hpp"

#include <optional>

namespace jumpcastle {

/// Returns the polygon contact that fully separates `box` along the minimum SAT exit.
///
/// This function is pure, allocation-free apart from the existing SAT path,
/// and safe to call concurrently when `polygon` is not being mutated.
///
/// @code
/// const auto contact = contact_aabb_polygon(player_box, floor_polygon);
/// if (contact) {
///     player_position = player_position + contact->normal * contact->depth;
/// }
/// @endcode
[[nodiscard]] std::optional<Contact> contact_aabb_polygon(
    Aabb box, const ConvexPolygon& polygon) noexcept;

/// Returns the exact radial contact between `box` and `circle`.
///
/// The normal points from the circle surface toward the box. A circle center
/// inside the box uses the minimum full-separation exit with deterministic
/// ties. This function is pure, allocation-free, and thread-safe.
///
/// @code
/// const auto contact = contact_aabb_circle(player_box, round_platform);
/// @endcode
[[nodiscard]] std::optional<Contact> contact_aabb_circle(
    Aabb box, const CircleGeometry& circle) noexcept;

/// Returns the exact contact between `box` and `capsule`.
///
/// The center-segment/AABB closest pair is selected from exact feature
/// candidates with deterministic tie ordering. This function is pure,
/// allocation-free, and thread-safe.
///
/// @code
/// const auto contact = contact_aabb_capsule(player_box, capsule_platform);
/// @endcode
[[nodiscard]] std::optional<Contact> contact_aabb_capsule(
    Aabb box, const CapsuleGeometry& capsule) noexcept;

}  // namespace jumpcastle
