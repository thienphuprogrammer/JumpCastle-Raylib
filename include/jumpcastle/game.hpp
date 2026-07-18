#pragma once

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

    PlayerState player_{};
    CampaignState campaign_{};
    bool debug_enabled_{};
    bool jump_release_latched_{};
    float respawn_animation_time_{};
    FixedStepClock fixed_clock_;
    std::optional<CampaignWorld> world_;
    std::optional<CameraBand> camera_;
    std::optional<Renderer> renderer_;
};

}  // namespace jumpcastle
