#pragma once

#include "jumpcastle/world.hpp"

namespace jumpcastle {

struct CameraBand {
    int screen{};
    float world_top{};
    WorldBiome biome{WorldBiome::courtyard};
};

[[nodiscard]] CameraBand select_camera_band(
    const WorldMap& world,
    float player_y) noexcept;

}  // namespace jumpcastle
