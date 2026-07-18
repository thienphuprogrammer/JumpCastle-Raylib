# Narrow Vertical Tower Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 32 by 18 landscape campaign bands with an 18-band, 28 by 36 tile vertical challenge tower that remains readable, solver-reachable, and sharp at supported presentation scales.

**Architecture:** Keep physics and world coordinates tile-based and unchanged. Introduce a raylib-free presentation helper for portrait sizing, generate the runtime collision file from explicit hand-authored chamber coordinates, and continue using the production solver and replay path as the reachability gate. The runtime still reads only `assets/levels/campaign.level`; the Python chamber builder is an authoring and validation tool with no runtime dependency.

**Tech Stack:** C++20, raylib 5.5, CMake 3.24+, Catch2 3.8.1, Python 3 with pytest and Pillow, JSON solver traces.

## Global Constraints

- One camera band is exactly 28 tiles wide and 36 tiles high.
- Eighteen bands form a 28 by 648 tile campaign.
- Gameplay tiles remain 16 logical pixels; the render target is 448 by 576 pixels.
- Preserve the current 120 Hz production physics and charge-and-release controls.
- No checkpoint, spike teleport, airborne steering, procedural generation, or hidden collision rule.
- Mandatory landing surfaces are at least one tile wide and mandatory charges do not exceed 90 percent.
- Every accepted jump passes the solver's existing `-2/+2` charge-tick and `-0.1/+0.1` launch-position tolerance checks.
- Preserve unrelated dirty files, especially `assets/levels/room-01.level` and `.superpowers/brainstorm/`.
- Prefix repository shell commands with `rtk`; use `rtk proxy` for unsupported commands.

---

## File Structure

- Create `include/jumpcastle/presentation.hpp`: neutral presentation scale and letterbox values.
- Create `src/presentation.cpp`: pure monitor/window fitting calculations.
- Create `tests/presentation_test.cpp`: portrait viewport and scaling contract tests.
- Modify `include/jumpcastle/game_config.hpp`: approved 28 by 36 viewport constants.
- Modify `src/game.cpp`: choose the initial integer window scale from monitor dimensions.
- Modify `src/renderer.cpp`: consume `PresentationLayout` instead of duplicating scale math.
- Modify `CMakeLists.txt`: build and test the presentation module.
- Create `tools/narrow_campaign.py`: deterministic renderer for explicit chamber rectangles.
- Create `tests/test_narrow_campaign.py`: authoring-data structure and output validation.
- Modify `assets/levels/campaign.level`: generated 28 by 648 collision map.
- Modify `tests/world_test.cpp`: committed shape, biome, spawn, goal, and chamber silhouette contract.
- Modify `tests/world_reachability_test.cpp`: route, tolerance, screen, charge, and jump-count gates.
- Create `assets/levels/campaign-route.json`: accepted production solver trace.
- Modify `tests/replay_test.cpp`: replay the committed trace against the committed campaign.
- Modify `tests/test_render_asset_smoke.py`: render three portrait bands and verify visible content.
- Modify `docs/LEVEL_DESIGN.md`: document 28 by 36 chamber authoring and verification commands.

---

### Task 1: Portrait Presentation Contract

**Files:**
- Create: `include/jumpcastle/presentation.hpp`
- Create: `src/presentation.cpp`
- Create: `tests/presentation_test.cpp`
- Modify: `include/jumpcastle/game_config.hpp`
- Modify: `src/game.cpp`
- Modify: `src/renderer.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `config::view_width`, `config::view_height`, monitor and window pixel dimensions.
- Produces: `int preferred_window_scale(int, int) noexcept` and `PresentationLayout fit_presentation(int, int) noexcept`.

- [ ] **Step 1: Write the failing presentation tests**

```cpp
#include "jumpcastle/game_config.hpp"
#include "jumpcastle/presentation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("narrow tower viewport is 28 by 36 tiles") {
    CHECK(config::viewport_tiles_width == 28);
    CHECK(config::viewport_tiles_height == 36);
    CHECK(config::view_width == 448);
    CHECK(config::view_height == 576);
}

TEST_CASE("preferred window scale stays integral") {
    CHECK(preferred_window_scale(1920, 1440) == 2);
    CHECK(preferred_window_scale(1536, 864) == 1);
    CHECK(preferred_window_scale(400, 500) == 1);
}

