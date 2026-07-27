#include "jumpcastle/collider.hpp"

#include "jumpcastle/convex.hpp"
#include "jumpcastle/shape_collision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace jumpcastle {
namespace {

[[nodiscard]] Vec2 clamp_to_aabb(const Vec2 point, const Aabb box) noexcept {
    return {
        std::clamp(point.x, box.min.x, box.max.x),
        std::clamp(point.y, box.min.y, box.max.y),
    };
}

[[nodiscard]] Vec2 closest_on_segment(
    const Vec2 point, const Vec2 a, const Vec2 b) noexcept {
    const Vec2 ab = b - a;
    const float denominator = dot(ab, ab);
    const float t = denominator == 0.0F
        ? 0.0F
        : std::clamp(dot(point - a, ab) / denominator, 0.0F, 1.0F);
    return a + ab * t;
}

[[nodiscard]] bool clip_segment_plane(
    const float direction,
    const float distance,
    float& enter,
    float& exit) noexcept {
    if (direction == 0.0F) {
        return distance >= 0.0F;
    }

    const float t = distance / direction;
    if (direction < 0.0F) {
        enter = std::max(enter, t);
    } else {
        exit = std::min(exit, t);
    }
    return enter <= exit;
}

[[nodiscard]] std::optional<Vec2> first_segment_aabb_intersection(
    const Vec2 a, const Vec2 b, const Aabb box) noexcept {
    const Vec2 delta = b - a;
    float enter = 0.0F;
    float exit = 1.0F;

    if (!clip_segment_plane(-delta.x, a.x - box.min.x, enter, exit) ||
        !clip_segment_plane(delta.x, box.max.x - a.x, enter, exit) ||
        !clip_segment_plane(-delta.y, a.y - box.min.y, enter, exit) ||
        !clip_segment_plane(delta.y, box.max.y - a.y, enter, exit)) {
        return std::nullopt;
    }
    return a + delta * enter;
}

struct ClosestPair {
    Vec2 segment_point{};
    Vec2 box_point{};
};

struct ExitCandidate {
    Vec2 normal{};
    float depth{};
};

struct Interval {
    float min{};
    float max{};
};

[[nodiscard]] ClosestPair closest_segment_aabb(
    const Vec2 a, const Vec2 b, const Aabb box) noexcept {
    if (const auto intersection = first_segment_aabb_intersection(a, b, box)) {
        return {*intersection, *intersection};
    }

    ClosestPair best{a, clamp_to_aabb(a, box)};
    float best_distance = dot(
        best.segment_point - best.box_point, best.segment_point - best.box_point);
    const auto consider = [&](const Vec2 segment_point, const Vec2 box_point) {
        const Vec2 delta = segment_point - box_point;
        const float candidate = dot(delta, delta);
        if (candidate < best_distance) {
            best = {segment_point, box_point};
            best_distance = candidate;
        }
    };

    consider(b, clamp_to_aabb(b, box));
    for (const Vec2 corner : std::array{
             box.min,
             Vec2{box.max.x, box.min.y},
             box.max,
             Vec2{box.min.x, box.max.y},
         }) {
        consider(closest_on_segment(corner, a, b), corner);
    }
    return best;
}

void consider_exit(
    ExitCandidate& best, const Vec2 normal, const float depth) noexcept {
    if (depth < best.depth) {
        best = {normal, depth};
    }
}

[[nodiscard]] Interval project_aabb_interval(
    const Aabb box, const Vec2 axis) noexcept {
    const Vec2 center = aabb_center(box);
    const Vec2 half = aabb_half(box);
    const float projected_center = dot(center, axis);
    const float projected_radius =
        std::abs(axis.x) * half.x + std::abs(axis.y) * half.y;
    return {
        projected_center - projected_radius,
        projected_center + projected_radius,
    };
}

[[nodiscard]] Interval project_polygon_interval(
    const std::vector<Vec2>& points, const Vec2 axis) noexcept {
    Interval interval{
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
    };
    for (const Vec2 point : points) {
        const float projection = dot(point, axis);
        interval.min = std::min(interval.min, projection);
        interval.max = std::max(interval.max, projection);
    }
    return interval;
}

[[nodiscard]] std::optional<ExitCandidate> polygon_contact_exit(
    const Aabb box, const ConvexPolygon& polygon) noexcept {
    if (polygon.points.empty()) {
        return std::nullopt;
    }

    ExitCandidate best{{0.0F, -1.0F}, std::numeric_limits<float>::max()};
    const auto consider_axis = [&](const Vec2 raw_axis) {
        const float magnitude = length(raw_axis);
        if (magnitude == 0.0F) {
            return true;
        }

        const Vec2 axis = raw_axis * (1.0F / magnitude);
        const Interval box_interval = project_aabb_interval(box, axis);
        const Interval polygon_interval =
            project_polygon_interval(polygon.points, axis);
        if (box_interval.max <= polygon_interval.min ||
            polygon_interval.max <= box_interval.min) {
            return false;
        }

        // Move the player AABB toward +axis or -axis until the intervals no
        // longer overlap. Considering both directions makes containment a full
        // separating translation instead of merely the intersection length.
        consider_exit(
            best, axis, polygon_interval.max - box_interval.min);
        consider_exit(
            best, axis * -1.0F, box_interval.max - polygon_interval.min);
        return true;
    };

    // Stable tie order: authored polygon axes, then AABB x/y axes. Each axis
    // keeps its supplied direction before its opposite.
    for (const Vec2 axis : polygon.edge_normals) {
        if (!consider_axis(axis)) {
            return std::nullopt;
        }
    }
    if (!consider_axis({1.0F, 0.0F}) ||
        !consider_axis({0.0F, 1.0F})) {
        return std::nullopt;
    }
    return best;
}

[[nodiscard]] Vec2 polygon_support_point(
    const ConvexPolygon& polygon, const Vec2 normal, const Aabb box) noexcept {
    constexpr float support_epsilon = 1.0e-5F;
    float maximum_projection = -std::numeric_limits<float>::max();
    for (const Vec2 point : polygon.points) {
        maximum_projection =
            std::max(maximum_projection, dot(point, normal));
    }

    const Vec2 tangent{-normal.y, normal.x};
    Vec2 minimum_point{};
    Vec2 maximum_point{};
    float minimum_tangent = std::numeric_limits<float>::max();
    float maximum_tangent = -std::numeric_limits<float>::max();
    for (const Vec2 point : polygon.points) {
        if (maximum_projection - dot(point, normal) > support_epsilon) {
            continue;
        }
        const float tangent_projection = dot(point, tangent);
        if (tangent_projection < minimum_tangent) {
            minimum_tangent = tangent_projection;
            minimum_point = point;
        }
        if (tangent_projection > maximum_tangent) {
            maximum_tangent = tangent_projection;
            maximum_point = point;
        }
    }

    if (minimum_tangent == maximum_tangent) {
        return minimum_point;
    }
    const Vec2 box_center = aabb_center(box);
    const Vec2 box_support{
        normal.x > 0.0F
            ? box.min.x
            : (normal.x < 0.0F ? box.max.x : box_center.x),
        normal.y > 0.0F
            ? box.min.y
            : (normal.y < 0.0F ? box.max.y : box_center.y),
    };
    const float desired_tangent = std::clamp(
        dot(box_support, tangent), minimum_tangent, maximum_tangent);
    const float interpolation =
        (desired_tangent - minimum_tangent) /
        (maximum_tangent - minimum_tangent);
    return minimum_point + (maximum_point - minimum_point) * interpolation;
}

[[nodiscard]] ExitCandidate circle_containment_exit(
    const Aabb box, const Vec2 center, const float radius) noexcept {
    // Fixed tie order: up, down, left, right.
    ExitCandidate best{{0.0F, -1.0F}, radius + box.max.y - center.y};
    consider_exit(best, {0.0F, 1.0F}, radius + center.y - box.min.y);
    consider_exit(best, {-1.0F, 0.0F}, radius + box.max.x - center.x);
    consider_exit(best, {1.0F, 0.0F}, radius + center.x - box.min.x);
    return best;
}

[[nodiscard]] Vec2 canonical_axis(Vec2 axis) noexcept {
    if (axis.y > 0.0F || (axis.y == 0.0F && axis.x > 0.0F)) {
        axis = axis * -1.0F;
    }
    return axis;
}

void consider_capsule_exit_direction(
    ExitCandidate& best,
    const Vec2 normal,
    const Aabb box,
    const Vec2 a,
    const Vec2 b,
    const float radius) noexcept {
    const Vec2 center = aabb_center(box);
    const Vec2 half = aabb_half(box);
    const float box_min =
        dot(center, normal) -
        (std::abs(normal.x) * half.x + std::abs(normal.y) * half.y);
    const float capsule_max = std::max(dot(a, normal), dot(b, normal)) + radius;
    consider_exit(best, normal, capsule_max - box_min);
}

void consider_capsule_exit_axis(
    ExitCandidate& best,
    const Vec2 raw_axis,
    const Aabb box,
    const Vec2 a,
    const Vec2 b,
    const float radius) noexcept {
    const float magnitude = length(raw_axis);
    if (magnitude == 0.0F) {
        return;
    }
    const Vec2 axis = canonical_axis(raw_axis * (1.0F / magnitude));
    consider_capsule_exit_direction(best, axis, box, a, b, radius);
    consider_capsule_exit_direction(best, axis * -1.0F, box, a, b, radius);
}

[[nodiscard]] ExitCandidate capsule_containment_exit(
    const Aabb box, const Vec2 a, const Vec2 b, const float radius) noexcept {
    ExitCandidate best{{0.0F, -1.0F}, std::numeric_limits<float>::max()};

    // Preserve cardinal tie order before considering curved-feature axes.
    consider_capsule_exit_direction(best, {0.0F, -1.0F}, box, a, b, radius);
    consider_capsule_exit_direction(best, {0.0F, 1.0F}, box, a, b, radius);
    consider_capsule_exit_direction(best, {-1.0F, 0.0F}, box, a, b, radius);
    consider_capsule_exit_direction(best, {1.0F, 0.0F}, box, a, b, radius);

    const Vec2 segment = b - a;
    consider_capsule_exit_axis(
        best, {-segment.y, segment.x}, box, a, b, radius);

    for (const Vec2 corner : std::array{
             box.min,
             Vec2{box.max.x, box.min.y},
             box.max,
             Vec2{box.min.x, box.max.y},
         }) {
        const Vec2 segment_point = closest_on_segment(corner, a, b);
        consider_capsule_exit_axis(
            best, corner - segment_point, box, a, b, radius);
    }
    return best;
}

[[nodiscard]] Vec2 capsule_surface_point(
    const Vec2 a, const Vec2 b, const Vec2 normal, const Aabb box) noexcept {
    const float a_projection = dot(a, normal);
    const float b_projection = dot(b, normal);
    if (a_projection > b_projection) {
        return a;
    }
    if (b_projection > a_projection) {
        return b;
    }
    return closest_on_segment(aabb_center(box), a, b);
}

[[nodiscard]] std::optional<Contact> radial_contact(
    const Aabb box, const Vec2 center, const float radius) noexcept {
    const Vec2 box_point = clamp_to_aabb(center, box);
    Vec2 delta = box_point - center;
    float distance = length(delta);
    if (distance >= radius) {
        return std::nullopt;
    }

    if (distance == 0.0F) {
        const ExitCandidate exit = circle_containment_exit(box, center, radius);
        return Contact{
            .point = center + exit.normal * radius,
            .normal = exit.normal,
            .depth = exit.depth,
        };
    } else {
        delta = delta * (1.0F / distance);
    }

    return Contact{
        .point = center + delta * radius,
        .normal = delta,
        .depth = radius - distance,
    };
}

}  // namespace

