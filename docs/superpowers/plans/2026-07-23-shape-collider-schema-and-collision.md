# Shape Collider Schema and Collision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add backward-compatible map schema v3 plus exact polygon, circle, and capsule collision/contact data without replacing JumpCastle's deterministic fixed-step physics.

**Architecture:** Authored `MapCollider` values preserve lossless Tiled geometry; `CollisionWorld::from_screens` converts them into world-space `WorldCollider` values and decomposes concave polygons only at runtime. A focused `shape_collision` module owns AABB-versus-shape narrow-phase math and returns a common `Contact`, while `CollisionWorld` keeps screen partitioning, sub-stepping, behavior types, and deterministic contact selection.

**Tech Stack:** C++20, `std::variant`, nlohmann/json 3.12.0, Catch2 3.8.1, CMake 3.24+, existing SAT/convex modules.

## Global Constraints

- Keep `config::fixed_delta == 1.0F / 120.0F`.
- Keep the player body as `Aabb`; all new geometry is static world geometry.
- Keep maximum movement sub-step at `0.2F` tile.
- Require circle/capsule radius `>= 0.5F`.
- Curved colliders may be `solid` or `hazard`; `oneway` remains polygon-only.
- Loader accepts map schema versions 1, 2, and 3.
- Existing `shape: "slope"` migrates to polygon geometry with tag `slope`.
- Preserve user changes in the dirty worktree; stage/commit only files from the current task.
- Do not run `git commit` unless the user explicitly authorizes commits.

---

## File Structure

- Create `include/jumpcastle/collider.hpp`: authored geometry, runtime geometry, common IDs/tags, `Contact`, and AABB helpers.
- Create `include/jumpcastle/shape_collision.hpp`: pure narrow-phase function declarations.
- Create `src/shape_collision.cpp`: exact AABB-versus-polygon/circle/capsule contact math.
- Create `tests/shape_collision_test.cpp`: shape-math unit tests independent of campaign/game state.
- Modify `include/jumpcastle/map_format.hpp`: replace `ScreenMap::polygons` with `ScreenMap::colliders`; expose named tile layers for schema v3.
- Modify `src/map_format.cpp`: parse, validate, migrate, and serialize schema v3.
- Modify `include/jumpcastle/collision_world.hpp`: expose world colliders and contact-aware resolve results.
- Modify `src/collision_world.cpp`: build runtime shapes, dispatch narrow phase, resolve contacts deterministically.
- Modify `include/jumpcastle/campaign_world.hpp` and `src/campaign_world.cpp`: retain authored maps while building the new world.
- Modify `include/jumpcastle/editor.hpp` and `src/editor.cpp`: continue editing polygon colliders while preserving unsupported circle/capsule values.
- Modify `src/renderer.cpp` and `src/solver.cpp`: consume polygon geometry through the new world-collider interface; circle/capsule rendering/solver support lands in later plans.
- Modify `tests/map_format_test.cpp`, `tests/collision_world_test.cpp`, `tests/editor_test.cpp`, and `tests/test_world_factory.hpp`.
- Modify `CMakeLists.txt`: compile `src/shape_collision.cpp` and `tests/shape_collision_test.cpp`.

### Task 1: Define authored and runtime collider contracts

**Files:**
- Create: `include/jumpcastle/collider.hpp`
- Modify: `include/jumpcastle/map_format.hpp`
- Test: `tests/map_format_test.cpp`

**Interfaces:**
- Produces: `PolygonGeometry`, `CircleGeometry`, `CapsuleGeometry`, `MapCollider`, `ConvexPolygon`, `WorldCollider`, `Contact`, `TileLayers`.
- Consumes: `Vec2`, `Aabb`, existing `ColliderType`.

- [ ] **Step 1: Write the compile-level schema test**

Add to `tests/map_format_test.cpp`:

```cpp
TEST_CASE("schema v3 collider types retain authored geometry") {
    const PolygonGeometry polygon{{{1.0F, 2.0F}, {5.0F, 2.0F}, {5.0F, 4.0F}}};
    const CircleGeometry circle{{8.0F, 6.0F}, 2.0F};
    const CapsuleGeometry capsule{{3.0F, 9.0F}, {11.0F, 9.0F}, 1.5F};

    const MapCollider a{.id = 1, .type = ColliderType::solid,
                        .geometry = polygon, .tag = "slope"};
    const MapCollider b{.id = 2, .type = ColliderType::hazard,
                        .geometry = circle, .tag = "orb"};
    const MapCollider c{.id = 3, .type = ColliderType::solid,
                        .geometry = capsule, .tag = "bridge"};

    CHECK(std::holds_alternative<PolygonGeometry>(a.geometry));
    CHECK(std::get<CircleGeometry>(b.geometry).radius == Approx(2.0F));
    CHECK(std::get<CapsuleGeometry>(c.geometry).b.x == Approx(11.0F));
}
```

