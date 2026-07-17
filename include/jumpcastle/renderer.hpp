#pragma once

#include "jumpcastle/assets.hpp"
#include "jumpcastle/campaign.hpp"

#include <array>
#include <filesystem>

namespace jumpcastle {

class TextureResource {
public:
    explicit TextureResource(const std::filesystem::path& path);
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
        const RoomSelection& room,
        const PlayerState& player,
        const CampaignState& campaign,
        float respawn_animation_time,
        bool debug_enabled) const;

private:
    AssetCatalog catalog_;
    TextureResource player_texture_;
    std::array<TextureResource, 3> biome_textures_;
    RenderTargetResource pixelart_target_;
};

}  // namespace jumpcastle
