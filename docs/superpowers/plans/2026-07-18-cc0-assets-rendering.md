# CC0 Assets and Continuous-World Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all prior runtime art with the approved CC0 castle, knight, and UI assets, render three cohesive biomes, and make missing-image failures reproducible, visible, and automatically tested.

**Architecture:** Keep raw download archives untracked, record immutable source metadata, and build committed runtime atlases from an explicit semantic selection manifest. A dedicated CMake `ALL` target synchronizes runtime data independently of executable relinking; a validated `AssetCatalog` and asset-root resolver feed the raylib renderer.

**Tech Stack:** C++20, CMake 3.24+, raylib 5.5, nlohmann/json 3.12.0, Python 3, Pillow 12.3.0, Catch2 3.8.1, GitHub Actions, CodeGraph CLI.

## Global Constraints

- Begin only after `2026-07-18-continuous-campaign-core.md` is complete and green.
- Use only the approved CC0 sources: rubberduck Pixel Art Castle Tileset, loveOS Gloomy Knight, and Kenney UI Pack - Pixel Adventure.
- Do not copy Jump King art, audio, text, map geometry, names, or branding.
- Project authorship is `Thien Phu (@thienphuprogrammer)`; retain accurate third-party source and CC0 metadata.
- Never choose a sprite by scanning for the first non-empty cell; every source rectangle must be named explicitly in a committed manifest.
- Reject zero-sized, out-of-bounds, fully transparent, or nearly transparent required regions.
- Generated atlases are committed; normal CMake builds never download assets and never require Pillow.
- `jumpcastle_assets` is an `ALL` target and must restore a deleted runtime asset even when `jumpcastle` is already up to date.
- Required manifest/atlas failures stop startup with searched paths; optional art uses a visible magenta checkerboard and a log message.
- Render at 512 by 288 logical pixels, default to 1536 by 864, use nearest filtering, integer scaling when at least 1x fits, and centered letterboxing.
- Use `rtk` for every shell command and CodeGraph before grep or direct file reads when locating code.

---

## File Map

### Source and Generated Data

- `assets/sources/castle-tileset/SOURCE.md`: OpenGameArt URL, author, CC0, retrieval date, filenames and SHA-256 hashes.
- `assets/sources/gloomy-knight/SOURCE.md`: itch.io URL, author, CC0, retrieval date, archive and SHA-256.
- `assets/sources/kenney-ui/SOURCE.md`: Kenney URL, author, CC0, retrieval date, archive and SHA-256.
- `assets/source-selection.json`: explicit source-file rectangles, palette roles, animation frames and UI regions.
- `assets/generated/castle.png`: normalized terrain, props and fallback region.
- `assets/generated/knight.png`: idle, walk, charge, rise, fall and reset frames.
- `assets/generated/ui.png`: menu/prompt/panel regions.
- `assets/generated/manifest.json`: runtime schema v2 with hashes, dimensions and semantic names.
- `THIRD_PARTY_ASSETS.md`: human-readable asset register.

### Tooling and Tests

- `tools/build_assets.py`: deterministic atlas compiler driven only by `source-selection.json`.
- `tools/verify_assets.py`: dependency-free structural/hash verifier.
- `tools/verify_asset_pixels.py`: Pillow alpha-coverage and smoke-composition verifier.
- `tests/asset_root_test.cpp`: asset-root search behavior.
- `tests/assets_test.cpp`: schema v2 catalog behavior.
- `tests/assets_manifest_test.cpp`: semantic region and diagnostic coverage.
- `tests/cmake/asset_recovery.cmake`: deletion/rebuild recovery test.
- `cmake/SyncAssets.cmake`: always-run, copy-if-different runtime data synchronization.

### Runtime Files

- `include/jumpcastle/asset_root.hpp`, `src/asset_root.cpp`: deterministic asset search.
- `include/jumpcastle/assets.hpp`, `src/assets.cpp`: schema v2 semantic catalog.
- `include/jumpcastle/renderer.hpp`, `src/renderer.cpp`: three-biome world rendering and smoke screenshots.
- `include/jumpcastle/game.hpp`, `src/game.cpp`, `src/main.cpp`: asset override and smoke-mode arguments.
- `CMakeLists.txt`: generated file list, `jumpcastle_assets`, install data and tests.
- `.github/workflows/ci.yml`: Pillow verification and Linux xvfb renderer smoke.
- `README.md`: screenshots, controls, authorship and asset documentation.

### Obsolete Data Removed