- [ ] **Step 2: Run the focused test and verify the contract is absent**

Run:

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
```

Expected: compilation fails because `PolygonGeometry`, `CircleGeometry`,
`CapsuleGeometry`, and `MapCollider` are undefined.

- [ ] **Step 3: Add the collider data model**

Create `include/jumpcastle/collider.hpp`:

```cpp
#pragma once

#include "jumpcastle/math.hpp"

#include <string>
#include <variant>
#include <vector>

namespace jumpcastle {

enum class ColliderType { solid, oneway, hazard };

struct PolygonGeometry {
    std::vector<Vec2> points;
};

struct CircleGeometry {
    Vec2 center{};
    float radius{};
};

struct CapsuleGeometry {
    Vec2 a{};
    Vec2 b{};
    float radius{};
};

using MapGeometry = std::variant<PolygonGeometry, CircleGeometry, CapsuleGeometry>;

struct MapCollider {
    int id{};
    ColliderType type{ColliderType::solid};
    MapGeometry geometry{PolygonGeometry{}};
    std::string tag;
};

struct ConvexPolygon {
    std::vector<Vec2> points;
    std::vector<Vec2> edge_normals;
    Aabb aabb{};
};

using WorldGeometry = std::variant<ConvexPolygon, CircleGeometry, CapsuleGeometry>;

struct WorldCollider {
    int id{};
    int piece_index{};
    ColliderType type{ColliderType::solid};
    WorldGeometry geometry{ConvexPolygon{}};
    Aabb aabb{};
    std::string tag;
};

struct Contact {
    int collider_id{-1};
    int piece_index{};
    ColliderType type{ColliderType::solid};
    Vec2 point{};
    Vec2 normal{};
    float depth{};
};

[[nodiscard]] Aabb geometry_aabb(const PolygonGeometry& polygon) noexcept;
[[nodiscard]] Aabb geometry_aabb(const CircleGeometry& circle) noexcept;
[[nodiscard]] Aabb geometry_aabb(const CapsuleGeometry& capsule) noexcept;

}  // namespace jumpcastle
```

Update `include/jumpcastle/map_format.hpp`:

```cpp
#include "jumpcastle/collider.hpp"

enum class EntityType { spawn, checkpoint, goal };

struct TileLayer {
    int columns{};
    int rows{};
    std::vector<std::uint32_t> gids;
};

struct TileLayers {
    TileLayer background{};
    TileLayer terrain{};
    TileLayer decor{};
    TileLayer foreground{};
};

struct ScreenMap {
    int schema_version{1};
    int index{};
    float width{};
    float height{};
    std::string biome;
    std::vector<MapCollider> colliders;
    std::vector<MapEntity> entities;
    TileLayers tiles{};
    int tileset_columns{24};
    int tileset_tile_size{16};
};
```

Remove the old duplicate `ColliderType` and `ConvexPolygon` declarations from
`map_format.hpp`.

- [ ] **Step 4: Add AABB implementations beside the data types**

Create the following initial implementation in `src/shape_collision.cpp`:

```cpp
#include "jumpcastle/collider.hpp"

#include "jumpcastle/convex.hpp"

#include <algorithm>

namespace jumpcastle {

Aabb geometry_aabb(const PolygonGeometry& polygon) noexcept {
    return polygon_aabb(polygon.points);
}

Aabb geometry_aabb(const CircleGeometry& circle) noexcept {
    const Vec2 extent{circle.radius, circle.radius};
    return {circle.center - extent, circle.center + extent};
}

Aabb geometry_aabb(const CapsuleGeometry& capsule) noexcept {
    return {
        {std::min(capsule.a.x, capsule.b.x) - capsule.radius,
         std::min(capsule.a.y, capsule.b.y) - capsule.radius},
        {std::max(capsule.a.x, capsule.b.x) + capsule.radius,
         std::max(capsule.a.y, capsule.b.y) + capsule.radius},
    };
}

}  // namespace jumpcastle
```

- [ ] **Step 5: Register the new compilation unit and run the test**

Add `src/shape_collision.cpp` to `jumpcastle_core` in `CMakeLists.txt`, then run:

```bash
rtk cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DJUMPCASTLE_BUILD_TESTS=ON
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "schema v3 collider types retain authored geometry"
```

Expected: the focused test passes.

- [ ] **Step 6: Commit only with explicit authorization**

If commits have been authorized:

```bash
rtk git add include/jumpcastle/collider.hpp include/jumpcastle/map_format.hpp \
  src/shape_collision.cpp tests/map_format_test.cpp CMakeLists.txt
