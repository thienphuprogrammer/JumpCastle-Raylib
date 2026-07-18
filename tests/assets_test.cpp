#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

std::string generated_manifest() {
    const auto path = std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets" / "generated" / "manifest.json";
    std::ifstream input{path};
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}  // namespace

TEST_CASE("asset manifest identifies all approved CC0 sources") {
    const std::string manifest = generated_manifest();
    for (const std::string_view source : {
             "castle_tileset", "gloomy_knight", "kenney_ui"}) {
        INFO("source " << source);
        CHECK(manifest.find(source) != std::string::npos);
    }
}

TEST_CASE("asset manifest exposes every knight movement state") {
    const std::string manifest = generated_manifest();
    for (const std::string_view state : {
             "idle", "run", "charge", "rise", "fall", "respawn"}) {
        INFO("state " << state);
        CHECK(manifest.find('"' + std::string{state} + '"') != std::string::npos);
    }
}
