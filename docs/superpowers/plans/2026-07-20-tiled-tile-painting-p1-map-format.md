# Tiled Tile Painting — P1: Map Format v2 + Fallback — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the screen-map format to schema v2 — an optional painted `tiles.terrain` GID grid + a `tileset` block on `ScreenMap` — while v1 files keep parsing unchanged (empty tiles → procedural fallback later).

**Architecture:** Additive change to `src/map_format.cpp` / `include/jumpcastle/map_format.hpp` only. `parse_screen_map` accepts `schema_version` 1 or 2 and reads optional `tiles`/`tileset`; `serialize_screen_map` emits v2 (with `tiles`/`tileset`) only when a tile layer is present, so colliders-only output stays byte-identical. No renderer/converter changes in P1 (those are P2/P3). This is the foundation every later phase builds on.

**Tech Stack:** C++20, nlohmann/json (`using Json = nlohmann::json`), Catch2 (`catch2/catch_test_macros.hpp`), CMake/CTest.

## Global Constraints

- Tile GIDs are `std::uint32_t`; `0` = empty. Top 3 bits are Tiled flip flags (0x80000000 H, 0x40000000 V, 0x20000000 anti-diagonal) — preserve them verbatim in P1 (masking happens in the P2 renderer).
- `tiles.terrain` is a `height`-row × `width`-column array (row-major, y-down). Row count MUST equal `screen.height`; each row length MUST equal `screen.width`.
- `schema_version` accepted: `1` or `2`. Serialize emits `2` only when `terrain.gids` is non-empty, else `1` (keeps existing colliders-only files/tests byte-stable).
- Do NOT commit unless the user asks (project rule). Steps below include commits per superpowers TDD convention; if executing for this user, stage but hold the commit for their go-ahead, or commit only when they have said to.

### Build / test notes (read before running)
- Configure once with tests ON (they default OFF and a stale `jumpcastle_tests` binary otherwise runs old tests):
  `cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON`
- Build tests: `cmake --build build/cmake --target jumpcastle_tests`
- Run: `./build/cmake/jumpcastle_tests "[map_format]"` (Catch2 tag filter).
- `rtk` can serve stale cmake/ctest output — if results look wrong, re-run via `rtk proxy <cmd>` or an `env` prefix, or write to a log and read it.

---

## File Structure

- `include/jumpcastle/map_format.hpp` — add `TileLayer` struct + three `ScreenMap` fields. Add `#include <cstdint>`.
- `src/map_format.cpp` — add `parse_tile_layer` helper; extend `parse_screen_map` (version check + optional tiles/tileset); extend `serialize_screen_map` (conditional v2 emit).
- `tests/map_format_test.cpp` — add v2 parse, v1 fallback, round-trip, and validation cases (tag `[map_format]`).

---

### Task 1: `TileLayer` type + `ScreenMap` fields

**Files:**
- Modify: `include/jumpcastle/map_format.hpp:1-34`

**Interfaces:**
- Produces: `struct TileLayer { int columns; int rows; std::vector<std::uint32_t> gids; };` and `ScreenMap::terrain` (TileLayer), `ScreenMap::tileset_columns` (int, default 21), `ScreenMap::tileset_tile_size` (int, default 16). Consumed by all later tasks and by P2/P3.

- [ ] **Step 1: Add the include and types**

In `include/jumpcastle/map_format.hpp`, add `#include <cstdint>` to the include block, then insert `TileLayer` above `ScreenMap` and extend `ScreenMap`:

```cpp
struct TileLayer {
    int columns{};
    int rows{};
    std::vector<std::uint32_t> gids;  // row-major, size == columns*rows; 0 == empty
};

struct ScreenMap {
    int index{};
    float width{};
    float height{};
    std::string biome;
    std::vector<ConvexPolygon> polygons;
    std::vector<MapEntity> entities;
    TileLayer terrain{};          // empty when the map is v1 / has no painted tiles
    int tileset_columns{21};      // tiles per atlas row; renderer re-derives from the texture
    int tileset_tile_size{16};    // px per tile
};
```

