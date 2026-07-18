#pragma once

#include "jumpcastle/game_config.hpp"
#include "jumpcastle/solver.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jumpcastle {

struct TraceJump {
    Vec2 launch_position{};
    JumpDirection direction{JumpDirection::neutral};
    int charge_ticks{};
    Vec2 expected_landing{};
    int expected_screen{};
};

struct SolverTrace {
    int schema_version{1};
    float fixed_delta{config::fixed_delta};
    std::vector<TraceJump> jumps;
};

struct ReplayResult {
    bool completed{};
    std::size_t executed_jumps{};
    std::string failure;
};

[[nodiscard]] SolverTrace make_trace(const SolverResult& result);
[[nodiscard]] std::string serialize_trace(const SolverTrace& trace);
[[nodiscard]] SolverTrace parse_trace(
    std::string_view source,
    std::string_view filename);
void write_trace(const std::filesystem::path& path, const SolverTrace& trace);
[[nodiscard]] SolverTrace read_trace(const std::filesystem::path& path);
[[nodiscard]] ReplayResult verify_trace(
    const CampaignWorld& world,
    const SolverTrace& trace);

}  // namespace jumpcastle
