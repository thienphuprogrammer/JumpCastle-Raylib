#include "jumpcastle/world.hpp"

#include "test_world_factory.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <set>
#include <string>

using namespace jumpcastle;

TEST_CASE("campaign parser reads dimensions markers and biome ranges") {
    const WorldMap world = parse_campaign(R"(version 2
tile_size 16
size 4 6
screen_height 2
spawn 1 4
goal 2 1
biome 1 1 courtyard
biome 2 2 frosted_keep
biome 3 3 crown_spire
---
[collision]
....
....
.##.
....
....
####
)", "memory.level");

    CHECK(world.width() == 4);
    CHECK(world.height() == 6);
    CHECK(world.screen_count() == 3);
    CHECK(world.spawn() == Vec2{1.5F, 4.5F});
    CHECK(world.goal() == Vec2{2.5F, 1.5F});
    CHECK(world.biome_for_screen(0) == WorldBiome::courtyard);
    CHECK(world.screen_for_y(4.5F) == 0);
    CHECK(world.decorations().empty());  // no [decoration] section -> backward compatible
}

TEST_CASE("campaign parser reads an optional decoration section") {
    const WorldMap world = parse_campaign(R"(version 2
tile_size 16
size 4 6
screen_height 2
spawn 1 4
goal 2 1
biome 1 1 courtyard
biome 2 2 frosted_keep
biome 3 3 crown_spire
---
[collision]
....
....
.##.
....
....
####
[decoration]
....
.t..
....
...b
....
....
)", "memory.level");

    REQUIRE(world.decorations().size() == 2);
    CHECK(world.decorations()[0].x == 1);
    CHECK(world.decorations()[0].y == 1);
    CHECK(world.decorations()[0].prop == Prop::torch);
    CHECK(world.decorations()[1].prop == Prop::banner);
}

TEST_CASE("campaign parser rejects an unknown decoration glyph") {
    CHECK_THROWS_WITH(
        parse_campaign(R"(version 2
tile_size 16
size 4 6
screen_height 2
spawn 1 4
goal 2 1
biome 1 1 courtyard
biome 2 2 frosted_keep
biome 3 3 crown_spire
---
[collision]
....
....
.##.
....
....
####
[decoration]
....
.Z..
....
....
....
....
)", "memory.level"),
        Catch::Matchers::ContainsSubstring("unknown decoration token"));
}

TEST_CASE("campaign parser reports row and column") {
    CHECK_THROWS_WITH(
        parse_campaign(
            "version 2\n"
            "tile_size 16\n"
            "size 2 1\n"
            "screen_height 1\n"
            "spawn 0 0\n"
            "goal 1 0\n"
            "biome 1 1 courtyard\n"
            "---\n"
            "[collision]\n"
            ".?\n",
            "broken.level"),
        Catch::Matchers::ContainsSubstring("broken.level:10:2"));
}

TEST_CASE("test world fixtures use full-world boundary rules") {
    const WorldMap world = test::flat_world();

    CHECK(world.width() == 10);
    CHECK(world.solid_at(-1, 4));
    CHECK_FALSE(world.solid_at(4, -1));
    CHECK(world.solid_at(4, 9));
}

TEST_CASE("committed campaign has the approved shape") {
    const auto path = std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets/levels/campaign.level";
    const WorldMap world = WorldMap::load(path);

    CHECK(world.width() == 28);
    CHECK(world.height() == 648);
    CHECK(world.screen_height() == 36);
    CHECK(world.screen_count() == 18);
    CHECK(world.spawn() == Vec2{3.5F, 646.5F});
    CHECK(world.goal() == Vec2{24.5F, 1.5F});
    CHECK(world.biome_for_screen(0) == WorldBiome::courtyard);
    CHECK(world.biome_for_screen(6) == WorldBiome::frosted_keep);
    CHECK(world.biome_for_screen(12) == WorldBiome::crown_spire);
}

TEST_CASE("every campaign screen has a distinct collision silhouette") {
    const auto path = std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets/levels/campaign.level";
    const WorldMap world = WorldMap::load(path);
    std::set<std::string> silhouettes;

    for (int screen = 0; screen < world.screen_count(); ++screen) {
        const int first_row = world.height() - (screen + 1) * world.screen_height();
        std::string signature;
        for (int y = first_row; y < first_row + world.screen_height(); ++y) {
            for (int x = 0; x < world.width(); ++x) {
                signature.push_back(world.solid_at(x, y) ? '#' : '.');
            }
        }
        INFO("screen " << screen + 1);
        CHECK(silhouettes.insert(std::move(signature)).second);
    }
}
