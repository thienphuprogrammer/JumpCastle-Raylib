#include "jumpcastle/renderer.hpp"

#include "jumpcastle/player_view.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace jumpcastle {
namespace {

Biome asset_biome(const WorldBiome biome) noexcept {
    switch (biome) {
    case WorldBiome::courtyard: return Biome::pixel_adventure;
    case WorldBiome::frosted_keep: return Biome::kenney;
    case WorldBiome::crown_spire: return Biome::kings_and_pigs;
    }
    return Biome::pixel_adventure;
}

std::string_view biome_name(const WorldBiome biome) noexcept {
    switch (biome) {
    case WorldBiome::courtyard: return "Castle Courtyard";
    case WorldBiome::frosted_keep: return "Frosted Keep";
    case WorldBiome::crown_spire: return "Crown Spire";
    }
    return "Unknown";
}

Color biome_background(const WorldBiome biome) noexcept {
    switch (biome) {
    case WorldBiome::courtyard: return {20, 36, 46, 255};
    case WorldBiome::frosted_keep: return {24, 27, 42, 255};
    case WorldBiome::crown_spire: return {36, 24, 42, 255};
    }
    return {15, 5, 45, 255};
}

void draw_region(
    const Texture2D texture,
    const SpriteRegion& region,
    const Rectangle destination,
    const Color tint = WHITE,
    const bool flip_horizontal = false) {
    const Rectangle source = sprite_source_rectangle(region, flip_horizontal);
    DrawTexturePro(texture, source, destination, {}, 0.0F, tint);
}

void draw_background(
    const Texture2D texture,
    const BiomeAssets& assets,
    const WorldBiome biome) {
    ClearBackground(biome_background(biome));
    for (int y = 0; y < config::view_height; y += config::tile_pixels) {
        for (int x = 0; x < config::view_width; x += config::tile_pixels) {
            draw_region(
                texture,
                assets.background,
                {
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(config::tile_pixels),
                    static_cast<float>(config::tile_pixels),
                },
                Fade(WHITE, 0.12F));
        }
    }
}

bool is_solid(const WorldMap& world, const int x, const int y) noexcept {
    return world.solid_at(x, y);
}

SpriteRegion terrain_region(
    const WorldMap& world,
    const int x,
    const int y,
    const TerrainGrid& grid) noexcept {
    const bool top = is_solid(world, x, y - 1);
    const bool bottom = is_solid(world, x, y + 1);
    const bool right = is_solid(world, x + 1, y);
    const bool left = is_solid(world, x - 1, y);
    const bool top_right = is_solid(world, x + 1, y - 1);
    const bool bottom_right = is_solid(world, x + 1, y + 1);
    const bool top_left = is_solid(world, x - 1, y - 1);
    const bool bottom_left = is_solid(world, x - 1, y + 1);

    int sprite_x = 1;
    int sprite_y = 1;
    if (top) ++sprite_y;
    if (bottom) --sprite_y;
    if (right) --sprite_x;
    if (left) ++sprite_x;

    if (!top && !bottom && !right && !left) {
        sprite_x = 3;
        sprite_y = 3;
    }
    if (!left && !right && sprite_x == 1) sprite_x = 3;
    if (!top && !bottom && sprite_y == 1) sprite_y = 3;

    if (sprite_x == 1 && sprite_y == 1) {
        if (!top_right && bottom_right && top_left && bottom_left) {
            sprite_x = 4;
            sprite_y = 2;
        } else if (top_right && !bottom_right && top_left && bottom_left) {
            sprite_x = 4;
            sprite_y = 0;
        } else if (top_right && bottom_right && !top_left && bottom_left) {
            sprite_x = 6;
            sprite_y = 2;
        } else if (top_right && bottom_right && top_left && !bottom_left) {
            sprite_x = 6;
            sprite_y = 0;
        }
    }

    sprite_x = std::clamp(sprite_x, 0, grid.columns - 1);
    sprite_y = std::clamp(sprite_y, 0, grid.rows - 1);
    return {
        grid.x + sprite_x * grid.tile_size,
        grid.y + sprite_y * grid.tile_size,
        grid.tile_size,
        grid.tile_size,
    };
}

void draw_world(
    const WorldMap& world,
    const CameraBand& camera,
    const BiomeAssets& assets,
    const Texture2D texture) {
    const int first_row = static_cast<int>(camera.world_top);
    const int last_row = first_row + world.screen_height();
    for (int y = first_row; y < last_row; ++y) {
        for (int x = 0; x < world.width(); ++x) {
            const Rectangle destination{
                static_cast<float>(x * config::tile_pixels),
                static_cast<float>((y - first_row) * config::tile_pixels),
                static_cast<float>(config::tile_pixels),
                static_cast<float>(config::tile_pixels),
            };
            if (world.solid_at(x, y)) {
                draw_region(
                    texture,
                    terrain_region(world, x, y, assets.terrain),
                    destination);
            }
        }
    }

    const Vec2 goal = world.goal();
    if (goal.y >= camera.world_top &&
        goal.y < camera.world_top + static_cast<float>(world.screen_height())) {
        draw_region(
            texture,
            assets.exit,
            {
                std::floor(goal.x) * config::tile_pixels,
                std::floor(goal.y - camera.world_top) * config::tile_pixels,
                static_cast<float>(config::tile_pixels),
                static_cast<float>(config::tile_pixels),
            });
    }
}

void draw_player(
    const PlayerState& player,
    const float screen_offset_y,
    const float respawn_animation_time,
    const AssetCatalog& catalog,
    const Texture2D texture) {
    const PlayerAnimation state = select_animation(player, respawn_animation_time);
    const AnimationClip& clip = catalog.animation(state);
    const std::size_t frame = animation_frame_index(
        player.animation_time, clip.fps, clip.frames.size());
    draw_region(
        texture,
        clip.frames[frame],
        player_sprite_destination(player.position, screen_offset_y),
        WHITE,
        !player.facing_right);
}

void draw_completion(const CampaignState& campaign) {
    DrawRectangle(20, 52, config::view_width - 40, 84, Fade(BLACK, 0.82F));
    DrawRectangleLines(20, 52, config::view_width - 40, 84, GOLD);
    DrawText("THE CROWN IS YOURS", 48, 66, 16, GOLD);
    DrawText(
        TextFormat("Time %.1fs   Falls %i", campaign.elapsed_seconds, campaign.falls),
        52,
        92,
        10,
        RAYWHITE);
    DrawText("Press ENTER to restart", 63, 112, 10, LIGHTGRAY);
}

void draw_debug_overlay(
    const WorldMap& world,
    const CameraBand& camera,
    const PlayerState& player,
    const CampaignState& campaign,
    const float scale,
    const ::Vector2 offset) {
    const int x = static_cast<int>(offset.x + 5.0F * scale);
    const int y = static_cast<int>(offset.y + 5.0F * scale);
    const int font_size = std::max(10, static_cast<int>(6.0F * scale));
    DrawRectangle(
        x - 3,
        y - 3,
        static_cast<int>(150.0F * scale),
        font_size * 6 + 8,
        Fade(BLACK, 0.78F));
    DrawText(
        TextFormat("Screen %i/%i", camera.screen + 1, world.screen_count()),
        x, y, font_size, RAYWHITE);
    DrawText(
        TextFormat("Biome: %s", biome_name(camera.biome).data()),
        x, y + font_size, font_size, SKYBLUE);
    DrawText(
        TextFormat("Falls %i", campaign.falls),
        x, y + font_size * 2, font_size, GOLD);
    DrawText(
        TextFormat("Position %.2f, %.2f", player.position.x, player.position.y),
        x, y + font_size * 3, font_size, LIGHTGRAY);
    DrawText(
        TextFormat("Velocity %.2f, %.2f", player.velocity.x, player.velocity.y),
        x, y + font_size * 4, font_size, LIGHTGRAY);
    DrawText(
        TextFormat(
            "Charge %.0f%%",
            std::clamp(
                player.jump_hold_time / config::maximum_charge_seconds,
                0.0F,
                1.0F) * 100.0F),
        x, y + font_size * 5, font_size, LIME);
}

}  // namespace

