#pragma once

#include "jumpcastle/campaign_world.hpp"
#include "jumpcastle/map_format.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace jumpcastle::test {

// Builds a polygon CampaignWorld straight from ASCII rows ('#' solid, '.' empty),
// merging horizontal runs of solids per row into rectangle colliders. Biome is
// not exercised by the solver/replay tests, so every screen is courtyard.
[[nodiscard]] inline CampaignWorld make_world(
    const std::vector<std::string_view>& rows,
    const int screen_height,
    const Vec2 spawn,
    const Vec2 goal) {
    if (rows.empty() || rows.front().empty()) {
        throw std::invalid_argument("test world rows cannot be empty");
    }
    const int width = static_cast<int>(rows.front().size());
    const int total_height = static_cast<int>(rows.size());
    const int screen_count = total_height / screen_height;

    std::vector<ScreenMap> screens;
    screens.reserve(static_cast<std::size_t>(screen_count));
    int next_collider_id = 1;
    for (int s = 0; s < screen_count; ++s) {
        ScreenMap screen;
        screen.index = s;
        screen.width = static_cast<float>(width);
        screen.height = static_cast<float>(screen_height);
        screen.biome = "courtyard";
        for (int ly = 0; ly < screen_height; ++ly) {
            const std::string_view row =
                rows[static_cast<std::size_t>(s * screen_height + ly)];
            if (static_cast<int>(row.size()) != width) {
                throw std::invalid_argument("test world rows must have equal width");
            }
            int x = 0;
            while (x < width) {
                if (row[static_cast<std::size_t>(x)] != '#') {
                    ++x;
                    continue;
                }
                int run = 1;
                while (x + run < width && row[static_cast<std::size_t>(x + run)] == '#') {
                    ++run;
                }
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(ly);
                const float fw = static_cast<float>(run);
                const std::vector<Vec2> points{
                    {fx, fy}, {fx + fw, fy}, {fx + fw, fy + 1.0F}, {fx, fy + 1.0F}};
                screen.colliders.push_back({
                    .id = next_collider_id++,
                    .type = ColliderType::solid,
                    .geometry = PolygonGeometry{points},
                });
                x += run;
            }
        }
        screens.push_back(std::move(screen));
    }

    const auto band_of = [&](const float y) {
        int band = static_cast<int>(y) / screen_height;
        if (band < 0) { band = 0; }
        if (band > screen_count - 1) { band = screen_count - 1; }
        return band;
    };
    const int spawn_band = band_of(spawn.y);
    const int goal_band = band_of(goal.y);
    screens[static_cast<std::size_t>(spawn_band)].entities.push_back(
        {EntityType::spawn,
         {spawn.x, spawn.y - static_cast<float>(spawn_band * screen_height)}});
    screens[static_cast<std::size_t>(goal_band)].entities.push_back(
        {EntityType::goal,
         {goal.x, goal.y - static_cast<float>(goal_band * screen_height)}});

    CampaignWorld world = CampaignWorld::from_screens(screens, screen_height);
    world.spawn = spawn;
    world.goal = goal;
    return world;
}

[[nodiscard]] inline CampaignWorld reachable_three_screen_world() {
    // Side-by-side staircase (2-row rises, non-overlapping left/right platforms)
    // so the player always hops across the seam onto a neighbour.
    return make_world(
        {
            "..........",
            "..........",
            ".....#####",
            "..........",
            "#####.....",
            "..........",
            ".....#####",
            "..........",
            "#####.....",
            "..........",
            ".....#####",
            "..........",
            "#####.....",
            "..........",
            ".....#####",
            "..........",
            "#####.....",
            "..........",
            ".....#####",
            "..........",
            "#####.....",
            "..........",
            ".....#####",
            "..........",
            "#####.....",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "##########",
        },
        10,
        {2.5F, 28.5F},
        {7.5F, 1.5F});
}

[[nodiscard]] inline CampaignWorld unreachable_three_screen_world() {
    return make_world(
        {
            "..........",
            "......###.",
            "..........",
            "..........",
            "..........",
            "########..",
            "..........",
            "..........",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            ".#####....",
            "..........",
            "..........",
            "..........",
            ".....####.",
            "..........",
            "..........",
            "..........",
            "##########",
        },
        10,
        {2.5F, 28.5F},
        {7.5F, 0.5F});
}

}  // namespace jumpcastle::test
