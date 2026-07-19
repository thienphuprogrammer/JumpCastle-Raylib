# Tiled Tile Painting — P2: Renderer Draws Painted Tiles — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the game render a screen's painted `terrain` tile layer (schema v2 from P1) exactly as authored, falling back to the existing procedural polygon texturing when a screen has no tiles.

**Architecture:** Add a pure `tile_source_cell` (GID→atlas-cell + flip flags) in a new header so the tricky bit-math is unit-tested without raylib. Add `draw_tile_layer` in `renderer.cpp` that blits each non-empty cell from the atlas (`castle_texture_`) with no tint (true WYSIWYG). Route terrain drawing through a new `draw_terrain` helper (tiles when present, else `draw_world_polygons`) used by both `Renderer::draw` and `capture_screen`. Extract `draw_goal` so the goal sprite draws exactly once in every path.

**Tech Stack:** C++20, raylib 5.5 (`DrawTexturePro`), Catch2, CMake/CTest.

## Global Constraints

- GID `0` (after masking flip bits) = empty → skip. `firstgid == 1`, so tile index `local = (gid & 0x1FFFFFFF) - 1`.
- Flip bits: 0x80000000 = H, 0x40000000 = V, 0x20000000 = anti-diagonal (ignored in v1). H/V flips are realized as negative source width/height (raylib's convention, matching `sprite_source_rectangle`).
- Draw tiles with tint `WHITE` — no biome recolor (WYSIWYG).
- Source tile size = the atlas's tile size (`ScreenMap::tileset_tile_size`, 16); destination tile size = `config::tile_pixels`. They may differ — `DrawTexturePro` scales. Do not assume they are equal.
- Tile grid is screen-local: cell (col,row) → render-target px (col·`tile_pixels`, row·`tile_pixels`).
- Fallback is mandatory: a screen with empty `terrain` MUST render exactly as it does today.
- Commits allowed on `feat/tiled-tile-painting` (user-authorized). Conventional Commits. NO "Co-Authored-By"/attribution trailer. No push.

### Build / test notes
- Configure with tests ON: `cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON`
- Build tests: `cmake --build build/cmake --target jumpcastle_tests`
- Build game: `cmake --build build/cmake --target jumpcastle`
- Run a tag: `./build/cmake/jumpcastle_tests "[tile_view]"`
- If cmake/ctest output looks stale/empty it is the `rtk` proxy caching — re-run via `rtk proxy <cmd>` / `env` prefix, or redirect to a log and read it.

---

## File Structure

- `include/jumpcastle/tile_view.hpp` — NEW. Pure `TileCell` + `tile_source_cell(gid, columns, tile_size)`. No raylib include.
- `tests/tile_view_test.cpp` — NEW. Unit tests (tag `[tile_view]`). Register in the test target (see Task 1 Step 0).
- `src/renderer.cpp` — add `draw_goal`, `draw_tile_layer`, `draw_terrain`; refactor `draw_world_polygons` to call `draw_goal`; route `Renderer::draw` and `capture_screen` through `draw_terrain`; `#include "jumpcastle/tile_view.hpp"`.
- (No change to `map_format`, `campaign_world`, or the editor in P2. Editor tile preview is deferred — the in-game editor authors *collision*, so showing procedural polygons there is correct.)

---

### Task 1: Pure `tile_source_cell` + unit tests

**Files:**
- Create: `include/jumpcastle/tile_view.hpp`
- Create: `tests/tile_view_test.cpp`
- Maybe modify: `CMakeLists.txt` (only if test sources are listed explicitly, not globbed)

**Interfaces:**
- Produces: `struct TileCell { int src_x; int src_y; bool flip_h; bool flip_v; };` and
  `TileCell tile_source_cell(std::uint32_t gid, int columns, int tile_size) noexcept;`
  (caller guarantees the masked id ≥ 1; empty cells are skipped by the caller). Consumed by `draw_tile_layer` in Task 2.

- [ ] **Step 0: Learn how tests are registered**

Run: `grep -nE "tests/|jumpcastle_tests|GLOB.*test|add_executable" CMakeLists.txt`
- If test sources are a `file(GLOB ...)` or a wildcard, no edit is needed (a new file is picked up on reconfigure).
- If they are an explicit list, add `tests/tile_view_test.cpp` to that list.
Reconfigure after any CMake edit: `cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON`.

- [ ] **Step 1: Write the failing test**

Create `tests/tile_view_test.cpp`:

```cpp
#include "jumpcastle/tile_view.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("tile_source_cell maps a plain GID to an atlas cell", "[tile_view]") {
    // firstgid==1, 21-column atlas, 16px tiles.
    const TileCell a = tile_source_cell(1u, 21, 16);   // local 0 -> col0,row0
    CHECK(a.src_x == 0);
    CHECK(a.src_y == 0);
    CHECK_FALSE(a.flip_h);
    CHECK_FALSE(a.flip_v);

    const TileCell b = tile_source_cell(2u, 21, 16);   // local 1 -> col1,row0
    CHECK(b.src_x == 16);
    CHECK(b.src_y == 0);

    const TileCell c = tile_source_cell(22u, 21, 16);  // local 21 -> col0,row1
    CHECK(c.src_x == 0);
    CHECK(c.src_y == 16);
}

TEST_CASE("tile_source_cell strips and reports flip bits", "[tile_view]") {
    const TileCell h = tile_source_cell(0x80000000u | 1u, 21, 16);
    CHECK(h.flip_h);
    CHECK_FALSE(h.flip_v);
    CHECK(h.src_x == 0);   // flip bits do not shift the source cell
    CHECK(h.src_y == 0);

    const TileCell v = tile_source_cell(0x40000000u | 22u, 21, 16);
    CHECK(v.flip_v);
    CHECK_FALSE(v.flip_h);
    CHECK(v.src_x == 0);
    CHECK(v.src_y == 16);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "[tile_view]"`
Expected: FAIL to compile/link — `tile_view.hpp` / `tile_source_cell` do not exist yet.

- [ ] **Step 3: Write the header**

Create `include/jumpcastle/tile_view.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace jumpcastle {

// A resolved atlas cell for one painted tile: the top-left source pixel plus
// whether it is mirrored. Pure data so the GID bit-math is unit-testable
// without raylib.
struct TileCell {
    int src_x{};
    int src_y{};
    bool flip_h{};
    bool flip_v{};
};

// Resolve a Tiled GID to its atlas cell. `columns` is the atlas width in tiles,
// `tile_size` its tile pixel size. firstgid is 1 (single tileset), so the tile
// index is (gid without flip bits) - 1. The caller must skip empty cells
// (masked id 0); this assumes id >= 1.
[[nodiscard]] inline TileCell tile_source_cell(
    const std::uint32_t gid, const int columns, const int tile_size) noexcept {
    constexpr std::uint32_t flip_horizontal = 0x80000000u;
    constexpr std::uint32_t flip_vertical = 0x40000000u;
    constexpr std::uint32_t id_mask = 0x1FFFFFFFu;

    const bool flip_h = (gid & flip_horizontal) != 0u;
    const bool flip_v = (gid & flip_vertical) != 0u;
    const std::uint32_t local = (gid & id_mask) - 1u;  // 0-based; caller ensures id >= 1
    const int cols = columns > 0 ? columns : 1;
    const int col = static_cast<int>(local % static_cast<std::uint32_t>(cols));
    const int row = static_cast<int>(local / static_cast<std::uint32_t>(cols));
    return {col * tile_size, row * tile_size, flip_h, flip_v};
}

}  // namespace jumpcastle
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "[tile_view]"`
Expected: PASS (both cases).

- [ ] **Step 5: Commit** (user-authorized)

```bash
git add include/jumpcastle/tile_view.hpp tests/tile_view_test.cpp CMakeLists.txt
git commit -m "feat(render): add pure tile_source_cell GID resolver + tests"
```
(Only `git add CMakeLists.txt` if you edited it in Step 0.)

---

### Task 2: `draw_tile_layer` + route terrain through `draw_terrain`

**Files:**
- Modify: `src/renderer.cpp` (add include; add `draw_goal`, `draw_tile_layer`, `draw_terrain`; refactor `draw_world_polygons`; update `Renderer::draw` and `capture_screen`).

**Interfaces:**
- Consumes: `tile_source_cell` (Task 1), `ScreenMap::{terrain, tileset_columns, tileset_tile_size}` (P1), `CampaignWorld::screen_map` (existing).
- Produces: on-screen tile rendering. No new public API.

- [ ] **Step 1: Add the include**

At the top of `src/renderer.cpp` with the other project includes, add:

```cpp
#include "jumpcastle/tile_view.hpp"
```

- [ ] **Step 2: Extract `draw_goal` (behavior-preserving)**

In `src/renderer.cpp`, add this helper in the anonymous namespace immediately BEFORE `draw_world_polygons` (before line ~260):

```cpp
// Draws the goal exit sprite when the goal falls within the visible band.
// Extracted so both the tile path and the polygon fallback draw it once.
void draw_goal(
    const CampaignWorld& world,
    const CameraBand& camera,
    const BiomeAssets& assets,
    const Texture2D texture) {
    const Vec2 goal = world.goal;
    if (goal.y >= camera.world_top &&
        goal.y < camera.world_top + static_cast<float>(world.screen_height)) {
        draw_region(
            texture,
            assets.exit,
            {
                std::floor(goal.x) * config::tile_pixels,
                std::floor(goal.y - camera.world_top) * config::tile_pixels,
                static_cast<float>(config::tile_pixels),
                static_cast<float>(config::tile_pixels),
            });
    }
}
```

Then in `draw_world_polygons`, DELETE its inline goal block (the `const Vec2 goal = world.goal; if (...) { draw_region(... assets.exit ...); }` at the end, ~lines 284-296) and replace it with:

```cpp
    draw_goal(world, camera, assets, texture);
```

(Net behavior of `draw_world_polygons` is unchanged — the editor and the fallback keep drawing the goal.)

- [ ] **Step 3: Add `draw_tile_layer` and `draw_terrain`**

Add these in the anonymous namespace, after `draw_world_polygons`:

```cpp
// Blit each painted tile from the atlas at its screen-local cell, no tint
// (true WYSIWYG). Empty cells (masked id 0) are skipped. Flip bits become a
// negative source width/height, matching raylib's mirroring convention.
void draw_tile_layer(
    const TileLayer& layer,
    const int atlas_columns,
    const int atlas_tile_size,
    const Texture2D texture) {
    const float dst_tile = static_cast<float>(config::tile_pixels);
    const float src_tile = static_cast<float>(atlas_tile_size);
    for (int row = 0; row < layer.rows; ++row) {
        for (int col = 0; col < layer.columns; ++col) {
            const std::uint32_t gid =
                layer.gids[static_cast<std::size_t>(row) * layer.columns + col];
            if ((gid & 0x1FFFFFFFu) == 0u) { continue; }  // empty cell
            const TileCell cell = tile_source_cell(gid, atlas_columns, atlas_tile_size);
            const Rectangle source = {
                static_cast<float>(cell.src_x),
                static_cast<float>(cell.src_y),
                cell.flip_h ? -src_tile : src_tile,
                cell.flip_v ? -src_tile : src_tile,
            };
            const Rectangle destination = {
                static_cast<float>(col) * dst_tile,
                static_cast<float>(row) * dst_tile,
                dst_tile,
                dst_tile,
            };
            DrawTexturePro(texture, source, destination, {}, 0.0F, WHITE);
        }
    }
}

// Draw the terrain for the current band: painted tiles when the screen has
// them, otherwise the procedural polygon texturing. Goal sprite drawn once.
void draw_terrain(
    const CampaignWorld& world,
    const CameraBand& camera,
    const BiomeAssets& assets,
    const Texture2D texture) {
    const ScreenMap* screen = world.screen_map(camera.screen);
    if (screen != nullptr && !screen->terrain.gids.empty()) {
        draw_tile_layer(
            screen->terrain, screen->tileset_columns, screen->tileset_tile_size, texture);
        draw_goal(world, camera, assets, texture);
    } else {
        draw_world_polygons(world, camera, assets, texture);  // already draws the goal
    }
}
```

- [ ] **Step 4: Route `Renderer::draw` and `capture_screen` through `draw_terrain`**

In `Renderer::draw` (line ~446) replace:
```cpp
    draw_world_polygons(world, camera, assets, biome_texture);
```
with:
```cpp
    draw_terrain(world, camera, assets, biome_texture);
```

In `Renderer::capture_screen` (line ~493) replace the identical `draw_world_polygons(...)` call with:
```cpp
    draw_terrain(world, camera, assets, biome_texture);
```

(Leave `draw_editor`'s `draw_world_polygons(authored, ...)` call unchanged — the editor previews collision polygons.)

- [ ] **Step 5: Build the game and the tests**

Run: `cmake --build build/cmake --target jumpcastle jumpcastle_tests`
Expected: both link cleanly. Run `./build/cmake/jumpcastle_tests` — full suite still green (80/80 from P1 + 2 new `[tile_view]` cases = 82).

- [ ] **Step 6: Commit** (user-authorized)

```bash
git add src/renderer.cpp
git commit -m "feat(render): draw painted terrain tile layer with procedural fallback"
```

---

### Task 3: Visual smoke — confirm painted tiles render where authored

**Files:**
- Temporary edit (reverted at the end): `assets/levels/screens/screen-00.map.json`

**Interfaces:** none (verification only).

- [ ] **Step 1: Author a tiny painted patch on screen 0**

Back up and edit `assets/levels/screens/screen-00.map.json`: bump `schema_version` to `2`, add a `tileset` block, and add a `tiles.terrain` grid sized exactly `screen.width × screen.height` that is all `0` except a short horizontal run of a solid brick GID (use `2` — a mid brick — for ~5 cells on the bottom row). Keep `colliders`/`entities` as they are. Example shape (dimensions must match this screen's actual width/height):

```jsonc
"schema_version": 2,
"tileset": { "name": "castle", "columns": 21, "tile_size": 16 },
"tiles": { "terrain": [ /* height rows, each width entries; mostly 0, a few 2s near the bottom */ ] },
```

- [ ] **Step 2: Capture the screen headlessly**

Build and run the existing smoke capture path (it uses `capture_screen`, now tile-aware). Check how it is invoked:
Run: `grep -nE "smoke|capture|render_smoke" src/main.cpp`
Then run the game's smoke/capture mode per that invocation (e.g. `./build/cmake/jumpcastle --smoke <outdir>` or the documented flag) to produce a PNG of screen 0.

- [ ] **Step 3: Verify visually**

Read the produced screen-0 PNG. Expected: the authored brick run appears as real atlas bricks at the painted cells (not the flat procedural fill), and the rest of the screen renders normally. If the bricks are offset or wrong, the atlas `columns`/`tile_size` or the cell math is off — fix before proceeding.

- [ ] **Step 4: Revert the temporary map edit**

Run: `git checkout -- assets/levels/screens/screen-00.map.json`
Confirm: `git status --short` shows no change to that file.

- [ ] **Step 5: Commit** (nothing to commit for Task 3 — verification only; the temp edit was reverted). Record the visual result in the report file instead.

---

## Self-Review

**Spec coverage (design spec §7):**
- Pure `tile_source_cell` (unit-testable GID/flip math) → Task 1. ✅
- `draw_tile_layer`, no tint, flip via negative src → Task 2. ✅
- Route into `draw` + `capture_screen`; fallback preserved → Task 2. ✅
- Uses `castle_texture_` (the atlas) — passed as `biome_texture` → Task 2. ✅
- Screen tiles via `screen_map(camera.screen)->terrain` (chose reuse over a new `tiles_for_screen` — simpler, DRY; noted deviation) → Task 2. ✅
- Visible-tiles verification → Task 3. ✅
- Deferred (documented): editor tile preview (editor authors collision, not tiles); per-biome atlas selection (P0/§13).

**Placeholder scan:** Task 3's `tiles.terrain` array is described by shape, not literal values, because it must match screen 0's real dimensions (discovered at execution) — this is a data-authoring step, not code; the constraints (size == width×height, mostly 0, a run of GID 2) are exact.

**Type consistency:** `TileCell{src_x,src_y,flip_h,flip_v}` and `tile_source_cell(gid, columns, tile_size)` identical across Task 1 (def/tests) and Task 2 (call in `draw_tile_layer`). `draw_terrain`/`draw_goal`/`draw_tile_layer` signatures consistent with their call sites in `Renderer::draw` and `capture_screen`.

---

## Execution Handoff

Subagent-driven (chosen): one implementer for the whole phase (cohesive: one header + renderer edits + tests + visual smoke), then an independent review of the diff. Commits per task on `feat/tiled-tile-painting`.
