#include "jumpcastle/map_format.hpp"

#include "jumpcastle/convex.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

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

void validate_point(const Vec2 point, const std::string& label) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::runtime_error(label + " must contain finite coordinates");
    }
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

PolygonGeometry parse_polygon_geometry(const Json& value, const std::string& label) {
    auto points = parse_points(required(value, "points", label), label + ".points");
    for (const Vec2 point : points) {
        validate_point(point, label + ".points");
    }
    return {std::move(points)};
}

void validate_curved_geometry(
    const float radius, const ColliderType type, const std::string& label) {
    if (!std::isfinite(radius) || radius < 0.5F) {
        throw std::runtime_error(label + " radius must be at least 0.5");
    }
    if (type == ColliderType::oneway) {
        throw std::runtime_error(label + " curved geometry cannot be oneway");
    }
}

CircleGeometry parse_circle_geometry(
    const Json& value, const ColliderType type, const std::string& label) {
    const Vec2 center = parse_point(required(value, "center", label), label + ".center");
    const float radius = required(value, "radius", label).get<float>();
    validate_point(center, label + ".center");
    validate_curved_geometry(radius, type, label);
    return {center, radius};
}

CapsuleGeometry parse_capsule_geometry(
    const Json& value, const ColliderType type, const std::string& label) {
    const Vec2 a = parse_point(required(value, "a", label), label + ".a");
    const Vec2 b = parse_point(required(value, "b", label), label + ".b");
    const float radius = required(value, "radius", label).get<float>();
    validate_point(a, label + ".a");
    validate_point(b, label + ".b");
    if (a == b) {
        throw std::runtime_error(label + " capsule endpoints must differ");
    }
    validate_curved_geometry(radius, type, label);
    return {a, b, radius};
}

MapGeometry parse_geometry(
    const Json& value, const std::string& geometry_name,
    const ColliderType type, const std::string& label) {
    if (geometry_name == "polygon") {
        return parse_polygon_geometry(value, label);
    }
    if (geometry_name == "circle") {
        return parse_circle_geometry(value, type, label);
    }
    if (geometry_name == "capsule") {
        return parse_capsule_geometry(value, type, label);
    }
    throw std::runtime_error(label + " has unknown geometry '" + geometry_name + "'");
}

MapCollider parse_collider(const Json& value, const std::size_t index, const int version) {
    const int id = value.value("id", static_cast<int>(index + 1));
    const std::string label =
        "colliders[" + std::to_string(index) + "] collider id " + std::to_string(id);
    const ColliderType type =
        collider_type_from_string(required(value, "type", label).get<std::string>(), label);
    const std::string geometry_name =
        version >= 3 ? required(value, "geometry", label).get<std::string>() : "polygon";
    const std::string tag = version < 3 && value.value("shape", "") == "slope"
        ? "slope"
        : value.value("tag", "");

    MapGeometry geometry = parse_geometry(value, geometry_name, type, label);
    return {.id = id, .type = type, .geometry = std::move(geometry), .tag = tag};
}

void append_legacy_polygons(const MapCollider& collider, std::vector<ConvexPolygon>& out) {
    const auto* polygon = std::get_if<PolygonGeometry>(&collider.geometry);
    if (polygon == nullptr) {
        return;
    }
    for (const auto& piece : split_to_convex(polygon->points)) {
        out.push_back({piece, outward_edge_normals(piece), polygon_aabb(piece), collider.type});
    }
}

void validate_bounds(const Aabb& bounds, const float width, const float height, const std::string& label) {
    if (bounds.min.x < 0.0F || bounds.min.y < 0.0F ||
        bounds.max.x > width || bounds.max.y > height) {
        throw std::runtime_error(label + " is outside screen bounds");
    }
}

ScreenMap parse_screen_metadata(const Json& root, const int version) {
    const Json& screen = required(root, "screen", "map");
    ScreenMap map;
    map.schema_version = version;
    map.index = required(screen, "index", "screen").get<int>();
    map.width = required(screen, "width", "screen").get<float>();
    map.height = required(screen, "height", "screen").get<float>();
    if (map.index < 0) {
        throw std::runtime_error("screen.index cannot be negative");
    }
    if (!std::isfinite(map.width) || !std::isfinite(map.height) ||
        map.width <= 0.0F || map.height <= 0.0F) {
        throw std::runtime_error("screen dimensions must be positive");
    }
    map.biome = required(root, "biome", "map").get<std::string>();
    return map;
}

