#include "jumpcastle/replay.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

constexpr std::string_view usage =
    "usage: jumpcastle_solver --level FILE (--campaign [--trace FILE]|"
    "--verify-trace FILE)\n";

void print_result(const jumpcastle::SolverResult& result) {
    if (!result.reachable) {
        std::cerr << "unreachable: " << result.failure << '\n';
        return;
    }
    std::cout << "reachable: " << result.jumps.size() << " jumps, max charge "
              << result.maximum_charge_ratio * 100.0F << "%, highest screen "
              << result.highest_screen + 1 << '\n';
    for (const auto& jump : result.jumps) {
        std::cout << "  screen " << jump.start_screen + 1 << " -> "
                  << jump.landing_screen + 1 << ": "
                  << jumpcastle::to_string(jump.direction) << ", "
                  << jump.charge_ticks << " charge ticks\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path level_path;
    std::filesystem::path trace_path;
    std::filesystem::path verify_path;
    bool campaign{};

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--level" && index + 1 < argc) {
            level_path = argv[++index];
        } else if (argument == "--campaign") {
            campaign = true;
        } else if (argument == "--trace" && index + 1 < argc) {
            trace_path = argv[++index];
        } else if (argument == "--verify-trace" && index + 1 < argc) {
            verify_path = argv[++index];
        } else {
            std::cerr << usage;
            return 2;
        }
    }

    if (level_path.empty() || campaign == !verify_path.empty() ||
        (!trace_path.empty() && !campaign)) {
        std::cerr << usage;
        return 2;
    }

    try {
        const jumpcastle::CampaignWorld world =
            jumpcastle::CampaignWorld::load(level_path);
        if (!verify_path.empty()) {
            const jumpcastle::ReplayResult replay = jumpcastle::verify_trace(
                world, jumpcastle::read_trace(verify_path));
            if (!replay.completed) {
                std::cerr << "replay failed after " << replay.executed_jumps
                          << " jumps: " << replay.failure << '\n';
                return 1;
            }
            std::cout << "replay complete: goal reached in "
                      << replay.executed_jumps << " jumps\n";
            return 0;
        }
        const jumpcastle::SolverResult result =
            jumpcastle::ReachabilitySolver{world}.solve_campaign();
        print_result(result);
        if (result.reachable && !trace_path.empty()) {
            jumpcastle::write_trace(trace_path, jumpcastle::make_trace(result));
        }
        return result.reachable ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
