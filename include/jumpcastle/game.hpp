#pragma once

#include "jumpcastle/renderer.hpp"

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
    bool debug_enabled_{};
    ScreenSelection active_screen_{select_screen(player_.position.y)};
    std::optional<Renderer> renderer_;
};

}  // namespace jumpcastle
