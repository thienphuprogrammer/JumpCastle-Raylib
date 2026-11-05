# Eighteen-Screen Shape Campaign Redesign Implementation Plan

> **Status:** Ready for implementation after the three prerequisite plans
>
> **Prerequisites:**
> - `docs/superpowers/plans/2026-07-23-shape-collider-schema-and-collision.md`
> - `docs/superpowers/plans/2026-07-23-surface-normal-movement-solver-replay.md`
> - `docs/superpowers/plans/2026-07-23-maximalist-tile-render-and-tiled-pipeline.md`
>
> **Implementation rule:** Execute with `superpowers:test-driven-development`.
> Before claiming completion, execute `superpowers:verification-before-completion`.
> Commit steps are gated and may run only after the user explicitly authorizes commits.

## Goal

Replace the current narrow tower campaign with eighteen intentionally paced,
solver-validated rooms that use the complete schema-v3 terrain vocabulary:
polygons, true circles, capsules, one-way surfaces, slopes, half blocks,
animated tiles, and layered Fantasy Maximalist decoration.

Tiled `.tmj` files remain the authoring source of truth. Exported `.map.json`
files, previews, route metadata, and guides are generated artifacts.

## Non-goals

- No procedural runtime level generation.
- No hidden rectangle fallback for curved or sloped terrain.
- No decorative foreground that obscures the active player route.
- No replacement of the deterministic in-house collision engine.
- No combat, enemy AI, or new player abilities.

## Campaign contract

The playable order remains `screen-17` through `screen-00`.

| Screen | Room name | Biome | Intensity | Required geometry | Required tags |
|---|---|---|---:|---|---|
| 17 | Gatehouse Rise | courtyard | 1 | polygon slope | `spawn_floor`, `gentle_slope` |
| 16 | Banner Switchbacks | courtyard | 2 | two polygon slopes | `slope_left`, `slope_right` |
| 15 | Moonwell Steps | courtyard | 2 | circle + polygon | `round_step`, `landing` |
| 14 | Processional Arches | courtyard | 3 | capsule + half block | `arch_top`, `half_block` |
| 13 | Rampart Relay | courtyard | 4 | capsule + polygon slope | `capsule_bridge`, `steep_slope` |
| 12 | Courtyard Crown | courtyard | 5 | circle + capsule + polygon | `courtyard_exam` |
| 11 | Frosted Gallery | frost | 3 | one-way polygon + slope | `one_way_intro`, `snow_slope` |
| 10 | Icicle Traverse | frost | 4 | thin polygon + capsule | `thin_ledge`, `ice_capsule` |
| 09 | Frozen Orbits | frost | 5 | two circles | `circle_left`, `circle_right` |
| 08 | Avalanche Zigzag | frost | 6 | alternating polygon slopes | `zigzag_slope` |
| 07 | Crystal Needle | frost | 7 | capsule + one-way polygon | `needle`, `one_way_chain` |
| 06 | Winter Rosette | frost | 8 | circle + capsule + slope | `frost_exam` |
| 05 | Royal Workshop | crown | 5 | half blocks + one-way polygon | `precision_half`, `one_way_reset` |
| 04 | Gear Chapel | crown | 6 | three circles + polygon | `gear_arc`, `safe_landing` |
| 03 | Scepter Spine | crown | 7 | vertical capsule + slopes | `scepter`, `tangent_transfer` |
| 02 | Throne Switchbacks | crown | 8 | steep polygon slopes + circle | `throne_switchback` |
| 01 | Crown Mechanism | crown | 9 | circle + capsule + one-way polygon | `mastery_chain` |
| 00 | Celestial Crown | crown | 10 | all supported geometry | `goal`, `final_exam` |

The intensity intentionally rises and falls at biome boundaries:
`1,2,2,3,4,5,3,4,5,6,7,8,5,6,7,8,9,10`.

## Exact per-room route requirements

Coordinates below use world pixels in a `28 × 36` grid of `16 px` tiles. They
define mandatory route anchors, not every decorative tile. Each route anchor
must be reachable in order by the deterministic solver.

