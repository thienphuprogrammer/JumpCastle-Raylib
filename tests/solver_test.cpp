#include "jumpcastle/solver.hpp"

#include "test_world_factory.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("solver finds a tolerant route across a continuous world") {
    const WorldMap world = test::reachable_three_screen_world();
    const SolverResult result = ReachabilitySolver{world}.solve_campaign();

    INFO(result.failure);
    REQUIRE(result.reachable);
    CHECK_FALSE(result.jumps.empty());
    CHECK(result.highest_screen == 2);
    CHECK(result.maximum_charge_ratio <= SolverConfig{}.max_charge_ratio);
    CHECK(result.tolerance_passed);
}

TEST_CASE("solver rejects a removed mandatory landing") {
    const WorldMap world = test::unreachable_three_screen_world();
    const SolverResult result = ReachabilitySolver{world}.solve_campaign();

    CHECK_FALSE(result.reachable);
    CHECK(result.failure.find("highest screen") != std::string::npos);
}