TEST_CASE("portrait target is centred in a landscape window") {
    const PresentationLayout layout = fit_presentation(1536, 864);
    CHECK(layout.scale == Catch::Approx(1.5F));
    CHECK(layout.width == Catch::Approx(672.0F));
    CHECK(layout.height == Catch::Approx(864.0F));
    CHECK(layout.offset_x == Catch::Approx(432.0F));
    CHECK(layout.offset_y == Catch::Approx(0.0F));
}
```

- [ ] **Step 2: Register and run the tests to prove RED**

Add `src/presentation.cpp` to `jumpcastle_core` and `tests/presentation_test.cpp` to
`jumpcastle_tests`, then run:

```bash
rtk cmake --build out/campaign-final --target jumpcastle_tests --parallel
```

Expected: FAIL because `jumpcastle/presentation.hpp` and the two functions do not exist.

- [ ] **Step 3: Add the neutral presentation API**

```cpp
// include/jumpcastle/presentation.hpp
#pragma once

namespace jumpcastle {

struct PresentationLayout {
    float scale{};
    float width{};
    float height{};
    float offset_x{};
    float offset_y{};
};

[[nodiscard]] int preferred_window_scale(
    int available_width,
    int available_height) noexcept;
[[nodiscard]] PresentationLayout fit_presentation(
    int window_width,
    int window_height) noexcept;

}  // namespace jumpcastle
```

Implement `preferred_window_scale` as the largest positive integer not greater than 2 that fits the
logical target. Implement `fit_presentation` with `min(window_width / 448, window_height / 576)`;
use the integral floor when it is at least 2 and the fitted ratio otherwise. Centre the result.

- [ ] **Step 4: Apply the approved constants and wire the runtime**

```cpp
inline constexpr int viewport_tiles_width = 28;
inline constexpr int viewport_tiles_height = 36;
```

After the initial logical-size `InitWindow`, compute the preferred scale from
`GetMonitorWidth(GetCurrentMonitor())` and `GetMonitorHeight(GetCurrentMonitor())`, then call
`SetWindowSize(config::view_width * scale, config::view_height * scale)`. Replace renderer-local
scale/offset calculations with `fit_presentation(GetScreenWidth(), GetScreenHeight())`.

- [ ] **Step 5: Run focused and camera tests**

```bash
rtk cmake --build out/campaign-final --target jumpcastle jumpcastle_tests --parallel
rtk proxy ./out/campaign-final/jumpcastle_tests "*presentation*,*camera*"
```

Expected: all selected tests pass and the game target links.

- [ ] **Step 6: Commit the presentation slice**

```bash
rtk git add CMakeLists.txt include/jumpcastle/game_config.hpp \
  include/jumpcastle/presentation.hpp src/presentation.cpp src/game.cpp \
  src/renderer.cpp tests/presentation_test.cpp
rtk git commit -m "feat(render): Add portrait tower presentation"
```

---

### Task 2: Explicit Hand-Authored Tower Spine

**Files:**
- Create: `tools/narrow_campaign.py`
- Create: `tests/test_narrow_campaign.py`
- Modify: `assets/levels/campaign.level`
- Modify: `tests/world_test.cpp`
- Modify: `tests/world_reachability_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: explicit `Chamber` records containing solid rectangles in local 28 by 36 coordinates.
- Produces: `render_campaign(chambers: tuple[Chamber, ...]) -> str` and the committed runtime level.

- [ ] **Step 1: Write failing authoring and committed-shape tests**

```python
from tools.narrow_campaign import CHAMBERS, render_campaign


def test_tower_has_eighteen_distinct_authored_chambers():
    assert len(CHAMBERS) == 18
    assert len({chamber.name for chamber in CHAMBERS}) == 18
    assert len({chamber.solids for chamber in CHAMBERS}) == 18


def test_rendered_campaign_has_approved_dimensions():
    text = render_campaign(CHAMBERS)
    rows = text.split("[collision]\n", 1)[1].splitlines()
    assert len(rows) == 648
    assert {len(row) for row in rows} == {28}
    assert "size 28 648" in text
    assert "screen_height 36" in text
```

Change the C++ committed-shape assertions to width 28, height 648, band height 36, spawn
`{3.5F, 646.5F}`, and goal `{24.5F, 1.5F}`.

- [ ] **Step 2: Run tests to prove RED**

```bash
rtk proxy out/assets-venv/bin/python -m pytest tests/test_narrow_campaign.py -q
rtk proxy ./out/campaign-final/jumpcastle_tests "committed campaign has the approved shape"
```