- `assets/generated/pixel-adventure.png`
- `assets/generated/kenney.png`
- `assets/generated/kings-and-pigs.png`
- old `assets/generated/player.png`
- `assets/sources/pixel-adventure/`
- `assets/sources/kenney-pixel-platformer/`
- `assets/sources/kings-and-pigs/`

---

### Task 1: Acquire and Record the Approved CC0 Sources

**Files:**
- Create: `assets/sources/castle-tileset/SOURCE.md`
- Create: `assets/sources/gloomy-knight/SOURCE.md`
- Create: `assets/sources/kenney-ui/SOURCE.md`
- Create: `THIRD_PARTY_ASSETS.md`
- Modify: `.gitignore`

**Interfaces:**
- Produces untracked archives/images under `assets/sources/downloads/` for the compiler.
- Produces source records with exact SHA-256 values consumed during review, not at runtime.

- [ ] **Step 1: Confirm the official pages and license declarations**

Open and record these exact pages:

- `https://opengameart.org/content/pixel-art-castle-tileset`
- `https://loveosstudio.itch.io/gloomy-knight-16x16`
- `https://kenney.nl/assets/ui-pack-pixel-adventure`

Expected: each page states CC0; the castle page names rubberduck, the knight page names loveOS,
and the UI page names Kenney.

- [ ] **Step 2: Download original files without tracking them**

Use the official page download controls. Save castle parts as
`assets/sources/downloads/castle_tileset_part1.png`, `part2.png`, and `part3.png`; save the other
downloads with their original archive filenames under the same directory.

Run: `rtk proxy shasum -a 256 assets/sources/downloads/*`

Expected: one digest per downloaded file. Copy the exact displayed digest into the matching
`SOURCE.md` using `apply_patch`.

- [ ] **Step 3: Add source records and third-party register**

Each source record must contain these exact fields with the real filename and 64-character digest
printed in Step 2:

```markdown
# Gloomy Knight Source

- Creator: loveOS by @cookiielove_
- Official page: https://loveosstudio.itch.io/gloomy-knight-16x16
- License: Creative Commons Zero 1.0 Universal
- Retrieved: 2026-07-18
- Original file: Gloomy Knight.zip
- SHA-256: the digest printed by `shasum -a 256` for `Gloomy Knight.zip`
- Derived runtime files: assets/generated/knight.png, assets/generated/manifest.json
```

Replace the descriptive SHA-256 sentence with the actual digest before commit. Validate it with
`rtk rg -n 'SHA-256: [0-9a-f]{64}$' assets/sources/*/SOURCE.md`, which must print exactly three
matching lines.

`THIRD_PARTY_ASSETS.md` lists all three creators, official pages, CC0 license, runtime role and
derived files. It also states that game/source authorship belongs to Thien Phu and third-party art
is not claimed as original work.

- [ ] **Step 4: Prove archives remain untracked**

Run: `rtk git status --short assets/sources && rtk git ls-files 'assets/sources/downloads/*'`

Expected: download files do not appear in `git ls-files`; only source metadata is staged later.

- [ ] **Step 5: Commit source policy**

```bash
rtk git add .gitignore THIRD_PARTY_ASSETS.md assets/sources/*/SOURCE.md
rtk git commit -m "docs(assets): Record approved CC0 sources"
```

---

### Task 2: Define an Explicit Semantic Source Selection

**Files:**
- Create: `assets/source-selection.json`
- Create: `tools/make_asset_contact_sheets.py`
- Create: `tests/test_asset_selection.py`
- Modify: `tools/requirements-assets.txt`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: downloaded source files from Task 1.
- Produces: schema 1 selection data with exact `{file, x, y, width, height}` per named sprite.
- Produces names: terrain variants `top_left`, `top`, `top_right`, `left`, `center`, `right`,
  `bottom_left`, `bottom`, `bottom_right`, `isolated`, `inner_corner_*`; props `banner`, `window`,
  `torch`, `door`, `crown`; player states `idle`, `walk`, `charge`, `rise`, `fall`, `reset`; UI
  regions `panel`, `button`, `button_pressed`, `keycap`.

- [ ] **Step 1: Write a failing schema test**

