#pragma once

#include "jumpcastle/simulation.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace jumpcastle {

enum class JumpDirection {
    left = -1,
    neutral = 0,
    right = 1,
};

struct SolverConfig {
    int simulation_hz{60};
    std::array<int, 9> charge_frames{8, 12, 16, 20, 24, 28, 32, 36, 39};
    int maximum_air_frames{180};
    float launch_sample_spacing{0.25F};
    float state_quantization{0.1F};
};

struct SolverJump {
    Vec2 start{};
    Vec2 landing{};
    JumpDirection direction{JumpDirection::neutral};
    int charge_frames{};
    std::size_t start_room{};
    std::size_t landing_room{};
};

struct SolverResult {
    bool reachable{};
    bool tolerance_passed{};
    float maximum_charge_ratio{};
    std::vector<SolverJump> jumps;
    std::string failure;
};

class ReachabilitySolver {
public:
    explicit ReachabilitySolver(
        const LevelRepository& level,
        SolverConfig config = {});

    [[nodiscard]] SolverResult solve_room(std::size_t room_index) const;
    [[nodiscard]] SolverResult solve_campaign() const;

private:
    const LevelRepository& level_;
    SolverConfig config_;
};

[[nodiscard]] std::string_view to_string(JumpDirection direction) noexcept;

}  // namespace jumpcastle