void parse_colliders(const Json& root, const int version, ScreenMap& map) {
    const Json& colliders = required(root, "colliders", "map");
    if (!colliders.is_array()) {
        throw std::runtime_error("map.colliders must be an array");
    }
    std::unordered_set<int> collider_ids;
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        MapCollider collider = parse_collider(colliders.at(i), i, version);
        if (!collider_ids.insert(collider.id).second) {
            throw std::runtime_error("duplicate collider id " + std::to_string(collider.id));
        }
        validate_bounds(
            std::visit([](const auto& geometry) { return geometry_aabb(geometry); }, collider.geometry),
            map.width, map.height, "collider id " + std::to_string(collider.id));
        append_legacy_polygons(collider, map.polygons);
        map.colliders.push_back(std::move(collider));
    }
}

MapEntity parse_entity(const Json& value, std::size_t index) {
    const std::string label = "entities[" + std::to_string(index) + "]";
    MapEntity entity;
    entity.type = entity_type_from_string(required(value, "type", label).get<std::string>(), label);
    entity.pos = parse_point(required(value, "pos", label), label + ".pos");
    return entity;
}

void parse_entities(const Json& root, ScreenMap& map) {
    const Json& entities = required(root, "entities", "map");
    if (!entities.is_array()) {
        throw std::runtime_error("map.entities must be an array");
    }
    for (std::size_t i = 0; i < entities.size(); ++i) {
        MapEntity entity = parse_entity(entities.at(i), i);
        validate_point(entity.pos, "entities[" + std::to_string(i) + "].pos");
        if (entity.pos.x < 0.0F || entity.pos.y < 0.0F ||
            entity.pos.x > map.width || entity.pos.y > map.height) {
            throw std::runtime_error(
                "entities[" + std::to_string(i) + "] is outside screen bounds");
        }
        map.entities.push_back(entity);
    }
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

void parse_tileset(const Json& root, ScreenMap& map) {
    if (!root.contains("tileset") || !root.at("tileset").is_object()) {
        return;
    }
    const Json& tileset = root.at("tileset");
    if (tileset.contains("columns")) {
        map.tileset_columns = tileset.at("columns").get<int>();
        if (map.tileset_columns <= 0) {
            throw std::runtime_error("map.tileset.columns must be positive");
        }
    }
    if (tileset.contains("tile_size")) {
        map.tileset_tile_size = tileset.at("tile_size").get<int>();
        if (map.tileset_tile_size <= 0) {
            throw std::runtime_error("map.tileset.tile_size must be positive");
        }
    }
}

void parse_named_tile_layer(
    const Json& tiles, const char* name, const ScreenMap& map, TileLayer& destination) {
    if (!tiles.contains(name)) {
        return;
    }
    destination = parse_tile_layer(
        tiles.at(name), static_cast<int>(map.width), static_cast<int>(map.height),
        std::string{"map.tiles."} + name);
}

void parse_tile_layers(const Json& root, ScreenMap& map) {
    if (!root.contains("tiles") || !root.at("tiles").is_object()) {
        return;
    }
    const Json& tiles = root.at("tiles");
    parse_named_tile_layer(tiles, "background", map, map.tiles.background);
    parse_named_tile_layer(tiles, "terrain", map, map.tiles.terrain);
    parse_named_tile_layer(tiles, "decor", map, map.tiles.decor);
    parse_named_tile_layer(tiles, "foreground", map, map.tiles.foreground);
    map.terrain = map.tiles.terrain;
}

Json serialize_tile_layer(const TileLayer& layer) {
    Json grid = Json::array();
    for (int row = 0; row < layer.rows; ++row) {
        Json values = Json::array();
        for (int column = 0; column < layer.columns; ++column) {
            values.push_back(layer.gids[
                static_cast<std::size_t>(row) * static_cast<std::size_t>(layer.columns) +
                static_cast<std::size_t>(column)]);
        }
        grid.push_back(std::move(values));
    }
    return grid;
}

Json serialize_collider(const MapCollider& collider) {
    Json value{
        {"id", collider.id},
        {"type", collider_type_to_string(collider.type)},
    };
    if (!collider.tag.empty()) {
        value["tag"] = collider.tag;
    }
    std::visit([&](const auto& geometry) {
        using T = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<T, PolygonGeometry>) {
            value["geometry"] = "polygon";
            value["points"] = Json::array();
            for (const Vec2 point : geometry.points) {
                value["points"].push_back({point.x, point.y});
            }
        } else if constexpr (std::is_same_v<T, CircleGeometry>) {
            value["geometry"] = "circle";
            value["center"] = {geometry.center.x, geometry.center.y};
            value["radius"] = geometry.radius;
        } else {
            value["geometry"] = "capsule";
            value["a"] = {geometry.a.x, geometry.a.y};
            value["b"] = {geometry.b.x, geometry.b.y};
            value["radius"] = geometry.radius;
        }
    }, collider.geometry);
    return value;
}

bool has_tiles(const TileLayer& layer) noexcept {
    return !layer.gids.empty();
}

