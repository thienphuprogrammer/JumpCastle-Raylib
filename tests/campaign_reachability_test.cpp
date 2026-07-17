#include "jumpcastle/solver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace jumpcastle;

namespace {

std::filesystem::path levels_path() {
    return std::filesystem::path{JUMPCASTLE_SOURCE_DIR} / "assets" / "levels";
}

}  // namespace

TEST_CASE("all committed rooms satisfy campaign schema") {
    const LevelRepository level = LevelRepository::load(levels_path());

    CHECK(level.room(0).metadata.biome == Biome::pixel_adventure);
    CHECK(level.room(4).metadata.biome == Biome::kenney);
    CHECK(level.room(8).metadata.biome == Biome::kings_and_pigs);
}

TEST_CASE("every committed room has tolerant route") {
    const LevelRepository level = LevelRepository::load(levels_path());
    const ReachabilitySolver solver{level};

    for (std::size_t room = 0; room < LevelRepository::room_count; ++room) {
        INFO("room " << room + 1);
        const SolverResult result = solver.solve_room(room);
        INFO(result.failure);
        REQUIRE(result.reachable);
        CHECK(result.tolerance_passed);
    }
}

TEST_CASE("full committed campaign is reachable") {
    const LevelRepository level = LevelRepository::load(levels_path());
    const SolverResult result = ReachabilitySolver{level}.solve_campaign();

    INFO(result.failure);
    REQUIRE(result.reachable);
    CHECK(result.tolerance_passed);
}
