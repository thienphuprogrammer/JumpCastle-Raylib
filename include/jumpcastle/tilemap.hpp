#pragma once

#include "jumpcastle/game_config.hpp"

#include <array>
#include <cstddef>

namespace jumpcastle {

enum class Tile : char {
    empty = '.',
    solid = '#',
    spike = '^',
    spawn = 'S',
    checkpoint = 'C',
    exit = 'E',
};

class Tilemap {
public:
    using Row = std::array<Tile, config::tilemap_width>;
    using Grid = std::array<Row, config::tilemap_height>;

    constexpr Tilemap() noexcept {
        for (auto& row : grid_) {
            row.fill(Tile::empty);
        }
    }
    constexpr explicit Tilemap(Grid grid) noexcept : grid_{grid} {}

    [[nodiscard]] Tile tile_at(int x, int y) const noexcept;
    [[nodiscard]] bool solid_at(int x, int y) const noexcept;
    [[nodiscard]] bool solid_for_render_at(int x, int y) const noexcept;
    [[nodiscard]] const Grid& grid() const noexcept { return grid_; }

private:
    Grid grid_{};
};

struct ScreenSelection {
    std::size_t index;
    const Tilemap* tilemap;
    float vertical_offset;
};

inline constexpr std::size_t invalid_screen_index = 0;
inline constexpr std::size_t starting_screen_index = 5;

[[nodiscard]] const Tilemap& screen(std::size_t index) noexcept;
[[nodiscard]] ScreenSelection select_screen(float world_y) noexcept;
[[nodiscard]] Vector2 starting_player_position() noexcept;

}  // namespace jumpcastle