Expected: Python import fails and the C++ shape test reports 32 by 324 instead of 28 by 648.

- [ ] **Step 3: Implement the deterministic chamber renderer**

```python
from dataclasses import dataclass

WIDTH = 28
BAND_HEIGHT = 36
SCREEN_COUNT = 18


@dataclass(frozen=True)
class SolidRect:
    x: int
    y: int
    width: int
    height: int = 1


@dataclass(frozen=True)
class Chamber:
    name: str
    mechanic: str
    route: tuple[SolidRect, ...]
    obstacles: tuple[SolidRect, ...] = ()

    @property
    def solids(self) -> tuple[SolidRect, ...]:
        return self.route + self.obstacles

    @property
    def route_jump_count(self) -> int:
        return len(self.route) - 1


ROUTE_Y = (35, 32, 29, 26, 23, 20, 17, 14, 11, 8, 5, 2)
```

Represent every route using explicit local X coordinates. Use these 18 authored tracks as the base
spine; every adjacent value differs by at most four tiles so the unchanged production jump can
reach the next surface:

```python
ROUTE_X = (
    (2, 5, 9, 13, 17, 14, 10, 6, 3, 7, 11, 15),
    (15, 12, 8, 4, 7, 11, 15, 18, 14, 10, 6, 3),
    (3, 7, 11, 15, 18, 14, 10, 6, 2, 5, 9, 13),
    (13, 9, 5, 2, 6, 10, 14, 18, 15, 11, 7, 3),
    (3, 6, 10, 14, 18, 15, 11, 7, 4, 8, 12, 16),
    (16, 12, 8, 4, 7, 11, 15, 18, 14, 10, 6, 2),
    (2, 6, 10, 14, 18, 14, 10, 6, 3, 7, 11, 15),
    (15, 11, 7, 3, 6, 10, 14, 18, 15, 11, 7, 3),
    (3, 7, 11, 15, 18, 15, 11, 7, 3, 6, 10, 14),
    (14, 10, 6, 2, 5, 9, 13, 17, 14, 10, 6, 3),
    (3, 6, 10, 14, 17, 13, 9, 5, 2, 6, 10, 14),
    (14, 10, 6, 3, 7, 11, 15, 18, 14, 10, 6, 2),
    (2, 5, 9, 13, 17, 14, 10, 6, 3, 7, 11, 15),
    (15, 11, 7, 3, 6, 10, 14, 18, 15, 11, 7, 3),
    (3, 7, 11, 15, 18, 14, 10, 6, 2, 5, 9, 13),
    (13, 9, 5, 2, 6, 10, 14, 18, 15, 11, 7, 3),
    (3, 6, 10, 14, 18, 15, 11, 7, 4, 8, 12, 16),
    (16, 12, 8, 4, 7, 11, 15, 18, 20, 17, 20, 21),
)
```

Start with seven-tile-wide platforms and add a full bottom floor only to screen 1. Convert local
rows to global top-down rows with `global_y = (SCREEN_COUNT - screen) * BAND_HEIGHT + local_y`.
Reject out-of-bounds or overlapping rectangles before rendering. Emit metadata, three six-screen
biome ranges, spawn `3 646`, goal `24 1`, and exactly 648 collision rows.

Register the authoring test in CMake so it is part of the full suite:

```cmake
add_test(
  NAME narrow_campaign_authoring
  COMMAND ${Python3_EXECUTABLE} -m pytest
          ${PROJECT_SOURCE_DIR}/tests/test_narrow_campaign.py -q
)
set_tests_properties(narrow_campaign_authoring PROPERTIES
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
```

- [ ] **Step 4: Generate the new runtime campaign**

```bash
rtk proxy out/assets-venv/bin/python tools/narrow_campaign.py \
  --output assets/levels/campaign.level
```

Expected: the file header contains `size 28 648` and `screen_height 36`.

- [ ] **Step 5: Run structure and baseline solver gates**

```bash
rtk proxy out/assets-venv/bin/python -m pytest tests/test_narrow_campaign.py -q
rtk cmake --build out/campaign-final --target jumpcastle_solver jumpcastle_tests --parallel
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
```

Expected: authoring tests pass and solver reports `reachable`, highest screen 18, maximum charge no
greater than 90 percent. If a track is unreachable, adjust only the failing adjacent X coordinate
reported by the highest-screen diagnostic and repeat the same command.

- [ ] **Step 6: Commit the reachable base spine**

