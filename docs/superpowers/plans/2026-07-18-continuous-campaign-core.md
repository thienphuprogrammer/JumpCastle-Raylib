# Continuous Campaign Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the twelve-room checkpoint campaign with one deterministic, solver-verified, 18-screen vertical world that uses committed charge jumps and continuous multi-screen falls.

**Architecture:** Introduce pure C++ value types and a dynamic `WorldMap`, then route gameplay, solver, replay, camera, and the existing renderer through one fixed-step simulation. Keep the current renderer and committed atlases working during this plan; the follow-up visual plan replaces them after the new campaign is playable and verified.

**Tech Stack:** C++20, CMake 3.24+, Catch2 3.8.1, nlohmann/json 3.12.0, raylib 5.5 only at the application/rendering boundary, CodeGraph CLI.

## Global Constraints

- Preserve every pre-existing dirty file before replacing its runtime path; specifically archive the current local `assets/levels/room-01.level` byte-for-byte.
- Use a 16-pixel tile, a 512 by 288 logical viewport, a 32 by 324 tile world, 18 camera screens, and 18 rows per screen.
- Use positive-down world coordinates: the top row is `y=0`, spawn is near the bottom, gravity is positive, and jump velocity is negative.
- Run authoritative simulation at exactly 120 Hz; rendering may use a different frame rate.
- Gameplay, solver, and replay must call the same collision and player-step functions.
- Do not add checkpoints, spike teleportation, airborne steering, procedural map generation, or copied Jump King content.
- A bottom-boundary fall resets to the single spawn; other missed jumps remain physical falls through previous screens.
- Keep Windows, macOS, and Linux builds warning-clean under the existing warning policy.
- Use `rtk` for every shell command and CodeGraph before grep or direct file reads when locating code.

---

## File Map

### New Core Files

- `include/jumpcastle/math.hpp`: raylib-independent `Vec2` and arithmetic helpers.
- `include/jumpcastle/world.hpp`: `WorldMap`, `Biome`, metadata, parser, and world queries.
- `src/world.cpp`: raylib-independent `WorldMap` storage and world queries.
- `src/world_loader.cpp`: strict campaign v2 file parser in the content target.
- `include/jumpcastle/camera.hpp`: fixed camera-band selection.
- `src/camera.cpp`: player-height-to-screen mapping.
- `include/jumpcastle/replay.hpp`: trace schema and production replay API.
- `src/replay.cpp`: JSON trace read/write and deterministic verification.
- `include/jumpcastle/fixed_step.hpp`: render-frame accumulator for exact 120 Hz updates.
- `tests/test_world_factory.hpp`: compact in-memory worlds shared by physics, camera, solver and replay tests.
- `assets/levels/campaign.level`: the sole runtime world definition.
- `assets/levels/legacy/README.md`: explains archived non-runtime room files.
- `assets/levels/legacy/room-01.local.level`: byte-for-byte snapshot of the dirty room before migration.
- `tests/math_test.cpp`, `tests/world_test.cpp`, `tests/camera_test.cpp`, `tests/replay_test.cpp`: focused core coverage.

### Existing Files to Refactor

- `include/jumpcastle/game_config.hpp`: numeric core constants only; no raylib types.
- `include/jumpcastle/player.hpp`, `src/player.cpp`: committed charge state and shared 120 Hz player step.
- `include/jumpcastle/collision.hpp`, `src/collision.cpp`: collision against the full dynamic world.
- `include/jumpcastle/campaign.hpp`, `src/campaign.cpp`: spawn reset, fall count, elapsed time, completion.
- `include/jumpcastle/simulation.hpp`, `src/simulation.cpp`: one production world step.
- `include/jumpcastle/solver.hpp`, `src/solver.cpp`, `src/solver_main.cpp`: full-world search, trace output, trace verification.
- `include/jumpcastle/game.hpp`, `src/game.cpp`: fixed-step accumulator and continuous world runtime.
- `include/jumpcastle/renderer.hpp`, `src/renderer.cpp`: temporary continuous-world rendering using existing atlases.
- `CMakeLists.txt`: pure core/content targets, new sources/tests, new solver executable name.
- `docs/LEVEL_DESIGN.md`: campaign v2 format and verification workflow.

### Files Removed After Migration

- `include/jumpcastle/level.hpp`, `src/level.cpp`: room repository replaced by `WorldMap`.
- `include/jumpcastle/tilemap.hpp`, `src/tilemap.cpp`: fixed 16 by 12 room grid replaced by dynamic storage.
- `tests/level_test.cpp`, `tests/tilemap_test.cpp`, `tests/test_level_factory.hpp`: replaced by world fixtures.
- `tests/campaign_reachability_test.cpp`: replaced by full-world solve and replay tests.
- Runtime use of `assets/levels/room-01.level` through `room-12.level`; archived copies are non-runtime.

---

### Task 1: Make Core Math Independent of Raylib

