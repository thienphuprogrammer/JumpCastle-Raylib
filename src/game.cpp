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

    active_screen_ = select_screen(player_.position.y);
    const Tilemap& tilemap = *active_screen_.tilemap;
    update_player(
        player_,
        tilemap,
        active_screen_.vertical_offset,
        sample_input(),
        delta);
    resolve_tilemap_collision(
        tilemap,
        active_screen_.vertical_offset,
        player_.position,
        player_.velocity,
        config::player_half_size);

    const int width = std::max(GetScreenWidth(), config::view_width);
    const int height = std::max(GetScreenHeight(), config::view_height);
    if (width != GetScreenWidth() || height != GetScreenHeight()) {
        SetWindowSize(width, height);
    }

    if (debug_enabled_) {
        if (IsKeyPressed(KEY_PAGE_UP)) {
            player_.position.y -= static_cast<float>(config::tilemap_height);
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            player_.position.y += static_cast<float>(config::tilemap_height);
        }
    }
}

void Game::run() {
    while (!WindowShouldClose()) {
        const float delta = std::clamp(GetFrameTime(), 0.0001F, 0.1F);
        update(delta);

        renderer_->draw(
            *active_screen_.tilemap,
            active_screen_.vertical_offset,
            player_,
            debug_enabled_,
            active_screen_.index);
    }
}

}  // namespace jumpcastle