```bash
rtk git add CMakeLists.txt tools/narrow_campaign.py tests/test_narrow_campaign.py \
  assets/levels/campaign.level tests/world_test.cpp tests/world_reachability_test.cpp
rtk git commit -m "feat(level): Establish narrow vertical tower spine"
```

---

### Task 3: Courtyard Challenge Chambers

**Files:**
- Modify: `tools/narrow_campaign.py`
- Modify: `assets/levels/campaign.level`
- Modify: `tests/test_narrow_campaign.py`

**Interfaces:**
- Consumes: `Chamber`, `SolidRect`, and `render_campaign` from Task 2.
- Produces: distinct screens 1-6 tagged `training_ascent`, `long_gap`, `wall_rebound`,
  `low_ceiling`, `central_tower`, and `gatehouse_exam`.

- [ ] **Step 1: Add failing mechanic-coverage assertions**

```python
def test_courtyard_covers_six_training_mechanics():
    assert {room.mechanic for room in CHAMBERS[:6]} == {
        "training_ascent", "long_gap", "wall_rebound",
        "low_ceiling", "central_tower", "gatehouse_exam",
    }
    assert all(8 <= room.route_jump_count <= 12 for room in CHAMBERS[:6])
```

- [ ] **Step 2: Run the focused test to prove RED**

```bash
rtk proxy out/assets-venv/bin/python -m pytest \
  tests/test_narrow_campaign.py::test_courtyard_covers_six_training_mechanics -q
```

Expected: FAIL because the base spine has not assigned the six mechanic tags.

- [ ] **Step 3: Author screens 1-6 without breaking the route**

Keep the twelve route surfaces from Task 2. Widen screens 1-2 route platforms to 8-10 tiles, add
one two-tile-thick rebound wall on screen 3, a two-row ceiling above the middle launch on screen 4,
a central 6 by 12 solid tower on screen 5 with route surfaces wrapping around it, and a combination
of the previous three shapes on screen 6. Every added solid rectangle must be included explicitly in
the corresponding `Chamber.solids`; no random placement or loop-generated obstacle is allowed.

- [ ] **Step 4: Regenerate and verify the Courtyard route**

```bash
rtk proxy out/assets-venv/bin/python tools/narrow_campaign.py \
  --output assets/levels/campaign.level
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
rtk proxy ./out/campaign-final/jumpcastle_tests \
  "committed campaign is reachable as one continuous route"
```

Expected: both solver invocations pass and highest screen remains 18.

- [ ] **Step 5: Commit Courtyard geometry**

```bash
rtk git add tools/narrow_campaign.py tests/test_narrow_campaign.py \
  assets/levels/campaign.level
rtk git commit -m "feat(level): Author Courtyard tower chambers"
```

---

### Task 4: Frosted Keep Challenge Chambers

**Files:**
- Modify: `tools/narrow_campaign.py`
- Modify: `assets/levels/campaign.level`
- Modify: `tests/test_narrow_campaign.py`

**Interfaces:**
- Consumes: the reachable tower spine and chamber authoring API.
- Produces: screens 7-12 tagged `split_shaft`, `window_steps`, `crossing_chamber`,
  `reversal_climb`, `narrow_gallery`, and `bell_tower_exam`.

- [ ] **Step 1: Add the failing Frosted Keep mechanic assertion**

```python
def test_frosted_keep_covers_midgame_mechanics():
    assert {room.mechanic for room in CHAMBERS[6:12]} == {
        "split_shaft", "window_steps", "crossing_chamber",
        "reversal_climb", "narrow_gallery", "bell_tower_exam",
    }
```

- [ ] **Step 2: Run it to prove RED**

```bash
rtk proxy out/assets-venv/bin/python -m pytest \
  tests/test_narrow_campaign.py::test_frosted_keep_covers_midgame_mechanics -q
```

Expected: FAIL until screens 7-12 are authored.

- [ ] **Step 3: Author screens 7-12**

Use 5-8 tile route platforms. Add two explicit vertical rectangles to create the split shaft on
screen 7; alternating 5-tile window ledges on screen 8; two crossing diagonal platform tracks on
screen 9; alternating rebound walls on screen 10; three low ceiling rectangles on screen 11; and a
combined long-gap/reversal sequence on screen 12. Keep the primary route coordinates explicit and
retain visible lower recovery platforms on every screen.

- [ ] **Step 4: Regenerate and run solver plus structure tests**

