#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "jumpcastle/asset_root.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

using namespace jumpcastle;

namespace {

class TemporaryAssetTree {
public:
    TemporaryAssetTree()
        : root_{std::filesystem::temp_directory_path() /
                ("jumpcastle-assets-" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(this)))} {
        std::filesystem::create_directories(root_);
    }
    ~TemporaryAssetTree() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TemporaryAssetTree(const TemporaryAssetTree&) = delete;
    TemporaryAssetTree& operator=(const TemporaryAssetTree&) = delete;

    [[nodiscard]] std::filesystem::path make_valid(std::string_view relative) const {
        const auto root = root_ / relative;
        std::filesystem::create_directories(root / "generated");
        std::filesystem::create_directories(root / "levels");
        std::ofstream{root / "generated/manifest.json"} << "{}\n";
        std::ofstream{root / "levels/campaign.level"} << "version 2\n";
        return root;
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

private:
    std::filesystem::path root_;
};

}  // namespace

TEST_CASE("explicit asset root wins over executable and install roots") {
    const TemporaryAssetTree temp;
    const auto explicit_root = temp.make_valid("explicit");
    static_cast<void>(temp.make_valid("bin/assets"));
    const auto resolved = resolve_asset_root({
        .override_root = explicit_root,
        .executable_directory = temp.root() / "bin",
        .installed_root = temp.root() / "share/jumpcastle",
    });
    CHECK(resolved == explicit_root);
}

TEST_CASE("executable-adjacent assets win when no override is given") {
    const TemporaryAssetTree temp;
    const auto bin_assets = temp.make_valid("bin/assets");
    const auto resolved = resolve_asset_root({
        .executable_directory = temp.root() / "bin",
        .installed_root = temp.root() / "share/jumpcastle",
    });
    CHECK(resolved == bin_assets);
}

TEST_CASE("installed root is the final fallback") {
    const TemporaryAssetTree temp;
    const auto installed = temp.make_valid("share/jumpcastle");
    const auto resolved = resolve_asset_root({
        .executable_directory = temp.root() / "bin",
        .installed_root = installed,
    });
    CHECK(resolved == installed);
}

TEST_CASE("failure lists every searched root") {
    CHECK_THROWS_WITH(
        resolve_asset_root({.executable_directory = "/missing/bin",
                            .installed_root = "/missing/share"}),
        Catch::Matchers::ContainsSubstring("/missing/bin/assets"));
}