**Files:**
- Create: `include/jumpcastle/math.hpp`
- Create: `tests/math_test.cpp`
- Modify: `include/jumpcastle/game_config.hpp`
- Modify: `include/jumpcastle/player.hpp`
- Modify: `include/jumpcastle/collision.hpp`
- Modify: `include/jumpcastle/campaign.hpp`
- Modify: `include/jumpcastle/tilemap.hpp`
- Modify: `include/jumpcastle/level.hpp`
- Modify: `include/jumpcastle/solver.hpp`
- Modify: `src/player.cpp`
- Modify: `src/collision.cpp`
- Modify: `src/campaign.cpp`
- Modify: `src/level.cpp`
- Modify: `src/solver.cpp`
- Modify: `src/renderer.cpp`
- Modify: all tests that construct `Vector2`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `jumpcastle::Vec2`, `operator+`, `operator-`, scalar multiplication, `length(Vec2)`, and `normalized(Vec2)`.
- Produces: `::Vector2 to_raylib(Vec2)` as a private renderer helper, never a core API.

- [ ] **Step 1: Write the failing raylib-independence test**

```cpp
// tests/math_test.cpp
#include "jumpcastle/math.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using jumpcastle::Vec2;

TEST_CASE("core vector arithmetic is deterministic") {
    const Vec2 value = Vec2{2.0F, -4.0F} + Vec2{1.0F, 3.0F};
    CHECK(value.x == Approx(3.0F));
    CHECK(value.y == Approx(-1.0F));
    CHECK(jumpcastle::length(Vec2{3.0F, 4.0F}) == Approx(5.0F));
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: compilation fails because `jumpcastle/math.hpp` does not exist.

- [ ] **Step 3: Add the value type and migrate core signatures**

```cpp
// include/jumpcastle/math.hpp
#pragma once

#include <cmath>

namespace jumpcastle {

struct Vec2 {
    float x{};
    float y{};
    friend constexpr bool operator==(Vec2, Vec2) = default;
};

[[nodiscard]] constexpr Vec2 operator+(Vec2 left, Vec2 right) noexcept {
    return {left.x + right.x, left.y + right.y};
}
[[nodiscard]] constexpr Vec2 operator-(Vec2 left, Vec2 right) noexcept {
    return {left.x - right.x, left.y - right.y};
}
[[nodiscard]] constexpr Vec2 operator*(Vec2 value, float scale) noexcept {
    return {value.x * scale, value.y * scale};
}
[[nodiscard]] inline float length(Vec2 value) noexcept {
    return std::hypot(value.x, value.y);
}
[[nodiscard]] inline Vec2 normalized(Vec2 value) noexcept {
    const float magnitude = length(value);
    return magnitude == 0.0F ? Vec2{} : value * (1.0F / magnitude);
}

}  // namespace jumpcastle
```

Replace every core `Vector2` with `Vec2`. Remove `#include "raylib.h"` and `Color` from
`game_config.hpp`; move the background color literal into `renderer.cpp`. At every raylib call in
the renderer, use this private adapter:

```cpp
[[nodiscard]] ::Vector2 to_raylib(const jumpcastle::Vec2 value) noexcept {
    return {value.x, value.y};
}
```

Change `jumpcastle_core` to link only `nlohmann_json::nlohmann_json`; keep raylib linked by the
`jumpcastle` executable.

- [ ] **Step 4: Build and run all existing tests**

Run: `rtk cmake --build out/campaign-final -j2 && rtk ctest --test-dir out/campaign-final --output-on-failure`

Expected: all existing tests plus `math_test` pass; link output shows `jumpcastle_core` no longer
needs raylib.

- [ ] **Step 5: Commit the boundary refactor**

```bash
rtk git add CMakeLists.txt include src tests
rtk git commit -m "ref(core): Remove Raylib types from simulation"
```

---

### Task 2: Add the Dynamic Campaign v2 Parser Alongside Legacy Rooms

**Files:**
- Create: `include/jumpcastle/world.hpp`
- Create: `src/world.cpp`
- Create: `src/world_loader.cpp`
- Create: `tests/world_test.cpp`
- Create: `tests/test_world_factory.hpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `jumpcastle::Vec2` from Task 1.
- Produces: `enum class WorldTile { empty, solid }`.
- Produces: `enum class Biome { courtyard, frosted_keep, crown_spire }`.
- Produces: `WorldMap parse_campaign(std::string_view, std::string_view)`.
- Produces: `WorldMap::load(const std::filesystem::path&)`, `tile_at`, `solid_at`, `spawn`, `goal`,
  `screen_for_y`, and `biome_for_screen`.

- [ ] **Step 1: Write parser success and diagnostic tests**

```cpp
// tests/world_test.cpp
#include "jumpcastle/world.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace jumpcastle;

TEST_CASE("campaign parser reads dimensions markers and biome ranges") {
    const WorldMap world = parse_campaign(R"(version 2
tile_size 16
size 4 6
screen_height 2
spawn 1 4
goal 2 1
biome 1 1 crown_spire
biome 2 2 frosted_keep
biome 3 3 courtyard
---
[collision]
....
....
.##.
....
....
####
)", "memory.level");

    CHECK(world.width() == 4);
    CHECK(world.height() == 6);
    CHECK(world.screen_count() == 3);
    CHECK(world.spawn() == Vec2{1.5F, 4.5F});
    CHECK(world.goal() == Vec2{2.5F, 1.5F});
    CHECK(world.biome_for_screen(0) == Biome::courtyard);
    CHECK(world.screen_for_y(4.5F) == 0);
}

