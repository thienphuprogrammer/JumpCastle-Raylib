#include "jumpcastle/map_format.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace jumpcastle;
using Catch::Approx;

namespace {
constexpr const char* kValid = R"({
  "schema_version": 1,
  "screen": { "index": 5, "width": 16.0, "height": 15.0 },
  "biome": "courtyard",
  "colliders": [
    { "id": 1, "type": "solid",  "points": [[0,14],[16,14],[16,15],[0,15]] },
    { "id": 2, "type": "oneway", "points": [[4,10],[8,10],[8,10.5],[4,10.5]] },
    { "id": 3, "type": "hazard", "points": [[2,13],[3,13],[2.5,12]] }
  ],
  "entities": [ { "type": "spawn", "pos": [2.5, 13.0] } ]
})";
}  // namespace

TEST_CASE("schema v3 collider types retain authored geometry") {
    const PolygonGeometry polygon{{{1.0F, 2.0F}, {5.0F, 2.0F}, {5.0F, 4.0F}}};
    const CircleGeometry circle{{8.0F, 6.0F}, 2.0F};
    const CapsuleGeometry capsule{{3.0F, 9.0F}, {11.0F, 9.0F}, 1.5F};

    const MapCollider a{.id = 1, .type = ColliderType::solid,
                        .geometry = polygon, .tag = "slope"};
    const MapCollider b{.id = 2, .type = ColliderType::hazard,
                        .geometry = circle, .tag = "orb"};
    const MapCollider c{.id = 3, .type = ColliderType::solid,
                        .geometry = capsule, .tag = "bridge"};

    CHECK(std::holds_alternative<PolygonGeometry>(a.geometry));
    CHECK(std::get<CircleGeometry>(b.geometry).radius == Approx(2.0F));
    CHECK(std::get<CapsuleGeometry>(c.geometry).b.x == Approx(11.0F));
}

TEST_CASE("geometry_aabb returns an empty box for empty polygons") {
    const Aabb box = geometry_aabb(PolygonGeometry{});

    CHECK(box.min == Vec2{});
    CHECK(box.max == Vec2{});
}

TEST_CASE("geometry_aabb bounds every supported geometry") {
    const Aabb polygon = geometry_aabb(
        PolygonGeometry{{{-1.0F, 2.0F}, {5.0F, 3.0F}, {3.0F, -4.0F}}});
    CHECK(polygon.min == Vec2{-1.0F, -4.0F});
    CHECK(polygon.max == Vec2{5.0F, 3.0F});

    const Aabb circle = geometry_aabb(CircleGeometry{{8.0F, 6.0F}, 2.0F});
    CHECK(circle.min == Vec2{6.0F, 4.0F});
    CHECK(circle.max == Vec2{10.0F, 8.0F});

    const Aabb capsule =
        geometry_aabb(CapsuleGeometry{{3.0F, 9.0F}, {11.0F, 9.0F}, 1.5F});
    CHECK(capsule.min == Vec2{1.5F, 7.5F});
    CHECK(capsule.max == Vec2{12.5F, 10.5F});
}

TEST_CASE("schema v3 parses polygon circle and capsule geometry", "[map_format]") {
    const auto map = parse_screen_map(R"json({
      "schema_version": 3,
      "screen": {"index": 2, "width": 28, "height": 36},
      "biome": "frosted_keep",
      "colliders": [
        {"id": 10, "type": "solid", "geometry": "polygon",
         "tag": "slope", "points": [[2,20],[8,16],[8,20]]},
        {"id": 11, "type": "hazard", "geometry": "circle",
         "tag": "orb", "center": [14,18], "radius": 2},
        {"id": 12, "type": "solid", "geometry": "capsule",
         "tag": "bridge", "a": [4,10], "b": [18,10], "radius": 1}
      ],
      "entities": []
    })json", "shape-map");

    REQUIRE(map.schema_version == 3);
    REQUIRE(map.colliders.size() == 3);
    CHECK(std::get<PolygonGeometry>(map.colliders[0].geometry).points.size() == 3);
    CHECK(std::get<CircleGeometry>(map.colliders[1].geometry).radius == Approx(2.0F));
    CHECK(std::get<CapsuleGeometry>(map.colliders[2].geometry).a.x == Approx(4.0F));
}

