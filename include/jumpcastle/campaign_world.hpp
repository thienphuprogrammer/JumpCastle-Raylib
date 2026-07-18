#pragma once

#include "jumpcastle/collision_world.hpp"
#include "jumpcastle/map_format.hpp"
#include "jumpcastle/math.hpp"
#include "jumpcastle/world.hpp"

#include <filesystem>
#include <vector>

namespace jumpcastle {

// The polygon-collision analog of WorldMap: the runtime collision plus the
// campaign metadata (spawn, goal, total height, per-screen biome) derived from
// per-screen maps.
struct CampaignWorld {
    CollisionWorld collision;
    Vec2 spawn{};
    Vec2 goal{};
    int height{};         // total tower height in tiles
    int screen_height{1};
    std::vector<WorldBiome> screen_biomes;  // indexed by screen index

    [[nodiscard]] int screen_count() const noexcept {
        return height / screen_height;
    }
    [[nodiscard]] WorldBiome biome_for_screen(int screen) const noexcept;

    [[nodiscard]] static CampaignWorld from_screens(
        const std::vector<ScreenMap>& screens,
        int screen_height);

    // Bridge: convert the legacy grid WorldMap into a polygon campaign world by
    // greedily merging runs of solid tiles into rectangles (merged rects avoid
    // the player snagging on seams between adjacent unit tiles).
    [[nodiscard]] static CampaignWorld from_world_map(const WorldMap& grid);

    // The per-screen, screen-local polygon maps a grid converts to. Exposed so a
    // one-time export can materialize campaign.level as screen-NN.map.json files.
    [[nodiscard]] static std::vector<ScreenMap> screen_maps_from_world_map(
        const WorldMap& grid);

    // Loads every screen-NN.map.json in a directory into one campaign world.
    // screen_height <= 0 derives the band height from the loaded screens, so the
    // caller need not know it up front.
    [[nodiscard]] static CampaignWorld load(
        const std::filesystem::path& directory,
        int screen_height = 0);
};

}  // namespace jumpcastle
