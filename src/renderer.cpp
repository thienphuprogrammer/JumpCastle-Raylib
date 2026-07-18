#include "jumpcastle/renderer.hpp"

#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player_view.hpp"
#include "jumpcastle/presentation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// Per-biome multiplicative tint applied to terrain tiles so the three biomes
// read as distinct climates while sharing one tileset: warm stone in the
// courtyard, icy blue in the frosted keep, twilight violet in the crown spire.
Color biome_terrain_tint(const WorldBiome biome) noexcept {
    switch (biome) {
    case WorldBiome::courtyard: return {255, 244, 222, 255};
    case WorldBiome::frosted_keep: return {176, 206, 255, 255};
    case WorldBiome::crown_spire: return {214, 178, 236, 255};
    }
    return WHITE;
}

// Atmospheric haze tint for the parallax sky layers: warm dusk over the
// courtyard, pale ice over the frosted keep, twilight violet over the spire.
// Kept separate from the terrain tint so the distant sky can read cooler and
// softer than the bricks in the foreground.
Color biome_sky_tint(const WorldBiome biome) noexcept {
    switch (biome) {
    case WorldBiome::courtyard: return {255, 232, 196, 255};
    case WorldBiome::frosted_keep: return {198, 222, 255, 255};
    case WorldBiome::crown_spire: return {222, 196, 246, 255};
    }
    return WHITE;
}