TextureResource::TextureResource(const std::filesystem::path& path)
    : texture_{LoadTexture(path.string().c_str())} {
    if (!IsTextureValid(texture_)) {
        throw std::runtime_error("Unable to load texture: " + path.string());
    }
    SetTextureFilter(texture_, TEXTURE_FILTER_POINT);
}

TextureResource::~TextureResource() {
    if (IsTextureValid(texture_)) {
        UnloadTexture(texture_);
    }
}

TextureResource::TextureResource(TextureResource&& other) noexcept
    : texture_{std::exchange(other.texture_, {})} {}

TextureResource& TextureResource::operator=(TextureResource&& other) noexcept {
    if (this != &other) {
        if (IsTextureValid(texture_)) {
            UnloadTexture(texture_);
        }
        texture_ = std::exchange(other.texture_, {});
    }
    return *this;
}

const Texture2D& TextureResource::get() const noexcept {
    return texture_;
}

RenderTargetResource::RenderTargetResource(const int width, const int height)
    : target_{LoadRenderTexture(width, height)} {
    if (!IsRenderTextureValid(target_)) {
        throw std::runtime_error(
            "Unable to create render target " + std::to_string(width) + "x" +
            std::to_string(height));
    }
    SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
}

RenderTargetResource::~RenderTargetResource() {
    if (IsRenderTextureValid(target_)) {
        UnloadRenderTexture(target_);
    }
}

