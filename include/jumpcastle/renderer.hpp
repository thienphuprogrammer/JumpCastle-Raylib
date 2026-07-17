#pragma once

#include "jumpcastle/player.hpp"

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
        const Tilemap& tilemap,
        float screen_offset_y,
        const PlayerState& player,
        bool debug_enabled,
        std::size_t screen_index) const;

private:
    TextureResource player_texture_;
    TextureResource tilemap_texture_;
    RenderTargetResource pixelart_target_;
};

}  // namespace jumpcastle
