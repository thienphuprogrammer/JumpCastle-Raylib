#pragma once

#include "jumpcastle/world.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace jumpcastle::test {

[[nodiscard]] inline WorldMap make_world(
    const std::vector<std::string_view>& rows,
    const int screen_height,
    const Vec2 spawn,
    const Vec2 goal,
    std::vector<BiomeRange> biomes) {
    if (rows.empty() || rows.front().empty()) {
        throw std::invalid_argument("test world rows cannot be empty");
    }
    const int width = static_cast<int>(rows.front().size());
    std::vector<WorldTile> tiles;
    tiles.reserve(rows.size() * rows.front().size());
    for (const std::string_view row : rows) {
        if (static_cast<int>(row.size()) != width) {
            throw std::invalid_argument("test world rows must have equal width");
        }
        for (const char token : row) {
            if (token == '.') {
                tiles.push_back(WorldTile::empty);
            } else if (token == '#') {
                tiles.push_back(WorldTile::solid);
            } else {
                throw std::invalid_argument("test world contains an unknown token");
            }
        }
    }
    return WorldMap{
        width,
        static_cast<int>(rows.size()),
        screen_height,
        std::move(tiles),
        spawn,
        goal,
        std::move(biomes),
    };
}

[[nodiscard]] inline WorldMap flat_world() {
    return make_world(
        {
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "##########",
        },
        10,
        {4.5F, 8.5F},
        {8.5F, 8.5F},
        {{1, 1, WorldBiome::courtyard}});
}

[[nodiscard]] inline WorldMap world_with_ceiling() {
    return make_world(
        {
            "..........",
            "..........",
            "..........",
            "..........",
            "..........",
            "##########",
            "..........",
            "..........",
            "..........",
            "##########",
        },
        10,
        {4.5F, 8.5F},
        {8.5F, 8.5F},
        {{1, 1, WorldBiome::courtyard}});
}

[[nodiscard]] inline WorldMap three_screen_world() {
    return make_world(
        {
            "..........",
            "......###.",
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
            ".#####....",
            "..........",
            "..........",
            "..........",
            ".....####.",
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
        {7.5F, 0.5F},
        {
            {1, 1, WorldBiome::courtyard},
            {2, 2, WorldBiome::frosted_keep},
            {3, 3, WorldBiome::crown_spire},
        });
}

[[nodiscard]] inline WorldMap reachable_three_screen_world() {
    return make_world(
        {
            "..........",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "########..",
            "..........",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "########..",
            "..........",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "########..",
            "..........",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "########..",
            "..........",
            "..........",
            ".....#####",
            "..........",
            "..........",
            "##########",
        },
        10,
        {2.5F, 28.5F},
        {7.5F, 1.5F},
        {
            {1, 1, WorldBiome::courtyard},
            {2, 2, WorldBiome::frosted_keep},
            {3, 3, WorldBiome::crown_spire},
        });
}

[[nodiscard]] inline WorldMap unreachable_three_screen_world() {
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
        {7.5F, 0.5F},
        {
            {1, 1, WorldBiome::courtyard},
            {2, 2, WorldBiome::frosted_keep},
            {3, 3, WorldBiome::crown_spire},
        });
}

}  // namespace jumpcastle::test
