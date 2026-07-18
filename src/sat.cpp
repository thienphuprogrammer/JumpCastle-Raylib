#include "jumpcastle/sat.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace jumpcastle {
namespace {

struct Interval { float min; float max; };

Interval project_box(const Aabb& box, const Vec2 axis) {
    const Vec2 c = aabb_center(box);
    const Vec2 h = aabb_half(box);
    const float center = dot(c, axis);
    const float radius = std::abs(axis.x) * h.x + std::abs(axis.y) * h.y;
    return {center - radius, center + radius};
}

Interval project_polygon(const std::vector<Vec2>& points, const Vec2 axis) {
    float lo = std::numeric_limits<float>::max();
    float hi = -std::numeric_limits<float>::max();
    for (const Vec2 p : points) {
        const float d = dot(p, axis);
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    return {lo, hi};
}

Vec2 centroid(const std::vector<Vec2>& points) {
    Vec2 s{};
    for (const Vec2 p : points) { s = s + p; }
    return s * (1.0F / static_cast<float>(points.size()));
}

}  // namespace

Mtv aabb_vs_convex(
    const Aabb& box,
    const std::vector<Vec2>& points,
    const std::vector<Vec2>& edge_normals) {
    std::vector<Vec2> axes = edge_normals;
    axes.push_back({1.0F, 0.0F});
    axes.push_back({0.0F, 1.0F});

    Mtv result{};
    result.depth = std::numeric_limits<float>::max();
    const Vec2 box_center = aabb_center(box);
    const Vec2 poly_center = centroid(points);

    for (const Vec2 axis : axes) {
        const Interval a = project_box(box, axis);
        const Interval b = project_polygon(points, axis);
        if (a.max <= b.min || b.max <= a.min) {
            return Mtv{};  // separating axis -> no overlap
        }
        const float overlap = std::min(a.max, b.max) - std::max(a.min, b.min);
        if (overlap < result.depth) {
            result.depth = overlap;
            Vec2 n = axis;
            if (dot(box_center - poly_center, n) < 0.0F) { n = n * -1.0F; }
            result.normal = n;
        }
    }
    result.overlapping = true;
    return result;
}

}  // namespace jumpcastle
