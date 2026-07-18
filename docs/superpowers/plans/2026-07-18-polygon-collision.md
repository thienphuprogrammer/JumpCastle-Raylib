# Polygon Collision + SAT + In-Game Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ASCII-grid map + tile collision with convex-polygon geometry resolved via SAT, plus an in-game editor to author it.

**Architecture:** World geometry is convex polygons (concave input split at load). The player stays an AABB; collision resolves by sub-stepping displacement and applying discrete SAT minimum-translation-vector (MTV) push-out per step, reusing the sub-stepping already in `resolve_world_collision`. Maps are JSON `.map.json`, one file per screen, stitched into the vertical tower by the existing `WorldMap`/`world_loader` machinery.

**Tech Stack:** C++20, raylib (rendering/input), nlohmann::json (already linked to `jumpcastle_core`), Catch2 v3.8.1 (tests, registered in `CMakeLists.txt`).

**Spec:** `docs/superpowers/specs/2026-07-18-polygon-collision-design.md`

## Global Constraints

- C++20; warnings-as-clean under `-Wall -Wextra -Wpedantic` (see `jumpcastle_enable_warnings`).
- New `.cpp` files MUST be added to the `jumpcastle_core` source list in `CMakeLists.txt` (lines ~38-49) and new tests to the `jumpcastle_tests` list (lines ~97-115).
- Coordinates are floats in **tile units**, origin top-left, **y-down** (matches `WorldMap`, `config::player_half_size`).
- Namespace `jumpcastle`. `Vec2` is defined in `include/jumpcastle/math.hpp` (has `+ - *`, `length`, `normalized`; NO dot/cross yet).
- Keep the game buildable and tests green after every task.
- Commit after every task (frequent commits).

---

## Stage 1 — Pure collision math (`sat`, `convex`)

### Task 1: Vec2 dot product + Aabb type

**Files:**
- Modify: `include/jumpcastle/math.hpp`
- Test: `tests/math_test.cpp` (append)

**Interfaces:**
- Produces: `constexpr float dot(Vec2, Vec2)`, `struct Aabb { Vec2 min; Vec2 max; }`, `Vec2 aabb_center(Aabb)`, `Vec2 aabb_half(Aabb)`.

- [ ] **Step 1: Write failing test** — append to `tests/math_test.cpp`:

```cpp
TEST_CASE("dot product and aabb helpers") {
    REQUIRE(jumpcastle::dot({1.0F, 2.0F}, {3.0F, 4.0F}) == Catch::Approx(11.0F));
    const jumpcastle::Aabb box{{0.0F, 0.0F}, {2.0F, 4.0F}};
    REQUIRE(jumpcastle::aabb_center(box) == jumpcastle::Vec2{1.0F, 2.0F});
    REQUIRE(jumpcastle::aabb_half(box) == jumpcastle::Vec2{1.0F, 2.0F});
}
```
(Ensure `#include <catch2/catch_approx.hpp>` is present in the file.)

- [ ] **Step 2: Run to verify fail** — `cmake --build build --target jumpcastle_tests` → FAIL (`dot` not declared).

- [ ] **Step 3: Implement** — add to `include/jumpcastle/math.hpp` inside `namespace jumpcastle`, before the closing brace:

```cpp
[[nodiscard]] constexpr float dot(const Vec2 a, const Vec2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}

struct Aabb {
    Vec2 min{};
    Vec2 max{};
};

[[nodiscard]] constexpr Vec2 aabb_center(const Aabb box) noexcept {
    return {(box.min.x + box.max.x) * 0.5F, (box.min.y + box.max.y) * 0.5F};
}

[[nodiscard]] constexpr Vec2 aabb_half(const Aabb box) noexcept {
    return {(box.max.x - box.min.x) * 0.5F, (box.max.y - box.min.y) * 0.5F};
}
```

- [ ] **Step 4: Run to verify pass** — `ctest --test-dir build -R math` → PASS.

- [ ] **Step 5: Commit** — `git add include/jumpcastle/math.hpp tests/math_test.cpp && git commit -m "feat(math): add dot product and Aabb helpers"`

---

### Task 2: Convex polygon utilities (`convex`)

**Files:**
- Create: `include/jumpcastle/convex.hpp`, `src/convex.cpp`
- Test: `tests/convex_test.cpp`
- Modify: `CMakeLists.txt` (add `src/convex.cpp` to core, `tests/convex_test.cpp` to tests)

**Interfaces:**
- Consumes: `Vec2`, `Aabb`, `dot` (Task 1).
- Produces:
  - `Vec2 polygon_centroid(const std::vector<Vec2>&)`
  - `std::vector<Vec2> outward_edge_normals(const std::vector<Vec2>&)` — one unit normal per edge `i -> i+1`, pointing away from the centroid (winding-agnostic).
  - `bool is_convex(const std::vector<Vec2>&)` — true iff >=3 pts and all turn the same way.
  - `Aabb polygon_aabb(const std::vector<Vec2>&)`
  - `std::vector<std::vector<Vec2>> split_to_convex(const std::vector<Vec2>&)` — returns `{polygon}` if already convex, else ear-clipped triangles.

- [ ] **Step 1: Write failing tests** — `tests/convex_test.cpp`:

