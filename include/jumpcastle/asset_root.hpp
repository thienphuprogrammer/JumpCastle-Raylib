#pragma once

#include <filesystem>
#include <optional>

namespace jumpcastle {

// Where to look for the runtime asset tree, in priority order. A valid root
// contains both `generated/manifest.json` and `levels/campaign.level`.
struct AssetSearchOptions {
    std::optional<std::filesystem::path> override_root;
    std::filesystem::path executable_directory;
    std::filesystem::path installed_root;
};

// Returns the first valid root among: the explicit override, the
// executable-adjacent `assets/` directory, then the installed root. Throws
// std::runtime_error naming every searched location when none is valid.
[[nodiscard]] std::filesystem::path resolve_asset_root(const AssetSearchOptions& options);

}  // namespace jumpcastle