```bash
rtk proxy out/assets-venv/bin/python tools/narrow_campaign.py \
  --output assets/levels/campaign.level
rtk proxy out/assets-venv/bin/python -m pytest tests/test_narrow_campaign.py -q
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
```

Expected: all pass; a failed highest-screen diagnostic is fixed by widening or shifting only the
reported mandatory landing, never by weakening solver tolerance.

- [ ] **Step 5: Commit Frosted Keep geometry**

```bash
rtk git add tools/narrow_campaign.py tests/test_narrow_campaign.py \
  assets/levels/campaign.level
rtk git commit -m "feat(level): Author Frosted Keep tower chambers"
```

---

### Task 5: Crown Spire Challenge Chambers

**Files:**
- Modify: `tools/narrow_campaign.py`
- Modify: `assets/levels/campaign.level`
- Modify: `tests/test_narrow_campaign.py`

**Interfaces:**
- Consumes: the reachable tower spine and chamber authoring API.
- Produces: screens 13-18 tagged `broken_bridge`, `crown_chamber`, `vertical_chimney`,
  `overhang_reversal`, `fall_funnel`, and `throne_leap`.

- [ ] **Step 1: Add the failing Crown Spire mechanic assertion**

```python
def test_crown_spire_covers_endgame_mechanics():
    assert {room.mechanic for room in CHAMBERS[12:]} == {
        "broken_bridge", "crown_chamber", "vertical_chimney",
        "overhang_reversal", "fall_funnel", "throne_leap",
    }
```

- [ ] **Step 2: Run it to prove RED**

```bash
rtk proxy out/assets-venv/bin/python -m pytest \
  tests/test_narrow_campaign.py::test_crown_spire_covers_endgame_mechanics -q
```

Expected: FAIL until screens 13-18 are authored.

- [ ] **Step 3: Author screens 13-18**

Use 3-7 tile primary landings while keeping one-tile minimum surfaces only for optional recovery.
Add an explicit central gap on screen 13, a 10 by 12 crown block wrapped by route platforms on screen
14, two 2-tile-wide chimney walls on screen 15, three overhang rectangles on screen 16, a symmetric
funnel with lower rescue ledges on screen 17, and a readable alternating ascent ending below goal
`(24.5, 1.5)` on screen 18. Keep route X changes within production reach and do not increase charge
samples beyond 90 percent.

- [ ] **Step 4: Regenerate and run all reachability gates**

```bash
rtk proxy out/assets-venv/bin/python tools/narrow_campaign.py \
  --output assets/levels/campaign.level
rtk proxy out/assets-venv/bin/python -m pytest tests/test_narrow_campaign.py -q
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
rtk proxy ./out/campaign-final/jumpcastle_tests \
  "committed campaign is reachable as one continuous route"
```

Expected: all pass, highest screen 18, tolerance passed, maximum charge at most 90 percent.

- [ ] **Step 5: Commit Crown Spire geometry**

```bash
rtk git add tools/narrow_campaign.py tests/test_narrow_campaign.py \
  assets/levels/campaign.level
rtk git commit -m "feat(level): Author Crown Spire tower chambers"
```

---

### Task 6: Committed Solver Trace and Replay Gate

**Files:**
- Create: `assets/levels/campaign-route.json`
- Modify: `tests/replay_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ReachabilitySolver::solve_campaign`, `make_trace`, `read_trace`, and `verify_trace`.
- Produces: a committed production-physics trace that reaches the new goal.

- [ ] **Step 1: Add the failing committed-trace replay test**

```cpp
TEST_CASE("committed narrow tower trace replays to the crown") {
    const auto root = std::filesystem::path{JUMPCASTLE_SOURCE_DIR};
    const WorldMap world = WorldMap::load(root / "assets/levels/campaign.level");
    const SolverTrace trace = read_trace(root / "assets/levels/campaign-route.json");
    const ReplayResult replay = verify_trace(world, trace);

    INFO(replay.failure);
    CHECK(replay.completed);
    CHECK(replay.executed_jumps >= 8 * 18);
    CHECK(replay.executed_jumps <= 12 * 18);
}
```

- [ ] **Step 2: Run the focused test to prove RED**

```bash
rtk cmake --build out/campaign-final --target jumpcastle_tests --parallel
rtk proxy ./out/campaign-final/jumpcastle_tests \
  "committed narrow tower trace replays to the crown"
```

Expected: FAIL because `campaign-route.json` is absent.

- [ ] **Step 3: Generate and verify the accepted trace**

