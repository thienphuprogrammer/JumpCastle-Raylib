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
