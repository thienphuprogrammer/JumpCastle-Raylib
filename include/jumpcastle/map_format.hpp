#pragma once

#include "jumpcastle/collider.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jumpcastle {

enum class EntityType { spawn, checkpoint, goal };

struct MapEntity {
    EntityType type{EntityType::spawn};
    Vec2 pos{};
};

struct TileLayer {
    int columns{};
    int rows{};
    std::vector<std::uint32_t> gids;
};

// Map tile layers ordered from back to front for rendering.
struct TileLayers {
    TileLayer background{};
    TileLayer terrain{};
    TileLayer decor{};
    TileLayer foreground{};
};

struct ScreenMap {
    int schema_version{1};
    int index{};
    float width{};
    float height{};
    std::string biome;
    std::vector<MapCollider> colliders;
    std::vector<ConvexPolygon> polygons;
    std::vector<MapEntity> entities;
    TileLayers tiles{};
    int tileset_columns{24};
    int tileset_tile_size{16};

    // Temporary v1/v2 compatibility bridge; Task 2 migrates parser/serializer,
    // then removes these legacy runtime fields.
    TileLayer terrain{};
};

// Parses a `.map.json` screen. Validates, splits concave colliders into convex
// pieces, and precomputes edge normals + AABB per piece. Throws std::runtime_error
// on invalid input.
[[nodiscard]] ScreenMap parse_screen_map(std::string_view json, std::string_view label);

// Reads and parses a `.map.json` file. Throws std::runtime_error if the file
// cannot be opened or the contents are invalid.
[[nodiscard]] ScreenMap parse_screen_map_file(const std::filesystem::path& path);

// Serializes a screen map, retaining v1/v2 for unchanged legacy data and
// promoting authored v3-only data (tags, curved colliders, or named layers) to v3.
[[nodiscard]] std::string serialize_screen_map(const ScreenMap& map);

}  // namespace jumpcastle
