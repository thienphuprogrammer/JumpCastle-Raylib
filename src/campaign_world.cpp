#include "jumpcastle/campaign_world.hpp"

#include <algorithm>
#include <string>

namespace jumpcastle {

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

    for (const ScreenMap& screen : screens) {
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
    return from_screens(screens, screen_height);
}

}  // namespace jumpcastle
