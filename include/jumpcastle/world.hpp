#pragma once

#include "jumpcastle/math.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace jumpcastle {

enum class WorldTile : char {
    empty = '.',
    solid = '#',
};

enum class WorldBiome {
    courtyard,
    frosted_keep,
    crown_spire,
};

struct BiomeRange {
    int first_screen{};
    int last_screen{};
    WorldBiome biome{WorldBiome::courtyard};
};

class WorldMap {
public:
    WorldMap(
        int width,
        int height,
        int screen_height,
        std::vector<WorldTile> tiles,
        Vec2 spawn,
        Vec2 goal,
        std::vector<BiomeRange> biomes);

    [[nodiscard]] static WorldMap load(const std::filesystem::path& path);

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] int screen_height() const noexcept;
    [[nodiscard]] int screen_count() const noexcept;
    [[nodiscard]] WorldTile tile_at(int x, int y) const noexcept;
    [[nodiscard]] bool solid_at(int x, int y) const noexcept;
    [[nodiscard]] Vec2 spawn() const noexcept;
    [[nodiscard]] Vec2 goal() const noexcept;
    [[nodiscard]] int screen_for_y(float world_y) const noexcept;
    [[nodiscard]] WorldBiome biome_for_screen(int zero_based_screen) const;

private:
    int width_{};
    int height_{};
    int screen_height_{};
    std::vector<WorldTile> tiles_;
    Vec2 spawn_{};
    Vec2 goal_{};
    std::vector<BiomeRange> biomes_;
};

[[nodiscard]] WorldMap parse_campaign(
    std::string_view source,
    std::string_view filename);

}  // namespace jumpcastle
