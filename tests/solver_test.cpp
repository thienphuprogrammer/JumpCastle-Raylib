#include "jumpcastle/solver.hpp"

#include "test_level_factory.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

namespace {

LevelRepository reachable_level() {
    std::vector<test::TestCell> cells;
    for (int x = 3; x <= 7; ++x) cells.push_back({0, x, 9, Tile::solid});
    for (int x = 8; x <= 13; ++x) cells.push_back({0, x, 7, Tile::solid});
    return test::make_level_with_cells(cells);
}

LevelRepository unreachable_level() {
    std::vector<test::TestCell> cells;
    for (int x = 10; x <= 12; ++x) cells.push_back({0, x, 1, Tile::solid});
    return test::make_level_with_cells(cells);
}

}  // namespace

TEST_CASE("solver finds a tolerant route across simple room") {
    const SolverResult result = ReachabilitySolver{reachable_level()}.solve_room(0);

    INFO(result.failure);
    REQUIRE(result.reachable);
    CHECK_FALSE(result.jumps.empty());
    CHECK(result.maximum_charge_ratio <= 0.85F);
    CHECK(result.tolerance_passed);
}

TEST_CASE("solver rejects ledge beyond jump envelope") {
    const SolverResult result = ReachabilitySolver{unreachable_level()}.solve_room(0);

    CHECK_FALSE(result.reachable);
    CHECK(result.failure.find("nearest surface") != std::string::npos);
}
