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

std::size_t biome_index(const Biome biome) noexcept {
    switch (biome) {
    case Biome::pixel_adventure: return 0;
    case Biome::kenney: return 1;
    case Biome::kings_and_pigs: return 2;
    }
    return 0;
}

std::string_view biome_name(const Biome biome) noexcept {
    switch (biome) {
    case Biome::pixel_adventure: return "Pixel Adventure";
    case Biome::kenney: return "Kenney Clockworks";
    case Biome::kings_and_pigs: return "Kings and Pigs";
    }
    return "Unknown";
}

Color biome_background(const Biome biome) noexcept {
    switch (biome) {
    case Biome::pixel_adventure: return {20, 36, 46, 255};
    case Biome::kenney: return {24, 27, 42, 255};
    case Biome::kings_and_pigs: return {36, 24, 42, 255};
    }
    return config::background_color;
}

Rectangle source_rectangle(const SpriteRegion& region) noexcept {
    return {
        static_cast<float>(region.x),
        static_cast<float>(region.y),
        static_cast<float>(region.width),
        static_cast<float>(region.height),
    };
}

void draw_region(
    const Texture2D texture,
    const SpriteRegion& region,
    const Rectangle destination,
    const Color tint = WHITE,
    const bool flip_horizontal = false) {
    Rectangle source = source_rectangle(region);
    if (flip_horizontal) {
        source.x += source.width;
        source.width = -source.width;
    }
    DrawTexturePro(texture, source, destination, {}, 0.0F, tint);
}

void draw_background(
    const Texture2D texture,
    const BiomeAssets& assets,
    const Biome biome) {
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

bool is_solid(const Tilemap& tilemap, const int x, const int y) noexcept {
    return tilemap.solid_for_render_at(x, y);
}

SpriteRegion terrain_region(
    const Tilemap& tilemap,
    const int x,
    const int y,
    const TerrainGrid& grid) noexcept {
    const bool top = is_solid(tilemap, x, y - 1);
    const bool bottom = is_solid(tilemap, x, y + 1);
    const bool right = is_solid(tilemap, x + 1, y);
    const bool left = is_solid(tilemap, x - 1, y);
    const bool top_right = is_solid(tilemap, x + 1, y - 1);
    const bool bottom_right = is_solid(tilemap, x + 1, y + 1);
    const bool top_left = is_solid(tilemap, x - 1, y - 1);
    const bool bottom_left = is_solid(tilemap, x - 1, y + 1);

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

void draw_room(
    const RoomSelection& selection,
    const CampaignState& campaign,
    const BiomeAssets& assets,
    const Texture2D texture) {
    const Tilemap& tilemap = selection.room->tilemap;
    for (int y = 0; y < config::tilemap_height; ++y) {
        for (int x = 0; x < config::tilemap_width; ++x) {
            const Rectangle destination{
                static_cast<float>(x * config::tile_pixels),
                static_cast<float>(y * config::tile_pixels),
                static_cast<float>(config::tile_pixels),
                static_cast<float>(config::tile_pixels),
            };
            switch (tilemap.tile_at(x, y)) {
            case Tile::solid:
                draw_region(
                    texture,
                    terrain_region(tilemap, x, y, assets.terrain),
                    destination);
                break;
            case Tile::spike:
                draw_region(texture, assets.spike, destination);
                break;
            case Tile::checkpoint:
                draw_region(
                    texture,
                    assets.checkpoint,
                    destination,
                    campaign.checkpoint_room == selection.index ? WHITE : GRAY);
                break;
            case Tile::exit:
                draw_region(texture, assets.exit, destination);
                break;
            case Tile::spawn:
                draw_region(texture, assets.checkpoint, destination, Fade(WHITE, 0.35F));
                break;
            case Tile::empty:
                break;
            }
        }
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
        TextFormat("Time %.1fs   Deaths %i", campaign.elapsed_seconds, campaign.deaths),
        52,
        92,
        10,
        RAYWHITE);
    DrawText("Press ENTER to restart", 63, 112, 10, LIGHTGRAY);
}

void draw_debug_overlay(
    const RoomSelection& room,
    const PlayerState& player,
    const CampaignState& campaign,
    const float scale,
    const Vector2 offset) {
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
        TextFormat("Room %i/12: %s", static_cast<int>(room.index + 1),
                   room.room->metadata.name.c_str()),
        x, y, font_size, RAYWHITE);
    DrawText(
        TextFormat("Biome: %s  difficulty %i", biome_name(room.room->metadata.biome).data(),
                   room.room->metadata.difficulty),
        x, y + font_size, font_size, SKYBLUE);
    DrawText(
        TextFormat("Checkpoint %i  deaths %i", static_cast<int>(campaign.checkpoint_room + 1),
                   campaign.deaths),
        x, y + font_size * 2, font_size, GOLD);
    DrawText(
        TextFormat("Position %.2f, %.2f", player.position.x, player.position.y),
        x, y + font_size * 3, font_size, LIGHTGRAY);
    DrawText(
        TextFormat("Velocity %.2f, %.2f", player.velocity.x, player.velocity.y),
        x, y + font_size * 4, font_size, LIGHTGRAY);
    DrawText(
        TextFormat("Charge %.0f%%", std::clamp(player.jump_hold_time / 0.77F, 0.0F, 1.0F) * 100.0F),
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
      biome_textures_{
          TextureResource{catalog_.biome(Biome::pixel_adventure).atlas},
          TextureResource{catalog_.biome(Biome::kenney).atlas},
          TextureResource{catalog_.biome(Biome::kings_and_pigs).atlas},
      },
      pixelart_target_{config::view_width, config::view_height} {}

void Renderer::draw(
    const RoomSelection& room,
    const PlayerState& player,
    const CampaignState& campaign,
    const float respawn_animation_time,
    const bool debug_enabled) const {
    const Biome biome = room.room->metadata.biome;
    const BiomeAssets& assets = catalog_.biome(biome);
    const Texture2D& biome_texture = biome_textures_[biome_index(biome)].get();

    BeginTextureMode(pixelart_target_.get());
    draw_background(biome_texture, assets, biome);
    draw_room(room, campaign, assets, biome_texture);
    draw_player(
        player,
        room.vertical_offset,
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
    const Vector2 size{
        scale * static_cast<float>(config::view_width),
        scale * static_cast<float>(config::view_height),
    };
    const Vector2 offset{
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
        draw_debug_overlay(room, player, campaign, scale, offset);
    }

    EndDrawing();
}

}  // namespace jumpcastle