rtk git commit -m "feat(map): define schema v3 collider geometry"
```

### Task 2: Parse, validate, migrate, and serialize schema v3

**Files:**
- Modify: `src/map_format.cpp`
- Test: `tests/map_format_test.cpp`

**Interfaces:**
- Consumes: `MapCollider`, `MapGeometry`, and `TileLayers` from Task 1.
- Produces: backward-compatible `parse_screen_map` and v3 `serialize_screen_map`.

- [ ] **Step 1: Add failing parser and validation cases**

Add:

```cpp
TEST_CASE("schema v3 parses polygon circle and capsule geometry") {
    const auto map = parse_screen_map(R"json({
      "schema_version": 3,
      "screen": {"index": 2, "width": 28, "height": 36},
      "biome": "frosted_keep",
      "colliders": [
        {"id": 10, "type": "solid", "geometry": "polygon",
         "tag": "slope", "points": [[2,20],[8,16],[8,20]]},
        {"id": 11, "type": "hazard", "geometry": "circle",
         "tag": "orb", "center": [14,18], "radius": 2},
        {"id": 12, "type": "solid", "geometry": "capsule",
         "tag": "bridge", "a": [4,10], "b": [18,10], "radius": 1}
      ],
      "entities": []
    })json", "shape-map");

    REQUIRE(map.schema_version == 3);
    REQUIRE(map.colliders.size() == 3);
    CHECK(std::get<PolygonGeometry>(map.colliders[0].geometry).points.size() == 3);
    CHECK(std::get<CircleGeometry>(map.colliders[1].geometry).radius == Approx(2.0F));
    CHECK(std::get<CapsuleGeometry>(map.colliders[2].geometry).a.x == Approx(4.0F));
}

TEST_CASE("schema v3 rejects invalid curved geometry with collider id") {
    CHECK_THROWS_WITH(
        parse_screen_map(R"json({
          "schema_version": 3,
          "screen": {"index": 0, "width": 28, "height": 36},
          "biome": "courtyard",
          "colliders": [
            {"id": 7, "type": "solid", "geometry": "circle",
             "center": [4,4], "radius": 0.25}
          ],
          "entities": []
        })json", "bad-circle"),
        Catch::Matchers::ContainsSubstring("collider id 7") &&
        Catch::Matchers::ContainsSubstring("radius must be at least 0.5"));
}

TEST_CASE("schema v2 slope migrates to polygon geometry and slope tag") {
    const auto map = parse_screen_map(R"json({
      "schema_version": 2,
      "screen": {"index": 0, "width": 28, "height": 36},
      "biome": "courtyard",
      "colliders": [
        {"id": 1, "type": "solid", "shape": "slope",
         "points": [[2,20],[8,16],[8,20]]}
      ],
      "entities": []
    })json", "legacy-slope");

    REQUIRE(map.colliders.size() == 1);
    CHECK(map.colliders[0].tag == "slope");
    CHECK(std::holds_alternative<PolygonGeometry>(map.colliders[0].geometry));
}
```

- [ ] **Step 2: Run and observe schema-version rejection**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[map_format]"
```

Expected: the v3 case fails with `unsupported map schema_version`.

- [ ] **Step 3: Implement geometry-specific parsing**

Add these helpers inside `src/map_format.cpp`:

```cpp
void validate_point(const Vec2 point, const std::string& label) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::runtime_error(label + " must contain finite coordinates");
    }
}

MapCollider parse_collider(const Json& value, const std::size_t index,
                           const int version) {
    const int id = value.value("id", static_cast<int>(index + 1));
    const std::string label =
        "colliders[" + std::to_string(index) + "] collider id " + std::to_string(id);
    const ColliderType type =
        collider_type_from_string(required(value, "type", label).get<std::string>(), label);
    const std::string geometry_name =
        version >= 3 ? required(value, "geometry", label).get<std::string>() : "polygon";
    const std::string tag = version < 3 && value.value("shape", "") == "slope"
        ? "slope"
        : value.value("tag", "");

    MapGeometry geometry;
    if (geometry_name == "polygon") {
        auto points = parse_points(required(value, "points", label), label + ".points");
        for (const Vec2 point : points) validate_point(point, label + ".points");
        geometry = PolygonGeometry{std::move(points)};
    } else if (geometry_name == "circle") {
        const Vec2 center = parse_point(required(value, "center", label), label + ".center");
        const float radius = required(value, "radius", label).get<float>();
        validate_point(center, label + ".center");
        if (!std::isfinite(radius) || radius < 0.5F) {
            throw std::runtime_error(label + " radius must be at least 0.5");
        }
        if (type == ColliderType::oneway) {
            throw std::runtime_error(label + " curved geometry cannot be oneway");
        }
        geometry = CircleGeometry{center, radius};
    } else if (geometry_name == "capsule") {
        const Vec2 a = parse_point(required(value, "a", label), label + ".a");
        const Vec2 b = parse_point(required(value, "b", label), label + ".b");
        const float radius = required(value, "radius", label).get<float>();
        validate_point(a, label + ".a");
        validate_point(b, label + ".b");
        if (a == b) throw std::runtime_error(label + " capsule endpoints must differ");
        if (!std::isfinite(radius) || radius < 0.5F) {
            throw std::runtime_error(label + " radius must be at least 0.5");
        }
        if (type == ColliderType::oneway) {
            throw std::runtime_error(label + " curved geometry cannot be oneway");
        }
        geometry = CapsuleGeometry{a, b, radius};
    } else {
        throw std::runtime_error(label + " has unknown geometry '" + geometry_name + "'");
    }
    return {.id = id, .type = type, .geometry = std::move(geometry), .tag = tag};
}
```

