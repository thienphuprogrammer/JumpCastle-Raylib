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
    // Denser charge sampling (step 3 ticks) models the player's continuous
    // charge control more faithfully, so tighter jumps a human can hit are no
    // longer rejected as unreachable.
    std::array<int, 29> charge_ticks{
        15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57,
        60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96, 99};
    int maximum_air_ticks{480};
    float launch_sample_spacing{0.15F};
    float state_quantization{0.10F};
    // Fraction of full charge the solver may certify. Below 1.0 leaves a human
    // safety cushion; raising it toward 1.0 permits brutally long/high jumps.
    // Fairness is still guaranteed by the tolerant_jump replay.
    float max_charge_ratio{0.98F};
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
        const CampaignWorld& world,
        SolverConfig config = {});

    [[nodiscard]] SolverResult solve_campaign() const;

private:
    const CampaignWorld& world_;
    SolverConfig config_;
};

[[nodiscard]] std::string_view to_string(JumpDirection direction) noexcept;

}  // namespace jumpcastle