```bash
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign \
  --trace assets/levels/campaign-route.json
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level \
  --verify-trace assets/levels/campaign-route.json
```

Expected: `replay complete: goal reached` and the trace contains between 144 and 216 jumps.

- [ ] **Step 4: Include the trace in runtime asset synchronization**

Add `levels/campaign-route.json` to `JUMPCASTLE_RUNTIME_ASSETS`, then rebuild assets and rerun the
focused replay test.

- [ ] **Step 5: Commit the replay proof**

```bash
rtk git add CMakeLists.txt assets/levels/campaign-route.json tests/replay_test.cpp
rtk git commit -m "test(level): Commit narrow tower solver replay"
```

---

### Task 7: Portrait Render Smoke and Level Documentation

**Files:**
- Modify: `tests/test_render_asset_smoke.py`
- Modify: `docs/LEVEL_DESIGN.md`

**Interfaces:**
- Consumes: committed campaign, generated atlases, 448 by 576 render contract.
- Produces: headless visibility coverage for bottom, middle, and top portrait bands.

- [ ] **Step 1: Add failing portrait assertions to the smoke test**

Assert the rendered image size is `(448, 576)`, the three sampled bands each contain non-background
terrain pixels, and the player/goal masks each contain opaque pixels within their expected band.

- [ ] **Step 2: Run the smoke test to prove RED**

```bash
rtk proxy out/assets-venv/bin/python -m pytest tests/test_render_asset_smoke.py -q
```

Expected: FAIL while the helper still assumes the previous 512 by 288 target.

- [ ] **Step 3: Update the smoke renderer and authoring guide**

Derive output dimensions from 28 tiles by 36 tiles at 16 pixels each. Update `docs/LEVEL_DESIGN.md`
with the new metadata, bottom-up screen numbering, local-to-global coordinate formula, mechanic tags,
generator command, solver command, and committed trace verification command.

- [ ] **Step 4: Run asset and render validation**

```bash
rtk proxy out/assets-venv/bin/python -m pytest \
  tests/test_asset_selection.py tests/test_narrow_campaign.py \
  tests/test_render_asset_smoke.py -q
rtk proxy out/assets-venv/bin/python tools/verify_asset_pixels.py \
  assets/generated/manifest.json
```

Expected: all Python tests and pixel verification pass.

- [ ] **Step 5: Commit render coverage and documentation**

```bash
rtk git add tests/test_render_asset_smoke.py docs/LEVEL_DESIGN.md
rtk git commit -m "docs(level): Document narrow tower workflow"
```

---

### Task 8: Full Verification and Manual Play Check

**Files:**
- No source changes expected.

**Interfaces:**
- Consumes: all prior task outputs.
- Produces: fresh build, test, solver, replay, and visible runtime evidence.

- [ ] **Step 1: Configure and build every deliverable**

```bash
rtk cmake -S . -B out/narrow-tower \
  -DCMAKE_BUILD_TYPE=Debug -DJUMPCASTLE_BUILD_TESTS=ON \
  -DPython3_EXECUTABLE="$PWD/out/assets-venv/bin/python"
rtk cmake --build out/narrow-tower \
  --target jumpcastle jumpcastle_solver jumpcastle_tests --parallel
```

Expected: all three targets build successfully.

- [ ] **Step 2: Run the complete automated suite**

```bash
rtk ctest --test-dir out/narrow-tower --output-on-failure
```

Expected: 100 percent tests passed.

- [ ] **Step 3: Re-run solver and replay from the clean build**

```bash
rtk proxy ./out/narrow-tower/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
rtk proxy ./out/narrow-tower/jumpcastle_solver \
  --level assets/levels/campaign.level \
  --verify-trace assets/levels/campaign-route.json
```

Expected: campaign reachable through screen 18 and replay complete at the crown.

- [ ] **Step 4: Launch and inspect the game**

```bash
rtk proxy ./out/narrow-tower/jumpcastle --asset-root assets
```

Check the bottom, middle, and top bands with debug Page Up/Page Down; confirm portrait centering,
sharp tiles, visible left- and right-facing player frames, readable entries/exits, continuous camera
crossing, and no invisible terrain or goal marker.

- [ ] **Step 5: Check repository scope**

```bash
rtk git diff --check
rtk git status --short
rtk git log --oneline -10
```

Expected: no unstaged implementation files, the user's legacy room edit remains untouched, and each
task is represented by a focused commit.