Accept versions 1–3, store `map.schema_version`, reject duplicate IDs with an
`std::unordered_set<int>`, and replace the old `map.polygons` loop with
`map.colliders.push_back(parse_collider(...))`.

- [ ] **Step 4: Implement named-layer parsing**

Replace the single terrain parse with:

```cpp
auto parse_named_layer = [&](const char* name, TileLayer& destination) {
    if (tiles.contains(name)) {
        destination = parse_tile_layer(
            tiles.at(name), static_cast<int>(map.width),
            static_cast<int>(map.height), std::string{"map.tiles."} + name);
    }
};
parse_named_layer("background", map.tiles.background);
parse_named_layer("terrain", map.tiles.terrain);
parse_named_layer("decor", map.tiles.decor);
parse_named_layer("foreground", map.tiles.foreground);
```

Validate positive screen dimensions, nonnegative index, positive tileset
columns/tile size, collider bounds, entity bounds, and GID masked IDs against
`tileset_columns * atlas_rows` when atlas rows are supplied by the caller. Keep
the parser's existing label-prefixed exception boundary.

- [ ] **Step 5: Implement v3 serialization without data loss**

Use a variant visitor:

```cpp
Json serialize_collider(const MapCollider& collider) {
    Json value{
        {"id", collider.id},
        {"type", collider_type_to_string(collider.type)},
    };
    if (!collider.tag.empty()) value["tag"] = collider.tag;
    std::visit([&](const auto& geometry) {
        using T = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<T, PolygonGeometry>) {
            value["geometry"] = "polygon";
            value["points"] = Json::array();
            for (const Vec2 point : geometry.points) {
                value["points"].push_back({point.x, point.y});
            }
        } else if constexpr (std::is_same_v<T, CircleGeometry>) {
            value["geometry"] = "circle";
            value["center"] = {geometry.center.x, geometry.center.y};
            value["radius"] = geometry.radius;
        } else {
            value["geometry"] = "capsule";
            value["a"] = {geometry.a.x, geometry.a.y};
            value["b"] = {geometry.b.x, geometry.b.y};
            value["radius"] = geometry.radius;
        }
    }, collider.geometry);
    return value;
}
```

Always serialize `schema_version: 3` when any curved collider or optional named
tile layer is present. Serialize v1/v2 only for unchanged legacy data. Add a
`serialize_tile_layer` helper and emit all non-empty named layers.

- [ ] **Step 6: Run parser, serializer, and full map tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[map_format]"
rtk ctest --test-dir build/cmake -R "map|tiled" --output-on-failure
```

Expected: all focused tests pass.

- [ ] **Step 7: Commit only with explicit authorization**

```bash
rtk git add src/map_format.cpp tests/map_format_test.cpp
rtk git commit -m "feat(map): parse and serialize shape schema v3"
```

### Task 3: Add exact AABB-versus-circle and AABB-versus-capsule contacts

**Files:**
- Create: `include/jumpcastle/shape_collision.hpp`
- Modify: `src/shape_collision.cpp`
- Create: `tests/shape_collision_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `contact_aabb_polygon`, `contact_aabb_circle`, `contact_aabb_capsule`.
- Consumes: `Contact` and geometry types from Task 1.

- [ ] **Step 1: Write failing pure-math tests**

Create `tests/shape_collision_test.cpp`:

