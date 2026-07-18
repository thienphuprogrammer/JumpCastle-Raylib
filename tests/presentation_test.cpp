#include "jumpcastle/game_config.hpp"
#include "jumpcastle/presentation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("narrow tower viewport is 28 by 36 tiles") {
    CHECK(config::viewport_tiles_width == 28);
    CHECK(config::viewport_tiles_height == 36);
    CHECK(config::view_width == 448);
    CHECK(config::view_height == 576);
}

TEST_CASE("preferred window scale stays integral") {
    CHECK(preferred_window_scale(1920, 1440) == 2);
    CHECK(preferred_window_scale(1536, 864) == 1);
    CHECK(preferred_window_scale(400, 500) == 1);
}

TEST_CASE("portrait target is centred in a landscape window") {
    const PresentationLayout layout = fit_presentation(1536, 864);

    CHECK(layout.scale == Catch::Approx(1.5F));
    CHECK(layout.width == Catch::Approx(672.0F));
    CHECK(layout.height == Catch::Approx(864.0F));
    CHECK(layout.offset_x == Catch::Approx(432.0F));
    CHECK(layout.offset_y == Catch::Approx(0.0F));
}
