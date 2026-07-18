#pragma once

#include "jumpcastle/campaign_world.hpp"
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

[[nodiscard]] CameraBand select_camera_band(
    const CampaignWorld& world,
    float player_y) noexcept;

}  // namespace jumpcastle