TEST_CASE("campaign parser reports row and column") {
    CHECK_THROWS_WITH(
        parse_campaign("version 2\ntile_size 16\nsize 2 1\nscreen_height 1\n"
                       "spawn 0 0\ngoal 1 0\nbiome 1 1 courtyard\n---\n"
                       "[collision]\n.?\n", "broken.level"),
        Catch::Matchers::ContainsSubstring("broken.level:10:2"));
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: compilation fails because `jumpcastle/world.hpp` is missing.

- [ ] **Step 3: Implement strict dynamic storage and parsing**

```cpp
// include/jumpcastle/world.hpp (public contract)
enum class WorldTile : char { empty = '.', solid = '#' };
enum class Biome { courtyard, frosted_keep, crown_spire };

struct BiomeRange {
    int first_screen{};  // one-based, bottom to top
    int last_screen{};
    Biome biome{Biome::courtyard};
};

class WorldMap {
public:
    WorldMap(int width, int height, int screen_height,
             std::vector<WorldTile> tiles, Vec2 spawn, Vec2 goal,
             std::vector<BiomeRange> biomes);
    [[nodiscard]] static WorldMap load(const std::filesystem::path& path);
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] int screen_height() const noexcept;
    [[nodiscard]] int screen_count() const noexcept;
    [[nodiscard]] WorldTile tile_at(int x, int y) const noexcept;
    [[nodiscard]] bool solid_at(int x, int y) const noexcept;
    [[nodiscard]] Vec2 spawn() const noexcept;
    [[nodiscard]] Vec2 goal() const noexcept;
    [[nodiscard]] int screen_for_y(float world_y) const noexcept;
    [[nodiscard]] Biome biome_for_screen(int zero_based_screen) const;
};

[[nodiscard]] WorldMap parse_campaign(
    std::string_view source, std::string_view filename);
```

Out-of-range `x` is solid so the castle side walls are reliable. Out-of-range `y` is empty so the
top goal and bottom reset can be handled by simulation. Convert spawn and goal metadata coordinates
to tile centers by adding `0.5F` to both axes; the cell below spawn is its initial standing surface.

Add this shared test contract in `tests/test_world_factory.hpp` and implement each function with
the public `WorldMap` constructor and literal row strings:

```cpp
namespace jumpcastle::test {

[[nodiscard]] WorldMap make_world(
    std::vector<std::string_view> rows,
    int screen_height,
    Vec2 spawn,
    Vec2 goal,
    std::vector<BiomeRange> biomes);
[[nodiscard]] WorldMap flat_world();
[[nodiscard]] WorldMap world_with_ceiling();
[[nodiscard]] WorldMap three_screen_world();
[[nodiscard]] WorldMap reachable_three_screen_world();
[[nodiscard]] WorldMap unreachable_three_screen_world();

}  // namespace jumpcastle::test
```

`flat_world` is 10 by 10 with a solid final row. `world_with_ceiling` adds a solid row at `y=5`.
The three-screen fixtures are 10 by 30 with ten rows per screen; the reachable fixture has five-tile
alternating ledges every four rows, while the unreachable fixture removes the only ledge in the
middle screen.

- [ ] **Step 4: Run parser tests and the existing suite**

Run: `rtk ctest --test-dir out/campaign-final -R "world|level|tilemap" --output-on-failure`

Expected: new world tests pass; legacy level and tilemap tests remain green.

- [ ] **Step 5: Commit the parser**

```bash
rtk git add CMakeLists.txt include/jumpcastle/world.hpp src/world.cpp src/world_loader.cpp \
  tests/world_test.cpp tests/test_world_factory.hpp
rtk git commit -m "feat(world): Add continuous campaign parser"
```

---

### Task 3: Implement Committed Charge Physics Against the Full World

**Files:**
- Modify: `include/jumpcastle/game_config.hpp`
- Modify: `include/jumpcastle/player.hpp`
- Modify: `src/player.cpp`
- Modify: `include/jumpcastle/collision.hpp`
- Modify: `src/collision.cpp`
- Modify: `tests/player_test.cpp`
- Modify: `tests/collision_test.cpp`

**Interfaces:**
- Consumes: `WorldMap`, `Vec2`.
- Produces: `enum class PlayerMode { airborne, grounded, charging }`.
- Produces: `step_player(PlayerState&, const WorldMap&, PlayerInput, float fixed_delta)`.
- Produces: `resolve_world_collision(const WorldMap&, Vec2 previous, PlayerState&)`.

- [ ] **Step 1: Add failing tests for charge commitment and collision continuity**

```cpp
TEST_CASE("release commits jump and airborne input cannot steer") {
    const WorldMap world = test::flat_world();
    PlayerState player{.position = {4.5F, 8.6F}, .mode = PlayerMode::grounded};

    for (int tick = 0; tick < 60; ++tick) {
        step_player(player, world,
                    PlayerInput{.right = true, .jump_down = true},
                    config::fixed_delta);
    }
    REQUIRE(player.mode == PlayerMode::charging);
    step_player(player, world, PlayerInput{.right = true, .jump_released = true}, 1.0F / 120.0F);
    const float committed_x = player.velocity.x;
    step_player(player, world, PlayerInput{.left = true}, 1.0F / 120.0F);

    CHECK(player.mode == PlayerMode::airborne);
    CHECK(player.velocity.x == Catch::Approx(committed_x));
}

TEST_CASE("high charge cannot tunnel through one tile ceiling") {
    const WorldMap world = test::world_with_ceiling();
    PlayerState player{.position = {4.5F, 8.6F}, .velocity = {0.0F, -25.0F},
                       .mode = PlayerMode::airborne};
    step_player(player, world, {}, 1.0F / 120.0F);
    CHECK(player.position.y >= 5.0F + config::player_half_size.y);
    CHECK(player.velocity.y >= 0.0F);
}
```

- [ ] **Step 2: Run focused tests and confirm the old API fails the new contract**

Run: `rtk ctest --test-dir out/campaign-final -R "player|collision" --output-on-failure`

Expected: compilation fails for `PlayerMode`, `step_player`, and world collision helpers.

- [ ] **Step 3: Implement the state machine and fixed constants**

```cpp
// game_config.hpp
inline constexpr float fixed_delta = 1.0F / 120.0F;
inline constexpr float minimum_charge_seconds = 0.12F;
inline constexpr float maximum_charge_seconds = 0.85F;
inline constexpr Vec2 player_half_size{0.30F, 0.40F};

// player.hpp
enum class PlayerMode { airborne, grounded, charging };
struct PlayerState {
    Vec2 position{};
    Vec2 velocity{};
    float jump_hold_time{};
    float animation_time{};
    PlayerMode mode{PlayerMode::airborne};
    bool facing_right{true};
};
```

Use a clamped smoothstep charge ratio:

```cpp
const float raw = std::clamp(
    (seconds - config::minimum_charge_seconds) /
    (config::maximum_charge_seconds - config::minimum_charge_seconds), 0.0F, 1.0F);
const float strength = raw * raw * (3.0F - 2.0F * raw);
```

Lock horizontal velocity after release. Resolve motion axis-by-axis using the previous position and
the swept tile range; walls multiply incoming horizontal velocity by `-config::horizontal_bounce`,
ceilings clamp upward velocity to zero, and floors enter `grounded`.

- [ ] **Step 4: Run focused and full tests**

Run: `rtk cmake --build out/campaign-final -j2 && rtk ctest --test-dir out/campaign-final --output-on-failure`

Expected: all tests pass at 120 Hz; no existing test retains a hard-coded 60 Hz physics assumption.

- [ ] **Step 5: Commit shared world physics**

```bash
rtk git add include/jumpcastle/game_config.hpp include/jumpcastle/player.hpp \
  include/jumpcastle/collision.hpp src/player.cpp src/collision.cpp tests
rtk git commit -m "feat(physics): Add committed continuous-world jumps"
```

---

### Task 4: Add World Session State and Fixed Camera Bands

**Files:**
- Create: `include/jumpcastle/camera.hpp`
- Create: `src/camera.cpp`
- Create: `tests/camera_test.cpp`
- Modify: `include/jumpcastle/campaign.hpp`
- Modify: `src/campaign.cpp`
- Modify: `include/jumpcastle/simulation.hpp`
- Modify: `src/simulation.cpp`
- Modify: `tests/campaign_test.cpp`
- Modify: `tests/simulation_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CameraBand select_camera_band(const WorldMap&, float player_y)`.
- Produces: `CampaignEvent step_world(PlayerState&, CampaignState&, const WorldMap&, PlayerInput)`.
- Produces: `CampaignState { Vec2 spawn; int falls; double elapsed_seconds; bool complete; }`.

- [ ] **Step 1: Write failing camera, fall, and goal tests**

```cpp
TEST_CASE("camera screens count from bottom to top") {
    const WorldMap world = test::three_screen_world();
    CHECK(select_camera_band(world, 5.0F).screen == 2);
    CHECK(select_camera_band(world, 25.0F).screen == 0);
    CHECK(select_camera_band(world, 25.0F).world_top == Catch::Approx(20.0F));
}

TEST_CASE("only falling below the world resets progress") {
    const WorldMap world = test::three_screen_world();
    CampaignState campaign{.spawn = world.spawn()};
    PlayerState player{.position = {2.0F, 30.5F}};

    CHECK(step_world(player, campaign, world, {}) == CampaignEvent::fell_below_world);
    CHECK(player.position == world.spawn());
    CHECK(campaign.falls == 1);
}
```

- [ ] **Step 2: Run the new tests to verify failure**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: missing camera API and obsolete checkpoint-oriented `CampaignState` errors.

- [ ] **Step 3: Implement session rules**

```cpp
enum class CampaignEvent { none, fell_below_world, completed };
struct CampaignState {
    Vec2 spawn{};
    int falls{};
    double elapsed_seconds{};
    bool complete{};
};

struct CameraBand {
    int screen{};       // zero-based, bottom to top
    float world_top{};  // positive-down tile coordinate
    Biome biome{Biome::courtyard};
};
```

`step_world` always advances exactly `config::fixed_delta`. It calls `step_player`, increments
elapsed time while incomplete, completes when the player's bounds overlap the goal cell, and resets
only when `player.position.y - player_half_size.y > world.height()`.

- [ ] **Step 4: Run session tests**

Run: `rtk ctest --test-dir out/campaign-final -R "camera|campaign|simulation" --output-on-failure`

Expected: all focused tests pass; no checkpoint or spike event remains in production session tests.

- [ ] **Step 5: Commit session behavior**

```bash
rtk git add CMakeLists.txt include/jumpcastle/camera.hpp src/camera.cpp \
  include/jumpcastle/campaign.hpp src/campaign.cpp include/jumpcastle/simulation.hpp \
  src/simulation.cpp tests
rtk git commit -m "feat(campaign): Add continuous falls and camera bands"
```

---

### Task 5: Preserve Legacy Edits and Author the 18-Screen Collision World

**Files:**
- Create: `assets/levels/legacy/README.md`
- Create: `assets/levels/legacy/room-01.local.level`
- Create: `assets/levels/campaign.level`
- Modify: `tests/world_test.cpp`
- Modify: `docs/LEVEL_DESIGN.md`

**Interfaces:**
- Consumes: `WorldMap::load`.
- Produces: a 32 by 324 real campaign with spawn `(4, 322)`, goal `(27, 2)`, and three six-screen
  biome ranges.

- [ ] **Step 1: Snapshot the dirty room before changing runtime level selection**

Run: `rtk git status --short assets/levels && rtk git diff -- assets/levels/room-01.level`

Expected: `assets/levels/room-01.level` is shown as modified. Use `apply_patch` to add its complete
current content to `assets/levels/legacy/room-01.local.level`. Add a README stating that legacy
rooms are excluded from runtime and retained only to preserve pre-redesign local work.

- [ ] **Step 2: Add a failing real-world contract test**

```cpp
TEST_CASE("committed campaign has the approved shape") {
    const auto path = std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets/levels/campaign.level";
    const WorldMap world = WorldMap::load(path);
    CHECK(world.width() == 32);
    CHECK(world.height() == 324);
    CHECK(world.screen_height() == 18);
    CHECK(world.screen_count() == 18);
    CHECK(world.biome_for_screen(0) == Biome::courtyard);
    CHECK(world.biome_for_screen(6) == Biome::frosted_keep);
    CHECK(world.biome_for_screen(12) == Biome::crown_spire);
}
```

- [ ] **Step 3: Run the contract test to verify it fails**

Run: `rtk ctest --test-dir out/campaign-final -R "committed campaign" --output-on-failure`

Expected: failure because `campaign.level` does not exist.

- [ ] **Step 4: Author the exact screen skeletons**

Create one 324-row collision grid. Use these global screen row ranges and required platform
silhouettes; coordinates are inclusive tile rectangles `(x1, y, x2)` and must be expanded to `#`
in the ASCII grid:

| Screen | Rows | Required standing surfaces from low to high |
| --- | --- | --- |
| 01 | 306-323 | `(0,323,31) (3,319,10) (17,315,26) (7,311,14) (21,307,30)` |
| 02 | 288-305 | `(21,305,30) (5,301,13) (18,297,25) (2,293,9) (14,289,22)` |
| 03 | 270-287 | `(14,287,22) (25,283,31) (8,279,15) (0,275,5) (18,271,27)` plus right rebound wall `x=31,y=276..286` |
| 04 | 252-269 | `(18,269,27) (4,266,12) (20,262,29) (8,258,15) (0,253,7)` plus ceiling strips above the two middle launches |
| 05 | 234-251 | `(0,251,7) (12,248,19) (23,244,31) (13,240,20) (3,235,10)` plus central tower `x=14..17,y=240..250` |
| 06 | 216-233 | `(3,233,10) (18,229,27) (7,225,14) (23,221,31) (11,217,19)` |
| 07 | 198-215 | `(11,215,19) (1,211,8) (23,207,30) (10,203,16) (0,199,7)` |
| 08 | 180-197 | `(0,197,7) (13,194,20) (24,190,31) (8,186,14) (18,181,26)` |
| 09 | 162-179 | `(18,179,26) (2,175,10) (21,171,29) (8,167,15) (0,163,6)` with open center crossing |
| 10 | 144-161 | `(0,161,6) (14,158,21) (25,154,31) (7,150,13) (18,145,26)` and alternating rebound walls |
| 11 | 126-143 | `(18,143,26) (3,139,9) (15,135,21) (25,131,31) (8,127,14)` with low ceilings over surfaces 2 and 4 |
| 12 | 108-125 | `(8,125,14) (21,121,29) (2,117,9) (16,113,23) (25,109,31)` |
| 13 | 90-107 | `(25,107,31) (8,103,15) (19,99,25) (2,95,8) (14,91,21)` |
| 14 | 72-89 | `(14,89,21) (26,85,31) (4,81,10) (20,77,27) (9,73,15)` around a central solid crown silhouette |
| 15 | 54-71 | `(9,71,15) (18,67,24) (7,63,12) (20,59,25) (10,55,16)` with chimney walls `x=5` and `x=27` |
| 16 | 36-53 | `(10,53,16) (24,49,30) (5,45,11) (19,41,25) (1,37,7)` with two overhang ceilings |
| 17 | 18-35 | `(1,35,7) (15,31,21) (26,27,31) (8,23,13) (19,19,25)` plus rescue ledges at `(0,30,3)` and `(28,22,31)` |
| 18 | 0-17 | `(19,17,25) (4,13,10) (16,9,22) (25,5,31) (23,3,30)` with goal at `(27,2)` |

Every screen also has solid side boundaries only where its rebound design requires them; do not
close all inter-screen fall routes. The solver tasks may adjust horizontal extents by at most two
tiles and vertical positions by at most one row while preserving each silhouette.

- [ ] **Step 5: Run parser and geometry tests**

Run: `rtk ctest --test-dir out/campaign-final -R "world" --output-on-failure`

Expected: parser and approved-shape tests pass; solver verification is intentionally added next.

- [ ] **Step 6: Commit the world source and archive**

```bash
rtk git add assets/levels/campaign.level assets/levels/legacy docs/LEVEL_DESIGN.md tests/world_test.cpp
rtk git commit -m "feat(level): Add eighteen-screen vertical world"
```

---

### Task 6: Replace Room-by-Room Solving with Full-World Search

**Files:**
- Modify: `include/jumpcastle/solver.hpp`
- Modify: `src/solver.cpp`
- Modify: `tests/solver_test.cpp`
- Replace: `tests/campaign_reachability_test.cpp` with `tests/world_reachability_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `step_world`, `WorldMap`, `config::fixed_delta`.
- Produces: `SolverResult ReachabilitySolver::solve_campaign() const` for one continuous state graph.
- Produces: `SolverJump` with start/landing, direction, charge ticks, start/landing screens, and
  collision events.

- [ ] **Step 1: Write failing full-world and mutation tests**

```cpp
TEST_CASE("committed campaign is reachable as one continuous route") {
    const WorldMap world = WorldMap::load(campaign_path());
    const SolverResult result = ReachabilitySolver{world}.solve_campaign();
    INFO(result.failure);
    REQUIRE(result.reachable);
    CHECK(result.tolerance_passed);
    CHECK(result.highest_screen == 17);
    CHECK(result.maximum_charge_ratio <= 0.90F);
}

TEST_CASE("solver rejects a removed mandatory landing") {
    const WorldMap world = test::unreachable_three_screen_world();
    const SolverResult result = ReachabilitySolver{world}.solve_campaign();
    CHECK_FALSE(result.reachable);
    CHECK(result.failure.find("highest screen") != std::string::npos);
}
```

- [ ] **Step 2: Run the focused tests to observe the legacy composition failure**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: compilation fails because the solver still consumes `LevelRepository` and exposes
`solve_room`.

- [ ] **Step 3: Implement stable-surface search across all 324 rows**

```cpp
struct SolverConfig {
    std::array<int, 14> charge_ticks{
        15, 21, 27, 33, 39, 45, 51, 57, 63, 69, 75, 81, 87, 93};
    int maximum_air_ticks{480};
    float launch_sample_spacing{0.25F};
    float state_quantization{0.10F};
};

struct SolverResult {
    bool reachable{};
    bool tolerance_passed{};
    float maximum_charge_ratio{};
    int highest_screen{};
    std::vector<SolverJump> jumps;
    std::string failure;
};
```

Extract every contiguous exposed top surface in the full `WorldMap`. BFS nodes are stable landing
states, not screen numbers. Each edge walks to a sampled `x`, charges for a sampled tick count,
releases once, and calls `step_world` until landing, completion, bottom reset, or timeout. Preserve
multi-screen airborne trajectories and wall rebounds.

For tolerance, replay variants `(x, ticks)` of `(0,0)`, `(0,-2)`, `(0,+2)`, `(-0.1,0)`, and
`(+0.1,0)`; at least three must land on the intended destination for teaching screens. Report the
highest reached screen and nearest surface on failure.

- [ ] **Step 4: Tune only within the Task 5 geometry limits until the real route passes**

Rename the executable target to `jumpcastle_solver`, then run:

`rtk ./out/campaign-final/jumpcastle_solver --level assets/levels/campaign.level --campaign`

Expected: output reports a positive jump count, max charge at or below 90%, and highest screen 18.

When a screen fails, use the printed source surface, destination envelope and screen number. Adjust
only platform widths/positions allowed by Task 5; do not insert checkpoints or change physics to
make a single map pass.

- [ ] **Step 5: Run the full suite**

Run: `rtk ctest --test-dir out/campaign-final --output-on-failure`

Expected: all tests pass, including the full 18-screen solve.

- [ ] **Step 6: Commit solver and verified geometry together**

```bash
rtk git add include/jumpcastle/solver.hpp src/solver.cpp tests CMakeLists.txt assets/levels/campaign.level
rtk git commit -m "feat(solver): Verify the continuous campaign"
```

---

### Task 7: Add JSON Trace Serialization and Production Replay

**Files:**
- Create: `include/jumpcastle/replay.hpp`
- Create: `src/replay.cpp`
- Create: `tests/replay_test.cpp`
- Modify: `src/solver_main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `SolverResult`, `step_world`, `WorldMap`.
- Produces: `write_trace(path, result)`, `read_trace(path)`, and
  `ReplayResult verify_trace(const WorldMap&, const SolverTrace&)`.
- Produces CLI: `jumpcastle_solver --level FILE --campaign --trace FILE` and
  `jumpcastle_solver --level FILE --verify-trace FILE`.

- [ ] **Step 1: Write a failing solve-write-read-replay test**

```cpp
TEST_CASE("solver trace replays through production physics") {
    const WorldMap world = test::reachable_three_screen_world();
    const SolverResult solved = ReachabilitySolver{world}.solve_campaign();
    REQUIRE(solved.reachable);

    const SolverTrace trace = make_trace(solved);
    const std::string encoded = serialize_trace(trace);
    const ReplayResult replay = verify_trace(world, parse_trace(encoded, "memory.json"));

    INFO(replay.failure);
    CHECK(replay.completed);
    CHECK(replay.executed_jumps == solved.jumps.size());
}
```

- [ ] **Step 2: Run the focused test to verify missing APIs**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: compilation fails for replay types and functions.

- [ ] **Step 3: Implement schema version 1 and exact replay**

```cpp
struct TraceJump {
    Vec2 launch_position{};
    JumpDirection direction{JumpDirection::neutral};
    int charge_ticks{};
    Vec2 expected_landing{};
    int expected_screen{};
};
struct SolverTrace {
    int schema_version{1};
    float fixed_delta{config::fixed_delta};
    std::vector<TraceJump> jumps;
};
struct ReplayResult {
    bool completed{};
    std::size_t executed_jumps{};
    std::string failure;
};
```

Replay walks to each stored launch `x` with grounded movement, charges for exactly `charge_ticks`,
releases, then advances empty input until landing. Reject a landing farther than 0.15 tile from the
stored point and include jump index, expected screen and actual position in the error.

- [ ] **Step 4: Verify both CLI modes on the real campaign**

Run:

```bash
rtk ./out/campaign-final/jumpcastle_solver --level assets/levels/campaign.level \
  --campaign --trace out/campaign-final/campaign-route.json
rtk ./out/campaign-final/jumpcastle_solver --level assets/levels/campaign.level \
  --verify-trace out/campaign-final/campaign-route.json
```

Expected: first command writes a reachable trace; second prints `replay complete: goal reached` and
returns zero.

- [ ] **Step 5: Commit replay support**

```bash
rtk git add CMakeLists.txt include/jumpcastle/replay.hpp src/replay.cpp \
  src/solver_main.cpp tests/replay_test.cpp
rtk git commit -m "feat(solver): Replay verified campaign routes"
```

---

### Task 8: Switch the Interactive Game to Continuous World Simulation

**Files:**
- Create: `include/jumpcastle/fixed_step.hpp`
- Modify: `include/jumpcastle/game.hpp`
- Modify: `src/game.cpp`
- Modify: `include/jumpcastle/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `include/jumpcastle/player_view.hpp`
- Modify: `tests/player_view_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `WorldMap`, `step_world`, `select_camera_band`.
- Produces: fixed-step `Game::update_frame(float frame_delta)` with an accumulator.
- Produces: `Renderer::draw(const WorldMap&, const CameraBand&, const PlayerState&,
  const CampaignState&, bool)` using the existing atlases temporarily.

- [ ] **Step 1: Add a failing frame-chunk determinism test**

Move the accumulator into a core-testable helper:

```cpp
TEST_CASE("frame chunking produces identical fixed steps") {
    FixedStepClock one;
    FixedStepClock two;
    CHECK(one.consume(1.0F / 30.0F) == 4);
    CHECK(two.consume(1.0F / 60.0F) == 2);
    CHECK(two.consume(1.0F / 60.0F) == 2);
    CHECK(one.remainder() == Catch::Approx(two.remainder()));
}
```

Implement the helper with a double-precision remainder and a maximum accepted frame delta of 0.1
seconds:

```cpp
#include <algorithm>

class FixedStepClock {
public:
    [[nodiscard]] int consume(float frame_delta) noexcept {
        accumulator_ += std::clamp(static_cast<double>(frame_delta), 0.0, 0.1);
        int ticks = 0;
        while (accumulator_ + 1.0e-12 >= config::fixed_delta) {
            accumulator_ -= config::fixed_delta;
            ++ticks;
        }
        return ticks;
    }
    [[nodiscard]] double remainder() const noexcept { return accumulator_; }

private:
    double accumulator_{};
};
```

- [ ] **Step 2: Run the test to verify the helper is absent**

Run: `rtk cmake --build out/campaign-final --target jumpcastle_tests -j2`

Expected: missing `FixedStepClock` compilation error.

- [ ] **Step 3: Replace room runtime state**

```cpp
class Game {
public:
    Game();
    ~Game();
    void run();

private:
    void update_frame(float frame_delta);
    WorldMap world_;
    PlayerState player_;
    CampaignState campaign_;
    CameraBand camera_;
    FixedStepClock fixed_clock_;
    std::optional<Renderer> renderer_;
};
```

Load `assets/levels/campaign.level`, initialize the player center at `world.spawn()`, call `step_world`
once per consumed tick using the latest sampled input, and select camera from authoritative player
`y`. A jump release must be latched until one fixed tick consumes it so low render rates cannot lose
the event.

Update the renderer to iterate only rows inside `camera.world_top` through
`camera.world_top + 18`, convert world `y` to local screen `y`, and choose the closest existing
atlas by new biome: courtyard→pixel_adventure, frosted_keep→kenney, crown_spire→kings_and_pigs.
Remove spike/checkpoint drawing and change the debug overlay to `Screen N/18`, biome, position,
velocity, charge, falls.

- [ ] **Step 4: Build and launch the game**

Run: `rtk cmake --build out/campaign-final -j2 && rtk ./out/campaign-final/jumpcastle`

Expected: 1536 by 864 resizable window; player can charge/release, cross a camera boundary, and fall
back to the previous screen without respawning. Close the window after the manual check.

- [ ] **Step 5: Run automated tests**

Run: `rtk ctest --test-dir out/campaign-final --output-on-failure`

Expected: all tests pass.

- [ ] **Step 6: Commit runtime migration**

```bash
rtk git add CMakeLists.txt include/jumpcastle/game.hpp include/jumpcastle/renderer.hpp \
  include/jumpcastle/player_view.hpp src/game.cpp src/renderer.cpp tests/player_view_test.cpp
rtk git commit -m "feat(game): Run the continuous vertical campaign"
```

---

### Task 9: Remove Legacy Runtime APIs and Lock the Target Boundaries

**Files:**
- Delete: `include/jumpcastle/level.hpp`
- Delete: `src/level.cpp`
- Delete: `include/jumpcastle/tilemap.hpp`
- Delete: `src/tilemap.cpp`
- Delete: `tests/level_test.cpp`
- Delete: `tests/tilemap_test.cpp`
- Delete: `tests/test_level_factory.hpp`
- Delete: `tests/campaign_reachability_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: remaining includes/tests
- Modify: `docs/LEVEL_DESIGN.md`

**Interfaces:**
- Consumes: every replacement from Tasks 1-8.
- Produces: final targets `jumpcastle_core`, `jumpcastle_content`, `jumpcastle_raylib`,
  `jumpcastle`, `jumpcastle_solver`, and `jumpcastle_tests`.

- [ ] **Step 1: Prove no production caller still uses legacy symbols**

Run: `rtk codegraph explore "Callers of LevelRepository, RoomSelection, Tilemap, parse_room, and solve_room"`

Expected: only files scheduled for deletion or no callers. If a live caller appears, migrate it to
`WorldMap` before continuing.

- [ ] **Step 2: Split CMake targets**

```cmake
add_library(jumpcastle_core STATIC
  src/camera.cpp src/campaign.cpp src/collision.cpp src/player.cpp
  src/replay.cpp src/simulation.cpp src/solver.cpp src/world.cpp)
target_include_directories(jumpcastle_core PUBLIC include)
target_link_libraries(jumpcastle_core PRIVATE nlohmann_json::nlohmann_json)

add_library(jumpcastle_content STATIC src/assets.cpp src/world_loader.cpp)
target_include_directories(jumpcastle_content PUBLIC include)
target_link_libraries(jumpcastle_content PUBLIC jumpcastle_core
                      PRIVATE nlohmann_json::nlohmann_json)

add_library(jumpcastle_raylib STATIC src/renderer.cpp)
target_link_libraries(jumpcastle_raylib PUBLIC jumpcastle_content PRIVATE raylib)
```

Link `jumpcastle` to `jumpcastle_raylib`; link `jumpcastle_solver` and tests without raylib.

- [ ] **Step 3: Delete legacy code and remove the twelve old room files from the runtime directory**

Use `apply_patch` for each deletion. The archived local room remains under `assets/levels/legacy/`.
The runtime asset copy must later include only `campaign.level`, not the legacy directory.

- [ ] **Step 4: Configure from scratch and run all tests**

Run:

```bash
rtk cmake -S . -B out/campaign-core-clean -DCMAKE_BUILD_TYPE=Debug
rtk cmake --build out/campaign-core-clean -j2
rtk ctest --test-dir out/campaign-core-clean --output-on-failure
```

Expected: configure/build succeeds and every test passes without linking raylib into the headless
solver or core tests.

- [ ] **Step 5: Refresh CodeGraph and verify the new flow**

Run:

```bash
rtk codegraph index
rtk codegraph explore "Trace input from Game through step_world and step_player, and trace solver replay through the same functions"
```

Expected: both paths converge on `step_world` and `step_player`; no `LevelRepository` path exists.

- [ ] **Step 6: Commit cleanup**

```bash
rtk git add -A CMakeLists.txt include src tests assets/levels docs/LEVEL_DESIGN.md
rtk git commit -m "ref(core): Remove legacy room campaign"
```

---

### Task 10: Core Acceptance Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/LEVEL_DESIGN.md`

**Interfaces:**
- Consumes: completed continuous campaign core.
- Produces: documented build, solve, replay, controls, campaign format, and fall behavior.

- [ ] **Step 1: Document the current playable behavior**

Add exact commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/jumpcastle_solver --level assets/levels/campaign.level --campaign \
  --trace build/campaign-route.json
./build/jumpcastle_solver --level assets/levels/campaign.level \
  --verify-trace build/campaign-route.json
./build/jumpcastle
```

Document arrows/A-D for ground positioning, Space hold/release, `I` for debug, and the absence of
air control/checkpoints.

- [ ] **Step 2: Run the complete core acceptance matrix**

Run:

```bash
rtk cmake --build out/campaign-core-clean -j2
rtk ctest --test-dir out/campaign-core-clean --output-on-failure
rtk ./out/campaign-core-clean/jumpcastle_solver --level assets/levels/campaign.level \
  --campaign --trace out/campaign-core-clean/campaign-route.json
rtk ./out/campaign-core-clean/jumpcastle_solver --level assets/levels/campaign.level \
  --verify-trace out/campaign-core-clean/campaign-route.json
rtk git diff --check
```

Expected: all commands return zero; solver reports all 18 screens and max charge at or below 90%.

- [ ] **Step 3: Manually verify the risk paths**

Launch the game and verify: charge/release, no air steering, wall and ceiling rebound, upward camera
transition, multi-screen fall, bottom reset, resize/letterbox, completion, restart, and debug overlay.

- [ ] **Step 4: Commit documentation**

```bash
rtk git add README.md docs/LEVEL_DESIGN.md
rtk git commit -m "docs: Document continuous campaign workflow"
```

- [ ] **Step 5: Record the handoff to the visual plan**

Run: `rtk git status --short`

Expected: only the pre-existing brainstorm session files remain untracked; no production or test
file is dirty. Continue with `2026-07-18-cc0-assets-rendering.md`.
