#include "jumpcastle/assets.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace jumpcastle {
namespace {

using Json = nlohmann::json;

constexpr std::size_t biome_index(const Biome biome) noexcept {
    switch (biome) {
    case Biome::pixel_adventure: return 0;
    case Biome::kenney: return 1;
    case Biome::kings_and_pigs: return 2;
    }
    return 0;
}

constexpr std::size_t animation_index(const PlayerAnimation animation) noexcept {
    switch (animation) {
    case PlayerAnimation::idle: return 0;
    case PlayerAnimation::run: return 1;
    case PlayerAnimation::charge: return 2;
    case PlayerAnimation::rise: return 3;
    case PlayerAnimation::fall: return 4;
    case PlayerAnimation::respawn: return 5;
    }
    return 0;
}

const Json& required(const Json& object, const char* key, const std::string& label) {
    if (!object.is_object() || !object.contains(key)) {
        throw std::runtime_error(label + " is missing '" + key + "'");
    }
    return object.at(key);
}

int positive_integer(const Json& value, const std::string& label) {
    if (!value.is_number_integer()) {
        throw std::runtime_error(label + " must be an integer");
    }
    const int parsed = value.get<int>();
    if (parsed <= 0) {
        throw std::runtime_error(label + " must be positive");
    }
    return parsed;
}

int nonnegative_integer(const Json& value, const std::string& label) {
    if (!value.is_number_integer()) {
        throw std::runtime_error(label + " must be an integer");
    }
    const int parsed = value.get<int>();
    if (parsed < 0) {
        throw std::runtime_error(label + " cannot be negative");
    }
    return parsed;
}

SpriteRegion parse_region(
    const Json& value,
    const int atlas_width,
    const int atlas_height,
    const std::string& label) {
    SpriteRegion region{
        nonnegative_integer(required(value, "x", label), label + ".x"),
        nonnegative_integer(required(value, "y", label), label + ".y"),
        positive_integer(required(value, "width", label), label + ".width"),
        positive_integer(required(value, "height", label), label + ".height"),
    };
    if (region.x + region.width > atlas_width ||
        region.y + region.height > atlas_height) {
        throw std::runtime_error(label + " exceeds atlas bounds");
    }
    return region;
}

TerrainGrid parse_terrain(
    const Json& value,
    const int atlas_width,
    const int atlas_height,
    const std::string& label) {
    TerrainGrid grid{
        nonnegative_integer(required(value, "x", label), label + ".x"),
        nonnegative_integer(required(value, "y", label), label + ".y"),
        positive_integer(required(value, "tile_size", label), label + ".tile_size"),
        positive_integer(required(value, "columns", label), label + ".columns"),
        positive_integer(required(value, "rows", label), label + ".rows"),
    };
    if (grid.x + grid.width() > atlas_width ||
        grid.y + grid.height() > atlas_height) {
        throw std::runtime_error(label + " exceeds atlas bounds");
    }
    return grid;
}

std::filesystem::path parse_atlas_path(
    const Json& atlas,
    const std::filesystem::path& directory,
    const std::string& label) {
    const Json& file = required(atlas, "file", label);
    if (!file.is_string()) {
        throw std::runtime_error(label + ".file must be a string");
    }
    const std::filesystem::path filename{file.get<std::string>()};
    if (filename.empty() || filename.has_parent_path() || filename.extension() != ".png") {
        throw std::runtime_error(label + ".file must be a local PNG filename");
    }
    const auto path = directory / filename;
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("atlas file is missing: " + path.string());
    }
    return path;
}

BiomeAssets parse_biome(
    const Json& atlas,
    const std::filesystem::path& directory,
    const std::string& label) {
    const int width = positive_integer(
        required(atlas, "width", label), label + ".width");
    const int height = positive_integer(
        required(atlas, "height", label), label + ".height");
    const Json& regions = required(atlas, "regions", label);
    return {
        parse_atlas_path(atlas, directory, label),
        width,
        height,
        parse_terrain(required(atlas, "terrain_grid", label), width, height,
                      label + ".terrain_grid"),
        parse_region(required(regions, "spike", label + ".regions"),
                     width, height, label + ".regions.spike"),
        parse_region(required(regions, "checkpoint", label + ".regions"),
                     width, height, label + ".regions.checkpoint"),
        parse_region(required(regions, "exit", label + ".regions"),
                     width, height, label + ".regions.exit"),
        parse_region(required(regions, "background", label + ".regions"),
                     width, height, label + ".regions.background"),
    };
}

