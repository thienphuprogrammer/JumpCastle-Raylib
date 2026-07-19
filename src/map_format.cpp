#include "jumpcastle/map_format.hpp"

#include "jumpcastle/convex.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace jumpcastle {
namespace {

using Json = nlohmann::json;

const Json& required(const Json& object, const char* key, const std::string& label) {
    if (!object.is_object() || !object.contains(key)) {
        throw std::runtime_error(label + " is missing '" + key + "'");
    }
    return object.at(key);
}

ColliderType collider_type_from_string(const std::string& value, const std::string& label) {
    if (value == "solid") { return ColliderType::solid; }
    if (value == "oneway") { return ColliderType::oneway; }
    if (value == "hazard") { return ColliderType::hazard; }
    throw std::runtime_error(label + " has unknown collider type '" + value + "'");
}

const char* collider_type_to_string(const ColliderType type) noexcept {
    switch (type) {
    case ColliderType::solid: return "solid";
    case ColliderType::oneway: return "oneway";
    case ColliderType::hazard: return "hazard";
    }
    return "solid";
}

EntityType entity_type_from_string(const std::string& value, const std::string& label) {
    if (value == "spawn") { return EntityType::spawn; }
    if (value == "checkpoint") { return EntityType::checkpoint; }
    if (value == "goal") { return EntityType::goal; }
    throw std::runtime_error(label + " has unknown entity type '" + value + "'");
}

const char* entity_type_to_string(const EntityType type) noexcept {
    switch (type) {
    case EntityType::spawn: return "spawn";
    case EntityType::checkpoint: return "checkpoint";
    case EntityType::goal: return "goal";
    }
    return "spawn";
}

Vec2 parse_point(const Json& value, const std::string& label) {
    if (!value.is_array() || value.size() != 2) {
        throw std::runtime_error(label + " must be a [x, y] pair");
    }
    return {value.at(0).get<float>(), value.at(1).get<float>()};
}

std::vector<Vec2> parse_points(const Json& value, const std::string& label) {
    if (!value.is_array()) {
        throw std::runtime_error(label + " must be an array of points");
    }
    std::vector<Vec2> points;
    points.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        points.push_back(parse_point(value.at(i), label + "[" + std::to_string(i) + "]"));
    }
    if (points.size() < 3) {
        throw std::runtime_error(label + " must have at least three points");
    }
    return points;
}

void parse_collider(const Json& value, std::size_t index, std::vector<ConvexPolygon>& out) {
    const std::string label = "colliders[" + std::to_string(index) + "]";
    const ColliderType type =
        collider_type_from_string(required(value, "type", label).get<std::string>(), label);
    const std::vector<Vec2> points = parse_points(required(value, "points", label), label + ".points");
    for (const auto& piece : split_to_convex(points)) {
        out.push_back({piece, outward_edge_normals(piece), polygon_aabb(piece), type});
    }
}

MapEntity parse_entity(const Json& value, std::size_t index) {
    const std::string label = "entities[" + std::to_string(index) + "]";
    MapEntity entity;
    entity.type = entity_type_from_string(required(value, "type", label).get<std::string>(), label);
    entity.pos = parse_point(required(value, "pos", label), label + ".pos");
    return entity;
}

TileLayer parse_tile_layer(
    const Json& grid, const int width, const int height, const std::string& label) {
    if (!grid.is_array()) {
        throw std::runtime_error(label + " must be an array of rows");
    }
    if (static_cast<int>(grid.size()) != height) {
        throw std::runtime_error(label + " row count must equal screen height");
    }
    TileLayer layer;
    layer.columns = width;
    layer.rows = height;
    layer.gids.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t r = 0; r < grid.size(); ++r) {
        const Json& row = grid.at(r);
        if (!row.is_array() || static_cast<int>(row.size()) != width) {
            throw std::runtime_error(
                label + " row " + std::to_string(r) + " length must equal screen width");
        }
        for (const auto& cell : row) {
            layer.gids.push_back(cell.get<std::uint32_t>());
        }
    }
    return layer;
}

}  // namespace

