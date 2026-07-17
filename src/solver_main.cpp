#include "jumpcastle/solver.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

constexpr std::string_view usage =
    "usage: jumpcastle_level_solver --levels DIR (--room N|--campaign) [--trace JSON]\n";

[[nodiscard]] bool write_trace(
    const std::filesystem::path& path,
    const jumpcastle::SolverResult& result) {
    std::ofstream output{path};
    if (!output) {
        std::cerr << "unable to write trace: " << path << '\n';
        return false;
    }

    output << "{\n  \"reachable\": " << (result.reachable ? "true" : "false")
           << ",\n  \"tolerance_passed\": " << (result.tolerance_passed ? "true" : "false")
           << ",\n  \"maximum_charge_ratio\": " << result.maximum_charge_ratio
           << ",\n  \"jumps\": [";
    for (std::size_t index = 0; index < result.jumps.size(); ++index) {
        const auto& jump = result.jumps[index];
        output << (index == 0 ? "" : ",") << "\n    {\"start\": ["
               << jump.start.x << ", " << jump.start.y << "], \"landing\": ["
               << jump.landing.x << ", " << jump.landing.y << "], \"direction\": \""
               << jumpcastle::to_string(jump.direction) << "\", \"charge_frames\": "
               << jump.charge_frames << ", \"start_room\": " << jump.start_room + 1
               << ", \"landing_room\": " << jump.landing_room + 1 << "}";
    }
    output << (result.jumps.empty() ? "" : "\n  ") << "]\n}\n";
    return static_cast<bool>(output);
}

void print_result(const jumpcastle::SolverResult& result) {
    if (!result.reachable) {
        std::cerr << "unreachable: " << result.failure << '\n';
        return;
    }
    std::cout << "reachable: " << result.jumps.size() << " jumps, max charge "
              << result.maximum_charge_ratio * 100.0F << "%\n";
    for (const auto& jump : result.jumps) {
        std::cout << "  room " << jump.start_room + 1 << ": "
                  << jumpcastle::to_string(jump.direction) << ", "
                  << jump.charge_frames << " charge frames\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path levels;
    std::filesystem::path trace;
    std::optional<std::size_t> room;
    bool campaign = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--levels" && index + 1 < argc) {
            levels = argv[++index];
        } else if (argument == "--room" && index + 1 < argc) {
            room = static_cast<std::size_t>(std::stoul(argv[++index]));
            if (*room > 0) --*room;
        } else if (argument == "--trace" && index + 1 < argc) {
            trace = argv[++index];
        } else if (argument == "--campaign") {
            campaign = true;
        } else {
            std::cerr << usage;
            return 2;
        }
    }

    if (levels.empty() || (campaign == room.has_value())) {
        std::cerr << usage;
        return 2;
    }

    try {
        const jumpcastle::LevelRepository level =
            jumpcastle::LevelRepository::load(levels);
        const jumpcastle::ReachabilitySolver solver{level};
        const auto result = campaign ? solver.solve_campaign() : solver.solve_room(*room);
        print_result(result);
        if (!trace.empty() && !write_trace(trace, result)) {
            return 2;
        }
        return result.reachable ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
