#include "jumpcastle/game.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

// CLI `--asset-root PATH` / `--asset-root=PATH` overrides the asset search;
// otherwise the JUMPCASTLE_ASSET_ROOT environment variable is honored.
std::optional<std::filesystem::path> resolve_override(const int argc, char** argv) {
    constexpr std::string_view flag{"--asset-root"};
    constexpr std::string_view assignment{"--asset-root="};
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == flag && index + 1 < argc) {
            return std::filesystem::path{argv[index + 1]};
        }
        if (argument.substr(0, assignment.size()) == assignment) {
            return std::filesystem::path{argument.substr(assignment.size())};
        }
    }
    if (const char* env = std::getenv("JUMPCASTLE_ASSET_ROOT");
        env != nullptr && env[0] != '\0') {
        return std::filesystem::path{env};
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        jumpcastle::Game game{resolve_override(argc, argv)};
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "JumpCastle: fatal error: " << error.what() << '\n';
        return 1;
    }
}