```python
# tests/test_asset_selection.py
import json
from pathlib import Path


def test_selection_has_explicit_rectangles_for_every_runtime_name():
    data = json.loads(Path("assets/source-selection.json").read_text())
    assert data["schema_version"] == 1
    assert set(data["biomes"]) == {"courtyard", "frosted_keep", "crown_spire"}
    assert set(data["player"]) == {"idle", "walk", "charge", "rise", "fall", "reset"}
    for group in [*data["biomes"].values(), data["props"], data["ui"]]:
        for region in group.values():
            assert set(region) == {"file", "x", "y", "width", "height"}
            assert region["width"] > 0 and region["height"] > 0
    for animation in data["player"].values():
        assert set(animation) == {"fps", "frames"}
        assert animation["frames"]
        for frame in animation["frames"]:
            assert {"file", "x", "y", "width", "height"} <= set(frame)
            assert set(frame) <= {"file", "x", "y", "width", "height", "derive"}
```

- [ ] **Step 2: Run the test to verify the selection file is absent**

Run: `rtk python -m pytest tests/test_asset_selection.py -q`

Expected: failure opening `assets/source-selection.json`.

- [ ] **Step 3: Generate numbered contact sheets for deliberate selection**

`tools/make_asset_contact_sheets.py` loads each source PNG or extracted archive PNG with Pillow,
draws a numbered grid without resampling, and writes contact sheets under
`assets/sources/downloads/contact-sheets/`. It never writes runtime files.

Run: `rtk python tools/make_asset_contact_sheets.py --downloads assets/sources/downloads`

Expected: contact sheets for all castle parts, knight sheets and Kenney UI; every cell shows source
pixel coordinates.

- [ ] **Step 4: Commit explicit regions, not scan heuristics**

Use the contact sheets to write exact rectangles. Courtyard, Frosted Keep and Crown Spire select
three different castle color rows but the same semantic terrain shapes. `charge` is an explicit
derived frame entry with `derive: "charge_squash"` and a named `source_region`; no function may
search for non-empty cells.

Extend the test to reject keys named `first_nonempty`, `scan`, or `auto_pick` anywhere in the JSON.

- [ ] **Step 5: Install Pillow/pytest in CI and run the selection test**

Add:

```yaml
- name: Install asset tooling
  run: python -m pip install -r tools/requirements-assets.txt pytest
```

Run: `rtk python -m pytest tests/test_asset_selection.py -q`

Expected: one passing test.

- [ ] **Step 6: Commit the selection contract**

```bash
rtk git add assets/source-selection.json tools/make_asset_contact_sheets.py \
  tools/requirements-assets.txt tests/test_asset_selection.py .github/workflows/ci.yml
rtk git commit -m "build(assets): Define semantic sprite selection"
```

---

### Task 3: Rebuild Atlases and Validate Their Pixels

**Files:**
- Rewrite: `tools/build_assets.py`
- Rewrite: `tools/verify_assets.py`
- Create: `tools/verify_asset_pixels.py`
- Create: `tests/test_asset_pixels.py`
- Replace: `assets/generated/manifest.json`
- Create: `assets/generated/castle.png`
- Create: `assets/generated/knight.png`
- Create: `assets/generated/ui.png`
- Delete: obsolete generated PNGs listed in the file map
- Modify: `tests/assets_manifest_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: selection schema 1 and untracked approved downloads.
- Produces: runtime manifest schema 2.
- Produces: alpha coverage of at least 8 percent for required 16 by 16 sprites and at least 3
  percent for intentionally thin props such as chains.

- [ ] **Step 1: Write failing verifier tests for transparent markers**

```python
import pytest
from PIL import Image


def test_required_region_rejects_three_pixel_strip(tmp_path):
    image = Image.new("RGBA", (16, 16))
    for y in range(3):
        for x in range(16):
            image.putpixel((x, y), (255, 255, 255, 255))
    assert alpha_coverage(image) == pytest.approx(3 / 16)
    with pytest.raises(ValueError, match="alpha coverage"):
        require_coverage(image, 0.20, "checkpoint")
