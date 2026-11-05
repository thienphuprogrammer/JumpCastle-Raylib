#include "jumpcastle/campaign_world.hpp"
#include "jumpcastle/map_format.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
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
        screen.colliders.push_back({
            .id = 1,
            .type = ColliderType::solid,
            .geometry = PolygonGeometry{floor},
        });
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

    // No screen_height passed: it is derived from the loaded screens.
    const CampaignWorld world = CampaignWorld::load(dir);

    CHECK(world.screen_height == 12);
    CHECK(world.screen_count() == 2);
    CHECK(world.spawn.x == Catch::Approx(2.0F));
    CHECK(world.biome_for_screen(0) == WorldBiome::crown_spire);
    CHECK(world.biome_for_screen(1) == WorldBiome::courtyard);
    REQUIRE(world.collision.colliders_for_screen(1) != nullptr);
    CHECK_FALSE(world.collision.colliders_for_screen(1)->empty());

    std::filesystem::remove_all(dir);
}