TEST_CASE("schema v3 rejects invalid curved geometry with collider id", "[map_format]") {
    CHECK_THROWS_WITH(
        parse_screen_map(R"json({
          "schema_version": 3,
          "screen": {"index": 0, "width": 28, "height": 36},
          "biome": "courtyard",
          "colliders": [
            {"id": 7, "type": "solid", "geometry": "circle",
             "center": [4,4], "radius": 0.25}
          ],
          "entities": []
        })json", "bad-circle"),
        Catch::Matchers::ContainsSubstring("collider id 7") &&
        Catch::Matchers::ContainsSubstring("radius must be at least 0.5"));
}

TEST_CASE("schema v2 slope migrates to polygon geometry and slope tag", "[map_format]") {
    const auto map = parse_screen_map(R"json({
      "schema_version": 2,
      "screen": {"index": 0, "width": 28, "height": 36},
      "biome": "courtyard",
      "colliders": [
        {"id": 1, "type": "solid", "shape": "slope",
         "points": [[2,20],[8,16],[8,20]]}
      ],
      "entities": []
    })json", "legacy-slope");

    REQUIRE(map.colliders.size() == 1);
    CHECK(map.colliders[0].tag == "slope");
    CHECK(std::holds_alternative<PolygonGeometry>(map.colliders[0].geometry));
}

TEST_CASE("tileless v2 slope round-trips with its authored tag", "[map_format]") {
    const ScreenMap map = parse_screen_map(R"json({
      "schema_version": 2,
      "screen": {"index": 0, "width": 28, "height": 36},
      "biome": "courtyard",
      "colliders": [
        {"id": 1, "type": "solid", "shape": "slope",
         "points": [[2,20],[8,16],[8,20]]}
      ],
      "entities": []
    })json", "tileless-slope");

    const std::string json = serialize_screen_map(map);
    const ScreenMap round_trip = parse_screen_map(json, "tileless-slope-round-trip");

    CHECK(json.find("\"schema_version\": 3") != std::string::npos);
    REQUIRE(round_trip.colliders.size() == 1);
    CHECK(round_trip.colliders[0].tag == "slope");
    CHECK(std::holds_alternative<PolygonGeometry>(round_trip.colliders[0].geometry));
}

TEST_CASE("schema v3 serializes and round-trips authored collider data", "[map_format]") {
    const ScreenMap map = parse_screen_map(R"json({
      "schema_version": 3,
      "screen": {"index": 2, "width": 28, "height": 36},
      "biome": "frosted_keep",
      "colliders": [
        {"id": 10, "type": "solid", "geometry": "polygon",
         "tag": "slope", "points": [[2,20],[8,16],[8,20]]},
        {"id": 11, "type": "hazard", "geometry": "circle",
         "tag": "orb", "center": [14,18], "radius": 2},
        {"id": 12, "type": "solid", "geometry": "capsule",
         "tag": "bridge", "a": [4,10], "b": [18,10], "radius": 1}
      ],
      "entities": []
    })json", "shape-map");

    const ScreenMap round_trip = parse_screen_map(serialize_screen_map(map), "round-trip");

    REQUIRE(round_trip.schema_version == 3);
    REQUIRE(round_trip.colliders.size() == 3);
    CHECK(round_trip.colliders[1].id == 11);
    CHECK(round_trip.colliders[1].tag == "orb");
    CHECK(std::get<CircleGeometry>(round_trip.colliders[1].geometry).radius == Approx(2.0F));
    CHECK(std::get<CapsuleGeometry>(round_trip.colliders[2].geometry).b.x == Approx(18.0F));
}