```cpp
#include "jumpcastle/shape_collision.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("AABB touching circle top returns upward contact") {
    const Aabb box{{4.5F, 2.5F}, {5.5F, 3.5F}};
    const CircleGeometry circle{{5.0F, 5.0F}, 2.0F};
    const auto contact = contact_aabb_circle(box, circle);
    REQUIRE(contact);
    CHECK(contact->normal.x == Approx(0.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(0.5F).margin(1e-5));
}

TEST_CASE("AABB inside circle uses deterministic nearest-face normal") {
    const Aabb box{{4.5F, 4.5F}, {5.5F, 5.5F}};
    const CircleGeometry circle{{5.0F, 5.0F}, 3.0F};
    const auto contact = contact_aabb_circle(box, circle);
    REQUIRE(contact);
    CHECK(std::isfinite(contact->normal.x));
    CHECK(std::isfinite(contact->normal.y));
    CHECK(length(contact->normal) == Approx(1.0F));
}

TEST_CASE("AABB contacts horizontal capsule end arc") {
    const Aabb box{{8.7F, 3.6F}, {9.4F, 4.4F}};
    const CapsuleGeometry capsule{{3.0F, 5.0F}, {8.0F, 5.0F}, 1.5F};
    const auto contact = contact_aabb_capsule(box, capsule);
    REQUIRE(contact);
    CHECK(contact->normal.x > 0.0F);
    CHECK(contact->normal.y < 0.0F);
}

TEST_CASE("separated AABB and capsule return no contact") {
    const Aabb box{{12.0F, 1.0F}, {13.0F, 2.0F}};
    const CapsuleGeometry capsule{{2.0F, 8.0F}, {6.0F, 8.0F}, 1.0F};
    CHECK_FALSE(contact_aabb_capsule(box, capsule));
}
```

- [ ] **Step 2: Register and run to verify missing declarations**

Add `tests/shape_collision_test.cpp` to `jumpcastle_tests`, then run:

```bash
rtk cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DJUMPCASTLE_BUILD_TESTS=ON
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
```

Expected: compile failure for missing `shape_collision.hpp`.

- [ ] **Step 3: Declare the pure contact functions**

Create `include/jumpcastle/shape_collision.hpp`:

```cpp
#pragma once

#include "jumpcastle/collider.hpp"

#include <optional>

namespace jumpcastle {

[[nodiscard]] std::optional<Contact> contact_aabb_polygon(
    Aabb box, const ConvexPolygon& polygon) noexcept;
[[nodiscard]] std::optional<Contact> contact_aabb_circle(
    Aabb box, const CircleGeometry& circle) noexcept;
[[nodiscard]] std::optional<Contact> contact_aabb_capsule(
    Aabb box, const CapsuleGeometry& capsule) noexcept;

}  // namespace jumpcastle
```

- [ ] **Step 4: Implement exact closest-pair helpers**

Add private helpers in `src/shape_collision.cpp`:

```cpp
Vec2 clamp_to_aabb(const Vec2 point, const Aabb box) noexcept {
    return {
        std::clamp(point.x, box.min.x, box.max.x),
        std::clamp(point.y, box.min.y, box.max.y),
    };
}

Vec2 closest_on_segment(const Vec2 point, const Vec2 a, const Vec2 b) noexcept {
    const Vec2 ab = b - a;
    const float denominator = dot(ab, ab);
    const float t = denominator == 0.0F
        ? 0.0F
        : std::clamp(dot(point - a, ab) / denominator, 0.0F, 1.0F);
    return a + ab * t;
}

struct ClosestPair {
    Vec2 segment_point{};
    Vec2 box_point{};
};

ClosestPair closest_segment_aabb(const Vec2 a, const Vec2 b, const Aabb box) noexcept {
    if (const auto intersection = first_segment_aabb_intersection(a, b, box)) {
        return {*intersection, *intersection};
    }

    ClosestPair best{a, clamp_to_aabb(a, box)};
    float best_distance = length_squared(best.segment_point - best.box_point);
    const auto consider = [&](const Vec2 segment_point, const Vec2 box_point) {
        const float candidate =
            length_squared(segment_point - box_point);
        if (candidate < best_distance) {
            best = {segment_point, box_point};
            best_distance = candidate;
        }
    };

    consider(b, clamp_to_aabb(b, box));
    for (const Vec2 corner : std::array{
             box.min,
             Vec2{box.max.x, box.min.y},
             box.max,
             Vec2{box.min.x, box.max.y},
         }) {
        consider(closest_on_segment(corner, a, b), corner);
    }
    return best;
}
```

Implement `first_segment_aabb_intersection` with the Liang-Barsky four-plane
clip. It returns `a + (b - a) * t_enter` for an intersection and
`std::nullopt` otherwise. The endpoint-to-box and corner-to-segment candidates
above are the complete closest-feature set for a disjoint segment and AABB, so
the result is exact and has no iteration tolerance. Preserve the listed
candidate order as the deterministic tie-break.

- [ ] **Step 5: Implement circle/capsule contact construction**

