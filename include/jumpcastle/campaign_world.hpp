#pragma once

#include "jumpcastle/collision_world.hpp"
#include "jumpcastle/map_format.hpp"
#include "jumpcastle/math.hpp"
#include "jumpcastle/world.hpp"

#include <filesystem>
#include <vector>

namespace jumpcastle {

// The runtime campaign: the polygon collision world plus the campaign
// metadata (spawn, goal, total height, per-screen biome) derived from
// per-screen maps.
struct CampaignWorld {
    CollisionWorld collision;
    Vec2 spawn{};
    Vec2 goal{};
    int height{};         // total tower height in tiles
    int screen_height{1};
    std::vector<WorldBiome> screen_biomes;  // indexed by screen index
    // The source screen maps (screen-local tile coords, with entities) retained
    // so the in-game editor can load a whole screen and save it back losslessly.
    std::vector<ScreenMap> screens;

    [[nodiscard]] int screen_count() const noexcept {
        return height / screen_height;
    }
    [[nodiscard]] WorldBiome biome_for_screen(int screen) const noexcept;
    // The retained source map for a screen index, or nullptr if none.
    [[nodiscard]] const ScreenMap* screen_map(int screen) const noexcept;

    [[nodiscard]] static CampaignWorld from_screens(
        const std::vector<ScreenMap>& screens,
        int screen_height);

    // Loads every screen-NN.map.json in a directory into one campaign world.
    // screen_height <= 0 derives the band height from the loaded screens, so the
    // caller need not know it up front.
    [[nodiscard]] static CampaignWorld load(
        const std::filesystem::path& directory,
        int screen_height = 0);
};

}  // namespace jumpcastle
