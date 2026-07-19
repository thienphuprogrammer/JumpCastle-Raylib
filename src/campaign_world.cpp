#include "jumpcastle/campaign_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace jumpcastle {
namespace {

WorldBiome biome_from_string(const std::string& name) noexcept {
    if (name == "frosted_keep") { return WorldBiome::frosted_keep; }
    if (name == "crown_spire") { return WorldBiome::crown_spire; }
    return WorldBiome::courtyard;
}

}  // namespace

WorldBiome CampaignWorld::biome_for_screen(const int screen) const noexcept {
    if (screen < 0 || screen >= static_cast<int>(screen_biomes.size())) {
        return WorldBiome::courtyard;
    }
    return screen_biomes[static_cast<std::size_t>(screen)];
}

const ScreenMap* CampaignWorld::screen_map(const int screen) const noexcept {
    for (const ScreenMap& map : screens) {
        if (map.index == screen) { return &map; }
    }
    return nullptr;
}

CampaignWorld CampaignWorld::from_screens(
    const std::vector<ScreenMap>& screens,
    const int screen_height) {
    CampaignWorld world;
    world.screen_height = screen_height > 0 ? screen_height : 1;

    int max_index = 0;
    for (const ScreenMap& screen : screens) {
        max_index = std::max(max_index, screen.index);
    }
    world.height = (max_index + 1) * world.screen_height;
    world.screen_biomes.assign(
        static_cast<std::size_t>(max_index) + 1, WorldBiome::courtyard);

    for (const ScreenMap& screen : screens) {
        world.screen_biomes[static_cast<std::size_t>(screen.index)] =
            biome_from_string(screen.biome);
        const float offset = static_cast<float>(screen.index * world.screen_height);
        for (const MapEntity& entity : screen.entities) {
            const Vec2 world_pos{entity.pos.x, entity.pos.y + offset};
            if (entity.type == EntityType::spawn) {
                world.spawn = world_pos;
            } else if (entity.type == EntityType::goal) {
                world.goal = world_pos;
            }
        }
    }

    world.collision = CollisionWorld::from_screens(screens, world.screen_height);
    world.screens = screens;  // retained for the in-game editor (lossless round-trip)
    return world;
}

CampaignWorld CampaignWorld::load(
    const std::filesystem::path& directory,
    const int screen_height) {
    std::vector<ScreenMap> screens;
    for (const auto& entry : std::filesystem::directory_iterator{directory}) {
        const std::filesystem::path& path = entry.path();
        const std::string name = path.filename().string();
        if (name.rfind("screen-", 0) == 0 && path.extension() == ".json") {
            screens.push_back(parse_screen_map_file(path));
        }
    }
    int height = screen_height;
    if (height <= 0 && !screens.empty()) {
        // Derive the band height from the files so callers need not know it.
        height = std::max(1, static_cast<int>(std::lround(screens.front().height)));
    }
    return from_screens(screens, height);
}

}  // namespace jumpcastle
