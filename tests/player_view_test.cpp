#include "jumpcastle/player_view.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("sprite is centred on the player and stands on its feet") {
    const Rectangle dest = player_sprite_destination({5.0F, -3.0F}, 0.0F);

    const float tile = static_cast<float>(config::tile_pixels);
    CHECK(dest.x + dest.width * 0.5F == Catch::Approx(5.0F * tile));
    const float feet = (-3.0F + config::player_half_size.y) * tile;
    CHECK(dest.y + dest.height == Catch::Approx(feet));
    CHECK(dest.width == Catch::Approx(dest.height));
}

TEST_CASE("sprite destination ignores facing so flipping never shifts it") {
    const Rectangle dest = player_sprite_destination({7.25F, -10.0F}, -12.0F);
    const float tile = static_cast<float>(config::tile_pixels);

    // The destination centres on the player x regardless of facing; the flip is
    // applied to the source rectangle, not this destination.
    CHECK(dest.x + dest.width * 0.5F == Catch::Approx(7.25F * tile));
}

TEST_CASE("animation state follows the player") {
    PlayerState grounded{};
    grounded.on_ground = true;
    CHECK(select_animation(grounded, 0.0F) == PlayerAnimation::idle);

    grounded.velocity.x = 2.0F;
    CHECK(select_animation(grounded, 0.0F) == PlayerAnimation::run);

    grounded.jump_hold_time = 0.2F;
    CHECK(select_animation(grounded, 0.0F) == PlayerAnimation::charge);

    CHECK(select_animation(grounded, 0.3F) == PlayerAnimation::respawn);

    PlayerState airborne{};
    airborne.velocity.y = -1.0F;
    CHECK(select_animation(airborne, 0.0F) == PlayerAnimation::rise);
    airborne.velocity.y = 1.0F;
    CHECK(select_animation(airborne, 0.0F) == PlayerAnimation::fall);
}

TEST_CASE("frame index loops within the clip and guards empty clips") {
    CHECK(animation_frame_index(0.0F, 10, 4) == 0);
    CHECK(animation_frame_index(0.25F, 10, 4) == 2);
    CHECK(animation_frame_index(0.55F, 10, 4) == 1);
    CHECK(animation_frame_index(100.0F, 10, 0) == 0);
}