// Multiply a color's RGB by a scalar (alpha preserved), clamped to byte range.
// Used to darken the gradient toward the top of the tower and to dim distant
// parallax layers so they recede.
Color scale_rgb(const Color color, const float factor) noexcept {
    const auto channel = [factor](const unsigned char value) noexcept {
        return static_cast<unsigned char>(
            std::clamp(static_cast<float>(value) * factor, 0.0F, 255.0F));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}

// A single vertical-parallax sky layer derived from the biome background tile.
// Distant layers use a small parallax fraction (they barely move as the camera
// climbs), a larger tile span (soft, out-of-focus shapes), lower opacity and
// reduced brightness; near layers scroll faster, stay crisp and bright.
struct ParallaxLayer {
    float parallax{};    // fraction of camera travel this layer scrolls
    float span{};        // on-screen tile size in pixels
    float alpha{};       // layer opacity
    float brightness{};  // RGB multiplier applied to the sky tint
};

inline constexpr std::array<ParallaxLayer, 3> parallax_layers{{
    {0.12F, 48.0F, 0.06F, 0.55F},  // far  — soft, dim, nearly static
    {0.30F, 32.0F, 0.09F, 0.78F},  // mid
    {0.55F, 16.0F, 0.12F, 1.00F},  // near — crisp, brighter, fastest
}};

void draw_region(
    const Texture2D texture,
    const SpriteRegion& region,
    const Rectangle destination,
    const Color tint = WHITE,
    const bool flip_horizontal = false) {
    const Rectangle source = sprite_source_rectangle(region, flip_horizontal);
    DrawTexturePro(texture, source, destination, {}, 0.0F, tint);
}

// Multi-layer vertical parallax sky. The camera only moves vertically as the
// player climbs the tower, so depth comes from scrolling distant layers slower
// than near ones. A per-biome vertical gradient (darker toward the top) forms
// the infinitely-distant backdrop; on top of it three layers derived from the
// biome background tile scroll at increasing fractions of the camera travel,
// each tinted by the biome's sky color. camera.world_top is a tile-row offset
// into the world, so multiplying it by tile_pixels yields the camera's pixel
// travel, and fmod keeps each layer's tiling seamless as it scrolls.
void draw_parallax_background(
    const Texture2D texture,
    const BiomeAssets& assets,
    const WorldBiome biome,
    const float world_top) {
    const Color base = biome_background(biome);
    ClearBackground(base);
    DrawRectangleGradientV(
        0, 0, config::view_width, config::view_height, scale_rgb(base, 0.35F), base);

    const Color sky = biome_sky_tint(biome);
    const float travel = world_top * static_cast<float>(config::tile_pixels);
    for (const ParallaxLayer& layer : parallax_layers) {
        const Color tint = Fade(scale_rgb(sky, layer.brightness), layer.alpha);
        const float span = layer.span;
        float shift = std::fmod(travel * layer.parallax, span);
        if (shift < 0.0F) shift += span;
        // Start one span above the top so the seam scrolled in from above is
        // always covered, and tile down past the bottom edge.
        for (float y = shift - span; y < static_cast<float>(config::view_height);
             y += span) {
            for (float x = 0.0F; x < static_cast<float>(config::view_width);
                 x += span) {
                draw_region(texture, assets.background, {x, y, span, span}, tint);
            }
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

    // The castle tileset's left-edge column (0) is a diagonal slope whose
    // top-left corner is transparent; its bricks only fill the lower-right of
    // the cell. Collision treats the whole tile as solid up to its top edge, so
    // selecting this slope for a tile with open sky above draws the surface a
    // few pixels below where the player actually stands and the knight appears
    // to hover. Any exposed walking surface must present a flat, fully opaque
    // top, so fall back to the solid fill column there.
    if (!top && sprite_x == 0) {
        sprite_x = 1;
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

const SpriteRegion& prop_region(const BiomeAssets& assets, const Prop prop) noexcept {
    switch (prop) {
    case Prop::torch: return assets.spike;
    case Prop::banner: return assets.checkpoint;
    case Prop::crown: return assets.exit;
    }
    return assets.spike;
}

void draw_world(
    const WorldMap& world,
    const CameraBand& camera,
    const BiomeAssets& assets,
    const Texture2D texture) {
    const int first_row = static_cast<int>(camera.world_top);
    const int last_row = first_row + world.screen_height();
    const Color terrain_tint = biome_terrain_tint(camera.biome);
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
                    destination,
                    terrain_tint);
            }
        }
    }

    for (const Decoration& decoration : world.decorations()) {
        if (decoration.y < first_row || decoration.y >= last_row) {
            continue;
        }
        draw_region(
            texture,
            prop_region(assets, decoration.prop),
            {
                static_cast<float>(decoration.x * config::tile_pixels),
                static_cast<float>((decoration.y - first_row) * config::tile_pixels),
                static_cast<float>(config::tile_pixels),
                static_cast<float>(config::tile_pixels),
            });
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

// ---- Polygon-world rendering (tinted polygon terrain) ----

::Vector2 world_to_screen(const Vec2 world_point, const float world_top) noexcept {
    return {
        world_point.x * static_cast<float>(config::tile_pixels),
        (world_point.y - world_top) * static_cast<float>(config::tile_pixels),
    };
}

Color polygon_fill_color(const ColliderType type, const WorldBiome biome) noexcept {
    switch (type) {
    case ColliderType::solid: return biome_terrain_tint(biome);
    case ColliderType::oneway: return scale_rgb(biome_terrain_tint(biome), 0.72F);
    case ColliderType::hazard: return {206, 74, 74, 255};
    }
    return biome_terrain_tint(biome);
}

void draw_convex_polygon(
    const ConvexPolygon& polygon, const float world_top, const Color fill) {
    std::vector<::Vector2> points;
    points.reserve(polygon.points.size());
    for (const Vec2 point : polygon.points) {
        points.push_back(world_to_screen(point, world_top));
    }
    // raylib's triangle fill expects a consistent winding; normalize it
    // (screen space is y-down).
    float area = 0.0F;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const ::Vector2 a = points[i];
        const ::Vector2 b = points[(i + 1) % points.size()];
        area += a.x * b.y - b.x * a.y;
    }
    if (area > 0.0F) {
        std::reverse(points.begin(), points.end());
    }
    DrawTriangleFan(points.data(), static_cast<int>(points.size()), fill);
    const Color outline = scale_rgb(fill, 0.5F);
    for (std::size_t i = 0; i < points.size(); ++i) {
        DrawLineV(points[i], points[(i + 1) % points.size()], outline);
    }
}

// The fully-surrounded interior fill sprite of the terrain tileset.
SpriteRegion terrain_fill_region(const TerrainGrid& grid) noexcept {
    const int col = std::min(1, grid.columns - 1);
    const int row = std::min(1, grid.rows - 1);
    return {
        grid.x + col * grid.tile_size,
        grid.y + row * grid.tile_size,
        grid.tile_size,
        grid.tile_size,
    };
}

bool is_axis_rect(const ConvexPolygon& polygon) noexcept {
    if (polygon.points.size() != 4) { return false; }
    for (const Vec2 point : polygon.points) {
        const bool on_x = point.x == polygon.aabb.min.x || point.x == polygon.aabb.max.x;
        const bool on_y = point.y == polygon.aabb.min.y || point.y == polygon.aabb.max.y;
        if (!on_x || !on_y) { return false; }
    }
    return true;
}

// Tile the terrain fill sprite across an axis-aligned rectangle collider so the
// merged polygon reads as textured terrain rather than a flat color.
void draw_textured_rect(
    const Texture2D texture,
    const SpriteRegion& fill,
    const ConvexPolygon& polygon,
    const float world_top,
    const Color tint) {
    const float tile = static_cast<float>(config::tile_pixels);
    for (float wy = polygon.aabb.min.y; wy < polygon.aabb.max.y; wy += 1.0F) {
        for (float wx = polygon.aabb.min.x; wx < polygon.aabb.max.x; wx += 1.0F) {
            const float w = std::min(1.0F, polygon.aabb.max.x - wx);
            const float h = std::min(1.0F, polygon.aabb.max.y - wy);
            const ::Vector2 screen = world_to_screen({wx, wy}, world_top);
            draw_region(texture, fill, {screen.x, screen.y, w * tile, h * tile}, tint);
        }
    }
}

void draw_world_polygons(
    const CampaignWorld& world,
    const CameraBand& camera,
    const BiomeAssets& assets,
    const Texture2D texture) {
    const SpriteRegion fill = terrain_fill_region(assets.terrain);
    const Color terrain_tint = biome_terrain_tint(camera.biome);
    const std::vector<ConvexPolygon>* polygons =
        world.collision.polygons_for_screen(camera.screen);
    if (polygons != nullptr) {
        for (const ConvexPolygon& polygon : *polygons) {
            if (polygon.type != ColliderType::hazard && is_axis_rect(polygon)) {
                const Color tint = polygon.type == ColliderType::oneway
                    ? scale_rgb(terrain_tint, 0.8F)
                    : terrain_tint;
                draw_textured_rect(texture, fill, polygon, camera.world_top, tint);
            } else {
                draw_convex_polygon(
                    polygon, camera.world_top,
                    polygon_fill_color(polygon.type, camera.biome));
            }
        }
    }

    const Vec2 goal = world.goal;
    if (goal.y >= camera.world_top &&
        goal.y < camera.world_top + static_cast<float>(world.screen_height)) {
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

void draw_debug_overlay(
    const int screen_count,
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
        TextFormat("Screen %i/%i", camera.screen + 1, screen_count),
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

namespace {

[[nodiscard]] Texture2D load_texture_or_placeholder(
    const std::filesystem::path& path, const TextureRequirement requirement) {
    Texture2D texture = LoadTexture(path.string().c_str());
    if (IsTextureValid(texture)) {
        return texture;
    }
    if (requirement == TextureRequirement::required) {
        throw std::runtime_error("Unable to load texture: " + path.string());
    }
    TraceLog(
        LOG_WARNING,
        "JumpCastle: optional texture missing, drawing placeholder for %s",
        path.string().c_str());
    Image placeholder = GenImageChecked(16, 16, 8, 8, MAGENTA, BLACK);
    texture = LoadTextureFromImage(placeholder);
    UnloadImage(placeholder);
    return texture;
}

}  // namespace

TextureResource::TextureResource(
    const std::filesystem::path& path, const TextureRequirement requirement)
    : texture_{load_texture_or_placeholder(path, requirement)} {
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
    draw_parallax_background(biome_texture, assets, camera.biome, camera.world_top);
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
    const PresentationLayout layout = fit_presentation(
        static_cast<int>(window_width), static_cast<int>(window_height));
    const ::Vector2 size{
        layout.width,
        layout.height,
    };
    const ::Vector2 offset{
        layout.offset_x,
        layout.offset_y,
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
        draw_debug_overlay(
            world.screen_count(), camera, player, campaign, layout.scale, offset);
    }

    EndDrawing();
}

Image Renderer::capture_screen(
    const WorldMap& world,
    const CameraBand& camera,
    const PlayerState& player) const {
    const Biome biome = asset_biome(camera.biome);
    const BiomeAssets& assets = catalog_.biome(biome);
    const Texture2D& biome_texture = castle_texture_.get();

    BeginTextureMode(pixelart_target_.get());
    draw_parallax_background(biome_texture, assets, camera.biome, camera.world_top);
    draw_world(world, camera, assets, biome_texture);
    draw_player(player, camera.world_top, 0.0F, catalog_, player_texture_.get());
    EndTextureMode();

    // Render textures are stored bottom-up; flip so the PNG is upright.
    Image image = LoadImageFromTexture(pixelart_target_.get().texture);
    ImageFlipVertical(&image);
    return image;
}

void Renderer::draw(
    const CampaignWorld& world,
    const CameraBand& camera,
    const PlayerState& player,
    const CampaignState& campaign,
    const float respawn_animation_time,
    const bool debug_enabled) const {
    const Biome biome = asset_biome(camera.biome);
    const BiomeAssets& assets = catalog_.biome(biome);
    const Texture2D& biome_texture = castle_texture_.get();

    BeginTextureMode(pixelart_target_.get());
    draw_parallax_background(biome_texture, assets, camera.biome, camera.world_top);
    draw_world_polygons(world, camera, assets, biome_texture);
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
    const PresentationLayout layout = fit_presentation(
        static_cast<int>(window_width), static_cast<int>(window_height));

    const Texture2D& target_texture = pixelart_target_.get().texture;
    DrawTexturePro(
        target_texture,
        {0.0F, 0.0F, static_cast<float>(target_texture.width),
         -static_cast<float>(target_texture.height)},
        {layout.offset_x, layout.offset_y, layout.width, layout.height},
        {},
        0.0F,
        WHITE);

    if (debug_enabled) {
        draw_debug_overlay(
            world.screen_count(), camera, player, campaign, layout.scale,
            {layout.offset_x, layout.offset_y});
    }

    EndDrawing();
}

Image Renderer::capture_screen(
    const CampaignWorld& world,
    const CameraBand& camera,
    const PlayerState& player) const {
    const Biome biome = asset_biome(camera.biome);
    const BiomeAssets& assets = catalog_.biome(biome);
    const Texture2D& biome_texture = castle_texture_.get();

    BeginTextureMode(pixelart_target_.get());
    draw_parallax_background(biome_texture, assets, camera.biome, camera.world_top);
    draw_world_polygons(world, camera, assets, biome_texture);
    draw_player(player, camera.world_top, 0.0F, catalog_, player_texture_.get());
    EndTextureMode();

    Image image = LoadImageFromTexture(pixelart_target_.get().texture);
    ImageFlipVertical(&image);
    return image;
}

void Renderer::draw_editor(
    const EditorState& editor,
    const CameraBand& camera,
    const int screen_height,
    const bool snap_enabled,
    const ColliderType current_type) const {
    const Biome biome = asset_biome(camera.biome);
    const BiomeAssets& assets = catalog_.biome(biome);
    const Texture2D& biome_texture = castle_texture_.get();
    const CampaignWorld authored =
        CampaignWorld::from_screens({editor.to_screen_map()}, screen_height);

    BeginTextureMode(pixelart_target_.get());
    draw_parallax_background(biome_texture, assets, camera.biome, camera.world_top);
    draw_world_polygons(authored, camera, assets, biome_texture);

    const Color grid_color = Fade(RAYWHITE, 0.08F);
    for (int x = 0; x <= config::view_width; x += config::tile_pixels) {
        DrawLine(x, 0, x, config::view_height, grid_color);
    }
    for (int y = 0; y <= config::view_height; y += config::tile_pixels) {
        DrawLine(0, y, config::view_width, y, grid_color);
    }

    const std::vector<Vec2>& draft = editor.draft_points();
    for (std::size_t i = 0; i < draft.size(); ++i) {
        const ::Vector2 point = world_to_screen(draft[i], camera.world_top);
        if (i + 1 < draft.size()) {
            DrawLineV(point, world_to_screen(draft[i + 1], camera.world_top), YELLOW);
        }
        DrawCircleV(point, 2.0F, YELLOW);
    }
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    const PresentationLayout layout =
        fit_presentation(GetScreenWidth(), GetScreenHeight());
    const Texture2D& target_texture = pixelart_target_.get().texture;
    DrawTexturePro(
        target_texture,
        {0.0F, 0.0F, static_cast<float>(target_texture.width),
         -static_cast<float>(target_texture.height)},
        {layout.offset_x, layout.offset_y, layout.width, layout.height},
        {},
        0.0F,
        WHITE);

    const char* type_name = current_type == ColliderType::solid ? "SOLID"
        : current_type == ColliderType::oneway ? "ONEWAY"
        : "HAZARD";
    DrawRectangle(4, 4, config::view_width - 8, 26, Fade(BLACK, 0.75F));
    DrawText(
        TextFormat("EDITOR  type:%s  snap:%s  polys:%d", type_name,
                   snap_enabled ? "on" : "off",
                   static_cast<int>(editor.polygon_count())),
        10, 8, 10, RAYWHITE);
    DrawText(
        "[LMB]add [Enter]close [Esc]cancel [T]type [G]snap [RMB+Del]erase "
        "[1/2/3]spawn/cp/goal [Ctrl+S]save [F1]exit",
        10, 20, 8, LIGHTGRAY);
    EndDrawing();
}

}  // namespace jumpcastle