```cpp
#include "jumpcastle/convex.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace jumpcastle;

TEST_CASE("square is convex with unit outward normals") {
    const std::vector<Vec2> square{{0,0},{2,0},{2,2},{0,2}};
    REQUIRE(is_convex(square));
    const auto normals = outward_edge_normals(square);
    REQUIRE(normals.size() == 4);
    for (const Vec2 n : normals) {
        REQUIRE(length(n) == Catch::Approx(1.0F));
    }
}

TEST_CASE("polygon_aabb bounds all points") {
    const Aabb box = polygon_aabb({{1,1},{4,2},{2,5}});
    REQUIRE(box.min == Vec2{1,1});
    REQUIRE(box.max == Vec2{4,5});
}

TEST_CASE("concave polygon is detected and split into convex pieces") {
    const std::vector<Vec2> dart{{0,0},{4,2},{0,4},{1,2}};
    REQUIRE_FALSE(is_convex(dart));
    const auto pieces = split_to_convex(dart);
    REQUIRE(pieces.size() >= 2);
    for (const auto& piece : pieces) {
        REQUIRE(is_convex(piece));
    }
}

TEST_CASE("already-convex polygon is not split") {
    const std::vector<Vec2> tri{{0,0},{2,0},{1,2}};
    REQUIRE(split_to_convex(tri).size() == 1);
}
```

- [ ] **Step 2: Run to verify fail** — build `jumpcastle_tests` → FAIL (no `convex.hpp`).

- [ ] **Step 3: Implement header** — `include/jumpcastle/convex.hpp`:

```cpp
#pragma once
#include "jumpcastle/math.hpp"
#include <vector>

namespace jumpcastle {

[[nodiscard]] Vec2 polygon_centroid(const std::vector<Vec2>& points);
[[nodiscard]] std::vector<Vec2> outward_edge_normals(const std::vector<Vec2>& points);
[[nodiscard]] bool is_convex(const std::vector<Vec2>& points);
[[nodiscard]] Aabb polygon_aabb(const std::vector<Vec2>& points);
[[nodiscard]] std::vector<std::vector<Vec2>> split_to_convex(const std::vector<Vec2>& points);

}  // namespace jumpcastle
```

- [ ] **Step 4: Implement source** — `src/convex.cpp` (add `#include <algorithm>`, `#include <cmath>`):

```cpp
#include "jumpcastle/convex.hpp"
#include <algorithm>
#include <cmath>

namespace jumpcastle {
namespace {
float cross(const Vec2 a, const Vec2 b) noexcept { return a.x * b.y - a.y * b.x; }
}  // namespace

Vec2 polygon_centroid(const std::vector<Vec2>& points) {
    Vec2 sum{};
    for (const Vec2 p : points) { sum = sum + p; }
    const float n = static_cast<float>(points.size());
    return n == 0.0F ? Vec2{} : sum * (1.0F / n);
}

std::vector<Vec2> outward_edge_normals(const std::vector<Vec2>& points) {
    const Vec2 center = polygon_centroid(points);
    std::vector<Vec2> normals;
    normals.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec2 a = points[i];
        const Vec2 b = points[(i + 1) % points.size()];
        const Vec2 edge = b - a;
        Vec2 normal = normalized({edge.y, -edge.x});
        const Vec2 mid = (a + b) * 0.5F;
        if (dot(normal, mid - center) < 0.0F) { normal = normal * -1.0F; }
        normals.push_back(normal);
    }
    return normals;
}

bool is_convex(const std::vector<Vec2>& points) {
    if (points.size() < 3) { return false; }
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec2 a = points[i];
        const Vec2 b = points[(i + 1) % points.size()];
        const Vec2 c = points[(i + 2) % points.size()];
        const float turn = cross(b - a, c - b);
        if (turn > 0.0F) { positive = true; }
        if (turn < 0.0F) { negative = true; }
        if (positive && negative) { return false; }
    }
    return true;
}

Aabb polygon_aabb(const std::vector<Vec2>& points) {
    Aabb box{points.front(), points.front()};
    for (const Vec2 p : points) {
        box.min.x = std::min(box.min.x, p.x);
        box.min.y = std::min(box.min.y, p.y);
        box.max.x = std::max(box.max.x, p.x);
        box.max.y = std::max(box.max.y, p.y);
    }
    return box;
}

std::vector<std::vector<Vec2>> split_to_convex(const std::vector<Vec2>& points) {
    if (is_convex(points)) { return {points}; }
    std::vector<Vec2> poly = points;
    float area = 0.0F;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        area += cross(poly[i], poly[(i + 1) % poly.size()]);
    }
    if (area < 0.0F) { std::reverse(poly.begin(), poly.end()); }

    const auto in_triangle = [](Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
        const float d1 = cross(b - a, p - a);
        const float d2 = cross(c - b, p - b);
        const float d3 = cross(a - c, p - c);
        const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(neg && pos);
    };

    std::vector<std::vector<Vec2>> triangles;
    std::vector<std::size_t> idx(poly.size());
    for (std::size_t i = 0; i < poly.size(); ++i) { idx[i] = i; }
    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        for (std::size_t i = 0; i < idx.size(); ++i) {
            const Vec2 a = poly[idx[(i + idx.size() - 1) % idx.size()]];
            const Vec2 b = poly[idx[i]];
            const Vec2 c = poly[idx[(i + 1) % idx.size()]];
            if (cross(b - a, c - b) <= 0.0F) { continue; }  // reflex, not an ear
            bool contains = false;
            for (const std::size_t j : idx) {
                const Vec2 p = poly[j];
                if (p == a || p == b || p == c) { continue; }
                if (in_triangle(p, a, b, c)) { contains = true; break; }
            }
            if (contains) { continue; }
            triangles.push_back({a, b, c});
            idx.erase(idx.begin() + static_cast<long>(i));
            clipped = true;
            break;
        }
        if (!clipped) { break; }
    }
    if (idx.size() == 3) {
        triangles.push_back({poly[idx[0]], poly[idx[1]], poly[idx[2]]});
    }
    return triangles;
}

}  // namespace jumpcastle
```

