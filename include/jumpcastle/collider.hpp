#pragma once

#include "jumpcastle/math.hpp"

#include <string>
#include <variant>
#include <vector>

namespace jumpcastle {

// Collision response category attached to authored and runtime colliders.
enum class ColliderType { solid, oneway, hazard };

// Authored polygon outline in map-local coordinates.
struct PolygonGeometry {
    std::vector<Vec2> points;
};

// Authored circle centered in map-local coordinates.
struct CircleGeometry {
    Vec2 center{};
    float radius{};
};

// Authored capsule joining two map-local endpoints.
struct CapsuleGeometry {
    Vec2 a{};
    Vec2 b{};
    float radius{};
};

// Geometry that can be authored directly in a map file.
using MapGeometry = std::variant<PolygonGeometry, CircleGeometry, CapsuleGeometry>;

// One authored collider, preserving its source identity and optional tag.
struct MapCollider {
    int id{};
    ColliderType type{ColliderType::solid};
    MapGeometry geometry{PolygonGeometry{}};
    std::string tag;
};

// Convex runtime piece derived from an authored polygon.
struct ConvexPolygon {
    std::vector<Vec2> points;
    std::vector<Vec2> edge_normals;
    Aabb aabb{};

    // Temporary v1/v2 compatibility bridge; Task 2 moves this to WorldCollider.
    ColliderType type{ColliderType::solid};
};

// Geometry supported by the runtime collision world.
using WorldGeometry = std::variant<ConvexPolygon, CircleGeometry, CapsuleGeometry>;

// One runtime collider or convex piece resolved from authored geometry.
struct WorldCollider {
    int id{};
    int piece_index{};
    ColliderType type{ColliderType::solid};
    WorldGeometry geometry{ConvexPolygon{}};
    Aabb aabb{};
    std::string tag;
};

// Collision information reported for a collider hit.
struct Contact {
    int collider_id{-1};
    int piece_index{};
    ColliderType type{ColliderType::solid};
    Vec2 point{};
    Vec2 normal{};
    float depth{};
};

// Returns the polygon bounds; an empty polygon returns a zero-sized box at the origin.
[[nodiscard]] Aabb geometry_aabb(const PolygonGeometry& polygon) noexcept;
// Returns the circle bounds including its radius on both axes.
[[nodiscard]] Aabb geometry_aabb(const CircleGeometry& circle) noexcept;
// Returns the bounds of both capsule endpoints expanded by its radius.
[[nodiscard]] Aabb geometry_aabb(const CapsuleGeometry& capsule) noexcept;

}  // namespace jumpcastle