TEST_CASE("v3 serializes authored polygon geometry", "[map_format]") {
    ScreenMap map;
    map.schema_version = 3;
    map.width = 8.0F;
    map.height = 6.0F;
    map.biome = "courtyard";
    const std::vector<Vec2> points{{0.0F, 5.0F}, {8.0F, 5.0F}, {8.0F, 6.0F}, {0.0F, 6.0F}};
    map.colliders.push_back({
        .id = 1,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{points},
    });

    const std::string json = serialize_screen_map(map);
    const ScreenMap round_trip = parse_screen_map(json, "polygon-fallback");

    CHECK(json.find("\"geometry\": \"polygon\"") != std::string::npos);
    REQUIRE(round_trip.colliders.size() == 1);
    CHECK(std::holds_alternative<PolygonGeometry>(round_trip.colliders[0].geometry));
}

TEST_CASE("non-legacy collider tag promotes serialization to v3", "[map_format]") {
    ScreenMap map;
    map.width = 8.0F;
    map.height = 6.0F;
    map.biome = "courtyard";
    map.colliders.push_back({
        .id = 7,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{{{0.0F, 5.0F}, {8.0F, 5.0F}, {8.0F, 6.0F}}},
        .tag = "moving-platform",
    });

    const std::string json = serialize_screen_map(map);
    const ScreenMap round_trip = parse_screen_map(json, "tagged-collider");

    CHECK(json.find("\"schema_version\": 3") != std::string::npos);
    REQUIRE(round_trip.colliders.size() == 1);
    CHECK(round_trip.colliders[0].tag == "moving-platform");
}

TEST_CASE("all named layers round-trip and optional layers promote to v3", "[map_format]") {
    ScreenMap map;
    map.width = 2.0F;
    map.height = 2.0F;
    map.biome = "courtyard";
    map.tileset_columns = 21;
    map.tileset_tile_size = 16;
    map.tiles.background = {2, 2, {1u, 2u, 3u, 4u}};
    map.tiles.terrain = {2, 2, {5u, 6u, 7u, 8u}};
    map.tiles.decor = {2, 2, {9u, 10u, 11u, 12u}};
    map.tiles.foreground = {2, 2, {13u, 14u, 15u, 16u}};

    const std::string json = serialize_screen_map(map);
    const ScreenMap round_trip = parse_screen_map(json, "named-layers");

    CHECK(json.find("\"schema_version\": 3") != std::string::npos);
    CHECK(round_trip.schema_version == 3);
    CHECK(round_trip.tiles.background.gids == map.tiles.background.gids);
    CHECK(round_trip.tiles.terrain.gids == map.tiles.terrain.gids);
    CHECK(round_trip.tiles.decor.gids == map.tiles.decor.gids);
    CHECK(round_trip.tiles.foreground.gids == map.tiles.foreground.gids);
    CHECK(round_trip.terrain.gids == map.tiles.terrain.gids);
}

TEST_CASE("parse_screen_map reads colliders, types, entities") {
    const ScreenMap map = parse_screen_map(kValid, "test");
    REQUIRE(map.index == 5);
    REQUIRE(map.polygons.size() == 3);
    REQUIRE(map.polygons[0].type == ColliderType::solid);
    REQUIRE(map.polygons[1].type == ColliderType::oneway);
    REQUIRE(map.polygons[2].type == ColliderType::hazard);
    REQUIRE(map.polygons[0].edge_normals.size() == map.polygons[0].points.size());
    REQUIRE(map.entities.size() == 1);
    REQUIRE(map.entities[0].type == EntityType::spawn);
}

TEST_CASE("parse rejects a polygon with fewer than three points") {
    const char* bad = R"({"schema_version":1,"screen":{"index":0,"width":16,"height":15},
      "biome":"courtyard","colliders":[{"id":1,"type":"solid","points":[[0,0],[1,1]]}],"entities":[]})";
    REQUIRE_THROWS_AS(parse_screen_map(bad, "bad"), std::runtime_error);
}

TEST_CASE("concave collider is split into multiple convex polygons") {
    const char* concave = R"({"schema_version":1,"screen":{"index":0,"width":16,"height":15},
      "biome":"courtyard","colliders":[{"id":1,"type":"solid","points":[[0,0],[4,2],[0,4],[1,2]]}],"entities":[]})";
    const ScreenMap map = parse_screen_map(concave, "concave");
    REQUIRE(map.polygons.size() >= 2);
}