- [ ] **Step 5: Register in CMake** — add `src/convex.cpp` to the `jumpcastle_core` source list and `tests/convex_test.cpp` to the `jumpcastle_tests` list.

- [ ] **Step 6: Run to verify pass** — `cmake --build build && ctest --test-dir build -R convex` → PASS.

- [ ] **Step 7: Commit** — `git add include/jumpcastle/convex.hpp src/convex.cpp tests/convex_test.cpp CMakeLists.txt && git commit -m "feat(collision): add convex polygon utilities"`

---

### Task 3: SAT AABB-vs-convex (`sat`)

**Files:**
- Create: `include/jumpcastle/sat.hpp`, `src/sat.cpp`
- Test: `tests/sat_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Vec2`, `Aabb`, `dot` (Task 1), `outward_edge_normals` (Task 2).
- Produces:
  - `struct Mtv { bool overlapping; Vec2 normal; float depth; };` — `normal` points from the polygon toward the box (push-out direction); `depth` >= 0.
  - `Mtv aabb_vs_convex(const Aabb& box, const std::vector<Vec2>& points, const std::vector<Vec2>& edge_normals)`.

- [ ] **Step 1: Write failing tests** — `tests/sat_test.cpp`:

```cpp
#include "jumpcastle/sat.hpp"
#include "jumpcastle/convex.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace jumpcastle;

namespace {
Mtv test_overlap(const Aabb& box, const std::vector<Vec2>& poly) {
    return aabb_vs_convex(box, poly, outward_edge_normals(poly));
}
}

TEST_CASE("box resting slightly inside a floor is pushed up") {
    const std::vector<Vec2> floor{{0,10},{16,10},{16,12},{0,12}};
    const Aabb box{{5.0F, 9.6F}, {5.6F, 10.1F}};  // overlaps floor top by 0.1 (y-down)
    const Mtv mtv = test_overlap(box, floor);
    REQUIRE(mtv.overlapping);
    REQUIRE(mtv.normal.y < 0.0F);            // push up (y-down => negative y is up)
    REQUIRE(mtv.depth == Catch::Approx(0.1F).margin(1e-4));
}

TEST_CASE("separated box reports no overlap") {
    const std::vector<Vec2> floor{{0,10},{16,10},{16,12},{0,12}};
    const Aabb box{{5.0F, 8.0F}, {5.6F, 9.0F}};  // gap of 1.0 above the floor
    REQUIRE_FALSE(test_overlap(box, floor).overlapping);
}

TEST_CASE("box against a wall is pushed sideways") {
    const std::vector<Vec2> wall{{10,0},{12,0},{12,15},{10,15}};
    const Aabb box{{9.7F, 5.0F}, {10.3F, 5.6F}};  // penetrates wall left face by 0.3
    const Mtv mtv = test_overlap(box, wall);
    REQUIRE(mtv.overlapping);
    REQUIRE(mtv.normal.x < 0.0F);            // pushed left, away from wall
    REQUIRE(mtv.depth == Catch::Approx(0.3F).margin(1e-4));
}
```

- [ ] **Step 2: Run to verify fail** — build → FAIL (no `sat.hpp`).

- [ ] **Step 3: Implement header** — `include/jumpcastle/sat.hpp`:

```cpp
#pragma once
#include "jumpcastle/math.hpp"
#include <vector>

namespace jumpcastle {

struct Mtv {
    bool overlapping{};
    Vec2 normal{};   // unit; points from polygon toward the box (push-out direction)
    float depth{};   // penetration depth along normal, >= 0
};

// SAT overlap of an axis-aligned box against a convex polygon.
// edge_normals must be outward_edge_normals(points).
[[nodiscard]] Mtv aabb_vs_convex(
    const Aabb& box,
    const std::vector<Vec2>& points,
    const std::vector<Vec2>& edge_normals);

}  // namespace jumpcastle
```

- [ ] **Step 4: Implement source** — `src/sat.cpp` (add `#include <algorithm>`, `#include <cmath>`, `#include <limits>`):

