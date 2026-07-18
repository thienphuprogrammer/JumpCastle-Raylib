#pragma once

#include "jumpcastle/math.hpp"

#include <vector>

namespace jumpcastle {

struct Mtv {
    bool overlapping{};
    Vec2 normal{};   // unit; points from polygon toward the box (push-out direction)
    float depth{};   // penetration depth along normal, >= 0
};

// SAT overlap of an axis-aligned box against a convex polygon.
// edge_normals must be outward_edge_normals(points).
[[nodiscard]] Mtv aabb_vs_convex(
    const Aabb& box,
    const std::vector<Vec2>& points,
    const std::vector<Vec2>& edge_normals);

}  // namespace jumpcastle
