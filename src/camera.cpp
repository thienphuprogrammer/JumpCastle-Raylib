#include "jumpcastle/camera.hpp"

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

}  // namespace jumpcastle