```cpp
#include "jumpcastle/sat.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace jumpcastle {
namespace {

struct Interval { float min; float max; };

Interval project_box(const Aabb& box, const Vec2 axis) {
    const Vec2 c = aabb_center(box);
    const Vec2 h = aabb_half(box);
    const float center = dot(c, axis);
    const float radius = std::abs(axis.x) * h.x + std::abs(axis.y) * h.y;
    return {center - radius, center + radius};
}

Interval project_polygon(const std::vector<Vec2>& points, const Vec2 axis) {
    float lo = std::numeric_limits<float>::max();
    float hi = -std::numeric_limits<float>::max();
    for (const Vec2 p : points) {
        const float d = dot(p, axis);
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    return {lo, hi};
}

Vec2 centroid(const std::vector<Vec2>& points) {
    Vec2 s{};
    for (const Vec2 p : points) { s = s + p; }
    return s * (1.0F / static_cast<float>(points.size()));
}

}  // namespace

Mtv aabb_vs_convex(
    const Aabb& box,
    const std::vector<Vec2>& points,
    const std::vector<Vec2>& edge_normals) {
    std::vector<Vec2> axes = edge_normals;
    axes.push_back({1.0F, 0.0F});
    axes.push_back({0.0F, 1.0F});

    Mtv result{};
    result.depth = std::numeric_limits<float>::max();
    const Vec2 box_center = aabb_center(box);
    const Vec2 poly_center = centroid(points);

    for (const Vec2 axis : axes) {
        const Interval a = project_box(box, axis);
        const Interval b = project_polygon(points, axis);
        if (a.max <= b.min || b.max <= a.min) {
            return Mtv{};  // separating axis -> no overlap
        }
        const float overlap = std::min(a.max, b.max) - std::max(a.min, b.min);
        if (overlap < result.depth) {
            result.depth = overlap;
            Vec2 n = axis;
            if (dot(box_center - poly_center, n) < 0.0F) { n = n * -1.0F; }
            result.normal = n;
        }
    }
    result.overlapping = true;
    return result;
}

}  // namespace jumpcastle
```

- [ ] **Step 5: Register in CMake** — add `src/sat.cpp` to core and `tests/sat_test.cpp` to tests.

- [ ] **Step 6: Run to verify pass** — `ctest --test-dir build -R sat` → PASS.

- [ ] **Step 7: Commit** — `git commit -am "feat(collision): add SAT AABB-vs-convex MTV"`

---

## Stage 2 — Map format (`map_format`)

### Task 4: Collider/screen types + JSON parse/serialize

**Files:**
- Create: `include/jumpcastle/map_format.hpp`, `src/map_format.cpp`
- Test: `tests/map_format_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Vec2`, `Aabb` (Task 1); `outward_edge_normals`, `polygon_aabb`, `is_convex`, `split_to_convex` (Task 2).
- Produces:
  - `enum class ColliderType { solid, oneway, hazard };`
  - `struct ConvexPolygon { std::vector<Vec2> points; std::vector<Vec2> edge_normals; Aabb aabb; ColliderType type; };`
  - `enum class EntityType { spawn, checkpoint, goal };`
  - `struct MapEntity { EntityType type; Vec2 pos; };`
  - `struct ScreenMap { int index; float width; float height; std::string biome; std::vector<ConvexPolygon> polygons; std::vector<MapEntity> entities; };`
  - `ScreenMap parse_screen_map(std::string_view json, std::string_view label);` — validates, splits concave, precomputes normals+aabb per convex piece. Throws `std::runtime_error` on invalid input.
  - `std::string serialize_screen_map(const ScreenMap&);`

- [ ] **Step 1: Write failing tests** — `tests/map_format_test.cpp`:

```cpp
#include "jumpcastle/map_format.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace jumpcastle;

constexpr const char* kValid = R"({
  "schema_version": 1,
  "screen": { "index": 5, "width": 16.0, "height": 15.0 },
  "biome": "courtyard",
  "colliders": [
    { "id": 1, "type": "solid",  "points": [[0,14],[16,14],[16,15],[0,15]] },
    { "id": 2, "type": "oneway", "points": [[4,10],[8,10],[8,10.5],[4,10.5]] },
    { "id": 3, "type": "hazard", "points": [[2,13],[3,13],[2.5,12]] }
  ],
  "entities": [ { "type": "spawn", "pos": [2.5, 13.0] } ]
})";

TEST_CASE("parse_screen_map reads colliders, types, entities") {
    const ScreenMap map = parse_screen_map(kValid, "test");
    REQUIRE(map.index == 5);
    REQUIRE(map.polygons.size() == 3);
    REQUIRE(map.polygons[0].type == ColliderType::solid);
    REQUIRE(map.polygons[1].type == ColliderType::oneway);
    REQUIRE(map.polygons[2].type == ColliderType::hazard);
    REQUIRE(map.polygons[0].edge_normals.size() == map.polygons[0].points.size());
    REQUIRE(map.entities.size() == 1);
    REQUIRE(map.entities[0].type == EntityType::spawn);
}

TEST_CASE("parse rejects a polygon with fewer than three points") {
    const char* bad = R"({"schema_version":1,"screen":{"index":0,"width":16,"height":15},
      "biome":"courtyard","colliders":[{"id":1,"type":"solid","points":[[0,0],[1,1]]}],"entities":[]})";
    REQUIRE_THROWS_AS(parse_screen_map(bad, "bad"), std::runtime_error);
}

TEST_CASE("concave collider is split into multiple convex polygons") {
    const char* concave = R"({"schema_version":1,"screen":{"index":0,"width":16,"height":15},
      "biome":"courtyard","colliders":[{"id":1,"type":"solid","points":[[0,0],[4,2],[0,4],[1,2]]}],"entities":[]})";
    const ScreenMap map = parse_screen_map(concave, "concave");
    REQUIRE(map.polygons.size() >= 2);
}

TEST_CASE("serialize then parse round-trips collider count and types") {
    const ScreenMap map = parse_screen_map(kValid, "test");
    const ScreenMap again = parse_screen_map(serialize_screen_map(map), "roundtrip");
    REQUIRE(again.polygons.size() == map.polygons.size());
    REQUIRE(again.entities.size() == map.entities.size());
    REQUIRE(again.index == map.index);
}
```

