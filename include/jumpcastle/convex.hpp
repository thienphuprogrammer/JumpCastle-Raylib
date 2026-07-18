#pragma once

#include "jumpcastle/math.hpp"

#include <vector>

namespace jumpcastle {

[[nodiscard]] Vec2 polygon_centroid(const std::vector<Vec2>& points);
[[nodiscard]] std::vector<Vec2> outward_edge_normals(const std::vector<Vec2>& points);
[[nodiscard]] bool is_convex(const std::vector<Vec2>& points);
[[nodiscard]] Aabb polygon_aabb(const std::vector<Vec2>& points);
[[nodiscard]] std::vector<std::vector<Vec2>> split_to_convex(const std::vector<Vec2>& points);

}  // namespace jumpcastle