- [ ] **Step 2: Verify it compiles**

Run: `cmake --build build/cmake --target jumpcastle_tests`
Expected: PASS (compiles; no behavior change yet).

- [ ] **Step 3: Commit** (hold if user hasn't authorized commits)

```bash
git add include/jumpcastle/map_format.hpp
git commit -m "feat(map): add TileLayer + tileset fields to ScreenMap"
```

---

### Task 2: Parse a v2 `tiles.terrain` grid

**Files:**
- Modify: `src/map_format.cpp:97-129` (parse_screen_map) and add `parse_tile_layer` in the anonymous namespace (near `parse_entity`, before line 95).
- Test: `tests/map_format_test.cpp`

**Interfaces:**
- Consumes: `TileLayer`, `ScreenMap` fields from Task 1.
- Produces: `parse_screen_map` populates `ScreenMap::terrain`, `::tileset_columns`, `::tileset_tile_size` from v2 JSON.

- [ ] **Step 1: Write the failing test**

Add to `tests/map_format_test.cpp`:

```cpp
TEST_CASE("parse reads a v2 tiles.terrain grid", "[map_format]") {
    const std::string json = R"({
        "schema_version": 2,
        "screen": {"index": 1, "width": 3, "height": 2},
        "biome": "courtyard",
        "tileset": {"name": "castle", "columns": 21, "tile_size": 16},
        "tiles": {"terrain": [[0, 1, 0], [2, 0, 3]]},
        "colliders": [],
        "entities": []
    })";
    const jumpcastle::ScreenMap map = jumpcastle::parse_screen_map(json, "v2");
    CHECK(map.terrain.columns == 3);
    CHECK(map.terrain.rows == 2);
    REQUIRE(map.terrain.gids.size() == 6);
    CHECK(map.terrain.gids[1] == 1u);   // row 0, col 1
    CHECK(map.terrain.gids[3] == 2u);   // row 1, col 0
    CHECK(map.terrain.gids[5] == 3u);   // row 1, col 2
    CHECK(map.tileset_columns == 21);
    CHECK(map.tileset_tile_size == 16);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "parse reads a v2 tiles.terrain grid"`
Expected: FAIL — currently `schema_version != 1` throws "unsupported map schema_version".

- [ ] **Step 3: Add the `parse_tile_layer` helper**

In `src/map_format.cpp`, inside the anonymous namespace (before the closing `}  // namespace` at line 95):

```cpp
TileLayer parse_tile_layer(
    const Json& grid, const int width, const int height, const std::string& label) {
    if (!grid.is_array()) {
        throw std::runtime_error(label + " must be an array of rows");
    }
    if (static_cast<int>(grid.size()) != height) {
        throw std::runtime_error(label + " row count must equal screen height");
    }
    TileLayer layer;
    layer.columns = width;
    layer.rows = height;
    layer.gids.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t r = 0; r < grid.size(); ++r) {
        const Json& row = grid.at(r);
        if (!row.is_array() || static_cast<int>(row.size()) != width) {
            throw std::runtime_error(
                label + " row " + std::to_string(r) + " length must equal screen width");
        }
        for (const auto& cell : row) {
            layer.gids.push_back(cell.get<std::uint32_t>());
        }
    }
    return layer;
}
```

- [ ] **Step 4: Accept v2 and read optional tiles/tileset**

In `parse_screen_map`, replace the version guard (line 100-102):

```cpp
        const int version = required(root, "schema_version", "map").get<int>();
        if (version != 1 && version != 2) {
            throw std::runtime_error("unsupported map schema_version");
        }
```

Then, immediately before `return map;` (after the entities loop, ~line 124), add:

```cpp
        if (root.contains("tileset") && root.at("tileset").is_object()) {
            const Json& tileset = root.at("tileset");
            if (tileset.contains("columns")) {
                map.tileset_columns = tileset.at("columns").get<int>();
            }
            if (tileset.contains("tile_size")) {
                map.tileset_tile_size = tileset.at("tile_size").get<int>();
            }
        }
        if (root.contains("tiles") && root.at("tiles").is_object()) {
            const Json& tiles = root.at("tiles");
            if (tiles.contains("terrain")) {
                map.terrain = parse_tile_layer(
                    tiles.at("terrain"), static_cast<int>(map.width),
                    static_cast<int>(map.height), "map.tiles.terrain");
            }
        }
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "parse reads a v2 tiles.terrain grid"`
Expected: PASS.

- [ ] **Step 6: Commit** (hold if user hasn't authorized commits)

```bash
git add src/map_format.cpp tests/map_format_test.cpp
git commit -m "feat(map): parse v2 tiles.terrain and tileset block"
```

---

### Task 3: v1 files still parse (fallback → empty terrain)

**Files:**
- Test: `tests/map_format_test.cpp`

**Interfaces:**
- Consumes: `parse_screen_map` from Task 2.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("parse of a v1 map leaves terrain empty", "[map_format]") {
    const std::string json = R"({
        "schema_version": 1,
        "screen": {"index": 0, "width": 16, "height": 12},
        "biome": "courtyard",
        "colliders": [],
        "entities": []
    })";
    const jumpcastle::ScreenMap map = jumpcastle::parse_screen_map(json, "v1");
    CHECK(map.terrain.gids.empty());
    CHECK(map.terrain.columns == 0);
    CHECK(map.terrain.rows == 0);
}
```

- [ ] **Step 2: Run test**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "parse of a v1 map leaves terrain empty"`
Expected: PASS immediately (Task 2 made `tiles`/`tileset` optional; this asserts the fallback contract holds — a regression guard, no new code).

