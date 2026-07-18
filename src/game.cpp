#include "jumpcastle/game.hpp"

#include "jumpcastle/asset_root.hpp"
#include "jumpcastle/presentation.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace jumpcastle {

Game::Game(std::optional<std::filesystem::path> override_root) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(
        config::view_width,
        config::view_height,
        "JumpCastle - Thien Phu (@thienphuprogrammer)");

    if (!IsWindowReady()) {
        throw std::runtime_error("Unable to initialize the game window");
    }

    const int monitor = GetCurrentMonitor();
    const int window_scale = preferred_window_scale(
        GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    SetWindowSize(
        config::view_width * window_scale,
        config::view_height * window_scale);

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // Asset failures propagate with the window still open so the executable
    // boundary can render an on-screen diagnostic instead of a silent exit.
    const std::filesystem::path executable_directory{GetApplicationDirectory()};
    const auto asset_directory = resolve_asset_root({
        .override_root = std::move(override_root),
        .executable_directory = executable_directory,
        .installed_root = executable_directory / ".." / "share" / "jumpcastle",
    });
    world_.emplace(CampaignWorld::from_world_map(
        WorldMap::load(asset_directory / "levels" / "campaign.level")));
    campaign_ = CampaignState{.spawn = world_->spawn};
    reset_player(player_, world_->spawn);
    camera_ = select_camera_band(*world_, player_.position.y);
    renderer_.emplace(asset_directory);
}

Game::~Game() {
    renderer_.reset();
    if (IsWindowReady()) {
        CloseWindow();
    }
}

PlayerInput Game::sample_input() noexcept {
    return {
        .left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A),
        .right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D),
        .left_pressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A),
        .right_pressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D),
        .jump_down = IsKeyDown(KEY_SPACE),
        .jump_released = IsKeyReleased(KEY_SPACE),
    };
}

void Game::update_frame(const float frame_delta) {
    if (IsKeyPressed(KEY_I)) {
        debug_enabled_ = !debug_enabled_;
    }

    PlayerInput frame_input = sample_input();
    jump_release_latched_ = jump_release_latched_ || frame_input.jump_released;

    if (campaign_.complete && IsKeyPressed(KEY_ENTER)) {
        campaign_ = CampaignState{.spawn = world_->spawn};
        reset_player(player_, world_->spawn);
        fixed_clock_.reset();
        jump_release_latched_ = false;
        respawn_animation_time_ = 0.0F;
    }

    if (debug_enabled_) {
        if (IsKeyPressed(KEY_PAGE_UP)) {
            player_.position.y -= static_cast<float>(world_->screen_height);
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            player_.position.y += static_cast<float>(world_->screen_height);
        }
    }

    const int ticks = fixed_clock_.consume(frame_delta);
    for (int tick = 0; tick < ticks && !campaign_.complete; ++tick) {
        PlayerInput tick_input = frame_input;
        tick_input.jump_released = jump_release_latched_;
        const CampaignEvent event = step_world(
            player_, campaign_, *world_, tick_input);
        if (jump_release_latched_) {
            jump_release_latched_ = false;
        }
        if (event == CampaignEvent::fell_below_world) {
            respawn_animation_time_ = 0.45F;
        }
    }

    respawn_animation_time_ = std::max(
        0.0F, respawn_animation_time_ - std::clamp(frame_delta, 0.0F, 0.1F));
    camera_ = select_camera_band(*world_, player_.position.y);

    const int width = std::max(GetScreenWidth(), config::view_width);
    const int height = std::max(GetScreenHeight(), config::view_height);
    if (width != GetScreenWidth() || height != GetScreenHeight()) {
        SetWindowSize(width, height);
    }
}

void Game::run() {
    while (!WindowShouldClose()) {
        update_frame(GetFrameTime());
        renderer_->draw(
            *world_,
            *camera_,
            player_,
            campaign_,
            respawn_animation_time_,
            debug_enabled_);
    }
}

}  // namespace jumpcastle