| Screen | Mandatory route anchors `(x, y)` | Exit |
|---|---|---|
| 17 | `(48,544) → (144,464) → (256,384) → (352,288)` | top |
| 16 | `(352,544) → (288,456) → (160,360) → (80,264)` | top |
| 15 | `(80,544) → (144,464) → (240,384) → (336,288)` | top |
| 14 | `(336,544) → (304,448) → (192,352) → (96,256)` | top |
| 13 | `(96,544) → (176,456) → (288,352) → (352,256)` | top |
| 12 | `(352,544) → (288,448) → (176,352) → (80,256)` | top |
| 11 | `(80,544) → (160,464) → (272,368) → (352,272)` | top |
| 10 | `(352,544) → (304,456) → (208,360) → (96,264)` | top |
| 09 | `(96,544) → (160,448) → (272,352) → (352,256)` | top |
| 08 | `(352,544) → (288,456) → (176,360) → (80,264)` | top |
| 07 | `(80,544) → (144,456) → (240,352) → (352,256)` | top |
| 06 | `(352,544) → (288,448) → (176,344) → (80,248)` | top |
| 05 | `(80,544) → (160,464) → (272,368) → (352,272)` | top |
| 04 | `(352,544) → (288,448) → (192,352) → (80,256)` | top |
| 03 | `(80,544) → (144,448) → (256,344) → (352,248)` | top |
| 02 | `(352,544) → (288,448) → (176,344) → (80,248)` | top |
| 01 | `(80,544) → (160,448) → (272,344) → (352,240)` | top |
| 00 | `(352,544) → (288,440) → (176,336) → (80,232) → (224,128)` | goal |

An anchor is satisfied when a support sample lies within `24 px` horizontally
and `20 px` vertically. The route may add intermediate samples, but may not
skip or reorder mandatory anchors.

## File structure

### Create

- `assets/levels/challenge-rooms.json`
- `tests/test_shape_campaign.cpp`

### Modify for all eighteen screens

- `assets/levels/tiled/screen-00.tmj` through `screen-17.tmj`
- `assets/levels/screens/screen-00.map.json` through `screen-17.map.json`
- `assets/levels/previews/screen-00.png` through `screen-17.png`
- `assets/levels/guides/screen-00.png` through `screen-17.png`
- `assets/levels/campaign-route.json`
- `tools/paint_screen.py`
- `CMakeLists.txt`

## Task 1: Lock the campaign metadata contract

### Step 1: Write the failing metadata test

Create `tests/test_shape_campaign.cpp`:

```cpp
#include "catch2/catch_test_macros.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

namespace {

const auto kRoot = std::filesystem::path{JUMPCASTLE_SOURCE_DIR};

nlohmann::json load_json(const std::filesystem::path& path) {
    std::ifstream stream{path};
    REQUIRE(stream.good());
    return nlohmann::json::parse(stream);
}

}  // namespace

TEST_CASE("challenge room manifest covers the complete descending campaign") {
    const auto manifest =
        load_json(kRoot / "assets/levels/challenge-rooms.json");

    REQUIRE(manifest.at("schema_version") == 1);
    REQUIRE(manifest.at("play_order").size() == 18);
    REQUIRE(manifest.at("rooms").size() == 18);

    std::set<int> screens;
    for (const auto& room : manifest.at("rooms")) {
        screens.insert(room.at("screen").get<int>());
        REQUIRE(room.at("route_anchors").size() >= 4);
        REQUIRE_FALSE(room.at("required_geometry").empty());
        REQUIRE_FALSE(room.at("required_tags").empty());
    }

    REQUIRE(screens == std::set<int>{
        0, 1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16, 17});
}
```

Add the test target in `CMakeLists.txt` using the existing test helper and
compile definition:

```cmake
add_executable(test_shape_campaign tests/test_shape_campaign.cpp)
target_link_libraries(test_shape_campaign PRIVATE Catch2::Catch2WithMain)
target_compile_definitions(
    test_shape_campaign
    PRIVATE JUMPCASTLE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME shape_campaign_contract COMMAND test_shape_campaign)
```

### Step 2: Run the test and confirm it fails