- [ ] **Step 3: Commit** (hold if user hasn't authorized commits)

```bash
git add tests/map_format_test.cpp
git commit -m "test(map): guard v1 fallback leaves terrain empty"
```

---

### Task 4: Serialize v2 and round-trip losslessly

**Files:**
- Modify: `src/map_format.cpp:141-172` (serialize_screen_map)
- Test: `tests/map_format_test.cpp`

**Interfaces:**
- Consumes: `TileLayer`, `serialize_screen_map`, `parse_screen_map`.
- Produces: `serialize_screen_map` emits `schema_version: 2` + `tiles.terrain` + `tileset` when `terrain.gids` is non-empty; else unchanged v1 output.

- [ ] **Step 1: Write the failing test (round-trip incl. a flipped GID)**

```cpp
TEST_CASE("serialize round-trips a terrain layer incl. flip bits", "[map_format]") {
    jumpcastle::ScreenMap map;
    map.index = 4;
    map.width = 2;
    map.height = 2;
    map.biome = "frosted_keep";
    map.tileset_columns = 21;
    map.tileset_tile_size = 16;
    map.terrain.columns = 2;
    map.terrain.rows = 2;
    map.terrain.gids = {0u, 5u, 0x80000000u | 6u, 3u};  // one horizontally-flipped GID

    const std::string json = jumpcastle::serialize_screen_map(map);
    const jumpcastle::ScreenMap back = jumpcastle::parse_screen_map(json, "roundtrip");

    CHECK(back.terrain.columns == 2);
    CHECK(back.terrain.rows == 2);
    REQUIRE(back.terrain.gids.size() == 4);
    CHECK(back.terrain.gids[2] == (0x80000000u | 6u));  // flip bit preserved
    CHECK(back.terrain.gids[1] == 5u);
    CHECK(back.tileset_columns == 21);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "serialize round-trips a terrain layer incl. flip bits"`
Expected: FAIL — `serialize_screen_map` writes `schema_version: 1` and no `tiles`, so `back.terrain` is empty.

- [ ] **Step 3: Emit v2 when a tile layer is present**

In `serialize_screen_map`, replace `root["schema_version"] = 1;` (line 143) with:

```cpp
    root["schema_version"] = map.terrain.gids.empty() ? 1 : 2;
```

Then, before `return root.dump(2);` (line 171), add:

```cpp
    if (!map.terrain.gids.empty()) {
        root["tileset"] = {
            {"name", "castle"},
            {"columns", map.tileset_columns},
            {"tile_size", map.tileset_tile_size},
        };
        Json terrain = Json::array();
        for (int r = 0; r < map.terrain.rows; ++r) {
            Json row = Json::array();
            for (int c = 0; c < map.terrain.columns; ++c) {
                row.push_back(
                    map.terrain.gids[static_cast<std::size_t>(r) * map.terrain.columns + c]);
            }
            terrain.push_back(row);
        }
        root["tiles"] = {{"terrain", terrain}};
    }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "serialize round-trips a terrain layer incl. flip bits"`
Expected: PASS.

- [ ] **Step 5: Commit** (hold if user hasn't authorized commits)

```bash
git add src/map_format.cpp tests/map_format_test.cpp
git commit -m "feat(map): serialize v2 tiles.terrain with lossless round-trip"
```

---

### Task 5: Reject a malformed grid (size ≠ width×height)

**Files:**
- Test: `tests/map_format_test.cpp`

**Interfaces:**
- Consumes: `parse_screen_map`, `parse_tile_layer` (from Task 2).

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("parse rejects a terrain grid with the wrong dimensions", "[map_format]") {
    const std::string json = R"({
        "schema_version": 2,
        "screen": {"index": 0, "width": 3, "height": 2},
        "biome": "courtyard",
        "tiles": {"terrain": [[0, 1, 0]]},
        "colliders": [],
        "entities": []
    })";
    CHECK_THROWS_AS(
        jumpcastle::parse_screen_map(json, "bad-grid"), std::runtime_error);
}
```

- [ ] **Step 2: Run test**

Run: `cmake --build build/cmake --target jumpcastle_tests && ./build/cmake/jumpcastle_tests "parse rejects a terrain grid with the wrong dimensions"`
Expected: PASS immediately — `parse_tile_layer` (Task 2) already throws on row-count mismatch (1 row ≠ height 2). This locks the validation contract.

- [ ] **Step 3: Run the full map_format suite (no regressions)**

Run: `./build/cmake/jumpcastle_tests "[map_format]"`
Expected: all `[map_format]` cases PASS (existing colliders-only serialize output is byte-identical because terrain is empty → v1 emit).

- [ ] **Step 4: Commit** (hold if user hasn't authorized commits)

```bash
git add tests/map_format_test.cpp
git commit -m "test(map): reject malformed terrain grid dimensions"
```

---

## Self-Review

**Spec coverage (§6 of the design spec):**
- `TileLayer` + `ScreenMap` fields → Task 1. ✅
- Parse `tiles.terrain` + `tileset`, accept v1/v2 → Task 2. ✅
- v1 fallback (empty terrain) → Task 3. ✅
- Serialize v2 + lossless round-trip, flip bits preserved → Task 4. ✅
- Validation (grid size == width×height) → Task 5. ✅
- Out of P1 scope (later plans): renderer `draw_tile_layer` / `tile_source_cell` (P2), converter tile layer + `.tsx` (P3), atlas enrichment (P0), seed-collision + migration (P4).

**Placeholder scan:** none — every code step shows full code and exact commands.

**Type consistency:** `TileLayer{columns,rows,gids}` and `ScreenMap::{terrain,tileset_columns,tileset_tile_size}` are used identically across Tasks 1–5. GID type `std::uint32_t` consistent. `parse_tile_layer(grid, width, height, label)` signature matches its one call site in Task 2.

---

## Execution Handoff

Two execution options once you approve:
1. **Subagent-Driven (recommended)** — a fresh subagent per task, review between tasks.
2. **Inline Execution** — run tasks in this session with checkpoints.

(Reminder: commits are gated on your say-so per project rule.)