- [ ] **Step 2: Run to verify fail** — build → FAIL (no `map_format.hpp`).

- [ ] **Step 3: Implement header** — `include/jumpcastle/map_format.hpp`:

```cpp
#pragma once
#include "jumpcastle/math.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace jumpcastle {

enum class ColliderType { solid, oneway, hazard };
enum class EntityType { spawn, checkpoint, goal };

struct ConvexPolygon {
    std::vector<Vec2> points;
    std::vector<Vec2> edge_normals;
    Aabb aabb{};
    ColliderType type{ColliderType::solid};
};

struct MapEntity {
    EntityType type{EntityType::spawn};
    Vec2 pos{};
};

struct ScreenMap {
    int index{};
    float width{};
    float height{};
    std::string biome;
    std::vector<ConvexPolygon> polygons;
    std::vector<MapEntity> entities;
};

[[nodiscard]] ScreenMap parse_screen_map(std::string_view json, std::string_view label);
[[nodiscard]] std::string serialize_screen_map(const ScreenMap& map);

}  // namespace jumpcastle
```

- [ ] **Step 4: Implement source** — `src/map_format.cpp`. Use `nlohmann::json` (already linked). Follow the validation style of `src/assets.cpp` (`required`, `positive_integer`). Keep functions <50 lines by splitting helpers: `collider_type_from_string`, `type_to_string`, `entity_type_from_string`, `parse_points`, `parse_collider`, `parse_entity`. Algorithm for `parse_screen_map`:
  1. `Json root = Json::parse(json)`; wrap in try/catch and rethrow as `std::runtime_error(label + ": " + e.what())`.
  2. Require `schema_version == 1`; read `screen.index/width/height`, `biome`.
  3. For each collider: read `type` string → `ColliderType`; read `points` (array of 2-element arrays) into `std::vector<Vec2>`; if `points.size() < 3` throw; run `split_to_convex(points)`; for each convex piece push a `ConvexPolygon{piece, outward_edge_normals(piece), polygon_aabb(piece), type}`.
  4. For each entity: read `type` → `EntityType`, `pos` → `Vec2`.
  `serialize_screen_map` writes `{schema_version, screen, biome, colliders:[{type,points}], entities:[{type,pos}]}` (one collider per `ConvexPolygon`).

- [ ] **Step 5: Register in CMake** — add `src/map_format.cpp` to core, `tests/map_format_test.cpp` to tests.

- [ ] **Step 6: Run to verify pass** — `ctest --test-dir build -R map_format` → PASS.

- [ ] **Step 7: Commit** — `git commit -am "feat(map): add polygon .map.json parse/serialize"`

---

## Stage 3 — Collision world + physics integration

### Task 5: CollisionWorld (screen gather + SAT resolve)

**Files:**
- Create: `include/jumpcastle/collision_world.hpp`, `src/collision_world.cpp`
- Test: `tests/collision_world_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ScreenMap`, `ConvexPolygon`, `outward_edge_normals`, `polygon_aabb` (Tasks 2,4); `aabb_vs_convex`/`Mtv` (Task 3); `PlayerState` (`include/jumpcastle/player.hpp`), `config::player_half_size`.
- Produces:
  - `struct ResolveResult { bool on_ground; bool hit_hazard; };`
  - `class CollisionWorld` with `static CollisionWorld from_screens(std::vector<ScreenMap> screens, int screen_height);` and `ResolveResult resolve(Vec2 previous_position, PlayerState& player) const;`
  - Internally stores world-space `ConvexPolygon`s grouped by screen index (points offset by `screen_index * screen_height` before storage), and `screen_height_`.

- [ ] **Step 1: Write failing test** — `tests/collision_world_test.cpp`:

