#include "jumpcastle/surface_samples.hpp"

#include "jumpcastle/collider.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <tuple>
#include <variant>

namespace jumpcastle {

namespace {

constexpr float kWalkableNormalY = -0.5F;

[[nodiscard]] bool is_walkable(const Vec2 normal) noexcept {
    return normal.y <= kWalkableNormalY;
}

[[nodiscard]] int interval_count(
    const float surface_length, const float spacing) noexcept {
    return std::max(1, static_cast<int>(std::ceil(surface_length / spacing)));
}

void append_sample(
    std::vector<SurfaceSample>& out, const Vec2 position, const Vec2 normal,
    const WorldCollider& collider, int& sample_index) {
    out.push_back({
        .position = position,
        .normal = normal,
        .collider_id = collider.id,
        .piece_index = collider.piece_index,
        .sample_index = sample_index++,
    });
}

void append_segment(
    std::vector<SurfaceSample>& out, const Vec2 a, const Vec2 b,
    const Vec2 normal, const WorldCollider& collider, const float spacing,
    int& sample_index) {
    const Vec2 segment = b - a;
    const int intervals = interval_count(length(segment), spacing);
    for (int i = 0; i <= intervals; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(intervals);
        append_sample(out, a + segment * t, normal, collider, sample_index);
    }
}

void append_circle_arc(
    std::vector<SurfaceSample>& out, const Vec2 center, const float radius,
    const WorldCollider& collider, const float spacing, int& sample_index) {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        return;
    }
    constexpr float start = 7.0F * std::numbers::pi_v<float> / 6.0F;
    constexpr float end = 11.0F * std::numbers::pi_v<float> / 6.0F;
    const int intervals = interval_count(radius * (end - start), spacing);
    for (int i = 0; i <= intervals; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(intervals);
        const float angle = std::lerp(start, end, t);
        Vec2 normal{std::cos(angle), std::sin(angle)};
        if (i == 0 || i == intervals) {
            normal = {
                (i == 0 ? -1.0F : 1.0F) * std::numbers::sqrt3_v<float> * 0.5F,
                kWalkableNormalY,
            };
        }
        append_sample(out, center + normal * radius, normal, collider, sample_index);
    }
}

void append_capsule_cap(
    std::vector<SurfaceSample>& out, const Vec2 center, const Vec2 outward_axis,
    const Vec2 perpendicular, const float radius, const WorldCollider& collider,
    const float spacing, int& sample_index) {
    constexpr float half_pi = std::numbers::pi_v<float> * 0.5F;
    const int intervals =
        interval_count(std::numbers::pi_v<float> * radius, spacing);
    for (int i = 0; i <= intervals; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(intervals);
        const float angle = std::lerp(-half_pi, half_pi, t);
        const Vec2 normal =
            outward_axis * std::cos(angle) + perpendicular * std::sin(angle);
        if (is_walkable(normal)) {
            append_sample(
                out, center + normal * radius, normal, collider, sample_index);
        }
    }
}

void append_capsule(
    std::vector<SurfaceSample>& out, const CapsuleGeometry& capsule,
    const WorldCollider& collider, const float spacing, int& sample_index) {
    if (!std::isfinite(capsule.radius) || capsule.radius <= 0.0F) {
        return;
    }
    const Vec2 axis = capsule.b - capsule.a;
    const float axis_length = length(axis);
    if (axis_length == 0.0F) {
        append_circle_arc(
            out, capsule.a, capsule.radius, collider, spacing, sample_index);
        return;
    }
    const Vec2 direction = axis * (1.0F / axis_length);
    const Vec2 perpendicular{-direction.y, direction.x};
    for (const float side : {-1.0F, 1.0F}) {
        const Vec2 normal = perpendicular * side;
        if (is_walkable(normal)) {
            append_segment(
                out, capsule.a + normal * capsule.radius,
                capsule.b + normal * capsule.radius, normal, collider, spacing,
                sample_index);
        }
    }
    append_capsule_cap(
        out, capsule.a, direction * -1.0F, perpendicular, capsule.radius,
        collider, spacing, sample_index);
    append_capsule_cap(
        out, capsule.b, direction, perpendicular, capsule.radius, collider,
        spacing, sample_index);
}

void append_polygon(
    std::vector<SurfaceSample>& out, const ConvexPolygon& polygon,
    const WorldCollider& collider, const float spacing, int& sample_index) {
    for (std::size_t edge = 0; edge < polygon.points.size(); ++edge) {
        if (edge >= polygon.edge_normals.size() ||
            !is_walkable(polygon.edge_normals[edge])) {
            continue;
        }
        append_segment(
            out, polygon.points[edge],
            polygon.points[(edge + 1) % polygon.points.size()],
            polygon.edge_normals[edge], collider, spacing, sample_index);
    }
}

void sort_and_reindex(std::vector<SurfaceSample>& samples) {
    std::stable_sort(
        samples.begin(), samples.end(),
        [](const SurfaceSample& left, const SurfaceSample& right) {
            return std::tie(
                       left.position.y, left.position.x, left.sample_index) <
                   std::tie(
                       right.position.y, right.position.x, right.sample_index);
        });
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index].sample_index = static_cast<int>(index);
    }
}

}  // namespace

std::vector<SurfaceSample> sample_walkable_surfaces(
    const WorldCollider& collider, const float spacing) {
    std::vector<SurfaceSample> samples;
    if (!std::isfinite(spacing) || spacing <= 0.0F) {
        return samples;
    }
    int sample_index = 0;

    if (const auto* circle = std::get_if<CircleGeometry>(&collider.geometry)) {
        append_circle_arc(
            samples, circle->center, circle->radius, collider, spacing,
            sample_index);
    } else if (const auto* capsule = std::get_if<CapsuleGeometry>(&collider.geometry)) {
        append_capsule(samples, *capsule, collider, spacing, sample_index);
        sort_and_reindex(samples);
    } else if (const auto* polygon = std::get_if<ConvexPolygon>(&collider.geometry)) {
        append_polygon(samples, *polygon, collider, spacing, sample_index);
    }
    return samples;
}

}  // namespace jumpcastle
