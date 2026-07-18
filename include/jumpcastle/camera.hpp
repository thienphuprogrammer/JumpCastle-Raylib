#pragma once

#include "jumpcastle/campaign_world.hpp"
#include "jumpcastle/presentation.hpp"
#include "jumpcastle/world.hpp"

namespace jumpcastle {

struct CameraBand {
    int screen{};
    float world_top{};
    WorldBiome biome{WorldBiome::courtyard};
};

[[nodiscard]] CameraBand select_camera_band(
    const CampaignWorld& world,
    float player_y) noexcept;

// Maps a window-space mouse position to a world-space tile coordinate, inverting
// the letterboxed pixel-art presentation and the vertical camera band. Pure so
// the editor's cursor mapping can be unit-tested without raylib.
[[nodiscard]] Vec2 screen_to_world(
    float mouse_x,
    float mouse_y,
    const PresentationLayout& layout,
    float world_top) noexcept;

}  // namespace jumpcastle
