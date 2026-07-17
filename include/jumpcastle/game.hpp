#pragma once

#include "jumpcastle/renderer.hpp"
#include "jumpcastle/simulation.hpp"

#include <optional>

namespace jumpcastle {

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void run();

private:
    [[nodiscard]] static PlayerInput sample_input() noexcept;
    void update(float delta);

    PlayerState player_{};
    CampaignState campaign_{};
    bool debug_enabled_{};
    float respawn_animation_time_{};
    std::optional<LevelRepository> levels_;
    std::optional<RoomSelection> active_room_;
    std::optional<Renderer> renderer_;
};

}  // namespace jumpcastle