RenderTargetResource::RenderTargetResource(RenderTargetResource&& other) noexcept
    : target_{std::exchange(other.target_, {})} {}

RenderTargetResource& RenderTargetResource::operator=(RenderTargetResource&& other) noexcept {
    if (this != &other) {
        if (IsRenderTextureValid(target_)) {
            UnloadRenderTexture(target_);
        }
        target_ = std::exchange(other.target_, {});
    }
    return *this;
}

const RenderTexture2D& RenderTargetResource::get() const noexcept {
    return target_;
}

Renderer::Renderer(const std::filesystem::path& asset_directory)
    : catalog_{AssetCatalog::load(asset_directory / "generated" / "manifest.json")},
      player_texture_{catalog_.player_atlas()},
      castle_texture_{catalog_.biome(Biome::pixel_adventure).atlas},
      pixelart_target_{config::view_width, config::view_height} {}

void Renderer::draw(
    const WorldMap& world,
    const CameraBand& camera,
    const PlayerState& player,
    const CampaignState& campaign,
    const float respawn_animation_time,
    const bool debug_enabled) const {
    const Biome biome = asset_biome(camera.biome);
    const BiomeAssets& assets = catalog_.biome(biome);
    const Texture2D& biome_texture = castle_texture_.get();

    BeginTextureMode(pixelart_target_.get());
    draw_background(biome_texture, assets, camera.biome);
    draw_world(world, camera, assets, biome_texture);
    draw_player(
        player,
        camera.world_top,
        respawn_animation_time,
        catalog_,
        player_texture_.get());
    if (campaign.complete) draw_completion(campaign);
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);

    const float window_width = static_cast<float>(GetScreenWidth());
    const float window_height = static_cast<float>(GetScreenHeight());
    const float scale = std::max(
        1.0F,
        std::floor(std::min(
            window_width / static_cast<float>(config::view_width),
            window_height / static_cast<float>(config::view_height))));
    const ::Vector2 size{
        scale * static_cast<float>(config::view_width),
        scale * static_cast<float>(config::view_height),
    };
    const ::Vector2 offset{
        (window_width - size.x) * 0.5F,
        (window_height - size.y) * 0.5F,
    };

    const Texture2D& target_texture = pixelart_target_.get().texture;
    DrawTexturePro(
        target_texture,
        {0.0F, 0.0F, static_cast<float>(target_texture.width),
         -static_cast<float>(target_texture.height)},
        {offset.x, offset.y, size.x, size.y},
        {},
        0.0F,
        WHITE);

    if (debug_enabled) {
        draw_debug_overlay(world, camera, player, campaign, scale, offset);
    }

    EndDrawing();
}

}  // namespace jumpcastle
