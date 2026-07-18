#pragma once

#include "jumpcastle/editor.hpp"
#include "jumpcastle/renderer.hpp"
#include "jumpcastle/fixed_step.hpp"
#include "jumpcastle/simulation.hpp"

#include <filesystem>
#include <optional>

namespace jumpcastle {

class Game {
public:
    explicit Game(std::optional<std::filesystem::path> override_root = std::nullopt);
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void run();

private:
    [[nodiscard]] static PlayerInput sample_input() noexcept;
    void update_frame(float frame_delta);
    void update_editor();
    void save_editor_screen() const;

    PlayerState player_{};
    CampaignState campaign_{};
    bool debug_enabled_{};
    bool jump_release_latched_{};
    float respawn_animation_time_{};
    FixedStepClock fixed_clock_;
    std::optional<CampaignWorld> world_;
    std::optional<CameraBand> camera_;
    std::optional<Renderer> renderer_;

    // In-game polygon editor (F1). Authors the current screen and saves it to a
    // screen-NN.map.json; the running world is unaffected until reloaded.
    std::filesystem::path asset_directory_;
    bool editor_mode_{};
    bool editor_snap_{true};
    ColliderType editor_type_{ColliderType::solid};
    EditorState editor_{};
};

}  // namespace jumpcastle