```cpp
std::optional<Contact> radial_contact(
    const Aabb box, const Vec2 center, const float radius) noexcept {
    const Vec2 box_point = clamp_to_aabb(center, box);
    Vec2 delta = box_point - center;
    float distance = length(delta);
    if (distance >= radius) return std::nullopt;

    if (distance == 0.0F) {
        const float left = center.x - box.min.x;
        const float right = box.max.x - center.x;
        const float top = center.y - box.min.y;
        const float bottom = box.max.y - center.y;
        const float nearest = std::min({left, right, top, bottom});
        if (nearest == top) delta = {0.0F, -1.0F};
        else if (nearest == bottom) delta = {0.0F, 1.0F};
        else if (nearest == left) delta = {-1.0F, 0.0F};
        else delta = {1.0F, 0.0F};
        distance = 0.0F;
    } else {
        delta = delta * (1.0F / distance);
    }
    return Contact{
        .point = center + delta * radius,
        .normal = delta,
        .depth = radius - distance,
    };
}

std::optional<Contact> contact_aabb_circle(
    const Aabb box, const CircleGeometry& circle) noexcept {
    return radial_contact(box, circle.center, circle.radius);
}

std::optional<Contact> contact_aabb_capsule(
    const Aabb box, const CapsuleGeometry& capsule) noexcept {
    const ClosestPair pair = closest_segment_aabb(capsule.a, capsule.b, box);
    const Vec2 delta = pair.box_point - pair.segment_point;
    const float distance = length(delta);
    if (distance >= capsule.radius) return std::nullopt;
    const Vec2 normal = distance > 0.0F
        ? delta * (1.0F / distance)
        : normalized(aabb_center(box) - pair.segment_point);
    const Vec2 stable_normal = length(normal) > 0.0F ? normal : Vec2{0.0F, -1.0F};
    return Contact{
        .point = pair.segment_point + stable_normal * capsule.radius,
        .normal = stable_normal,
        .depth = capsule.radius - distance,
    };
}
```

Wrap the existing `aabb_vs_convex` MTV in `contact_aabb_polygon`.

- [ ] **Step 6: Run focused and existing SAT tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[shape_collision]"
rtk ctest --test-dir build/cmake -R "SAT|collision" --output-on-failure
```

Expected: all shape/SAT tests pass without NaN or unstable normals.

- [ ] **Step 7: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/shape_collision.hpp src/shape_collision.cpp \
  tests/shape_collision_test.cpp CMakeLists.txt
rtk git commit -m "feat(collision): add exact circle and capsule contacts"
```

### Task 4: Build world-space shape colliders from authored maps

**Files:**
- Modify: `include/jumpcastle/collision_world.hpp`
- Modify: `src/collision_world.cpp`
- Test: `tests/collision_world_test.cpp`

**Interfaces:**
- Consumes: `MapCollider`, `WorldCollider`, and shape contact functions.
- Produces: `CollisionWorld::colliders_for_screen`, contact-aware `ResolveResult`.

- [ ] **Step 1: Write world-conversion and behavior tests**

Add:

```cpp
TEST_CASE("CollisionWorld offsets circle and capsule into screen world space") {
    ScreenMap screen;
    screen.index = 2;
    screen.width = 28;
    screen.height = 36;
    screen.colliders = {
        {.id = 4, .type = ColliderType::solid,
         .geometry = CircleGeometry{{8.0F, 5.0F}, 2.0F}, .tag = "orb"},
        {.id = 5, .type = ColliderType::solid,
         .geometry = CapsuleGeometry{{3.0F, 8.0F}, {12.0F, 8.0F}, 1.0F},
         .tag = "bridge"},
    };

    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    const auto* colliders = world.colliders_for_screen(2);
    REQUIRE(colliders);
    REQUIRE(colliders->size() == 2);
    CHECK(std::get<CircleGeometry>((*colliders)[0].geometry).center.y == Approx(77.0F));
    CHECK(std::get<CapsuleGeometry>((*colliders)[1].geometry).a.y == Approx(80.0F));
}

TEST_CASE("falling player lands on circle with upward contact") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders.push_back({
        .id = 9,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{{14.0F, 20.0F}, 4.0F},
        .tag = "round_platform",
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    PlayerState player{.position = {14.0F, 15.0F}, .velocity = {0.0F, 4.0F}};
    const ResolveResult result = world.resolve(player.position, player);
    CHECK(result.on_ground);
    REQUIRE(result.ground_contact);
    CHECK(result.ground_contact->collider_id == 9);
    CHECK(result.ground_contact->normal.y <= -0.5F);
}
```

- [ ] **Step 2: Update the public world interface**

Use:

```cpp
struct ResolveResult {
    bool on_ground{};
    bool hit_hazard{};
    std::optional<Contact> ground_contact;
};

[[nodiscard]] const std::vector<WorldCollider>* colliders_for_screen(
    int screen_index) const noexcept;
```

Replace `by_screen_` with `std::vector<std::vector<WorldCollider>>`.

- [ ] **Step 3: Convert authored geometry to world geometry**

In `CollisionWorld::from_screens`, visit each `MapCollider`:

