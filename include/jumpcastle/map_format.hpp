#pragma once

#include "jumpcastle/math.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jumpcastle {

enum class ColliderType { solid, oneway, hazard };
enum class EntityType { spawn, checkpoint, goal };

struct ConvexPolygon {
    std::vector<Vec2> points;
    std::vector<Vec2> edge_normals;
    Aabb aabb{};
    ColliderType type{ColliderType::solid};
};

struct MapEntity {
    EntityType type{EntityType::spawn};
    Vec2 pos{};
};

struct ScreenMap {
    int index{};
    float width{};
    float height{};
    std::string biome;
    std::vector<ConvexPolygon> polygons;
    std::vector<MapEntity> entities;
};

// Parses a `.map.json` screen. Validates, splits concave colliders into convex
// pieces, and precomputes edge normals + AABB per piece. Throws std::runtime_error
// on invalid input.
[[nodiscard]] ScreenMap parse_screen_map(std::string_view json, std::string_view label);

// Reads and parses a `.map.json` file. Throws std::runtime_error if the file
// cannot be opened or the contents are invalid.
[[nodiscard]] ScreenMap parse_screen_map_file(const std::filesystem::path& path);

[[nodiscard]] std::string serialize_screen_map(const ScreenMap& map);

}  // namespace jumpcastle
