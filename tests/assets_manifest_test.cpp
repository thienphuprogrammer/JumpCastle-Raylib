#include "jumpcastle/assets.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace {

std::filesystem::path manifest_path() {
    return std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets" / "generated" / "manifest.json";
}

}  // namespace

TEST_CASE("catalog exposes every biome and King animation") {
    const jumpcastle::AssetCatalog catalog =
        jumpcastle::AssetCatalog::load(manifest_path());

    CHECK(catalog.biome(jumpcastle::Biome::pixel_adventure).terrain.width() > 0);
    CHECK(catalog.biome(jumpcastle::Biome::kenney).terrain.width() > 0);
    CHECK(catalog.biome(jumpcastle::Biome::kings_and_pigs).terrain.width() > 0);
    CHECK(catalog.animation(jumpcastle::PlayerAnimation::idle).frames.size() >= 1);
    CHECK(catalog.animation(jumpcastle::PlayerAnimation::run).frames.size() >= 2);
    CHECK(catalog.animation(jumpcastle::PlayerAnimation::charge).frames.size() >= 1);
    CHECK(catalog.animation(jumpcastle::PlayerAnimation::rise).frames.size() >= 1);
    CHECK(catalog.animation(jumpcastle::PlayerAnimation::fall).frames.size() >= 1);
    CHECK(catalog.animation(jumpcastle::PlayerAnimation::respawn).frames.size() >= 1);
}

TEST_CASE("catalog resolves atlas files relative to manifest") {
    const jumpcastle::AssetCatalog catalog =
        jumpcastle::AssetCatalog::load(manifest_path());

    CHECK(catalog.player_atlas().filename() == "player.png");
    CHECK(catalog.biome(jumpcastle::Biome::kenney).atlas.filename() == "kenney.png");
}
