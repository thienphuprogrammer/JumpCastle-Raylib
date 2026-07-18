#include "jumpcastle/game.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace jumpcastle {

Game::Game() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(
        config::view_width * 3,
        config::view_height * 3,
        "JumpCastle - Thien Phu (@thienphuprogrammer)");

    if (!IsWindowReady()) {
        throw std::runtime_error("Unable to initialize the game window");
    }

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    try {
        const auto asset_directory =
            std::filesystem::path{GetApplicationDirectory()} / "assets";
        levels_.emplace(LevelRepository::load(asset_directory / "levels"));
        const Vec2 spawn = levels_->campaign_spawn();
        restart_campaign(campaign_, spawn, 0);
        reset_player(player_, spawn);
        active_room_ = levels_->select(player_.position.y);
        if (!active_room_) {
            throw std::runtime_error("Campaign spawn is outside the room stack");
        }
        renderer_.emplace(asset_directory);
    } catch (...) {
        CloseWindow();
        throw;
    }
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

void Game::update(const float delta) {
    if (IsKeyPressed(KEY_I)) {
        debug_enabled_ = !debug_enabled_;
    }

    if (campaign_.complete) {
        if (IsKeyPressed(KEY_ENTER)) {
            const Vec2 spawn = levels_->campaign_spawn();
            restart_campaign(campaign_, spawn, 0);
            reset_player(player_, spawn);
            respawn_animation_time_ = 0.0F;
        }
    } else {
        const CampaignEvent event = simulate_step(
            player_, campaign_, *levels_, sample_input(), delta);
        if (event == CampaignEvent::respawned) {
            respawn_animation_time_ = 0.45F;
        }
    }

    respawn_animation_time_ = std::max(0.0F, respawn_animation_time_ - delta);

    if (debug_enabled_) {
        if (IsKeyPressed(KEY_PAGE_UP)) {
            player_.position.y -= static_cast<float>(config::tilemap_height);
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            player_.position.y += static_cast<float>(config::tilemap_height);
        }
    }

    active_room_ = levels_->select(player_.position.y);
    if (!active_room_) {
        respawn_player(campaign_, player_);
        respawn_animation_time_ = 0.45F;
        active_room_ = levels_->select(player_.position.y);
    }

    const int width = std::max(GetScreenWidth(), config::view_width);
    const int height = std::max(GetScreenHeight(), config::view_height);
    if (width != GetScreenWidth() || height != GetScreenHeight()) {
        SetWindowSize(width, height);
    }
}

void Game::run() {
    while (!WindowShouldClose()) {
        const float delta = std::clamp(GetFrameTime(), 0.0001F, 0.1F);
        update(delta);
        renderer_->draw(
            *active_room_,
            player_,
            campaign_,
            respawn_animation_time_,
            debug_enabled_);
    }
}

}  // namespace jumpcastle
