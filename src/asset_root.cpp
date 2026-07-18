#include "jumpcastle/asset_root.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace jumpcastle {
namespace {

[[nodiscard]] bool is_valid_asset_root(const std::filesystem::path& root) {
    std::error_code ignored;
    return std::filesystem::exists(root / "generated" / "manifest.json", ignored) &&
           std::filesystem::exists(root / "levels" / "campaign.level", ignored);
}

}  // namespace

std::filesystem::path resolve_asset_root(const AssetSearchOptions& options) {
    std::vector<std::filesystem::path> candidates;
    if (options.override_root.has_value()) {
        candidates.push_back(*options.override_root);
    }
    candidates.push_back(options.executable_directory / "assets");
    candidates.push_back(options.installed_root);

    for (const auto& candidate : candidates) {
        if (is_valid_asset_root(candidate)) {
            return candidate;
        }
    }

    std::string message = "unable to locate JumpCastle assets; searched:";
    for (const auto& candidate : candidates) {
        message += "\n  " + candidate.string();
    }
    throw std::runtime_error(message);
}

}  // namespace jumpcastle