```cpp
#include "jumpcastle/collision_world.hpp"
#include "jumpcastle/player.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace jumpcastle;

namespace {
ConvexPolygon solid(std::vector<Vec2> pts) {
    return {pts, outward_edge_normals(pts), polygon_aabb(pts), ColliderType::solid};
}
ScreenMap floor_screen() {
    ScreenMap s; s.index = 0; s.width = 16; s.height = 15;
    s.polygons.push_back(solid({{0,14},{16,14},{16,15},{0,15}}));
    return s;
}
}

TEST_CASE("falling player lands on the floor and is grounded") {
    CollisionWorld world = CollisionWorld::from_screens({floor_screen()}, 15);
    PlayerState player;
    const Vec2 prev{8.0F, 13.0F};
    player.position = {8.0F, 13.9F};
    player.velocity = {0.0F, 10.0F};
    const auto result = world.resolve(prev, player);
    REQUIRE(result.on_ground);
    REQUIRE(player.velocity.y == Catch::Approx(0.0F).margin(1e-3));
    REQUIRE(player.position.y + config::player_half_size.y == Catch::Approx(14.0F).margin(1e-2));
}

TEST_CASE("hazard overlap is reported without positional resolve") {
    ScreenMap s; s.index = 0; s.width = 16; s.height = 15;
    ConvexPolygon spike{{{7,13},{9,13},{8,12}}, {}, {}, ColliderType::hazard};
    spike.edge_normals = outward_edge_normals(spike.points);
    spike.aabb = polygon_aabb(spike.points);
    s.polygons.push_back(spike);
    CollisionWorld world = CollisionWorld::from_screens({s}, 15);
    PlayerState player; player.position = {8.0F, 12.6F};
    const auto result = world.resolve({8.0F, 12.0F}, player);
    REQUIRE(result.hit_hazard);
}
```

- [ ] **Step 2: Run to verify fail** — build → FAIL.

- [ ] **Step 3: Implement** — `resolve` mirrors `resolve_world_collision` (`src/collision.cpp:202`): keep `previous_position`; sub-step the move to `target = player.position` in steps of length `<= 0.2`; after each nudge build `Aabb box{player.position - config::player_half_size, player.position + config::player_half_size}` and iterate candidate polygons from the player's screen ± 1. For each, `Mtv m = aabb_vs_convex(box, poly.points, poly.edge_normals)`; if not overlapping, skip. Then:
  - `solid`: `player.position = player.position + m.normal * m.depth;` cancel velocity along normal: `player.velocity = player.velocity - m.normal * dot(player.velocity, m.normal);` set `on_ground` if `m.normal.y < -0.5F`.
  - `oneway`: apply the `solid` branch **only if** `m.normal.y < -0.5F` **and** `(previous_position.y + config::player_half_size.y) <= poly.aabb.min.y` (previous bottom above the platform top). Else skip.
  - `hazard`: set `hit_hazard = true`; do not move.
  Screen index = `static_cast<int>(std::floor(player.position.y / screen_height_))`; gather that index and its ±1 neighbors.

- [ ] **Step 4: Run to verify pass** — `ctest --test-dir build -R collision_world` → PASS.

- [ ] **Step 5: Commit** — `git commit -am "feat(collision): add polygon CollisionWorld with SAT resolve"`

---

### Task 6: Step the player against CollisionWorld

**Files:**
- Modify: `include/jumpcastle/player.hpp`, `src/player.cpp`
- Test: `tests/player_test.cpp` (add a polygon-floor landing case)

**Interfaces:**
- Consumes: `CollisionWorld::resolve` (Task 5).
- Produces: `void step_player(PlayerState&, const CollisionWorld&, PlayerInput, float fixed_delta)` — integrates as today, then calls `world.resolve(previous_position, player)`, sets `on_ground` from the result, and respawns on `hit_hazard`.

- [ ] **Step 1: Write failing test** — in `tests/player_test.cpp`, build a `CollisionWorld` with a floor, call `step_player` repeatedly under gravity, assert the player rests grounded at the floor top.
- [ ] **Step 2: Run to verify fail** — build → FAIL (signature mismatch).
- [ ] **Step 3: Implement** the new `step_player` (keep the old `WorldMap` overload only if still referenced; otherwise replace it).
- [ ] **Step 4: Run to verify pass** — `ctest --test-dir build -R player` → PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(player): step against polygon CollisionWorld"`

---

### Task 7: Load `.map.json` screens into the campaign

**Files:**
- Modify: `src/world_loader.cpp`, `include/jumpcastle/world.hpp`, `src/world.cpp`, `src/game.cpp`
- Create: `assets/levels/screen-05.map.json` (one hand-authored polygon screen)
- Test: `tests/world_test.cpp`

**Interfaces:**
- Consumes: `parse_screen_map` (Task 4), `CollisionWorld::from_screens` (Task 5).
- Produces: campaign loading that reads `screen-NN.map.json` files → `std::vector<ScreenMap>` → `CollisionWorld`, and derives spawn/goal (from `spawn`/`goal` entities) and per-screen biome. Expose the `CollisionWorld` + spawn/goal/biome to `game.cpp`.

- [ ] **Step 1: Write failing test** — in `tests/world_test.cpp`, write a `.map.json` to a temp path, load it, assert spawn matches the file's spawn entity and polygon count > 0.
- [ ] **Step 2: Run to verify fail.**
- [ ] **Step 3: Implement** the loader (enumerate `screen-*.map.json`, parse each, build `CollisionWorld::from_screens`, derive metadata).
- [ ] **Step 4: Run to verify pass** — `ctest --test-dir build -R world` → PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(world): load polygon .map.json screens"`

---

### Task 8: Renderer debug-draw of polygons