```bash
rtk cmake --build build --target test_shape_campaign
rtk ctest --test-dir build -R shape_campaign_contract --output-on-failure
```

Expected: failure because `assets/levels/challenge-rooms.json` does not exist.

### Step 3: Add the exact metadata

Create `assets/levels/challenge-rooms.json` with this top-level shape:

```json
{
  "schema_version": 1,
  "play_order": [
    17, 16, 15, 14, 13, 12,
    11, 10, 9, 8, 7, 6,
    5, 4, 3, 2, 1, 0
  ],
  "rooms": []
}
```

Populate `rooms` directly from the campaign-contract and route-anchor tables
above. Every room object must contain exactly:

```json
{
  "screen": 17,
  "name": "Gatehouse Rise",
  "biome": "courtyard",
  "intensity": 1,
  "required_geometry": ["polygon"],
  "required_tags": ["spawn_floor", "gentle_slope"],
  "route_anchors": [
    {"x": 48, "y": 544},
    {"x": 144, "y": 464},
    {"x": 256, "y": 384},
    {"x": 352, "y": 288}
  ],
  "exit": "top"
}
```

Screen `00` uses `"exit": "goal"`. Geometry values are limited to
`polygon`, `circle`, and `capsule`; one-way behavior belongs in tags.

### Step 4: Run the test and confirm it passes

```bash
rtk cmake --build build --target test_shape_campaign
rtk ctest --test-dir build -R shape_campaign_contract --output-on-failure
```

Expected: `shape_campaign_contract` passes.

### Step 5: Commit only with explicit authorization

```bash
rtk git add assets/levels/challenge-rooms.json tests/test_shape_campaign.cpp CMakeLists.txt
rtk git commit -m "test: define shape campaign contract"
```

## Task 2: Make campaign validation data-driven

### Step 1: Add failing validator fixtures

Extend `tests/test_shape_campaign.cpp`:

```cpp
TEST_CASE("exported rooms satisfy their shape and tag contracts") {
    const auto manifest =
        load_json(kRoot / "assets/levels/challenge-rooms.json");

    for (const auto& room : manifest.at("rooms")) {
        const auto number = room.at("screen").get<int>();
        const auto filename =
            "screen-" + (number < 10 ? std::string{"0"} : std::string{}) +
            std::to_string(number) + ".map.json";
        const auto map =
            load_json(kRoot / "assets/levels/screens" / filename);

        REQUIRE(map.at("schema_version") == 3);

        std::set<std::string> geometry;
        std::set<std::string> tags;
        for (const auto& collider : map.at("colliders")) {
            geometry.insert(collider.at("geometry").get<std::string>());
            if (collider.contains("tag")) {
                tags.insert(collider.at("tag").get<std::string>());
            }
        }

        for (const auto& required : room.at("required_geometry")) {
            CHECK(geometry.contains(required.get<std::string>()));
        }
        for (const auto& required : room.at("required_tags")) {
            CHECK(tags.contains(required.get<std::string>()));
        }
    }
}

TEST_CASE("every room has the complete maximalist layer stack") {
    const auto manifest =
        load_json(kRoot / "assets/levels/challenge-rooms.json");
    const std::array required_layers{
        "background", "terrain", "decor", "foreground"};

    for (const auto& room : manifest.at("rooms")) {
        const auto number = room.at("screen").get<int>();
        const auto filename =
            "screen-" + (number < 10 ? std::string{"0"} : std::string{}) +
            std::to_string(number) + ".map.json";
        const auto map =
            load_json(kRoot / "assets/levels/screens" / filename);

        for (const auto* layer : required_layers) {
            CAPTURE(filename, layer);
            REQUIRE(map.at("tiles").contains(layer));
        }
    }
}
```

### Step 2: Run the test and confirm the current maps fail

```bash
rtk cmake --build build --target test_shape_campaign
rtk ctest --test-dir build -R shape_campaign_contract --output-on-failure
```

Expected: current schema-v2 maps fail the version, geometry, tag, and layer
assertions.

### Step 3: Extend the Python checker

Modify `tools/paint_screen.py`:

