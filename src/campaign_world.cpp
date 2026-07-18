#include "jumpcastle/campaign_world.hpp"

#include "jumpcastle/convex.hpp"

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

std::string biome_to_string(const WorldBiome biome) noexcept {
    switch (biome) {
    case WorldBiome::frosted_keep: return "frosted_keep";
    case WorldBiome::crown_spire: return "crown_spire";
    case WorldBiome::courtyard: return "courtyard";
    }
    return "courtyard";
}

}  // namespace

WorldBiome CampaignWorld::biome_for_screen(const int screen) const noexcept {
    if (screen < 0 || screen >= static_cast<int>(screen_biomes.size())) {
        return WorldBiome::courtyard;
    }
    return screen_biomes[static_cast<std::size_t>(screen)];
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
    return world;
}

std::vector<ScreenMap> CampaignWorld::screen_maps_from_world_map(const WorldMap& grid) {
    const int screen_height = grid.screen_height();
    const int screen_count = grid.screen_count();
    const int width = grid.width();
    std::vector<ScreenMap> screens;
    screens.reserve(static_cast<std::size_t>(screen_count));

    for (int screen_index = 0; screen_index < screen_count; ++screen_index) {
        const int y0 = screen_index * screen_height;
        ScreenMap map;
        map.index = screen_index;
        map.width = static_cast<float>(width);
        map.height = static_cast<float>(screen_height);
        // WorldMap numbers biome screens from the bottom; the polygon world
        // indexes bands from the top, so invert when looking up the biome.
        map.biome = biome_to_string(
            grid.biome_for_screen(screen_count - screen_index - 1));

        std::vector<char> used(
            static_cast<std::size_t>(width * screen_height), 0);
        const auto used_at = [&](const int lx, const int ly) -> char& {
            return used[static_cast<std::size_t>(ly * width + lx)];
        };
        const auto free_solid = [&](const int lx, const int ly) {
            return grid.solid_at(lx, y0 + ly) && used_at(lx, ly) == 0;
        };

        for (int ly = 0; ly < screen_height; ++ly) {
            for (int lx = 0; lx < width; ++lx) {
                if (!free_solid(lx, ly)) { continue; }
                int rect_w = 1;
                while (lx + rect_w < width && free_solid(lx + rect_w, ly)) {
                    ++rect_w;
                }
                int rect_h = 1;
                bool grow = true;
                while (grow && ly + rect_h < screen_height) {
                    for (int k = 0; k < rect_w; ++k) {
                        if (!free_solid(lx + k, ly + rect_h)) { grow = false; break; }
                    }
                    if (grow) { ++rect_h; }
                }
                for (int yy = 0; yy < rect_h; ++yy) {
                    for (int xx = 0; xx < rect_w; ++xx) {
                        used_at(lx + xx, ly + yy) = 1;
                    }
                }
                const float fx = static_cast<float>(lx);
                const float fy = static_cast<float>(ly);
                const float fw = static_cast<float>(rect_w);
                const float fh = static_cast<float>(rect_h);
                std::vector<Vec2> points{
                    {fx, fy}, {fx + fw, fy}, {fx + fw, fy + fh}, {fx, fy + fh}};
                map.polygons.push_back(
                    {points, outward_edge_normals(points), polygon_aabb(points),
                     ColliderType::solid});
            }
        }
        screens.push_back(std::move(map));
    }

    const Vec2 spawn = grid.spawn();
    const Vec2 goal = grid.goal();
    const auto band_of = [&](const float y) {
        return std::clamp(
            static_cast<int>(std::floor(y / static_cast<float>(screen_height))),
            0,
            screen_count - 1);
    };
    const int spawn_band = band_of(spawn.y);
    const int goal_band = band_of(goal.y);
    screens[static_cast<std::size_t>(spawn_band)].entities.push_back(
        {EntityType::spawn,
         {spawn.x, spawn.y - static_cast<float>(spawn_band * screen_height)}});
    screens[static_cast<std::size_t>(goal_band)].entities.push_back(
        {EntityType::goal,
         {goal.x, goal.y - static_cast<float>(goal_band * screen_height)}});

    return screens;
}

CampaignWorld CampaignWorld::from_world_map(const WorldMap& grid) {
    CampaignWorld world =
        from_screens(screen_maps_from_world_map(grid), grid.screen_height());
    world.spawn = grid.spawn();  // preserve exact fractional spawn/goal
    world.goal = grid.goal();
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
