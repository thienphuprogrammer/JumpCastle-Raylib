#include "jumpcastle/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace jumpcastle {
namespace {

[[nodiscard]] bool is_solid(const Tilemap& tilemap, const int x, const int y) noexcept {
    return tilemap.solid_for_render_at(x, y);
}

void draw_sprite_sheet_tile(
    const Texture2D texture,
    const int sprite_x,
    const int sprite_y,
    const int sprite_size,
    const Vector2 position,
    const Vector2 scale) {
    DrawTextureRec(
        texture,
        {
            static_cast<float>(sprite_x * sprite_size),
            static_cast<float>(sprite_y * sprite_size),
            static_cast<float>(sprite_size) * scale.x,
            static_cast<float>(sprite_size) * scale.y,
        },
        position,
        WHITE);
}

[[nodiscard]] Vector2 world_to_screen(const Vector2 position) noexcept {
    return {
        position.x * static_cast<float>(config::tile_pixels),
        position.y * static_cast<float>(config::tile_pixels),
    };
}

void draw_tilemap(const Tilemap& tilemap, const Texture2D texture) {
    for (int x = 0; x < config::tilemap_width; ++x) {
        for (int y = 0; y < config::tilemap_height; ++y) {
            if (!tilemap.solid_at(x, y)) {
                continue;
            }

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
            if (top) sprite_y += 1;
            if (bottom) sprite_y -= 1;
            if (right) sprite_x -= 1;
            if (left) sprite_x += 1;

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

            draw_sprite_sheet_tile(
                texture,
                sprite_x,
                sprite_y,
                config::tile_pixels,
                {
                    static_cast<float>(x * config::tile_pixels),
                    static_cast<float>(y * config::tile_pixels),
                },
                {1.0F, 1.0F});
        }
    }
}

void draw_player(
    const PlayerState& player,
    const float screen_offset_y,
    const Texture2D texture) {
    int sprite = 0;
    if (player.on_ground) {
        if (std::abs(player.velocity.x) > 0.01F) {
            sprite = 1 + static_cast<int>(std::floor(player.animation_time * 6.0F)) % 2;
        }
        if (player.jump_hold_time > 0.001F) {
            sprite = 4;
        }
    } else {
        sprite = player.velocity.y > 0.0F ? 5 : 6;
    }

    const Vector2 screen_position = world_to_screen({
        player.position.x,
        player.position.y - screen_offset_y,
    });
    draw_sprite_sheet_tile(
        texture,
        sprite,
        0,
        config::tile_pixels,
        {screen_position.x - 8.0F, screen_position.y - 10.0F},
        {player.facing_right ? 1.0F : -1.0F, 1.0F});
}

void draw_debug_overlay(
    const Tilemap& tilemap,
    const PlayerState& player,
    const float screen_offset_y,
    const std::size_t screen_index,
    const float scale,
    const Vector2 offset) {
    for (int x = 0; x < config::tilemap_width; ++x) {
        for (int y = 0; y < config::tilemap_height; ++y) {
            const Tile tile = tilemap.tile_at(x, y);
            const Vector2 label_position{
                offset.x + static_cast<float>(x * config::tile_pixels) * scale + 3.0F,
                offset.y + static_cast<float>(y * config::tile_pixels) * scale + 3.0F,
            };
            DrawTextEx(
                GetFontDefault(),
                TextFormat("[%i,%i]\n%i\n'%c'", x, y, static_cast<int>(tile),
                           static_cast<char>(tile)),
                label_position,
                10.0F,
                1.0F,
                RED);
        }
    }

    const TileRange range = overlapped_tiles(
        {player.position.x, player.position.y - screen_offset_y},
        config::player_half_size);
    for (int x = range.start_x; x <= range.end_x; ++x) {
        for (int y = range.start_y; y <= range.end_y; ++y) {
            DrawRectangle(
                static_cast<int>(offset.x + static_cast<float>(x * config::tile_pixels) * scale + 1.0F),
                static_cast<int>(offset.y + static_cast<float>(y * config::tile_pixels) * scale + 1.0F),
                static_cast<int>(static_cast<float>(config::tile_pixels) * scale - 2.0F),
                static_cast<int>(static_cast<float>(config::tile_pixels) * scale - 2.0F),
                Fade(RED, 0.4F));
        }
    }

    DrawFPS(1, 1);
    DrawText(TextFormat("player.position = [%.3f, %.3f]", player.position.x, player.position.y),
             1, 88, 20, WHITE);
    DrawText(TextFormat("player.jump_hold_time = %.3f", player.jump_hold_time),
             1, 110, 20, WHITE);
    DrawText(TextFormat("screen_offset = %.3f", screen_offset_y), 1, 132, 20, WHITE);
    DrawText(TextFormat("screen_index = %i", static_cast<int>(screen_index)),
             1, 154, 20, WHITE);
}

}  // namespace

TextureResource::TextureResource(const std::filesystem::path& path)
    : texture_{LoadTexture(path.string().c_str())} {
    if (!IsTextureValid(texture_)) {
        throw std::runtime_error("Unable to load texture: " + path.string());
    }
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
    : player_texture_{asset_directory / "player.png"},
      tilemap_texture_{asset_directory / "tilemap.png"},
      pixelart_target_{config::view_width, config::view_height} {}

void Renderer::draw(
    const Tilemap& tilemap,
    const float screen_offset_y,
    const PlayerState& player,
    const bool debug_enabled,
    const std::size_t screen_index) const {
    BeginTextureMode(pixelart_target_.get());
    ClearBackground(config::background_color);
    draw_tilemap(tilemap, tilemap_texture_.get());
    draw_player(player, screen_offset_y, player_texture_.get());
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
        {0.0F, 0.0F},
        0.0F,
        WHITE);

    if (debug_enabled) {
        draw_debug_overlay(
            tilemap,
            player,
            screen_offset_y,
            screen_index,
            scale,
            offset);
    }

    EndDrawing();
}

}  // namespace jumpcastle