```

- [ ] **Step 2: Run verifier tests to confirm the helper is missing**

Run: `rtk python -m pytest tests/test_asset_selection.py tests/test_asset_pixels.py -q`

Expected: missing `alpha_coverage` or `test_asset_pixels.py` failure.

- [ ] **Step 3: Implement deterministic atlas layout**

Use these stable layouts:

```text
castle.png: 3 biome rows × 16 semantic 16x16 cells
knight.png: 6 animation rows × N 16x16 frame cells
ui.png:     explicit packed regions with 1px transparent gutters
```

Build manifest schema 2 with this concrete Python structure so sizes and hashes always match the
files that were just written:

```python
def atlas_manifest(path: Path) -> dict[str, object]:
    with Image.open(path) as image:
        width, height = image.size
    return {
        "file": path.name,
        "width": width,
        "height": height,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


manifest = {
    "schema_version": 2,
    "license": "CC0-1.0",
    "sources": source_records,
    "atlases": {name: atlas_manifest(output / f"{name}.png")
                for name in ("castle", "knight", "ui")},
    "biomes": biome_records,
    "props": prop_records,
    "animations": animation_records,
    "ui": ui_records,
}
```

Every manifest region includes atlas name, rectangle, minimum alpha coverage and source provenance.

- [ ] **Step 4: Generate and verify twice for reproducibility**

Run:

```bash
rtk python tools/build_assets.py --downloads assets/sources/downloads \
  --selection assets/source-selection.json --output assets/generated \
  --manifest assets/generated/manifest.json
rtk python tools/verify_assets.py assets/generated/manifest.json
rtk python tools/verify_asset_pixels.py assets/generated/manifest.json
rtk proxy shasum -a 256 assets/generated/* > /tmp/jumpcastle-assets-first.sha
rtk python tools/build_assets.py --downloads assets/sources/downloads \
  --selection assets/source-selection.json --output assets/generated \
  --manifest assets/generated/manifest.json
rtk proxy shasum -a 256 -c /tmp/jumpcastle-assets-first.sha
```

Expected: both verifiers succeed and every checksum reports `OK`.

- [ ] **Step 5: Run C++ manifest tests**

Run: `rtk ctest --test-dir out/campaign-final -R "asset" --output-on-failure`

Expected: all schema v2 semantic region and animation tests pass.

- [ ] **Step 6: Commit generated runtime art**

```bash
rtk git add -A assets/generated tools/build_assets.py tools/verify_assets.py \
  tools/verify_asset_pixels.py tests CMakeLists.txt
rtk git commit -m "build(assets): Generate validated CC0 atlases"
```

---

### Task 4: Add Asset-Root Resolution and Reliable CMake Synchronization

**Files:**
- Create: `include/jumpcastle/asset_root.hpp`
- Create: `src/asset_root.cpp`
- Create: `tests/asset_root_test.cpp`
- Create: `tests/cmake/asset_recovery.cmake`
- Create: `cmake/SyncAssets.cmake`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `src/game.cpp`

**Interfaces:**
- Produces: `AssetSearchOptions` and `resolve_asset_root`.
- Produces CLI: `--asset-root PATH`; environment fallback: `JUMPCASTLE_ASSET_ROOT`.
- Produces CMake target: `jumpcastle_assets ALL`.

- [ ] **Step 1: Write failing root-order tests**

```cpp
#include <cstdint>
#include <fstream>

class TemporaryAssetTree {
public:
    TemporaryAssetTree()
        : root_{std::filesystem::temp_directory_path() /
                ("jumpcastle-assets-" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(this)))} {
        std::filesystem::create_directories(root_);
    }
    ~TemporaryAssetTree() { std::filesystem::remove_all(root_); }

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

TEST_CASE("failure lists every searched root") {
    CHECK_THROWS_WITH(
        resolve_asset_root({.executable_directory = "/missing/bin",
                            .installed_root = "/missing/share"}),
        Catch::Matchers::ContainsSubstring("/missing/bin/assets"));
}
```

- [ ] **Step 2: Run the focused test to verify missing API**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: missing `asset_root.hpp` error.

- [ ] **Step 3: Implement resolution contract**

```cpp
struct AssetSearchOptions {
    std::optional<std::filesystem::path> override_root;
    std::filesystem::path executable_directory;
    std::filesystem::path installed_root;
};

[[nodiscard]] std::filesystem::path resolve_asset_root(
    const AssetSearchOptions& options);
```

A valid root contains both `generated/manifest.json` and `levels/campaign.level`. Search override,
`executable_directory/assets`, then installed root; canonicalize only after existence checks.

- [ ] **Step 4: Replace `POST_BUILD` with an independent `ALL` target**

```cmake
set(JUMPCASTLE_RUNTIME_ASSETS
  generated/castle.png generated/knight.png generated/ui.png
  generated/manifest.json levels/campaign.level)

add_custom_target(jumpcastle_assets ALL
  COMMAND ${CMAKE_COMMAND}
          -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/assets
          -DDESTINATION_ROOT=$<TARGET_FILE_DIR:jumpcastle>/assets
          "-DFILES=${JUMPCASTLE_RUNTIME_ASSETS}"
          -P ${PROJECT_SOURCE_DIR}/cmake/SyncAssets.cmake
  VERBATIM)
add_dependencies(jumpcastle jumpcastle_assets)
```

```cmake
# cmake/SyncAssets.cmake
foreach(relative_path IN LISTS FILES)
  get_filename_component(relative_dir "${relative_path}" DIRECTORY)
  file(MAKE_DIRECTORY "${DESTINATION_ROOT}/${relative_dir}")
  file(COPY_FILE
       "${SOURCE_ROOT}/${relative_path}"
       "${DESTINATION_ROOT}/${relative_path}"
       ONLY_IF_DIFFERENT)
endforeach()
```

- [ ] **Step 5: Add and run deletion recovery**

`tests/cmake/asset_recovery.cmake` removes the build-tree `assets/generated/castle.png`, invokes
`cmake --build out/campaign-final --target jumpcastle_assets`, and fails unless the file is restored and its
SHA-256 matches the source copy.

Run: `rtk ctest --test-dir out/campaign-final -R asset_recovery --output-on-failure`

Expected: pass even when the `jumpcastle` executable timestamp does not change.

- [ ] **Step 6: Commit runtime asset delivery**

```bash
rtk git add CMakeLists.txt include/jumpcastle/asset_root.hpp src/asset_root.cpp \
  src/main.cpp src/game.cpp tests/asset_root_test.cpp tests/cmake/asset_recovery.cmake
rtk git commit -m "fix(assets): Restore runtime data on every build"
```

---

### Task 5: Parse Semantic Catalog v2 and Make Failures Visible

**Files:**
- Rewrite: `include/jumpcastle/assets.hpp`
- Rewrite: `src/assets.cpp`
- Modify: `include/jumpcastle/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `src/main.cpp`
- Modify: `src/game.cpp`
- Modify: `tests/assets_test.cpp`
- Modify: `tests/assets_manifest_test.cpp`
- Modify: `tests/resource_traits_test.cpp`

**Interfaces:**
- Consumes: manifest schema 2.
- Produces: `const BiomeArt& biome(Biome)`, `const AnimationClip& animation(PlayerAnimation)`,
  `const UiArt& ui()`, `const SpriteRegion& prop(std::string_view)`.
- Produces: `TextureResource(path, TextureRequirement)` where required failures throw and optional
  failures use the committed checkerboard region.

- [ ] **Step 1: Write failing schema and error tests**

```cpp
std::filesystem::path generated_manifest_path() {
    return std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets/generated/manifest.json";
}

TEST_CASE("catalog exposes semantic biome art") {
    const AssetCatalog catalog = AssetCatalog::load(generated_manifest_path());
    const BiomeArt& art = catalog.biome(Biome::courtyard);
    CHECK(art.terrain.at("top").width == 16);
    CHECK(art.standable_rim.width == 16);
    CHECK(catalog.animation(PlayerAnimation::charge).frames.size() >= 1);
}

TEST_CASE("catalog reports the missing manifest path") {
    CHECK_THROWS_WITH(
        AssetCatalog::load("missing-manifest.json"),
        Catch::Matchers::ContainsSubstring("missing-manifest.json"));
}
```

- [ ] **Step 2: Run focused tests to see schema 1 assumptions fail**

Run: `rtk ctest --test-dir out/campaign-final -R "assets|manifest|resource" --output-on-failure`

Expected: old exact atlas keys and King animation assertions fail.

- [ ] **Step 3: Implement catalog v2 types**

```cpp
struct ColorRgb {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};
enum class TextureRequirement { required, optional };
struct NamedRegion {
    std::string atlas;
    SpriteRegion rectangle;
    float minimum_alpha_coverage{};
};
struct BiomeArt {
    std::map<std::string, SpriteRegion, std::less<>> terrain;
    SpriteRegion standable_rim;
    ColorRgb background_top;
    ColorRgb background_bottom;
};
struct UiArt {
    SpriteRegion panel;
    SpriteRegion button;
    SpriteRegion button_pressed;
    SpriteRegion keycap;
};
```

Require exact top-level keys and exact animation states. Include the manifest path and semantic JSON
path in every exception. Keep raylib texture ownership in `jumpcastle_raylib`, not `AssetCatalog`.

- [ ] **Step 4: Add visible optional fallback behavior**

Generate a semantic `missing` region in `castle.png` as a magenta/black checkerboard. Renderer
methods that request optional decoration catch only the missing-decoration lookup, log the semantic
name and draw `missing`; required atlases/animations still abort startup.

For an optional texture path, `TextureResource(path, TextureRequirement::optional)` creates a
16 by 16 GPU checkerboard with `GenImageChecked` and logs the failed path when `LoadTexture` is
invalid. `TextureRequirement::required` throws with the path.

At the executable boundary, catch `std::exception`, print the full message to stderr, and, when a
raylib window is ready, draw a dark fatal-error screen containing `Asset loading failed` plus the
first readable line of the exception until Enter or Escape. Remove the constructor-level catch in
`Game` that closes the window before this boundary can render the diagnostic. If window creation
itself failed, return `EXIT_FAILURE` after stderr output.

- [ ] **Step 5: Run catalog and full tests**

Run: `rtk ctest --test-dir out/campaign-final --output-on-failure`

Expected: all tests pass; no assertion mentions Pixel Adventure, Kenney biome, Kings and Pigs, or
King Human.

- [ ] **Step 6: Commit catalog replacement**

```bash
rtk git add include/jumpcastle/assets.hpp include/jumpcastle/renderer.hpp \
  src/assets.cpp src/renderer.cpp src/main.cpp src/game.cpp tests
rtk git commit -m "feat(assets): Load semantic CC0 catalog"
```

---

### Task 6: Render Three Biomes, Knight Animations, UI, and Correct Scaling

**Files:**
- Modify: `include/jumpcastle/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `include/jumpcastle/player_view.hpp`
- Modify: `tests/player_view_test.cpp`
- Modify: `src/game.cpp`

**Interfaces:**
- Consumes: `WorldMap`, `CameraBand`, schema v2 catalog.
- Produces: `RenderLayout calculate_render_layout(int window_width, int window_height)`.
- Produces: biome-specific parallax, semantic autotiles, knight animations, completion/pause UI.

- [ ] **Step 1: Write failing layout and animation tests**

```cpp
TEST_CASE("small windows fit instead of cropping the logical viewport") {
    const RenderLayout layout = calculate_render_layout(400, 240);
    CHECK(layout.scale == Catch::Approx(0.78125F));
    CHECK(layout.destination.width == Catch::Approx(400.0F));
    CHECK(layout.destination.height == Catch::Approx(225.0F));
    CHECK(layout.destination.y == Catch::Approx(7.5F));
}

TEST_CASE("charging selects the explicit knight charge animation") {
    PlayerState player{.mode = PlayerMode::charging, .jump_hold_time = 0.4F};
    CHECK(select_animation(player) == PlayerAnimation::charge);
}
```

- [ ] **Step 2: Run focused tests to verify old scale clamping fails**

Run: `rtk ctest --test-dir out/campaign-final -R "player_view|render_layout" --output-on-failure`

Expected: layout helper is missing and/or old `max(1, floor(scale))` behavior fails.

- [ ] **Step 3: Implement render layout and pixel rules**

```cpp
RenderLayout calculate_render_layout(const int width, const int height) noexcept {
    const float fit = std::min(width / 512.0F, height / 288.0F);
    const float scale = fit >= 1.0F ? std::floor(fit) : fit;
    const float draw_width = 512.0F * scale;
    const float draw_height = 288.0F * scale;
    return {scale, {(width - draw_width) * 0.5F,
                    (height - draw_height) * 0.5F,
                    draw_width, draw_height}};
}
```

Use nearest filtering on every texture and render target. Round all world-to-logical destination
coordinates to integer pixels.

- [ ] **Step 4: Implement biome presentation without changing collision**

- Courtyard: warm-to-green vertical gradient, distant wall/tree silhouettes, moss palette terrain.
- Frosted Keep: blue gradient, window/fog silhouettes, cold palette terrain; no friction change.
- Crown Spire: purple-black gradient, moon/tower silhouettes, gold-highlight terrain.

Autotile from four cardinal and four diagonal `WorldMap::solid_at` neighbors into explicit semantic
regions. Draw the high-contrast `standable_rim` only when the tile above is empty. Decorations are
looked up by semantic name and never affect collision.

- [ ] **Step 5: Replace player and UI drawing**

Use `idle`, `walk`, `charge`, `rise`, `fall`, and `reset` clips from `knight.png`. Align every frame
by feet, not by source rectangle top-left. Use Kenney panel/button/keycap regions for pause,
completion and restart prompts; keep normal climbing HUD to screen number, timer and falls.

- [ ] **Step 6: Launch and inspect each biome**

Run: `rtk cmake --build out/campaign-final -j2 && rtk ./out/campaign-final/jumpcastle`

Use debug Page Up/Page Down only in debug mode to visit screens 1, 7 and 13. Expected: distinct
background/palette, visible knight in every state, no blurry filtering, no decorative fake landing,
and correct letterboxing after resize.

- [ ] **Step 7: Commit renderer replacement**

```bash
rtk git add include/jumpcastle/renderer.hpp include/jumpcastle/player_view.hpp \
  src/renderer.cpp src/game.cpp tests/player_view_test.cpp
rtk git commit -m "feat(rendering): Add three-biome castle art"
```

---

### Task 7: Add Deterministic Asset and Runtime Render Smoke Tests

**Files:**
- Create: `tools/render_asset_smoke.py`
- Create: `tests/test_render_asset_smoke.py`
- Modify: `src/main.cpp`
- Modify: `include/jumpcastle/game.hpp`
- Modify: `src/game.cpp`
- Modify: `include/jumpcastle/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces CLI: `jumpcastle --smoke-screens DIR --asset-root PATH`.
- Produces files: `courtyard.png`, `frosted-keep.png`, `crown-spire.png` at exactly 512 by 288.

- [ ] **Step 1: Write failing smoke-output tests**

```python
def test_three_smoke_images_are_nonempty(tmp_path):
    render_asset_smoke(MANIFEST, CAMPAIGN, tmp_path)
    for name in ("courtyard.png", "frosted-keep.png", "crown-spire.png"):
        image = Image.open(tmp_path / name).convert("RGBA")
        assert image.size == (512, 288)
        assert alpha_coverage(image) > 0.50
        assert len(image.getcolors(maxcolors=512 * 288)) > 24
```

- [ ] **Step 2: Run the Python test to verify smoke renderer is absent**

Run: `rtk python -m pytest tests/test_render_asset_smoke.py -q`

Expected: import/file-not-found failure for `render_asset_smoke`.

- [ ] **Step 3: Implement dependency-independent scene composition test**

`tools/render_asset_smoke.py` uses Pillow, manifest semantic regions and campaign collision to draw
representative screens 1, 7 and 13. It validates file dimensions, region alpha and color count and
writes only to its output argument.

- [ ] **Step 4: Implement actual raylib smoke mode**

`--smoke-screens DIR` creates a hidden window, renders camera screens 0, 6 and 12 through the real
`Renderer`, calls `TakeScreenshot` for the three filenames, validates they exist, and exits zero.
It never enters the interactive loop.

- [ ] **Step 5: Add CI coverage**

Install `xvfb` with Linux graphics dependencies. Add cross-platform Python smoke:

```yaml
- name: Verify asset pixels and smoke compositions
  run: |
    python tools/verify_asset_pixels.py assets/generated/manifest.json
    python tools/render_asset_smoke.py --manifest assets/generated/manifest.json \
      --campaign assets/levels/campaign.level --output build/asset-smoke
```

Add Linux real-render smoke:

```yaml
- name: Smoke test raylib renderer
  if: runner.os == 'Linux'
  run: xvfb-run -a build/cmake/jumpcastle --asset-root build/cmake/assets \
       --smoke-screens build/runtime-smoke
```

- [ ] **Step 6: Run both smoke paths locally**

Run:

```bash
rtk python -m pytest tests/test_render_asset_smoke.py -q
rtk python tools/render_asset_smoke.py --manifest assets/generated/manifest.json \
  --campaign assets/levels/campaign.level --output out/campaign-final/asset-smoke
rtk ./out/campaign-final/jumpcastle --asset-root out/campaign-final/assets \
  --smoke-screens out/campaign-final/runtime-smoke
```

Expected: six non-empty PNGs total and zero exit codes.

- [ ] **Step 7: Commit smoke coverage**

```bash
rtk git add tools/render_asset_smoke.py tests/test_render_asset_smoke.py \
  src/main.cpp src/game.cpp src/renderer.cpp include CMakeLists.txt .github/workflows/ci.yml
rtk git commit -m "test(rendering): Add three-biome smoke snapshots"
```

---

### Task 8: Remove Old Asset Sources and Update Product Documentation

**Files:**
- Delete: obsolete source metadata directories listed in the file map
- Modify: `README.md`
- Modify: `THIRD_PARTY_ASSETS.md`
- Modify: `docs/LEVEL_DESIGN.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: final asset pipeline and renderer.
- Produces: accurate user-facing controls, screenshots, credits, build/install and asset recovery docs.

- [ ] **Step 1: Prove old runtime identifiers have no callers**

Run: `rtk codegraph explore "References to pixel_adventure, kings_and_pigs, King Human, checkpoint sprites, and spike sprites"`

Expected: no production runtime references. Documentation scheduled for replacement may still appear.

- [ ] **Step 2: Remove obsolete source/generated records with `apply_patch`**

Delete only the old pack metadata and generated files listed in this plan. Keep
`assets/sources/CC0-1.0.txt`. Confirm `assets/sources/downloads/` remains ignored.

- [ ] **Step 3: Update README**

Document:

- title/author: `JumpCastle` by `Thien Phu (@thienphuprogrammer)`;
- 18-screen Jump King-style campaign and three biome names;
- 1536 by 864 default window and controls;
- configure/build/test/solver/replay commands;
- asset regeneration command and required local downloads;
- runtime asset search and `--asset-root` override;
- three generated smoke screenshots; and
- a link to `THIRD_PARTY_ASSETS.md` rather than claiming third-party art as original.

- [ ] **Step 4: Verify install layout**

Run:

```bash
rtk cmake --install out/campaign-final --prefix out/campaign-final/install
rtk ./out/campaign-final/install/bin/jumpcastle \
  --smoke-screens out/campaign-final/install-smoke
```

Expected: installed executable resolves installed `assets/generated` and `assets/levels` without an
override and writes three screenshots.

- [ ] **Step 5: Commit documentation and cleanup**

```bash
rtk git add -A README.md THIRD_PARTY_ASSETS.md docs CMakeLists.txt assets/sources assets/generated
rtk git commit -m "docs: Document final JumpCastle art pipeline"
```

---

### Task 9: Final Cross-Platform and Failure-Mode Verification

**Files:**
- Modify only if verification exposes a defect; keep fixes scoped to the failing subsystem and add
  a regression test in the same commit.

**Interfaces:**
- Consumes: all completed core and visual tasks.
- Produces: release-ready, indexed branch with evidence for build, solver, replay, assets and runtime.

- [ ] **Step 1: Configure and build from an empty directory**

Run:

```bash
rtk cmake -S . -B out/jumpcastle-final-clean -DCMAKE_BUILD_TYPE=Release \
  -DJUMPCASTLE_BUILD_TESTS=ON
rtk cmake --build out/jumpcastle-final-clean -j2
rtk ctest --test-dir out/jumpcastle-final-clean --output-on-failure
```

Expected: all C++, Python, manifest, asset recovery, solver and replay tests pass.

- [ ] **Step 2: Reproduce the original missing-image failure and prove recovery**

Run:

```bash
rtk proxy rm out/jumpcastle-final-clean/assets/generated/castle.png
rtk cmake --build out/jumpcastle-final-clean --target jumpcastle_assets
rtk proxy test -s out/jumpcastle-final-clean/assets/generated/castle.png
rtk python tools/verify_assets.py out/jumpcastle-final-clean/assets/generated/manifest.json
```

Expected: the deleted image returns without relinking `jumpcastle`; manifest verification succeeds.

- [ ] **Step 3: Prove campaign and visible runtime**

Run:

```bash
rtk ./out/jumpcastle-final-clean/jumpcastle_solver --level assets/levels/campaign.level \
  --campaign --trace out/jumpcastle-final-clean/final-route.json
rtk ./out/jumpcastle-final-clean/jumpcastle_solver --level assets/levels/campaign.level \
  --verify-trace out/jumpcastle-final-clean/final-route.json
rtk ./out/jumpcastle-final-clean/jumpcastle \
  --smoke-screens out/jumpcastle-final-clean/runtime-smoke
```

Expected: route reaches screen 18 and goal; replay completes; three runtime screenshots are
non-empty and visually distinct.

- [ ] **Step 4: Run manual gameplay checklist**

Verify default window, resize below 512 by 288, integer scaling above 1x, charge/release, no air
steering, wall/ceiling rebounds, camera transitions, multi-screen fall, bottom reset, pause,
completion, restart, and error output for an invalid `--asset-root`.

- [ ] **Step 5: Refresh and query CodeGraph**

Run:

```bash
rtk codegraph index
rtk codegraph explore "Trace asset root resolution through AssetCatalog into Renderer, and trace Game and solver into the shared physics step"
```

Expected: graph shows the new asset and shared-physics flows with no obsolete room or pack paths.

- [ ] **Step 6: Check repository hygiene**

Run:

```bash
rtk git diff --check
rtk git status --short
rtk git ls-files '*.zip' '*.aseprite'
```

Expected: no whitespace errors, no unexpected dirty production files, and no source archive or
Aseprite file tracked.

- [ ] **Step 7: Commit any verification-only regression fix separately**

If and only if a defect was fixed, stage only its implementation and regression test. Use
`fix(assets): Preserve verified runtime asset behavior` for delivery/catalog defects or
`fix(rendering): Preserve verified smoke behavior` for renderer defects. If all checks passed
without edits, do not create an empty commit.
