#include "jumpcastle/replay.hpp"

#include "test_world_factory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace jumpcastle;

TEST_CASE("solver trace replays through production physics") {
    const WorldMap world = test::reachable_three_screen_world();
    const SolverResult solved = ReachabilitySolver{world}.solve_campaign();
    REQUIRE(solved.reachable);

    const SolverTrace trace = make_trace(solved);
    const std::string encoded = serialize_trace(trace);
    const ReplayResult replay = verify_trace(
        world, parse_trace(encoded, "memory.json"));

    INFO(replay.failure);
    CHECK(replay.completed);
    CHECK(replay.executed_jumps == solved.jumps.size());
}

TEST_CASE("trace parser rejects a different fixed step") {
    CHECK_THROWS(parse_trace(
        R"({"schema_version":1,"fixed_delta":0.016,"jumps":[]})",
        "wrong-rate.json"));
}

TEST_CASE("committed narrow tower trace replays to the crown") {
    const auto root = std::filesystem::path{JUMPCASTLE_SOURCE_DIR};
    const WorldMap world = WorldMap::load(root / "assets/levels/campaign.level");
    const SolverTrace trace = read_trace(root / "assets/levels/campaign-route.json");
    const ReplayResult replay = verify_trace(world, trace);

    INFO(replay.failure);
    CHECK(replay.completed);
    CHECK(replay.executed_jumps >= 8U * 18U);
    CHECK(replay.executed_jumps <= 12U * 18U);
}