ScreenMap parse_screen_map(const std::string_view json, const std::string_view label) {
    try {
        const Json root = Json::parse(json);
        const int version = required(root, "schema_version", "map").get<int>();
        if (version != 1 && version != 2) {
            throw std::runtime_error("unsupported map schema_version");
        }
        const Json& screen = required(root, "screen", "map");
        ScreenMap map;
        map.index = required(screen, "index", "screen").get<int>();
        map.width = required(screen, "width", "screen").get<float>();
        map.height = required(screen, "height", "screen").get<float>();
        map.biome = required(root, "biome", "map").get<std::string>();

        const Json& colliders = required(root, "colliders", "map");
        if (!colliders.is_array()) {
            throw std::runtime_error("map.colliders must be an array");
        }
        for (std::size_t i = 0; i < colliders.size(); ++i) {
            parse_collider(colliders.at(i), i, map.polygons);
        }

        const Json& entities = required(root, "entities", "map");
        if (!entities.is_array()) {
            throw std::runtime_error("map.entities must be an array");
        }
        for (std::size_t i = 0; i < entities.size(); ++i) {
            map.entities.push_back(parse_entity(entities.at(i), i));
        }

        if (root.contains("tileset") && root.at("tileset").is_object()) {
            const Json& tileset = root.at("tileset");
            if (tileset.contains("columns")) {
                map.tileset_columns = tileset.at("columns").get<int>();
            }
            if (tileset.contains("tile_size")) {
                map.tileset_tile_size = tileset.at("tile_size").get<int>();
            }
        }
        if (root.contains("tiles") && root.at("tiles").is_object()) {
            const Json& tiles = root.at("tiles");
            if (tiles.contains("terrain")) {
                map.terrain = parse_tile_layer(
                    tiles.at("terrain"), static_cast<int>(map.width),
                    static_cast<int>(map.height), "map.tiles.terrain");
            }
        }
        return map;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string{label} + ": " + error.what());
    }
}

ScreenMap parse_screen_map_file(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("unable to open map file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse_screen_map(buffer.str(), path.string());
}

std::string serialize_screen_map(const ScreenMap& map) {
    Json root;
    root["schema_version"] = map.terrain.gids.empty() ? 1 : 2;
    root["screen"] = {{"index", map.index}, {"width", map.width}, {"height", map.height}};
    root["biome"] = map.biome;

    Json colliders = Json::array();
    for (std::size_t i = 0; i < map.polygons.size(); ++i) {
        const ConvexPolygon& polygon = map.polygons[i];
        Json points = Json::array();
        for (const Vec2 p : polygon.points) {
            points.push_back({p.x, p.y});
        }
        colliders.push_back({
            {"id", static_cast<int>(i + 1)},
            {"type", collider_type_to_string(polygon.type)},
            {"points", points},
        });
    }
    root["colliders"] = colliders;

    Json entities = Json::array();
    for (const MapEntity& entity : map.entities) {
        entities.push_back({
            {"type", entity_type_to_string(entity.type)},
            {"pos", {entity.pos.x, entity.pos.y}},
        });
    }
    root["entities"] = entities;

    if (!map.terrain.gids.empty()) {
        root["tileset"] = {
            {"name", "castle"},
            {"columns", map.tileset_columns},
            {"tile_size", map.tileset_tile_size},
        };
        Json terrain = Json::array();
        for (int r = 0; r < map.terrain.rows; ++r) {
            Json row = Json::array();
            for (int c = 0; c < map.terrain.columns; ++c) {
                row.push_back(
                    map.terrain.gids[static_cast<std::size_t>(r) * map.terrain.columns + c]);
            }
            terrain.push_back(row);
        }
        root["tiles"] = {{"terrain", terrain}};
    }

    return root.dump(2);
}

}  // namespace jumpcastle