```python
def validate_campaign_contract(
    root: Path,
    manifest: dict[str, object],
) -> list[str]:
    errors: list[str] = []
    for room in manifest["rooms"]:
        screen = int(room["screen"])
        path = root / "assets/levels/screens" / f"screen-{screen:02}.map.json"
        exported = json.loads(path.read_text(encoding="utf-8"))

        geometry = {
            collider["geometry"]
            for collider in exported["colliders"]
        }
        tags = {
            collider["tag"]
            for collider in exported["colliders"]
            if "tag" in collider
        }
        for required in room["required_geometry"]:
            if required not in geometry:
                errors.append(
                    f"screen-{screen:02}: missing geometry {required}"
                )
        for required in room["required_tags"]:
            if required not in tags:
                errors.append(f"screen-{screen:02}: missing tag {required}")

        for layer in ("background", "terrain", "decor", "foreground"):
            if layer not in exported["tiles"]:
                errors.append(f"screen-{screen:02}: missing layer {layer}")

    return errors
```

Call this function from `--check` after the existing structural checks. Load
the manifest from `assets/levels/challenge-rooms.json`. Print one line per
error and return a non-zero exit code if any error exists.

### Step 4: Keep the failure visible

```bash
rtk python3 tools/paint_screen.py --screens 00-17 --check
```

Expected: exact missing schema-v3 geometry, tag, and layer messages for the
not-yet-redesigned rooms.

### Step 5: Commit only with explicit authorization

```bash
rtk git add tools/paint_screen.py tests/test_shape_campaign.cpp
rtk git commit -m "test: validate campaign room contracts"
```

## Task 3: Author the Courtyard movement-teaching arc

Screens `17–12` teach one terrain idea at a time before combining them.

### Step 1: Author exact collider beats in Tiled

Use the schema-v3 Tiled templates from the render/pipeline plan. Each screen
must include the route-anchor sequence from the metadata plus these authored
beats:

| Screen | Collider beats in route order |
|---|---|
| 17 | spawn polygon → `26.6°` rising polygon → flat landing → exit polygon |
| 16 | `26.6°` rising polygon → `26.6°` falling polygon → flat reset → exit |
| 15 | circle radius `16` → circle radius `24` → flat landing → exit |
| 14 | horizontal capsule radius `8`, spine `32` → half block `16×8` → arch-top capsule → exit |
| 13 | horizontal capsule radius `8`, spine `48` → `45°` polygon → flat reset → exit |
| 12 | circle radius `24` → capsule radius `8`, spine `48` → `45°` polygon → exam landing |

Use stable collider IDs `sNN-route-00`, `sNN-route-01`, and so on in route
order. Add the required semantic tags from `challenge-rooms.json` to route
colliders; an additional `route` tag is not needed.

### Step 2: Paint the complete visual stack

For each screen:

- `background`: courtyard sky, distant wall silhouette, and banners;
- `terrain`: collision-aligned structural tiles;
- `decor`: columns, masonry variants, trim, chains, flags, and lamps;
- `foreground`: edge framing only, with no tile inside the `48 px`-wide
  corridor centered on each route-anchor segment.

Use at least:

- `6` terrain variants per screen;
- `4` decor families per screen;
- `2` animated-tile placements per screen;
- `3` silhouette depth bands across the six-screen biome.

### Step 3: Export and validate each room before continuing

Run after every screen, replacing `NN` with its two-digit number:

```bash
rtk python3 tools/tiled_convert.py \
  assets/levels/tiled/screen-NN.tmj \
  assets/levels/screens/screen-NN.map.json
rtk python3 tools/paint_screen.py --screens NN --check --preview
rtk ./build/campaign_solver --screen NN --manifest assets/levels/challenge-rooms.json
```

Expected for each screen:

- schema and layer validation pass;
- required geometry and tags are present;
- all route anchors pass in order;
- the solver exits through the top;
- no support contact uses an AABB fallback.

### Step 4: Run the biome gate

```bash
rtk python3 tools/paint_screen.py --screens 12-17 --check --preview
rtk ./build/campaign_solver --screens 12-17 \
  --manifest assets/levels/challenge-rooms.json
rtk ctest --test-dir build -R "shape_campaign|campaign_solver" \
  --output-on-failure
```

