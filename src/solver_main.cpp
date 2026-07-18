#include "jumpcastle/solver.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

constexpr std::string_view usage =
    "usage: jumpcastle_solver --level FILE --campaign\n";

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
    bool campaign{};

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--level" && index + 1 < argc) {
            level_path = argv[++index];
        } else if (argument == "--campaign") {
            campaign = true;
        } else {
            std::cerr << usage;
            return 2;
        }
    }

    if (level_path.empty() || !campaign) {
        std::cerr << usage;
        return 2;
    }

    try {
        const jumpcastle::WorldMap world = jumpcastle::WorldMap::load(level_path);
        const jumpcastle::SolverResult result =
            jumpcastle::ReachabilitySolver{world}.solve_campaign();
        print_result(result);
        return result.reachable ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
