#include "jumpcastle/game.hpp"

#include "jumpcastle/asset_root.hpp"
#include "jumpcastle/presentation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
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
    world_.emplace(CampaignWorld::load(asset_directory / "levels" / "screens"));
    campaign_ = CampaignState{.spawn = world_->spawn};
    reset_player(player_, world_->spawn);
    camera_ = select_camera_band(*world_, player_.position.y);
    asset_directory_ = asset_directory;
    editor_ = EditorState{
        camera_->screen,
        static_cast<float>(config::tilemap_width),
        static_cast<float>(world_->screen_height)};
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
    if (IsKeyPressed(KEY_F1)) {
        editor_mode_ = !editor_mode_;
    }
    if (editor_mode_) {
        update_editor();
        return;
    }

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

void Game::update_editor() {
    const PresentationLayout layout =
        fit_presentation(GetScreenWidth(), GetScreenHeight());
    const ::Vector2 mouse = GetMousePosition();
    const float world_top = camera_ ? camera_->world_top : 0.0F;
    const Vec2 world = screen_to_world(mouse.x, mouse.y, layout, world_top);

    if (IsKeyPressed(KEY_G)) {
        editor_snap_ = !editor_snap_;
    }
    if (IsKeyPressed(KEY_T)) {
        editor_type_ = editor_type_ == ColliderType::solid ? ColliderType::oneway
            : editor_type_ == ColliderType::oneway ? ColliderType::hazard
            : ColliderType::solid;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!editor_.is_drafting()) {
            editor_.begin_polygon(editor_type_);
        }
        editor_.add_vertex(world, editor_snap_);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        editor_.close_polygon();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        editor_.cancel_polygon();
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        editor_.select_polygon(world);
    }
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        editor_.delete_selected_polygon();
    }
    if (IsKeyPressed(KEY_ONE)) {
        editor_.place_entity(EntityType::spawn, world, editor_snap_);
    }
    if (IsKeyPressed(KEY_TWO)) {
        editor_.place_entity(EntityType::checkpoint, world, editor_snap_);
    }
    if (IsKeyPressed(KEY_THREE)) {
        editor_.place_entity(EntityType::goal, world, editor_snap_);
    }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) &&
        IsKeyPressed(KEY_S)) {
        save_editor_screen();
    }
}

void Game::save_editor_screen() const {
    const ScreenMap screen = editor_.to_screen_map();
    const std::string filename = "screen-" +
        (screen.index < 10 ? std::string{"0"} : std::string{}) +
        std::to_string(screen.index) + ".map.json";
    const std::filesystem::path path = asset_directory_ / "levels" / filename;
    std::ofstream out{path};
    if (out) {
        out << serialize_screen_map(screen);
    }
}

void Game::run() {
    while (!WindowShouldClose()) {
        update_frame(GetFrameTime());
        if (editor_mode_) {
            renderer_->draw_editor(
                editor_, *camera_, world_->screen_height, editor_snap_, editor_type_);
        } else {
            renderer_->draw(
                *world_,
                *camera_,
                player_,
                campaign_,
                respawn_animation_time_,
                debug_enabled_);
        }
    }
}

}  // namespace jumpcastle
