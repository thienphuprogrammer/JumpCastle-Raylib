#pragma once

#include "jumpcastle/assets.hpp"
#include "jumpcastle/camera.hpp"
#include "jumpcastle/campaign.hpp"

#include "raylib.h"

#include <array>
#include <filesystem>

namespace jumpcastle {

// Required textures abort startup when missing; optional decorations fall back
// to a visible magenta/black placeholder so the failure is obvious in-game.
enum class TextureRequirement { required, optional };

class TextureResource {
public:
    explicit TextureResource(
        const std::filesystem::path& path,
        TextureRequirement requirement = TextureRequirement::required);
    ~TextureResource();

    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    TextureResource(TextureResource&& other) noexcept;
    TextureResource& operator=(TextureResource&& other) noexcept;

    [[nodiscard]] const Texture2D& get() const noexcept;

private:
    Texture2D texture_{};
};

class RenderTargetResource {
public:
    RenderTargetResource(int width, int height);
    ~RenderTargetResource();

    RenderTargetResource(const RenderTargetResource&) = delete;
    RenderTargetResource& operator=(const RenderTargetResource&) = delete;
    RenderTargetResource(RenderTargetResource&& other) noexcept;
    RenderTargetResource& operator=(RenderTargetResource&& other) noexcept;

    [[nodiscard]] const RenderTexture2D& get() const noexcept;

private:
    RenderTexture2D target_{};
};

class Renderer {
public:
    explicit Renderer(const std::filesystem::path& asset_directory);

    void draw(
        const WorldMap& world,
        const CameraBand& camera,
        const PlayerState& player,
        const CampaignState& campaign,
        float respawn_animation_time,
        bool debug_enabled) const;

    // Polygon-world overloads (tinted polygon terrain). Kept alongside the grid
    // versions until the ASCII path is removed.
    void draw(
        const CampaignWorld& world,
        const CameraBand& camera,
        const PlayerState& player,
        const CampaignState& campaign,
        float respawn_animation_time,
        bool debug_enabled) const;

    // Render one biome screen into the offscreen pixel-art target and return it
    // as a CPU Image (exactly view_width by view_height). Used by headless
    // smoke capture so results never depend on window presentation.
    [[nodiscard]] Image capture_screen(
        const WorldMap& world,
        const CameraBand& camera,
        const PlayerState& player) const;

    [[nodiscard]] Image capture_screen(
        const CampaignWorld& world,
        const CameraBand& camera,
        const PlayerState& player) const;

private:
    AssetCatalog catalog_;
    TextureResource player_texture_;
    TextureResource castle_texture_;
    RenderTargetResource pixelart_target_;
};

}  // namespace jumpcastle