Expected: all six Courtyard screens and all relevant tests pass.

### Step 5: Commit only with explicit authorization

```bash
rtk git add \
  assets/levels/tiled/screen-{12..17}.tmj \
  assets/levels/screens/screen-{12..17}.map.json \
  assets/levels/previews/screen-{12..17}.png \
  assets/levels/guides/screen-{12..17}.png
rtk git commit -m "feat: redesign courtyard shape rooms"
```

## Task 4: Author the Frosted challenge arc

Screens `11–06` introduce one-way timing and denser curved transfers while
retaining generous reset landings.

### Step 1: Author exact collider beats in Tiled

| Screen | Collider beats in route order |
|---|---|
| 11 | one-way flat polygon → `26.6°` snow slope → broad reset → exit |
| 10 | `8 px`-high thin ledge → horizontal capsule radius `8`, spine `48` → reset → exit |
| 09 | circle radius `24` → circle radius `32` → flat reset → exit |
| 08 | four alternating `26.6°` polygon slopes → reset → exit |
| 07 | vertical capsule radius `8`, spine `48` → three one-way polygons → reset → exit |
| 06 | circle radius `32` → horizontal capsule radius `8`, spine `48` → `45°` polygon → exam landing |

All one-way colliders use `"type": "oneway"` with
`"geometry": "polygon"`; `oneway` is behavior, not a geometry type.

### Step 2: Paint the complete visual stack

Use frost-specific maximalist vocabulary:

- deep-blue masonry and snow caps on `terrain`;
- frozen windows, icicles, crystals, hanging braziers, and torn banners on
  `decor`;
- slow snow, crystal pulse, and brazier animation sequences;
- distant tower silhouettes and storm bands on `background`;
- sparse near icicles on `foreground`, outside route corridors.

Meet the same numerical density rules as Task 3.

### Step 3: Export and validate per screen

Run the three per-screen commands from Task 3 for screens `11` through `06`.
Additionally run:

```bash
rtk ./build/campaign_solver --screens 06-11 \
  --assert-one-way-descents \
  --manifest assets/levels/challenge-rooms.json
```

Expected: every intended upward pass through a one-way collider succeeds,
every descending landing produces an upward support normal, and no solver
state penetrates a curved collider by more than `0.01 px`.

### Step 4: Run the biome gate

```bash
rtk python3 tools/paint_screen.py --screens 06-11 --check --preview
rtk ./build/campaign_solver --screens 06-11 \
  --manifest assets/levels/challenge-rooms.json
rtk ctest --test-dir build -R \
  "shape_campaign|campaign_solver|one_way|surface_normal" \
  --output-on-failure
```

Expected: all six Frosted screens and relevant tests pass.

### Step 5: Commit only with explicit authorization

```bash
rtk git add \
  assets/levels/tiled/screen-{06..11}.tmj \
  assets/levels/screens/screen-{06..11}.map.json \
  assets/levels/previews/screen-{06..11}.png \
  assets/levels/guides/screen-{06..11}.png
rtk git commit -m "feat: redesign frosted shape rooms"
```

## Task 5: Author the Crown mastery arc

Screens `05–00` combine the learned geometry into increasingly precise route
chains. Every screen retains one broad recovery landing before its final beat.

### Step 1: Author exact collider beats in Tiled

| Screen | Collider beats in route order |
|---|---|
| 05 | two half blocks `16×8` → one-way reset → broad recovery → exit |
| 04 | circles radius `16`, `24`, `32` → flat recovery → exit |
| 03 | vertical capsule radius `8`, spine `64` → `26.6°` slope → `45°` slope → exit |
| 02 | two `45°` switchback polygons → circle radius `24` → recovery → exit |
| 01 | circle radius `24` → horizontal capsule radius `8`, spine `48` → two one-way polygons → recovery → exit |
| 00 | `26.6°` slope → circle radius `32` → capsule radius `8`, spine `64` → one-way polygon → crown goal polygon |

The screen-00 goal must be an authored polygon tagged `goal`, visually aligned
to the celestial crown dais. It must not be inferred from a tile GID.