```cpp
std::visit([&](const auto& geometry) {
    using T = std::decay_t<decltype(geometry)>;
    if constexpr (std::is_same_v<T, PolygonGeometry>) {
        int piece_index = 0;
        for (auto piece : split_to_convex(geometry.points)) {
            for (Vec2& point : piece) point.y += offset;
            ConvexPolygon polygon{
                piece,
                outward_edge_normals(piece),
                polygon_aabb(piece),
            };
            target.push_back({
                collider.id, piece_index++, collider.type,
                std::move(polygon), polygon.aabb, collider.tag,
            });
        }
    } else if constexpr (std::is_same_v<T, CircleGeometry>) {
        CircleGeometry circle = geometry;
        circle.center.y += offset;
        target.push_back({
            collider.id, 0, collider.type, circle,
            geometry_aabb(circle), collider.tag,
        });
    } else {
        CapsuleGeometry capsule = geometry;
        capsule.a.y += offset;
        capsule.b.y += offset;
        target.push_back({
            collider.id, 0, collider.type, capsule,
            geometry_aabb(capsule), collider.tag,
        });
    }
}, collider.geometry);
```

Store the AABB before moving the polygon or construct the `WorldCollider`
through named fields to avoid evaluation-order mistakes.

- [ ] **Step 4: Dispatch contact queries and preserve behavior rules**

Add:

```cpp
std::optional<Contact> contact_for(
    const Aabb box, const WorldCollider& collider) noexcept {
    auto contact = std::visit([&](const auto& geometry) {
        using T = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<T, ConvexPolygon>) {
            return contact_aabb_polygon(box, geometry);
        } else if constexpr (std::is_same_v<T, CircleGeometry>) {
            return contact_aabb_circle(box, geometry);
        } else {
            return contact_aabb_capsule(box, geometry);
        }
    }, collider.geometry);
    if (contact) {
        contact->collider_id = collider.id;
        contact->piece_index = collider.piece_index;
        contact->type = collider.type;
    }
    return contact;
}
```

Retain neighboring-screen lookup, AABB broad phase, hazard semantics, one-way
polygon checks, positional correction, and impact restitution. Select ground
contacts by lowest `normal.y`, then greatest depth, then `(id, piece_index)`.

- [ ] **Step 5: Run collision world tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[collision_world]"
```

Expected: polygon regression cases plus new circle/capsule cases pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/collision_world.hpp src/collision_world.cpp \
  tests/collision_world_test.cpp
rtk git commit -m "feat(collision): resolve world shape colliders"
```

### Task 5: Migrate editor, renderer fallback, solver extraction, and test factories

**Files:**
- Modify: `include/jumpcastle/editor.hpp`
- Modify: `src/editor.cpp`
- Modify: `src/renderer.cpp`
- Modify: `src/solver.cpp`
- Modify: `tests/test_world_factory.hpp`
- Modify: `tests/editor_test.cpp`
- Modify: all tests that directly populate `ScreenMap::polygons`

**Interfaces:**
- Consumes: `ScreenMap::colliders`, `CollisionWorld::colliders_for_screen`.
- Produces: a compiling application while later plans add curved editor/render/solver behavior.

- [ ] **Step 1: Add a preservation test for non-polygon editor content**

```cpp
TEST_CASE("polygon editor preserves curved colliders loaded from a screen") {
    ScreenMap map;
    map.index = 3;
    map.width = 28;
    map.height = 36;
    map.colliders.push_back({
        .id = 42,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{{10.0F, 12.0F}, 2.0F},
        .tag = "orb",
    });

    EditorState editor{3, 28.0F, 36.0F};
    editor.load_screen(map);
    const ScreenMap output = editor.to_screen_map();

    REQUIRE(output.colliders.size() == 1);
    CHECK(output.colliders[0].id == 42);
    CHECK(std::holds_alternative<CircleGeometry>(output.colliders[0].geometry));
}
```

- [ ] **Step 2: Preserve curved colliders in the polygon editor**

Add `std::vector<MapCollider> passthrough_colliders_` to `EditorState`. On load,
put polygon colliders into the editable polygon collection and circle/capsule
colliders into `passthrough_colliders_`. On export, append untouched passthrough
values after editable polygons and allocate new IDs above the current maximum:

```cpp
int next_id = 1;
for (const MapCollider& collider : passthrough_colliders_) {
    next_id = std::max(next_id, collider.id + 1);
    map.colliders.push_back(collider);
}
for (const EditorPolygon& polygon : polygons_) {
    map.colliders.push_back({
        .id = next_id++,
        .type = polygon.type,
        .geometry = PolygonGeometry{polygon.points},
    });
}
```

- [ ] **Step 3: Adapt renderer polygon fallback**

Replace `polygons_for_screen` traversal with `colliders_for_screen`; render only
`ConvexPolygon` alternatives in the legacy procedural path:

```cpp
for (const WorldCollider& collider : *colliders) {
    const auto* polygon = std::get_if<ConvexPolygon>(&collider.geometry);
    if (polygon == nullptr) continue;
    if (collider.type != ColliderType::hazard && is_axis_rect(*polygon)) {
        draw_textured_rect(texture, fill, *polygon, camera.world_top, terrain_tint);
    } else {
        draw_convex_polygon(
            *polygon, camera.world_top,
            polygon_fill_color(collider.type, camera.biome));
    }
}
```

