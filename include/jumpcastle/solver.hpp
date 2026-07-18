#pragma once

#include "jumpcastle/simulation.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace jumpcastle {

enum class JumpDirection {
    left = -1,
    neutral = 0,
    right = 1,
};

struct SolverConfig {
    std::array<int, 14> charge_ticks{
        15, 21, 27, 33, 39, 45, 51, 57, 63, 69, 75, 81, 87, 93};
    int maximum_air_ticks{480};
    float launch_sample_spacing{0.25F};
    float state_quantization{0.10F};
};

struct SolverJump {
    Vec2 start{};
    Vec2 landing{};
    JumpDirection direction{JumpDirection::neutral};
    int charge_ticks{};
    int start_screen{};
    int landing_screen{};
};

struct SolverResult {
    bool reachable{};
    bool tolerance_passed{};
    float maximum_charge_ratio{};
    int highest_screen{};
    std::vector<SolverJump> jumps;
    std::string failure;
};

class ReachabilitySolver {
public:
    explicit ReachabilitySolver(
        const WorldMap& world,
        SolverConfig config = {});

    [[nodiscard]] SolverResult solve_campaign() const;

private:
    const WorldMap& world_;
    SolverConfig config_;
};

[[nodiscard]] std::string_view to_string(JumpDirection direction) noexcept;

}  // namespace jumpcastle
