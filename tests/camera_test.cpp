#include "jumpcastle/camera.hpp"

#include "test_world_factory.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("camera screens count from bottom to top") {
    const WorldMap world = test::three_screen_world();

    CHECK(select_camera_band(world, 5.0F).screen == 2);
    CHECK(select_camera_band(world, 25.0F).screen == 0);
    CHECK(select_camera_band(world, 25.0F).world_top == Catch::Approx(20.0F));
}

TEST_CASE("camera selection clamps outside the campaign") {
    const WorldMap world = test::three_screen_world();

    CHECK(select_camera_band(world, -20.0F).screen == 2);
    CHECK(select_camera_band(world, 50.0F).screen == 0);
}

TEST_CASE("camera on a polygon campaign world tracks screen and biome") {
    ScreenMap top;
    top.index = 0;
    top.width = 16;
    top.height = 10;
    top.biome = "crown_spire";
    ScreenMap bottom;
    bottom.index = 1;
    bottom.width = 16;
    bottom.height = 10;
    bottom.biome = "courtyard";
    const CampaignWorld world = CampaignWorld::from_screens({top, bottom}, 10);

    CHECK(world.screen_count() == 2);
    CHECK(select_camera_band(world, 5.0F).screen == 0);
    CHECK(select_camera_band(world, 5.0F).biome == WorldBiome::crown_spire);
    CHECK(select_camera_band(world, 15.0F).screen == 1);
    CHECK(select_camera_band(world, 15.0F).biome == WorldBiome::courtyard);
    CHECK(select_camera_band(world, 15.0F).world_top == Catch::Approx(10.0F));
}