**Files:**
- Modify: `src/renderer.cpp`, `include/jumpcastle/renderer.hpp`

**Interfaces:**
- Consumes: the per-screen `ConvexPolygon` list from `CollisionWorld`.
- Produces: a debug draw pass (toggle flag) drawing each polygon outline + translucent fill in world→screen space, following existing raylib patterns in `renderer.cpp` (camera transform, `DrawLineV`, `DrawTriangleFan`). Color by type (solid/oneway/hazard).

- [ ] **Step 1: Implement** the draw pass (visual — no unit test).
- [ ] **Step 2: Verify** with `python tools/render_asset_smoke.py` that polygons render in the capture.
- [ ] **Step 3: Commit** — `git commit -am "feat(render): debug-draw collision polygons"`

---

### Task 9: Remove the ASCII grid collision path

**Files:**
- Modify/remove: `include/jumpcastle/tilemap.hpp`, `src/tilemap.cpp`, grid functions in `src/collision.cpp` (`overlapped_tiles`, `resolve_tilemap_collision`, `collides_with_world`, grid `resolve_world_collision`), `src/level.cpp`, `CMakeLists.txt`
- Delete: `assets/levels/*.level` (after screens are authored as `.map.json`)
- Test: delete/replace `tests/tilemap_test.cpp`, grid cases in `tests/collision_test.cpp`, `tests/level_test.cpp`

- [ ] **Step 1:** Grep for remaining callers: `grep -rn "resolve_tilemap_collision\|overlapped_tiles\|Tilemap\|\.level" src include tests`.
- [ ] **Step 2:** Remove dead code + tests + CMake entries; author the real campaign screens as `.map.json`.
- [ ] **Step 3:** Full suite — `ctest --test-dir build` → all PASS; run the game to confirm it plays.
- [ ] **Step 4: Commit** — `git commit -am "refactor(collision): remove ASCII grid path"`

---

## Stage 3c — Visual layer (textured polygons) + game cutover (decision 2026-07-18)

> Chosen visual approach: **textured polygon fill** (see spec 8b). The renderer/camera/`capture_screen` move to `const CampaignWorld&`; the game loads a `CampaignWorld` from `screen-NN.map.json` and runs the polygon `step_world`. Progress so far: `CampaignWorld` + polygon `step_world` overload + `parse_screen_map_file` + first asset are DONE and green (commits 70a909b, 704f6d1).

### Task 7c: Camera on CampaignWorld
**Files:** `include/jumpcastle/camera.hpp`, `src/camera.cpp`. Add a `select_camera_band(const CampaignWorld&, float y)` overload (or template) returning the same `CameraBand`; keep the WorldMap overload until Task 9. Test in `tests/camera_test.cpp`.

### Task 8: Renderer textured-polygon draw
**Files:** `include/jumpcastle/renderer.hpp`, `src/renderer.cpp`.
- [ ] Add `Renderer::draw(const CampaignWorld&, const CameraBand&, const PlayerState&, const CampaignState&, float, bool)` (and a `capture_screen(const CampaignWorld&, …)`), leaving the WorldMap versions until Task 9.
- [ ] Terrain: for each `ConvexPolygon` in the camera's screens, triangulate (reuse `split_to_convex`) and draw textured triangles from the biome terrain texture; UVs = `world_pos * kTerrainUvScale` for seamless tiling. Tint `oneway`/`hazard` per spec 8b.
- [ ] Background: reuse the biome background region + parallax already in `draw`.
- [ ] Debug: keep polygon outline draw under `debug_enabled`.
- [ ] Verify with `python tools/render_asset_smoke.py` (headless capture) that terrain renders textured, not wireframe.

### Task 8b: Game cutover
**Files:** `include/jumpcastle/game.hpp`, `src/game.cpp`.
- [ ] `world_` becomes `std::optional<CampaignWorld>`, loaded via `CampaignWorld::load(asset_directory / "levels", screen_height)`.
- [ ] `step_world(player_, campaign_, *world_, input)` now resolves the `CampaignWorld` overload; camera via Task 7c overload; renderer via Task 8 overload.
- [ ] Author the remaining `screen-NN.map.json` tower screens (replace `campaign.level`).
- [ ] Manual verify: run the game — player collides with polygons, terrain is textured, one-way/hazard behave.

## Stage 3b — Solver port to polygon collision (decision 2026-07-18)

> The user chose to **port the solver to polygon collision** rather than retire it. The solver (`src/solver.cpp`, `src/solver_main.cpp`, `include/jumpcastle/solver.hpp`) and replay (`src/replay.cpp`, `verify_trace`) currently reason about the grid `WorldMap` (`solid_at`) to auto-generate and validate beatable campaigns. They MUST move to `CollisionWorld` so the "beatable" guarantee matches real in-game collision. This stage runs after Task 7 (loader) so a polygon world exists to solve against.

### Task 7b: Solver reachability on CollisionWorld

**Files:** Modify `include/jumpcastle/solver.hpp`, `src/solver.cpp`, `src/solver_main.cpp`, `include/jumpcastle/replay.hpp`, `src/replay.cpp`; Tests `tests/solver_test.cpp`, `tests/world_reachability_test.cpp`, `tests/replay_test.cpp`.

