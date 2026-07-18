#include "jumpcastle/camera.hpp"

#include "jumpcastle/game_config.hpp"

#include <algorithm>
#include <cmath>

namespace jumpcastle {

CameraBand select_camera_band(
    const WorldMap& world,
    const float player_y) noexcept {
    const int band_from_top = std::clamp(
        static_cast<int>(std::floor(player_y / static_cast<float>(world.screen_height()))),
        0,
        world.screen_count() - 1);
    const int screen = world.screen_count() - band_from_top - 1;
    return {
        .screen = screen,
        .world_top = static_cast<float>(band_from_top * world.screen_height()),
        .biome = world.biome_for_screen(screen),
    };
}

CameraBand select_camera_band(
    const CampaignWorld& world,
    const float player_y) noexcept {
    const int band_from_top = std::clamp(
        static_cast<int>(std::floor(player_y / static_cast<float>(world.screen_height))),
        0,
        world.screen_count() - 1);
    return {
        .screen = band_from_top,
        .world_top = static_cast<float>(band_from_top * world.screen_height),
        .biome = world.biome_for_screen(band_from_top),
    };
}

Vec2 screen_to_world(
    const float mouse_x,
    const float mouse_y,
    const PresentationLayout& layout,
    const float world_top) noexcept {
    const float pixel_x = layout.width > 0.0F
        ? (mouse_x - layout.offset_x) * static_cast<float>(config::view_width) / layout.width
        : 0.0F;
    const float pixel_y = layout.height > 0.0F
        ? (mouse_y - layout.offset_y) * static_cast<float>(config::view_height) / layout.height
        : 0.0F;
    return {
        pixel_x / static_cast<float>(config::tile_pixels),
        pixel_y / static_cast<float>(config::tile_pixels) + world_top,
    };
}

}  // namespace jumpcastle
