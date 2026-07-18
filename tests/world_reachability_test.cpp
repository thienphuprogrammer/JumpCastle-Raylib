#include "jumpcastle/solver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace jumpcastle;

TEST_CASE("committed campaign is reachable as one continuous route") {
    const auto screens = std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets/levels/screens";
    const CampaignWorld world = CampaignWorld::load(screens);
    const SolverResult result = ReachabilitySolver{world}.solve_campaign();

    INFO(result.failure);
    REQUIRE(result.reachable);
    CHECK(result.tolerance_passed);
    CHECK(result.highest_screen == 17);
    CHECK(result.maximum_charge_ratio <= SolverConfig{}.max_charge_ratio);
}
