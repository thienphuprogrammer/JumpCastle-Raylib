#include "jumpcastle/campaign_world.hpp"

#include "jumpcastle/assets.hpp"
#include "jumpcastle/map_validation.hpp"

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

// Pixel dimensions of the shared terrain atlas, resolved from the asset
// manifest that sits alongside the screens directory (`<root>/generated/
// manifest.json` next to `<root>/levels/screens`). `available` is false when
// no manifest is found there -- e.g. callers that load a bare directory of
// screen files with no asset tree (tests, ad hoc solver runs) -- so
// atlas-dependent GID validation is skipped rather than failing to locate
// assets that were never provided.
struct AtlasPixelSize {
    int width{};
    int height{};
    bool available{};
};

AtlasPixelSize resolve_atlas_pixel_size(const std::filesystem::path& screens_directory) {
    const std::filesystem::path manifest_path =
        screens_directory.parent_path().parent_path() / "generated" / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        return {};
    }
    const AssetCatalog catalog = AssetCatalog::load(manifest_path);
    // All three biomes share one "castle" atlas texture, so any biome's
    // stored atlas dimensions describe the whole atlas.
    const BiomeAssets& reference = catalog.biome(Biome::pixel_adventure);
    return {reference.atlas_width, reference.atlas_height, true};
}

// Validates every loaded screen's tile GIDs against the resolved atlas, using
// each screen's own authored tile size to convert the atlas' pixel
// dimensions into a tile count. Never hardcodes the atlas' tile count -- it
// is always the product of the atlas' pixel size and the screen's tile size.
void validate_against_atlas(
    const std::vector<ScreenMap>& screens, const AtlasPixelSize& atlas) {
    if (!atlas.available) {
        return;
    }
    for (const ScreenMap& screen : screens) {
        const int tile_size = screen.tileset_tile_size > 0 ? screen.tileset_tile_size : 1;
        validate_screen_map(screen, MapValidationContext{
            .atlas_columns = atlas.width / tile_size,
            .atlas_rows = atlas.height / tile_size,
        });
    }
}

}  // namespace

WorldBiome CampaignWorld::biome_for_screen(const int screen) const noexcept {
    if (screen < 0 || screen >= static_cast<int>(screen_biomes.size())) {
        return WorldBiome::courtyard;
    }
    return screen_biomes[static_cast<std::size_t>(screen)];
}

const ScreenMap* CampaignWorld::screen_map(const int screen) const noexcept {
    for (const ScreenMap& map : screens) {
        if (map.index == screen) { return &map; }
    }
    return nullptr;
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
    world.screens = screens;  // retained for the in-game editor (lossless round-trip)
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
    validate_against_atlas(screens, resolve_atlas_pixel_size(directory));

    int height = screen_height;
    if (height <= 0 && !screens.empty()) {
        // Derive the band height from the files so callers need not know it.
        height = std::max(1, static_cast<int>(std::lround(screens.front().height)));
    }
    return from_screens(screens, height);
}

}  // namespace jumpcastle
