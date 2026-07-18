#include "jumpcastle/world.hpp"

#include "jumpcastle/campaign_world.hpp"
#include "test_world_factory.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace jumpcastle;

TEST_CASE("CampaignWorld::load reads a directory of screen-NN.map.json files") {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "jumpcastle_campaign_load_test";
    std::filesystem::create_directories(dir);

    const auto write_screen = [&](const int index, const char* biome) {
        ScreenMap screen;
        screen.index = index;
        screen.width = 16.0F;
        screen.height = 12.0F;
        screen.biome = biome;
        const std::vector<Vec2> floor{{0, 11}, {16, 11}, {16, 12}, {0, 12}};
        screen.polygons.push_back(
            {floor, {}, {}, ColliderType::solid});
        if (index == 1) {
            screen.entities.push_back({EntityType::spawn, {2.0F, 10.0F}});
        }
        if (index == 0) {
            screen.entities.push_back({EntityType::goal, {8.0F, 10.0F}});
        }
        std::ofstream out{dir / ("screen-0" + std::to_string(index) + ".map.json")};
        out << serialize_screen_map(screen);
    };
    write_screen(0, "crown_spire");
    write_screen(1, "courtyard");

    const CampaignWorld world = CampaignWorld::load(dir, 12);

    CHECK(world.screen_count() == 2);
    CHECK(world.spawn.x == Catch::Approx(2.0F));
    CHECK(world.biome_for_screen(0) == WorldBiome::crown_spire);
    CHECK(world.biome_for_screen(1) == WorldBiome::courtyard);
    REQUIRE(world.collision.polygons_for_screen(1) != nullptr);
    CHECK_FALSE(world.collision.polygons_for_screen(1)->empty());

    std::filesystem::remove_all(dir);
}

TEST_CASE("from_world_map converts a grid into a merged polygon campaign world") {
    const WorldMap grid = parse_campaign(R"(version 2
tile_size 16
size 4 6
screen_height 2
spawn 0 2
goal 1 4
biome 1 3 courtyard
---
[collision]
....
....
....
##..
....
####)",
        "convert.level");

    const CampaignWorld world = CampaignWorld::from_world_map(grid);

    CHECK(world.spawn == grid.spawn());
    CHECK(world.goal == grid.goal());
    CHECK(world.screen_count() == grid.screen_count());
    CHECK(world.height == grid.height());

    // The bottom row is four solid tiles; greedy merge collapses them into a
    // single wide rectangle instead of four unit squares (avoids seam snags).
    const auto* bottom = world.collision.polygons_for_screen(2);
    REQUIRE(bottom != nullptr);
    int wide_floor = 0;
    for (const ConvexPolygon& polygon : *bottom) {
        if (polygon.aabb.max.x - polygon.aabb.min.x >= 4.0F) { ++wide_floor; }
    }
    CHECK(wide_floor == 1);
}

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
