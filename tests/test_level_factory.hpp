#pragma once

#include "jumpcastle/level.hpp"

#include <array>
#include <vector>

namespace jumpcastle::test {

struct TestCell {
    std::size_t room;
    int x;
    int y;
    Tile tile;
};

inline LevelRepository make_level(
    const Tile extra = Tile::empty,
    const std::size_t extra_room = 0,
    const int extra_x = 6,
    const int extra_y = 8) {
    std::array<Room, LevelRepository::room_count> rooms;

    for (std::size_t index = 0; index < rooms.size(); ++index) {
        Tilemap::Grid grid{};
        for (auto& row : grid) row.fill(Tile::empty);
        grid.back().fill(Tile::solid);

        const Biome biome = index < 4
            ? Biome::pixel_adventure
            : index < 8 ? Biome::kenney : Biome::kings_and_pigs;
        rooms[index] = {
            {"Test Room " + std::to_string(index + 1), biome, 1},
            Tilemap{grid},
        };
    }

    auto mutable_grid = [](const Tilemap& map) { return map.grid(); };

    auto room_zero = mutable_grid(rooms[0].tilemap);
    room_zero[8][8] = Tile::spawn;
    room_zero[9][8] = Tile::checkpoint;
    rooms[0].tilemap = Tilemap{room_zero};

    for (const std::size_t index : {4U, 8U}) {
        auto grid = mutable_grid(rooms[index].tilemap);
        grid[9][8] = Tile::checkpoint;
        rooms[index].tilemap = Tilemap{grid};
    }

    auto final_grid = mutable_grid(rooms[11].tilemap);
    final_grid[1][8] = Tile::exit;
    rooms[11].tilemap = Tilemap{final_grid};

    if (extra != Tile::empty) {
        auto grid = mutable_grid(rooms[extra_room].tilemap);
        grid[static_cast<std::size_t>(extra_y)][static_cast<std::size_t>(extra_x)] = extra;
        rooms[extra_room].tilemap = Tilemap{grid};
    }

    return LevelRepository{std::move(rooms)};
}

inline LevelRepository make_level_with_cells(const std::vector<TestCell>& cells) {
    std::array<Room, LevelRepository::room_count> rooms;
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        Tilemap::Grid grid{};
        for (auto& row : grid) row.fill(Tile::empty);
        grid.back().fill(Tile::solid);
        const Biome biome = index < 4
            ? Biome::pixel_adventure
            : index < 8 ? Biome::kenney : Biome::kings_and_pigs;
        rooms[index] = {{"Solver Room " + std::to_string(index + 1), biome, 1}, Tilemap{grid}};
    }

    auto apply = [&rooms](const std::size_t room, const int x, const int y, const Tile tile) {
        auto grid = rooms[room].tilemap.grid();
        grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
        rooms[room].tilemap = Tilemap{grid};
    };
    apply(0, 8, 9, Tile::spawn);
    apply(0, 8, 10, Tile::checkpoint);
    apply(4, 8, 10, Tile::checkpoint);
    apply(8, 8, 10, Tile::checkpoint);
    apply(11, 8, 1, Tile::exit);
    for (const auto& cell : cells) apply(cell.room, cell.x, cell.y, cell.tile);
    return LevelRepository{std::move(rooms)};
}

}  // namespace jumpcastle::test