### Step 2: Paint the complete visual stack

Use crown-specific maximalist vocabulary:

- gold-trimmed dark masonry, royal carpets, and mechanical insets on
  `terrain`;
- gears, stained glass, scepters, chains, braziers, and crown emblems on
  `decor`;
- animated gears, stained-glass shimmer, braziers, and crown radiance;
- throne-room, mechanism, and celestial-sky depth bands on `background`;
- arch and chain framing on `foreground`, outside route corridors.

Meet the density rules from Task 3. Screen `00` may use `8` terrain variants,
`6` decor families, and `4` animation placements, but the goal silhouette must
remain readable at gameplay zoom.

### Step 3: Export and validate per screen

Run the three per-screen commands from Task 3 for screens `05` through `00`.
For screen `00`, also run:

```bash
rtk ./build/campaign_solver --screen 00 \
  --require-goal \
  --manifest assets/levels/challenge-rooms.json
```

Expected: the solver touches the `goal` polygon after satisfying all five
mandatory route anchors.

### Step 4: Run the biome gate

```bash
rtk python3 tools/paint_screen.py --screens 00-05 --check --preview
rtk ./build/campaign_solver --screens 00-05 \
  --manifest assets/levels/challenge-rooms.json
rtk ctest --test-dir build -R \
  "shape_campaign|campaign_solver|surface_normal|replay" \
  --output-on-failure
```

Expected: all Crown screens and relevant tests pass.

### Step 5: Commit only with explicit authorization

```bash
rtk git add \
  assets/levels/tiled/screen-{00..05}.tmj \
  assets/levels/screens/screen-{00..05}.map.json \
  assets/levels/previews/screen-{00..05}.png \
  assets/levels/guides/screen-{00..05}.png
rtk git commit -m "feat: redesign crown shape rooms"
```

## Task 6: Regenerate campaign route data and prove end-to-end playability

### Step 1: Write the failing route consistency test

Extend `tests/test_shape_campaign.cpp`:

```cpp
TEST_CASE("campaign route mirrors the approved room manifest") {
    const auto manifest =
        load_json(kRoot / "assets/levels/challenge-rooms.json");
    const auto route =
        load_json(kRoot / "assets/levels/campaign-route.json");

    REQUIRE(route.at("version") == 2);
    REQUIRE(route.at("play_order") == manifest.at("play_order"));
    REQUIRE(route.at("rooms").size() == manifest.at("rooms").size());

    for (std::size_t index = 0; index < route.at("rooms").size(); ++index) {
        CHECK(route.at("rooms").at(index).at("screen") ==
              manifest.at("rooms").at(index).at("screen"));
        CHECK(route.at("rooms").at(index).at("name") ==
              manifest.at("rooms").at(index).at("name"));
        CHECK(route.at("rooms").at(index).at("route_anchors") ==
              manifest.at("rooms").at(index).at("route_anchors"));
    }
}
```

### Step 2: Run the test and confirm the old route fails

```bash
rtk cmake --build build --target test_shape_campaign
rtk ctest --test-dir build -R shape_campaign_contract --output-on-failure
```

Expected: the old route version or room metadata differs.

### Step 3: Regenerate the route and all derived visual artifacts

Add a `--campaign` mode to `tools/paint_screen.py`:

```python
def build_campaign_route(manifest: dict[str, object]) -> dict[str, object]:
    return {
        "version": 2,
        "play_order": manifest["play_order"],
        "rooms": [
            {
                "screen": room["screen"],
                "name": room["name"],
                "biome": room["biome"],
                "intensity": room["intensity"],
                "route_anchors": room["route_anchors"],
                "exit": room["exit"],
            }
            for room in manifest["rooms"]
        ],
    }
```

The CLI must:

1. load `assets/levels/challenge-rooms.json`;
2. write `assets/levels/campaign-route.json` using stable indentation and key
   order;
3. regenerate all eighteen previews;
4. regenerate all eighteen route guides from the route anchors;
5. run the same validations as `--check`;
6. return non-zero without replacing an existing artifact when validation
   fails.

Run:

```bash
rtk python3 tools/paint_screen.py --campaign --screens 00-17 \
  --check --preview
```