TEST_CASE("serialize then parse round-trips collider count and types") {
    const ScreenMap map = parse_screen_map(kValid, "test");
    const ScreenMap again = parse_screen_map(serialize_screen_map(map), "roundtrip");
    REQUIRE(again.polygons.size() == map.polygons.size());
    REQUIRE(again.entities.size() == map.entities.size());
    REQUIRE(again.index == map.index);
}

TEST_CASE("parse_screen_map_file reads a .map.json from disk") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "jumpcastle_screen_test.map.json";
    {
        std::ofstream out{path};
        out << serialize_screen_map(parse_screen_map(kValid, "seed"));
    }
    const ScreenMap map = parse_screen_map_file(path);
    REQUIRE(map.index == 5);
    REQUIRE(map.polygons.size() == 3);
    std::filesystem::remove(path);
}

TEST_CASE("parse_screen_map_file throws on a missing file") {
    REQUIRE_THROWS_AS(
        parse_screen_map_file("/nonexistent/jumpcastle/does-not-exist.map.json"),
        std::runtime_error);
}

TEST_CASE("parse reads a v2 tiles.terrain grid", "[map_format]") {
    const std::string json = R"({
        "schema_version": 2,
        "screen": {"index": 1, "width": 3, "height": 2},
        "biome": "courtyard",
        "tileset": {"name": "castle", "columns": 21, "tile_size": 16},
        "tiles": {"terrain": [[0, 1, 0], [2, 0, 3]]},
        "colliders": [],
        "entities": []
    })";
    const jumpcastle::ScreenMap map = jumpcastle::parse_screen_map(json, "v2");
    CHECK(map.terrain.columns == 3);
    CHECK(map.terrain.rows == 2);
    REQUIRE(map.terrain.gids.size() == 6);
    CHECK(map.terrain.gids[1] == 1u);   // row 0, col 1
    CHECK(map.terrain.gids[3] == 2u);   // row 1, col 0
    CHECK(map.terrain.gids[5] == 3u);   // row 1, col 2
    CHECK(map.tileset_columns == 21);
    CHECK(map.tileset_tile_size == 16);
}

TEST_CASE("parse of a v1 map leaves terrain empty", "[map_format]") {
    const std::string json = R"({
        "schema_version": 1,
        "screen": {"index": 0, "width": 16, "height": 12},
        "biome": "courtyard",
        "colliders": [],
        "entities": []
    })";
    const jumpcastle::ScreenMap map = jumpcastle::parse_screen_map(json, "v1");
    CHECK(map.terrain.gids.empty());
    CHECK(map.terrain.columns == 0);
    CHECK(map.terrain.rows == 0);
}

TEST_CASE("serialize round-trips a terrain layer incl. flip bits", "[map_format]") {
    jumpcastle::ScreenMap map;
    map.index = 4;
    map.width = 2;
    map.height = 2;
    map.biome = "frosted_keep";
    map.tileset_columns = 21;
    map.tileset_tile_size = 16;
    map.terrain.columns = 2;
    map.terrain.rows = 2;
    map.terrain.gids = {0u, 5u, 0x80000000u | 6u, 3u};  // one horizontally-flipped GID

    const std::string json = jumpcastle::serialize_screen_map(map);
    const jumpcastle::ScreenMap back = jumpcastle::parse_screen_map(json, "roundtrip");

    CHECK(back.terrain.columns == 2);
    CHECK(back.terrain.rows == 2);
    REQUIRE(back.terrain.gids.size() == 4);
    CHECK(back.terrain.gids[2] == (0x80000000u | 6u));  // flip bit preserved
    CHECK(back.terrain.gids[1] == 5u);
    CHECK(back.tileset_columns == 21);
}

TEST_CASE("parse rejects a terrain grid with the wrong dimensions", "[map_format]") {
    const std::string json = R"({
        "schema_version": 2,
        "screen": {"index": 0, "width": 3, "height": 2},
        "biome": "courtyard",
        "tiles": {"terrain": [[0, 1, 0]]},
        "colliders": [],
        "entities": []
    })";
    CHECK_THROWS_AS(
        jumpcastle::parse_screen_map(json, "bad-grid"), std::runtime_error);
}