Curved WYSIWYG art arrives in Plan 3; debug outlines may use Raylib primitives
immediately.

- [ ] **Step 4: Adapt solver and test factory to polygon alternatives**

Until Plan 2 adds curved samples, make polygon extraction explicit:

```cpp
for (const WorldCollider& collider : *colliders) {
    const auto* polygon = std::get_if<ConvexPolygon>(&collider.geometry);
    if (polygon == nullptr || collider.type != ColliderType::solid) continue;
    // Existing horizontal-surface extraction from polygon->points.
}
```

Update `tests/test_world_factory.hpp` helpers to build `MapCollider` values with
stable IDs.

- [ ] **Step 5: Run compile and regression suite**

```bash
rtk cmake --build build/cmake --parallel
rtk ctest --test-dir build/cmake --output-on-failure
```

Expected: all current tests and new schema/shape tests pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/editor.hpp src/editor.cpp src/renderer.cpp \
  src/solver.cpp tests
rtk git commit -m "refactor(world): migrate consumers to shape colliders"
```

### Task 6: Add runtime map validation and final integration gates

**Files:**
- Create: `include/jumpcastle/map_validation.hpp`
- Create: `src/map_validation.cpp`
- Create: `tests/map_validation_test.cpp`
- Modify: `src/map_format.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `validate_screen_map(const ScreenMap&, const MapValidationContext&)`.
- Consumes: authored schema v3 contract from Tasks 1–2.

- [ ] **Step 1: Write failing validation tests**

```cpp
TEST_CASE("validator rejects duplicate ids and out-of-bounds geometry") {
    ScreenMap map;
    map.index = 0;
    map.width = 28;
    map.height = 36;
    map.colliders = {
        {.id = 3, .geometry = CircleGeometry{{27.5F, 10.0F}, 1.0F}},
        {.id = 3, .geometry = PolygonGeometry{{{2,2},{4,2},{4,4}}}},
    };
    CHECK_THROWS_WITH(
        validate_screen_map(map, {.atlas_columns = 24, .atlas_rows = 8}),
        Catch::Matchers::ContainsSubstring("duplicate collider id 3"));
}

TEST_CASE("validator rejects a GID outside the atlas") {
    ScreenMap map;
    map.width = 1;
    map.height = 1;
    map.tiles.terrain = {.columns = 1, .rows = 1, .gids = {193}};
    CHECK_THROWS_WITH(
        validate_screen_map(map, {.atlas_columns = 24, .atlas_rows = 8}),
        Catch::Matchers::ContainsSubstring("terrain GID 193 exceeds atlas tile count 192"));
}
```

- [ ] **Step 2: Declare and implement validation context**

```cpp
struct MapValidationContext {
    int atlas_columns{};
    int atlas_rows{};
};

void validate_screen_map(
    const ScreenMap& map,
    const MapValidationContext& context);
```

Validate finite/positive dimensions, bounds, unique IDs, geometry invariants,
tile dimensions, masked GID range, entity bounds, and one-way geometry type.
Use helpers named `validate_geometry`, `validate_layer`, and `validate_entity`
so each error includes screen index and object/layer identity.

- [ ] **Step 3: Call structural validation after parse**

Call validation with atlas-independent checks from `parse_screen_map`; call
atlas-dependent checks after `AssetCatalog` supplies atlas dimensions during
`CampaignWorld::load`. Do not hard-code 192 in production code.

- [ ] **Step 4: Run the complete verification set**

```bash
rtk cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DJUMPCASTLE_BUILD_TESTS=ON
rtk cmake --build build/cmake --parallel
rtk ctest --test-dir build/cmake --output-on-failure
rtk ./build/cmake/jumpcastle_solver \
  --levels assets/levels/screens --campaign \
  --trace build/campaign-route-schema-v3.json
```

Expected:

- All tests pass.
- The unchanged polygon campaign remains reachable.
- The new trace is written successfully.

- [ ] **Step 5: Review task diff and commit only with explicit authorization**

```bash
rtk git diff --check
rtk git status --short
rtk git add include/jumpcastle/map_validation.hpp src/map_validation.cpp \
  tests/map_validation_test.cpp src/map_format.cpp CMakeLists.txt
rtk git commit -m "feat(map): validate runtime shape maps"
```

## Plan 1 Completion Gate

- Schema v1/v2 maps load unchanged.
- Schema v3 polygon/circle/capsule maps round-trip without geometry loss.
- Exact circle/capsule contacts return finite normalized normals.
- `CollisionWorld` resolves all three geometry families.
- Existing campaign still builds, solves, and replays.
- Full CTest suite passes.
