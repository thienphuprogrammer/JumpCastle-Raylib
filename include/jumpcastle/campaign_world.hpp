#pragma once

#include "jumpcastle/collision_world.hpp"
#include "jumpcastle/map_format.hpp"
#include "jumpcastle/math.hpp"

#include <filesystem>
#include <vector>

namespace jumpcastle {

// The polygon-collision analog of WorldMap: the runtime collision plus the
// campaign metadata (spawn, goal, total height) derived from per-screen maps.
struct CampaignWorld {
    CollisionWorld collision;
    Vec2 spawn{};
    Vec2 goal{};
    int height{};         // total tower height in tiles
    int screen_height{1};

    [[nodiscard]] static CampaignWorld from_screens(
        const std::vector<ScreenMap>& screens,
        int screen_height);

    // Loads every screen-NN.map.json in a directory into one campaign world.
    [[nodiscard]] static CampaignWorld load(
        const std::filesystem::path& directory,
        int screen_height);
};

}  // namespace jumpcastle