AnimationClip parse_animation(
    const Json& value,
    const int atlas_width,
    const int atlas_height,
    const std::string& label) {
    const Json& atlas = required(value, "atlas", label);
    if (!atlas.is_string() || atlas.get<std::string>() != "player") {
        throw std::runtime_error(label + " must reference the player atlas");
    }
    AnimationClip clip;
    clip.fps = positive_integer(required(value, "fps", label), label + ".fps");
    const Json& frames = required(value, "frames", label);
    if (!frames.is_array() || frames.empty()) {
        throw std::runtime_error(label + ".frames must be a non-empty array");
    }
    clip.frames.reserve(frames.size());
    for (std::size_t index = 0; index < frames.size(); ++index) {
        clip.frames.push_back(parse_region(
            frames.at(index), atlas_width, atlas_height,
            label + ".frames[" + std::to_string(index) + ']'));
    }
    return clip;
}

void require_exact_keys(
    const Json& object,
    const std::set<std::string>& expected,
    const std::string& label) {
    if (!object.is_object()) {
        throw std::runtime_error(label + " must be an object");
    }
    std::set<std::string> actual;
    for (const auto& [key, value] : object.items()) {
        static_cast<void>(value);
        actual.insert(key);
    }
    if (actual != expected) {
        throw std::runtime_error(label + " contains missing or unknown entries");
    }
}

}  // namespace

AssetCatalog AssetCatalog::load(const std::filesystem::path& manifest_path) {
    try {
        std::ifstream input{manifest_path};
        if (!input) {
            throw std::runtime_error("unable to open manifest");
        }
        Json manifest;
        input >> manifest;
        if (required(manifest, "schema_version", "manifest") != 1) {
            throw std::runtime_error("unsupported manifest schema");
        }

        const Json& atlases = required(manifest, "atlases", "manifest");
        require_exact_keys(
            atlases,
            {"pixel_adventure", "kenney", "kings_and_pigs", "player"},
            "manifest.atlases");

        const auto directory = manifest_path.parent_path();
        AssetCatalog catalog;
        catalog.biomes_[biome_index(Biome::pixel_adventure)] = parse_biome(
            atlases.at("pixel_adventure"), directory, "atlas.pixel_adventure");
        catalog.biomes_[biome_index(Biome::kenney)] = parse_biome(
            atlases.at("kenney"), directory, "atlas.kenney");
        catalog.biomes_[biome_index(Biome::kings_and_pigs)] = parse_biome(
            atlases.at("kings_and_pigs"), directory, "atlas.kings_and_pigs");

        const Json& player = atlases.at("player");
        const int player_width = positive_integer(
            required(player, "width", "atlas.player"), "atlas.player.width");
        const int player_height = positive_integer(
            required(player, "height", "atlas.player"), "atlas.player.height");
        catalog.player_atlas_ = parse_atlas_path(player, directory, "atlas.player");

        const Json& animations = required(manifest, "animations", "manifest");
        require_exact_keys(
            animations,
            {"idle", "run", "charge", "rise", "fall", "respawn"},
            "manifest.animations");
        for (const PlayerAnimation animation : {
                 PlayerAnimation::idle,
                 PlayerAnimation::run,
                 PlayerAnimation::charge,
                 PlayerAnimation::rise,
                 PlayerAnimation::fall,
                 PlayerAnimation::respawn}) {
            const std::string name{to_string(animation)};
            catalog.animations_[animation_index(animation)] = parse_animation(
                animations.at(name), player_width, player_height,
                "animation." + name);
        }
        return catalog;
    } catch (const std::exception& error) {
        throw std::runtime_error(
            "Unable to load asset manifest " + manifest_path.string() + ": " + error.what());
    }
}

const BiomeAssets& AssetCatalog::biome(const Biome biome_value) const noexcept {
    return biomes_[biome_index(biome_value)];
}

const AnimationClip& AssetCatalog::animation(
    const PlayerAnimation animation_value) const noexcept {
    return animations_[animation_index(animation_value)];
}

const std::filesystem::path& AssetCatalog::player_atlas() const noexcept {
    return player_atlas_;
}

std::string_view to_string(const PlayerAnimation animation) noexcept {
    switch (animation) {
    case PlayerAnimation::idle: return "idle";
    case PlayerAnimation::run: return "run";
    case PlayerAnimation::charge: return "charge";
    case PlayerAnimation::rise: return "rise";
    case PlayerAnimation::fall: return "fall";
    case PlayerAnimation::respawn: return "respawn";
    }
    return "idle";
}

}  // namespace jumpcastle