bool has_optional_layers(const ScreenMap& map) noexcept {
    return has_tiles(map.tiles.background) || has_tiles(map.tiles.decor) ||
        has_tiles(map.tiles.foreground);
}

bool has_curved_colliders(const ScreenMap& map) noexcept {
    return std::any_of(map.colliders.begin(), map.colliders.end(), [](const MapCollider& collider) {
        return !std::holds_alternative<PolygonGeometry>(collider.geometry);
    });
}

bool has_authored_tags(const ScreenMap& map) noexcept {
    return std::any_of(map.colliders.begin(), map.colliders.end(), [](const MapCollider& collider) {
        return !collider.tag.empty();
    });
}

int serialization_schema_version(const ScreenMap& map, const TileLayer& terrain) noexcept {
    const bool legacy_data = map.schema_version <= 2 && !has_optional_layers(map) &&
        !has_curved_colliders(map) && !has_authored_tags(map);
    return legacy_data ? (has_tiles(terrain) ? 2 : 1) : 3;
}

Json serialize_legacy_collider(const MapCollider& collider, const int version) {
    const auto* polygon = std::get_if<PolygonGeometry>(&collider.geometry);
    if (polygon == nullptr) {
        return nullptr;
    }
    Json points = Json::array();
    for (const Vec2 point : polygon->points) {
        points.push_back({point.x, point.y});
    }
    Json value{{"id", collider.id}, {"type", collider_type_to_string(collider.type)},
               {"points", std::move(points)}};
    if (version == 2 && collider.tag == "slope") {
        value["shape"] = "slope";
    }
    return value;
}

Json serialize_colliders(const ScreenMap& map, const int version) {
    Json colliders = Json::array();
    if (!map.colliders.empty()) {
        for (const MapCollider& collider : map.colliders) {
            Json value = version == 3 ? serialize_collider(collider)
                                      : serialize_legacy_collider(collider, version);
            if (!value.is_null()) {
                colliders.push_back(std::move(value));
            }
        }
        return colliders;
    }

    for (std::size_t i = 0; i < map.polygons.size(); ++i) {
        const ConvexPolygon& polygon = map.polygons[i];
        const MapCollider collider{
            .id = static_cast<int>(i + 1),
            .type = polygon.type,
            .geometry = PolygonGeometry{polygon.points},
        };
        colliders.push_back(version == 3 ? serialize_collider(collider)
                                         : serialize_legacy_collider(collider, version));
    }
    return colliders;
}

Json serialize_tiles(const ScreenMap& map, const TileLayer& terrain) {
    Json tiles = Json::object();
    if (has_tiles(map.tiles.background)) {
        tiles["background"] = serialize_tile_layer(map.tiles.background);
    }
    if (has_tiles(terrain)) {
        tiles["terrain"] = serialize_tile_layer(terrain);
    }
    if (has_tiles(map.tiles.decor)) {
        tiles["decor"] = serialize_tile_layer(map.tiles.decor);
    }
    if (has_tiles(map.tiles.foreground)) {
        tiles["foreground"] = serialize_tile_layer(map.tiles.foreground);
    }
    return tiles;
}

}  // namespace

ScreenMap parse_screen_map(const std::string_view json, const std::string_view label) {
    try {
        const Json root = Json::parse(json);
        const int version = required(root, "schema_version", "map").get<int>();
        if (version != 1 && version != 2 && version != 3) {
            throw std::runtime_error("unsupported map schema_version");
        }
        ScreenMap map = parse_screen_metadata(root, version);
        parse_colliders(root, version, map);
        parse_entities(root, map);
        parse_tileset(root, map);
        parse_tile_layers(root, map);
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
    const TileLayer& terrain = has_tiles(map.tiles.terrain) ? map.tiles.terrain : map.terrain;
    const bool include_tiles = has_tiles(terrain) || has_optional_layers(map);
    const int version = serialization_schema_version(map, terrain);

    Json root;
    root["schema_version"] = version;
    root["screen"] = {{"index", map.index}, {"width", map.width}, {"height", map.height}};
    root["biome"] = map.biome;
    root["colliders"] = serialize_colliders(map, version);

    Json entities = Json::array();
    for (const MapEntity& entity : map.entities) {
        entities.push_back({
            {"type", entity_type_to_string(entity.type)},
            {"pos", {entity.pos.x, entity.pos.y}},
        });
    }
    root["entities"] = entities;

    if (include_tiles) {
        root["tileset"] = {
            {"name", "castle"},
            {"columns", map.tileset_columns},
            {"tile_size", map.tileset_tile_size},
        };
        root["tiles"] = serialize_tiles(map, terrain);
    }

    return root.dump(2);
}

}  // namespace jumpcastle