**Interfaces:** solver/replay take `const CollisionWorld&` instead of `const WorldMap&`; jump simulation reuses `step_player(PlayerState&, const CollisionWorld&, …)` (already built in Task 6) so the solver simulates with the exact runtime collision.

- [ ] **Step 1:** Change the jump-simulation inner loop in `solver.cpp` to advance a `PlayerState` with the `CollisionWorld` `step_player` overload and read landing from `ResolveResult`/`player.on_ground` (replaces grid `resolve_world_collision`/`solid_at` probing).
- [ ] **Step 2:** Replace reachability queries (`solid_at`, floor probes) with `CollisionWorld::overlaps_blocking` support probes.
- [ ] **Step 3:** Update `verify_trace(const CollisionWorld&, …)` to replay against the polygon world.
- [ ] **Step 4:** Port `solver_test`/`world_reachability_test`/`replay_test` to build `CollisionWorld` from hand-authored `ScreenMap`s; keep them green.
- [ ] **Step 5:** Commit — `git commit -am "refactor(solver): reason about polygon CollisionWorld"`.

**Note:** because the solver now shares `step_player`+`CollisionWorld` with the runtime, solver guarantees and gameplay collision are the same code path — no drift.

## Stage 4 — In-game editor

### Task 10: Editor state model (headless, unit-tested)

**Files:**
- Create: `include/jumpcastle/editor.hpp`, `src/editor.cpp`
- Test: `tests/editor_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ScreenMap`, `ConvexPolygon`, `MapEntity`, `serialize_screen_map` (Task 4); `is_convex`, `split_to_convex` (Task 2).
- Produces: `class EditorState` — pure logic, no raylib:
  - `void begin_polygon(ColliderType);`
  - `void add_vertex(Vec2 world_pos, bool snap);` (snap rounds each component to 0.25)
  - `void close_polygon();` (validates >=3; splits concave on commit)
  - `int hit_vertex(Vec2 world_pos, float radius) const;` / `void move_selected_vertex(Vec2);`
  - `void delete_selected_polygon();`
  - `void place_entity(EntityType, Vec2, bool snap);`
  - `ScreenMap to_screen_map() const;`

- [ ] **Step 1: Write failing tests** — snap rounds to 0.25; `begin_polygon`+3×`add_vertex`+`close_polygon` yields one polygon in `to_screen_map()`; a concave draft closes into >=2 polygons; `place_entity` records the entity.
- [ ] **Step 2: Run to verify fail.**
- [ ] **Step 3: Implement** `EditorState` (snap = `std::round(v / 0.25F) * 0.25F` per component).
- [ ] **Step 4: Run to verify pass** — `ctest --test-dir build -R editor` → PASS.
- [ ] **Step 5: Commit** — `git commit -am "feat(editor): headless polygon editor state"`

---

### Task 11: Editor input + rendering + save (raylib)

**Files:**
- Modify: `src/game.cpp`, `include/jumpcastle/game.hpp`, `src/renderer.cpp`, `src/main.cpp`

**Interfaces:**
- Consumes: `EditorState` (Task 10), renderer polygon draw (Task 8).
- Produces: `F1` toggles editor + pauses physics; mouse→world mapping feeds `EditorState`; keys bound per spec §5 (`Enter` close, `Esc` cancel, `Del` delete, `PageUp/PageDown` switch screen, `Alt` disable snap, a key cycles collider type); `Ctrl+S` writes `serialize_screen_map(editor.to_screen_map())` to the current `screen-NN.map.json`.

- [ ] **Step 1: Implement** input handling + overlay draw (vertex handles, entity icons, grid, red invalid-draft feedback), following `renderer.cpp` patterns.
- [ ] **Step 2: Verify** manually: toggle editor, draw a polygon, drag a vertex, place a spawn, save; reload and confirm the polygon persists and collides.
- [ ] **Step 3: Commit** — `git commit -am "feat(editor): in-game polygon editing and save"`

---

## Self-Review

- **Spec coverage:** §2 approach → Tasks 1-3,5,6. §3 format → Tasks 4,7. §4 runtime → Tasks 3,5,6. §5 editor → Tasks 10,11. §6 architecture units → sat/convex/map_format/collision_world/editor are each separate files. §7 testing → each task's tests + Task 8 smoke render. §8 build order → stage order matches. §9 out-of-scope respected (no swept, no moving platforms, no undo/multi-select, no TMX).
- **Placeholder note:** Tasks 4,7-9,11 give concrete algorithms, exact files, interfaces, and tests but describe (rather than fully transcribe) the raylib/loader glue in large existing files (`assets.cpp`/`renderer.cpp`/`world_loader.cpp`), which must follow established local patterns. The pure-logic tasks (1,2,3,5,10) carry complete literal code. Glue is verified against the running game (Task 8/9/11 verification steps).
- **Type consistency:** `Mtv{overlapping,normal,depth}`, `Aabb{min,max}`, `ConvexPolygon{points,edge_normals,aabb,type}`, `ScreenMap{index,width,height,biome,polygons,entities}`, `ResolveResult{on_ground,hit_hazard}`, `CollisionWorld::from_screens/resolve`, and the `EditorState` methods are used consistently across all tasks.
