#include "jumpcastle/solver.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {

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
    std::optional<std::size_t> room;
    bool campaign = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--levels" && index + 1 < argc) {
            levels = argv[++index];
        } else if (argument == "--room" && index + 1 < argc) {
            room = static_cast<std::size_t>(std::stoul(argv[++index]));
            if (*room > 0) --*room;
        } else if (argument == "--campaign") {
            campaign = true;
        } else {
            std::cerr << "usage: jumpcastle_level_solver --levels DIR (--room N|--campaign)\n";
            return 2;
        }
    }

    if (levels.empty() || (campaign == room.has_value())) {
        std::cerr << "usage: jumpcastle_level_solver --levels DIR (--room N|--campaign)\n";
        return 2;
    }

    try {
        const jumpcastle::LevelRepository level =
            jumpcastle::LevelRepository::load(levels);
        const jumpcastle::ReachabilitySolver solver{level};
        const auto result = campaign ? solver.solve_campaign() : solver.solve_room(*room);
        print_result(result);
        return result.reachable ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
