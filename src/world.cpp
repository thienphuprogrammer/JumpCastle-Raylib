#include "jumpcastle/world.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace jumpcastle {

WorldMap::WorldMap(
    const int width,
    const int height,
    const int screen_height,
    std::vector<WorldTile> tiles,
    const Vec2 spawn,
    const Vec2 goal,
    std::vector<BiomeRange> biomes)
    : width_{width},
      height_{height},
      screen_height_{screen_height},
      tiles_{std::move(tiles)},
      spawn_{spawn},
      goal_{goal},
      biomes_{std::move(biomes)} {
    if (width_ <= 0 || height_ <= 0 || screen_height_ <= 0 ||
        height_ % screen_height_ != 0) {
        throw std::invalid_argument("world dimensions must be positive and screen-aligned");
    }
    if (tiles_.size() != static_cast<std::size_t>(width_ * height_)) {
        throw std::invalid_argument("world tile count does not match its dimensions");
    }

    const auto valid_marker = [this](const Vec2 marker) {
        const int x = static_cast<int>(std::floor(marker.x));
        const int y = static_cast<int>(std::floor(marker.y));
        return x >= 0 && x < width_ && y >= 0 && y < height_ &&
            !solid_at(x, y) && solid_at(x, y + 1);
    };
    if (!valid_marker(spawn_)) {
        throw std::invalid_argument("spawn must be empty and stand above solid terrain");
    }
    if (!valid_marker(goal_)) {
        throw std::invalid_argument("goal must be empty and stand above solid terrain");
    }

    std::vector<int> biome_coverage(static_cast<std::size_t>(screen_count()));
    for (const BiomeRange& range : biomes_) {
        if (range.first_screen < 1 || range.last_screen < range.first_screen ||
            range.last_screen > screen_count()) {
            throw std::invalid_argument("biome range is outside the world screens");
        }
        for (int screen = range.first_screen; screen <= range.last_screen; ++screen) {
            ++biome_coverage[static_cast<std::size_t>(screen - 1)];
        }
    }
    for (const int count : biome_coverage) {
        if (count != 1) {
            throw std::invalid_argument("biome ranges must cover every screen exactly once");
        }
    }
}

int WorldMap::width() const noexcept {
    return width_;
}

int WorldMap::height() const noexcept {
    return height_;
}

int WorldMap::screen_height() const noexcept {
    return screen_height_;
}

int WorldMap::screen_count() const noexcept {
    return height_ / screen_height_;
}

WorldTile WorldMap::tile_at(const int x, const int y) const noexcept {
    if (x < 0 || x >= width_) {
        return WorldTile::solid;
    }
    if (y < 0 || y >= height_) {
        return WorldTile::empty;
    }
    return tiles_[static_cast<std::size_t>(y * width_ + x)];
}

bool WorldMap::solid_at(const int x, const int y) const noexcept {
    return tile_at(x, y) == WorldTile::solid;
}

Vec2 WorldMap::spawn() const noexcept {
    return spawn_;
}

Vec2 WorldMap::goal() const noexcept {
    return goal_;
}

int WorldMap::screen_for_y(const float world_y) const noexcept {
    const int band_from_top = static_cast<int>(std::floor(world_y / screen_height_));
    if (band_from_top < 0 || band_from_top >= screen_count()) {
        return -1;
    }
    return screen_count() - band_from_top - 1;
}

Biome WorldMap::biome_for_screen(const int zero_based_screen) const {
    if (zero_based_screen < 0 || zero_based_screen >= screen_count()) {
        throw std::out_of_range("screen is outside the world");
    }
    const int one_based_screen = zero_based_screen + 1;
    for (const BiomeRange& range : biomes_) {
        if (one_based_screen >= range.first_screen &&
            one_based_screen <= range.last_screen) {
            return range.biome;
        }
    }
    throw std::logic_error("screen has no biome");
}

}  // namespace jumpcastle
