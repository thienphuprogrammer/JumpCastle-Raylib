#include "jumpcastle/map_format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace jumpcastle;

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
