#include "jumpcastle/level.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <string_view>

using namespace jumpcastle;

namespace {

constexpr std::string_view valid_room = R"(name=First Steps
biome=pixel_adventure
difficulty=1
---
################
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#......S.......#
#.....###......#
################
)";

}  // namespace

TEST_CASE("room parser reads metadata and markers") {
    const Room room = parse_room(valid_room, "valid-room.level");

    CHECK(room.metadata.name == "First Steps");
    CHECK(room.metadata.biome == Biome::pixel_adventure);
    CHECK(room.metadata.difficulty == 1);
    CHECK(room.tilemap.tile_at(7, 9) == Tile::spawn);
}

TEST_CASE("room parser reports line for invalid width") {
    std::string malformed{valid_room};
    const auto row = malformed.find("################");
    malformed.erase(row, 1);

    CHECK_THROWS_WITH(
        parse_room(malformed, "broken.level"),
        Catch::Matchers::ContainsSubstring("broken.level:5"));
}

TEST_CASE("room parser rejects unknown token") {
    std::string malformed{valid_room};
    malformed[malformed.rfind('S')] = '?';

    CHECK_THROWS_WITH(
        parse_room(malformed, "unknown.level"),
        Catch::Matchers::ContainsSubstring("unknown token '?'"));
}

TEST_CASE("world height maps to bottom-to-top room index") {
    CHECK(room_index_for_world_y(-6.0F) == 0);
    CHECK(room_index_for_world_y(-18.0F) == 1);
    CHECK_FALSE(room_index_for_world_y(6.0F));
    CHECK_FALSE(room_index_for_world_y(-150.0F));
}
