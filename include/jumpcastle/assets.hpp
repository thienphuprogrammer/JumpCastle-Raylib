#pragma once

#include "jumpcastle/level.hpp"

#include <array>
#include <filesystem>
#include <string_view>
#include <vector>

namespace jumpcastle {

struct SpriteRegion {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct TerrainGrid {
    int x{};
    int y{};
    int tile_size{};
    int columns{};
    int rows{};

    [[nodiscard]] constexpr int width() const noexcept {
        return columns * tile_size;
    }

    [[nodiscard]] constexpr int height() const noexcept {
        return rows * tile_size;
    }
};

struct BiomeAssets {
    std::filesystem::path atlas;
    int atlas_width{};
    int atlas_height{};
    TerrainGrid terrain;
    SpriteRegion spike;
    SpriteRegion checkpoint;
    SpriteRegion exit;
    SpriteRegion background;
};

enum class PlayerAnimation {
    idle,
    run,
    charge,
    rise,
    fall,
    respawn,
};

struct AnimationClip {
    int fps{};
    std::vector<SpriteRegion> frames;
};

class AssetCatalog {
public:
    [[nodiscard]] static AssetCatalog load(
        const std::filesystem::path& manifest_path);

    [[nodiscard]] const BiomeAssets& biome(Biome biome) const noexcept;
    [[nodiscard]] const AnimationClip& animation(
        PlayerAnimation animation) const noexcept;
    [[nodiscard]] const std::filesystem::path& player_atlas() const noexcept;

private:
    std::array<BiomeAssets, 3> biomes_;
    std::array<AnimationClip, 6> animations_;
    std::filesystem::path player_atlas_;
};

[[nodiscard]] std::string_view to_string(PlayerAnimation animation) noexcept;

}  // namespace jumpcastle
