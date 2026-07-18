#include "jumpcastle/camera.hpp"

#include "jumpcastle/game_config.hpp"
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

TEST_CASE("screen_to_world inverts the presentation layout and camera band") {
    // A 1:1 layout with no letterbox offset: window pixels map straight to the
    // pixel-art target.
    const PresentationLayout layout{
        .scale = 1.0F,
        .width = static_cast<float>(config::view_width),
        .height = static_cast<float>(config::view_height),
        .offset_x = 0.0F,
        .offset_y = 0.0F,
    };

    const Vec2 origin = screen_to_world(0.0F, 0.0F, layout, 0.0F);
    CHECK(origin.x == Catch::Approx(0.0F));
    CHECK(origin.y == Catch::Approx(0.0F));

    // One tile is tile_pixels window pixels at 1:1.
    const Vec2 one_tile = screen_to_world(
        static_cast<float>(config::tile_pixels),
        static_cast<float>(config::tile_pixels),
        layout,
        0.0F);
    CHECK(one_tile.x == Catch::Approx(1.0F));
    CHECK(one_tile.y == Catch::Approx(1.0F));

    // The camera band offset shifts world-y by world_top tiles.
    const Vec2 banded = screen_to_world(0.0F, 0.0F, layout, 12.0F);
    CHECK(banded.y == Catch::Approx(12.0F));

    // A letterboxed, 2x-scaled layout: offset and scale are inverted.
    const PresentationLayout scaled{
        .scale = 2.0F,
        .width = static_cast<float>(config::view_width) * 2.0F,
        .height = static_cast<float>(config::view_height) * 2.0F,
        .offset_x = 40.0F,
        .offset_y = 20.0F,
    };
    const Vec2 scaled_tile = screen_to_world(
        40.0F + static_cast<float>(config::tile_pixels) * 2.0F,
        20.0F + static_cast<float>(config::tile_pixels) * 2.0F,
        scaled,
        0.0F);
    CHECK(scaled_tile.x == Catch::Approx(1.0F));
    CHECK(scaled_tile.y == Catch::Approx(1.0F));
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