Aabb geometry_aabb(const PolygonGeometry& polygon) noexcept {
    if (polygon.points.empty()) { return {}; }
    return polygon_aabb(polygon.points);
}

Aabb geometry_aabb(const CircleGeometry& circle) noexcept {
    const Vec2 extent{circle.radius, circle.radius};
    return {circle.center - extent, circle.center + extent};
}

Aabb geometry_aabb(const CapsuleGeometry& capsule) noexcept {
    return {
        {std::min(capsule.a.x, capsule.b.x) - capsule.radius,
         std::min(capsule.a.y, capsule.b.y) - capsule.radius},
        {std::max(capsule.a.x, capsule.b.x) + capsule.radius,
         std::max(capsule.a.y, capsule.b.y) + capsule.radius},
    };
}

std::optional<Contact> contact_aabb_polygon(
    const Aabb box, const ConvexPolygon& polygon) noexcept {
    const auto exit = polygon_contact_exit(box, polygon);
    if (!exit) {
        return std::nullopt;
    }

    return Contact{
        .point = polygon_support_point(polygon, exit->normal, box),
        .normal = exit->normal,
        .depth = exit->depth,
    };
}

std::optional<Contact> contact_aabb_circle(
    const Aabb box, const CircleGeometry& circle) noexcept {
    return radial_contact(box, circle.center, circle.radius);
}

std::optional<Contact> contact_aabb_capsule(
    const Aabb box, const CapsuleGeometry& capsule) noexcept {
    if (capsule.a == capsule.b) {
        return radial_contact(box, capsule.a, capsule.radius);
    }

    const ClosestPair pair = closest_segment_aabb(capsule.a, capsule.b, box);
    const Vec2 delta = pair.box_point - pair.segment_point;
    const float distance = length(delta);
    if (distance >= capsule.radius) {
        return std::nullopt;
    }

    if (distance == 0.0F) {
        const ExitCandidate exit =
            capsule_containment_exit(box, capsule.a, capsule.b, capsule.radius);
        const Vec2 segment_point =
            capsule_surface_point(capsule.a, capsule.b, exit.normal, box);
        return Contact{
            .point = segment_point + exit.normal * capsule.radius,
            .normal = exit.normal,
            .depth = exit.depth,
        };
    }

    const Vec2 normal = delta * (1.0F / distance);
    return Contact{
        .point = pair.segment_point + normal * capsule.radius,
        .normal = normal,
        .depth = capsule.radius - distance,
    };
}

}  // namespace jumpcastle
