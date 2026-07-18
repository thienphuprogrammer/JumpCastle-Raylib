#include "jumpcastle/game.hpp"

#include "jumpcastle/asset_root.hpp"
#include "jumpcastle/camera.hpp"
#include "jumpcastle/game_config.hpp"
#include "jumpcastle/renderer.hpp"
#include "jumpcastle/simulation.hpp"
#include "jumpcastle/world.hpp"

#include "raylib.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

// CLI `--asset-root PATH` / `--asset-root=PATH` overrides the asset search;
// otherwise the JUMPCASTLE_ASSET_ROOT environment variable is honored.
std::optional<std::filesystem::path> resolve_override(const int argc, char** argv) {
    constexpr std::string_view flag{"--asset-root"};
    constexpr std::string_view assignment{"--asset-root="};
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == flag && index + 1 < argc) {
            return std::filesystem::path{argv[index + 1]};
        }
        if (argument.substr(0, assignment.size()) == assignment) {
            return std::filesystem::path{argument.substr(assignment.size())};
        }
    }
    if (const char* env = std::getenv("JUMPCASTLE_ASSET_ROOT");
        env != nullptr && env[0] != '\0') {
        return std::filesystem::path{env};
    }
    return std::nullopt;
}

// `--smoke-screens DIR` requests three headless biome screenshots into DIR.
std::optional<std::filesystem::path> smoke_output(const int argc, char** argv) {
    constexpr std::string_view flag{"--smoke-screens"};
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == flag && index + 1 < argc) {
            return std::filesystem::path{argv[index + 1]};
        }
    }
    return std::nullopt;
}

[[nodiscard]] const char* smoke_filename(const jumpcastle::WorldBiome biome) noexcept {
    switch (biome) {
        case jumpcastle::WorldBiome::courtyard: return "courtyard.png";
        case jumpcastle::WorldBiome::frosted_keep: return "frosted-keep.png";
        case jumpcastle::WorldBiome::crown_spire: return "crown-spire.png";
    }
    return "screen.png";
}

// Render one representative screen per biome through the real renderer into a
// hidden window and export each framebuffer as a PNG. Never enters the loop.
int render_smoke_screens(
    std::optional<std::filesystem::path> override_root,
    const std::filesystem::path& output_directory) {
    using namespace jumpcastle;

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(config::view_width, config::view_height, "JumpCastle smoke");
    if (!IsWindowReady()) {
        std::cerr << "JumpCastle: smoke render could not open a window\n";
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;
    try {
        const std::filesystem::path executable_directory{GetApplicationDirectory()};
        const auto asset_directory = resolve_asset_root({
            .override_root = std::move(override_root),
            .executable_directory = executable_directory,
            .installed_root = executable_directory / ".." / "share" / "jumpcastle",
        });
        const WorldMap world =
            WorldMap::load(asset_directory / "levels" / "campaign.level");
        const Renderer renderer{asset_directory};
        std::filesystem::create_directories(output_directory);

        const int screen_height = world.screen_height();
        const int world_height = world.height();
        // One screen inside each biome band (bottom to top).
        for (const int zero_based_screen : {2, 8, 14}) {
            const int first_row = world_height - (zero_based_screen + 1) * screen_height;
            // Stand the knight on the first platform top in the screen so the
            // capture shows grounded gameplay rather than a mid-air pose.
            float stand_x = world.spawn().x;
            float stand_y = static_cast<float>(first_row) +
                static_cast<float>(screen_height) * 0.5F;
            for (int row = first_row + 1; row < first_row + screen_height; ++row) {
                bool placed = false;
                for (int column = 0; column < world.width(); ++column) {
                    if (world.solid_at(column, row) && !world.solid_at(column, row - 1)) {
                        stand_x = static_cast<float>(column) + 0.5F;
                        stand_y = static_cast<float>(row) - config::player_half_size.y;
                        placed = true;
                        break;
                    }
                }
                if (placed) break;
            }
            PlayerState player{};
            player.position = {stand_x, stand_y};
            player.on_ground = true;
            const CameraBand camera = select_camera_band(world, stand_y);

            Image frame = renderer.capture_screen(world, camera, player);
            const std::filesystem::path destination =
                output_directory / smoke_filename(camera.biome);
            if (!ExportImage(frame, destination.string().c_str())) {
                std::cerr << "JumpCastle: failed to write " << destination << '\n';
                status = EXIT_FAILURE;
            }
            UnloadImage(frame);
        }
    } catch (const std::exception& error) {
        std::cerr << "JumpCastle: smoke render failed: " << error.what() << '\n';
        status = EXIT_FAILURE;
    }

    CloseWindow();
    return status;
}

// Show a dark diagnostic screen with the first readable line of the failure.
// Reuses an already-open window (the game constructor leaves it open on asset
// failure) or opens one; dismissed by Enter/Escape/close, and auto-dismisses
// after a bounded time so a headless run can never hang.
void show_fatal_error(const std::string& message) {
    if (!IsWindowReady()) {
        InitWindow(jumpcastle::config::view_width, jumpcastle::config::view_height,
                   "JumpCastle - asset error");
    }
    if (!IsWindowReady()) {
        return;  // No display available; stderr already carried the message.
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(30);
    const std::string first_line = message.substr(0, message.find('\n'));
    for (int frame = 0; frame < 300; ++frame) {
        if (WindowShouldClose() || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            break;
        }
        BeginDrawing();
        ClearBackground(Color{18, 12, 20, 255});
        DrawText("Asset loading failed", 20, 40, 20, RAYWHITE);
        DrawText(first_line.c_str(), 20, 78, 10, Color{232, 160, 174, 255});
        DrawText("Press Enter or Escape to quit", 20,
                 jumpcastle::config::view_height - 34, 10, GRAY);
        EndDrawing();
    }
    CloseWindow();
}

}  // namespace

int main(int argc, char** argv) {
    if (const auto smoke_directory = smoke_output(argc, argv)) {
        return render_smoke_screens(resolve_override(argc, argv), *smoke_directory);
    }
    try {
        jumpcastle::Game game{resolve_override(argc, argv)};
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "JumpCastle: fatal error: " << error.what() << '\n';
        show_fatal_error(error.what());
        return 1;
    }
}