Expected: the route, previews, and guides regenerate successfully.

### Step 4: Run the deterministic full-campaign solver and replay

```bash
rtk ./build/campaign_solver \
  --campaign assets/levels/campaign-route.json \
  --manifest assets/levels/challenge-rooms.json \
  --write-replay build/shape-campaign.replay.json
rtk ./build/replay_verify build/shape-campaign.replay.json \
  --repeat 20 \
  --expect-final-screen 00 \
  --expect-goal
```

Expected:

- all `18/18` rooms solve;
- every mandatory route anchor is visited in order;
- screen `00` reaches the goal;
- all twenty replay hashes are identical;
- no contact penetration exceeds `0.01 px`;
- no unsupported state lasts longer than `0.20 s` unless the player is
  intentionally airborne.

### Step 5: Run the complete automated suite

```bash
rtk cmake --build build --parallel
rtk ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

## Task 7: Capture and inspect the native renderer

### Step 1: Capture representative and boundary rooms

Use the native renderer-smoke executable produced by the render/pipeline plan:

```bash
rtk ./build/renderer_smoke --screen 17 \
  --output build/captures/screen-17.png --frames 120
rtk ./build/renderer_smoke --screen 12 \
  --output build/captures/screen-12.png --frames 120
rtk ./build/renderer_smoke --screen 11 \
  --output build/captures/screen-11.png --frames 120
rtk ./build/renderer_smoke --screen 06 \
  --output build/captures/screen-06.png --frames 120
rtk ./build/renderer_smoke --screen 05 \
  --output build/captures/screen-05.png --frames 120
rtk ./build/renderer_smoke --screen 00 \
  --output build/captures/screen-00.png --frames 120
```

Expected: six PNG files rendered through raylib, covering every biome start and
end.

### Step 2: Inspect each capture against exact visual gates

For every capture confirm:

- the collision route is visually readable without debug overlays;
- slopes use slope art rather than stair-step flat tiles;
- circles and capsules have continuous silhouettes;
- terrain variants do not break collision alignment;
- animated frames stay within their assigned tile role;
- background, terrain, decor, and foreground have distinct depth;
- foreground does not overlap the player route corridor;
- screen `00` goal is unambiguous.

Any failed gate requires editing the Tiled source, re-exporting, rerunning that
screen's solver, and recapturing it.

### Step 3: Run performance smoke

```bash
rtk ./build/renderer_smoke --campaign \
  --frames-per-screen 600 \
  --assert-frame-ms-p99 8.33 \
  --assert-draw-calls-max 64
```

Expected at the reference desktop resolution:

- 99th-percentile frame time at or below `8.33 ms`;
- at most `64` draw calls per screen;
- no texture allocation after the first frame of a screen;
- no collision allocation during a fixed update.

### Step 4: Review the final diff without staging

```bash
rtk git status --short
rtk git diff --stat
rtk git diff --check
```

Expected:

- only approved production, map, test, tool, and generated-artifact paths are
  modified;
- `git diff --check` reports no whitespace errors;
- unrelated dirty-worktree files remain untouched and unstaged.

### Step 5: Final commit only with explicit authorization

```bash
rtk git add \
  assets/levels/challenge-rooms.json \
  assets/levels/campaign-route.json \
  assets/levels/tiled \
  assets/levels/screens \
  assets/levels/previews \
  assets/levels/guides \
  tools/paint_screen.py \
  tests/test_shape_campaign.cpp \
  CMakeLists.txt
rtk git commit -m "feat: deliver eighteen-room shape campaign"
```

## Completion criteria

This plan is complete only when:

- all eighteen Tiled sources export as schema v3;
- every room contains the approved geometry, tags, visual layers, and route
  anchors;
- every room is individually solver-valid;
- the complete descending campaign solves and reaches the screen-00 goal;
- replay v2 produces identical hashes across twenty runs;
- all CTest tests pass;
- six native raylib captures pass manual visual inspection;
- performance smoke meets the frame-time, draw-call, and allocation budgets;
- no unrelated dirty-worktree changes were overwritten, staged, or committed.
