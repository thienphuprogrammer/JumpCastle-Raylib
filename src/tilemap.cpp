#include "jumpcastle/tilemap.hpp"

#include <array>
#include <cmath>
#include <string_view>

namespace jumpcastle {
namespace {

using LevelRows = std::array<std::string_view, config::tilemap_height>;

consteval Tilemap make_tilemap(const LevelRows& rows) {
    Tilemap::Grid grid{};

    for (std::size_t y = 0; y < grid.size(); ++y) {
        for (std::size_t x = 0; x < grid[y].size(); ++x) {
            const char value = x < rows[y].size() ? rows[y][x] : ' ';
            grid[y][x] = value == '#' ? Tile::solid : Tile::empty;
        }
    }

    return Tilemap{grid};
}

constexpr std::array screens{
    make_tilemap({
        "", "", "", "", "", "", "", "", "", "", "", "",
    }),
    make_tilemap({
        "################",
        "#              #",
        "# #### #### #  #",
        "# #    #    #  #",
        "# # ## # ## #  #",
        "# #  # #  #    #",
        "# #### #### #  #",
        "#              #",
        "#              #",
        "#              #",
        "#              #",
        "#########      #",
    }),
    make_tilemap({
        "#########      #",
        "#########    ###",
        "########      ##",
        "########      ##",
        "##########     #",
        "##########     #",
        "########      ##",
        "########      ##",
        "##########    ##",
        "######        ##",
        "###           ##",
        "###         ####",
    }),
    make_tilemap({
        "###         ####",
        "###    ##   ####",
        "###         ####",
        "###          ###",
        "#####        ###",
        "###          ###",
        "#            ###",
        "##        ######",
        "##         #####",
        "##         #####",
        "######     #####",
        "#####      #####",
    }),
    make_tilemap({
        "#####      #####",
        "###      #######",
        "##        ######",
        "##          ####",
        "######      ####",
        "######       ###",
        "######   #   ###",
        "#####    ##  ###",
        "#####        ###",
        "##           ###",
        "##        ######",
        "##    ##########",
    }),
    make_tilemap({
        "##    ##########",
        "##            ##",
        "####          ##",
        "########       #",
        "#####          #",
        "##             #",
        "##       #######",
        "#        #######",
        "#         ######",
        "#####     ######",
        "#####     ######",
        "################",
    }),
};

}  // namespace

Tile Tilemap::tile_at(const int x, const int y) const noexcept {
    if (x < 0 || x >= config::tilemap_width) {
        return Tile::solid;
    }
    if (y < 0 || y >= config::tilemap_height) {
        return Tile::empty;
    }

    return grid_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

bool Tilemap::solid_at(const int x, const int y) const noexcept {
    return tile_at(x, y) == Tile::solid;
}

bool Tilemap::solid_for_render_at(const int x, const int y) const noexcept {
    if (x < 0 || x >= config::tilemap_width || y < 0 || y >= config::tilemap_height) {
        return true;
    }

    return solid_at(x, y);
}

const Tilemap& screen(const std::size_t index) noexcept {
    return screens[index < screens.size() ? index : invalid_screen_index];
}

ScreenSelection select_screen(const float world_y) noexcept {
    const int height_index = static_cast<int>(
        std::floor(-world_y / static_cast<float>(config::tilemap_height)));
    const int index = static_cast<int>(starting_screen_index) - height_index;

    if (index <= static_cast<int>(invalid_screen_index) ||
        index >= static_cast<int>(screens.size())) {
        return {invalid_screen_index, &screens[invalid_screen_index], 0.0F};
    }

    const float offset =
        -static_cast<float>((height_index + 1) * config::tilemap_height);
    return {
        static_cast<std::size_t>(index),
        &screens[static_cast<std::size_t>(index)],
        offset,
    };
}

Vector2 starting_player_position() noexcept {
    return {
        config::tilemap_width / 2.0F,
        -config::tilemap_height / 2.0F,
    };
}

}  // namespace jumpcastle
