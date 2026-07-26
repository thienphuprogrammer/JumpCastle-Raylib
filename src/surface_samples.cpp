#include "jumpcastle/surface_samples.hpp"

#include "jumpcastle/collider.hpp"

#include <cmath>
#include <variant>

namespace jumpcastle {

namespace {

// A surface is standable when its outward normal points up steeply enough. This
// matches the runtime grounding limit (collision_world kGroundNormalY = -0.5),
// so the solver can eventually certify the same inclines the game lets you walk.
constexpr float kWalkableNormalY = -0.5F;

// Upper arc of a circle: the outward normal is (cos phi, sin phi); in screen
// space (y down) the walkable top is where sin phi <= kWalkableNormalY, i.e.
// phi in [7pi/6, 11pi/6]. Samples are spaced at most `spacing` in arc length.
void append_arc(
    std::vector<SurfaceSample>& out, const Vec2 center, const float radius,
    const int collider_id, const int piece_index, const float spacing,
    int& sample_index) {
    if (radius <= 0.0F) {
        return;
    }
    const float pi = static_cast<float>(M_PI);
    const float start = 7.0F * pi / 6.0F;
    const float end = 11.0F * pi / 6.0F;
    const float step = spacing / radius;  // arc-length spacing -> angular step
    const int steps = std::max(1, static_cast<int>(std::ceil((end - start) / step)));
    for (int i = 0; i <= steps; ++i) {
        const float phi = start + (end - start) * (static_cast<float>(i) /
                                                   static_cast<float>(steps));
        const Vec2 normal{std::cos(phi), std::sin(phi)};
        if (normal.y > kWalkableNormalY) {
            continue;  // guard float rounding at the arc endpoints (sin = -0.5)
        }
        out.push_back({
            .position = {center.x + radius * normal.x, center.y + radius * normal.y},
            .normal = normal,
            .collider_id = collider_id,
            .piece_index = piece_index,
            .sample_index = sample_index++,
        });
    }
}

// Even sampling of a straight segment [a, b] with a fixed normal.
void append_segment(
    std::vector<SurfaceSample>& out, const Vec2 a, const Vec2 b,
    const Vec2 normal, const int collider_id, const int piece_index,
    const float spacing, int& sample_index) {
    const Vec2 d{b.x - a.x, b.y - a.y};
    const float length = std::sqrt(d.x * d.x + d.y * d.y);
    const int steps = std::max(1, static_cast<int>(std::ceil(length / spacing)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        out.push_back({
            .position = {a.x + d.x * t, a.y + d.y * t},
            .normal = normal,
            .collider_id = collider_id,
            .piece_index = piece_index,
            .sample_index = sample_index++,
        });
    }
}

}  // namespace

std::vector<SurfaceSample> sample_walkable_surfaces(
    const WorldCollider& collider, const float spacing) {
    std::vector<SurfaceSample> samples;
    if (spacing <= 0.0F) {
        return samples;
    }
    int sample_index = 0;

    if (const auto* circle = std::get_if<CircleGeometry>(&collider.geometry)) {
        append_arc(samples, circle->center, circle->radius, collider.id,
                   collider.piece_index, spacing, sample_index);
    } else if (const auto* capsule = std::get_if<CapsuleGeometry>(&collider.geometry)) {
        // Walkable capsule top = the segment between the endpoints, offset by the
        // radius along the up-facing perpendicular, plus each end cap's upper arc.
        const Vec2 axis{capsule->b.x - capsule->a.x, capsule->b.y - capsule->a.y};
        const float length = std::sqrt(axis.x * axis.x + axis.y * axis.y);
        Vec2 up_normal{0.0F, -1.0F};
        if (length > 1e-4F) {
            const Vec2 perp{-axis.y / length, axis.x / length};
            up_normal = (perp.y <= 0.0F) ? perp : Vec2{-perp.x, -perp.y};
        }
        const Vec2 top_a{capsule->a.x + up_normal.x * capsule->radius,
                         capsule->a.y + up_normal.y * capsule->radius};
        const Vec2 top_b{capsule->b.x + up_normal.x * capsule->radius,
                         capsule->b.y + up_normal.y * capsule->radius};
        append_segment(samples, top_a, top_b, up_normal, collider.id,
                       collider.piece_index, spacing, sample_index);
        append_arc(samples, capsule->a, capsule->radius, collider.id,
                   collider.piece_index, spacing, sample_index);
        append_arc(samples, capsule->b, capsule->radius, collider.id,
                   collider.piece_index, spacing, sample_index);
    } else if (const auto* polygon = std::get_if<ConvexPolygon>(&collider.geometry)) {
        const auto& points = polygon->points;
        for (std::size_t edge = 0; edge < points.size(); ++edge) {
            if (edge >= polygon->edge_normals.size() ||
                polygon->edge_normals[edge].y > kWalkableNormalY) {
                continue;  // not an upward-facing (walkable) edge
            }
            append_segment(samples, points[edge], points[(edge + 1) % points.size()],
                           polygon->edge_normals[edge], collider.id,
                           collider.piece_index, spacing, sample_index);
        }
    }
    return samples;
}

}  // namespace jumpcastle
