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
#include <fstream>
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
        const std::filesystem::path screens_directory =
            asset_directory / "levels" / "screens";
        const CampaignWorld world = std::filesystem::is_directory(screens_directory)
            ? CampaignWorld::load(screens_directory)
            : CampaignWorld::from_world_map(
                  WorldMap::load(asset_directory / "levels" / "campaign.level"));
        const Renderer renderer{asset_directory};
        std::filesystem::create_directories(output_directory);

        const int screen_height = world.screen_height;
        const int screen_count = world.screen_count();
        // One screen per biome band, spread across the tower (top-down indices).
        for (const int requested_band : {1, screen_count / 2, screen_count - 2}) {
            int band = requested_band;
            if (band < 0) band = 0;
            if (band > screen_count - 1) band = screen_count - 1;
            const float stand_y = static_cast<float>(band * screen_height) +
                static_cast<float>(screen_height) * 0.5F;
            PlayerState player{};
            player.position = {world.spawn.x, stand_y};
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

// `--export-polygons DIR` writes the campaign as per-screen screen-NN.map.json
// polygon files (the polygon-native form of campaign.level). No window needed.
std::optional<std::filesystem::path> export_output(const int argc, char** argv) {
    constexpr std::string_view flag{"--export-polygons"};
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == flag && index + 1 < argc) {
            return std::filesystem::path{argv[index + 1]};
        }
    }
    return std::nullopt;
}

int export_polygons(
    std::optional<std::filesystem::path> override_root,
    const std::filesystem::path& output_directory) {
    using namespace jumpcastle;
    try {
        const std::filesystem::path executable_directory{GetApplicationDirectory()};
        const auto asset_directory = resolve_asset_root({
            .override_root = std::move(override_root),
            .executable_directory = executable_directory,
            .installed_root = executable_directory / ".." / "share" / "jumpcastle",
        });
        const WorldMap grid =
            WorldMap::load(asset_directory / "levels" / "campaign.level");
        const std::vector<ScreenMap> screens =
            CampaignWorld::screen_maps_from_world_map(grid);
        std::filesystem::create_directories(output_directory);
        for (const ScreenMap& screen : screens) {
            const std::string filename = "screen-" +
                (screen.index < 10 ? std::string{"0"} : std::string{}) +
                std::to_string(screen.index) + ".map.json";
            std::ofstream out{output_directory / filename};
            if (!out) {
                std::cerr << "JumpCastle: cannot write " << filename << '\n';
                return EXIT_FAILURE;
            }
            out << serialize_screen_map(screen);
        }
        std::cerr << "JumpCastle: exported " << screens.size()
                  << " polygon screens to " << output_directory << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "JumpCastle: export failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (const auto export_directory = export_output(argc, argv)) {
        return export_polygons(resolve_override(argc, argv), *export_directory);
    }
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
