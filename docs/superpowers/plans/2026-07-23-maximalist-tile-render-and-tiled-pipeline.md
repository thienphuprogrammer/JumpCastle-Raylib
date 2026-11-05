# Maximalist Tile Render and Tiled Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the biome-complete slope/round/arch/decor atlas, render four WYSIWYG tile layers plus deterministic animations through Raylib, and round-trip the complete schema v3 authoring contract through Tiled.

**Architecture:** The deterministic asset builder owns every atlas cell and emits semantic region/GID/animation metadata. `AssetCatalog` validates and exposes that metadata; the renderer draws named layers in a fixed order around entities/player. Tiled uses the same atlas, GIDs, Terrain Sets, object templates, and shape classes, while converter/validator tooling prevents visual-collision drift and unlicensed assets.

**Tech Stack:** Python 3, Pillow, C++20, raylib 5.5, nlohmann/json, Tiled 1.10 `.tmj/.tsx/.tx`, pytest, Catch2, CMake.

## Global Constraints

- Depends on Plan 1 `TileLayers` and schema v3 `MapCollider` contract.
- Depends on Plan 2 contact/debug data but does not change physics.
- Render order is parallax → background → terrain → decor → entities/player →
  foreground → UI/debug.
- Structural tile layers render with `WHITE` tint and the same atlas/GIDs as Tiled.
- Atlas generation is deterministic; generated PNGs are never edited by hand.
- Runtime animation time derives from fixed-step campaign time.
- Unknown or unverified asset sources fail the asset build.
- Foreground may not obscure solver-certified landing zones.
- Preserve dirty-worktree changes; commit only with explicit authorization.

---

## File Structure

- Modify `assets/source-selection.json`: approved source rectangles and derived structural roles.
- Modify `assets/sources/*/SOURCE.md`, `THIRD_PARTY_ASSETS.md`: provenance for every selected pack.
- Modify `tools/build_assets.py`: expanded atlas, deterministic masks, animation records.
- Create `tools/shape_tiles.py`: pure slope/round/arch mask generation.
- Create `tests/test_shape_tiles.py`: deterministic pixel/mask tests.
- Modify `assets/generated/castle.png` and `assets/generated/manifest.json`: generated outputs.
- Modify `include/jumpcastle/assets.hpp` and `src/assets.cpp`: tile animation and atlas metadata.
- Modify `include/jumpcastle/tile_view.hpp`, `src/renderer.cpp`, and `include/jumpcastle/renderer.hpp`: animated named-layer draw path.
- Modify `src/game.cpp`: pass fixed-step presentation time.
- Modify `tools/tiled_convert.py`: four layers, schema v3 shapes, stable IDs.
- Modify `assets/levels/tiled/castle.tsx`: Terrain Sets and tile animations.
- Create `assets/levels/tiled/templates/*.tx`: collider/entity stamps.
- Create `tools/validate_shape_map.py`: visual/collision, foreground, GID, and provenance checks.
- Modify `tests/test_tiled_convert.py`, `tests/test_asset_selection.py`,
  `tests/assets_test.cpp`, `tests/tile_view_test.cpp`,
  `tests/test_render_asset_smoke.py`, and `CMakeLists.txt`.

### Task 1: Define and verify the complete tile-role vocabulary

**Files:**
- Modify: `assets/source-selection.json`
- Modify: `assets/sources/castle-tileset/SOURCE.md`
- Modify: `THIRD_PARTY_ASSETS.md`
- Modify: `tests/test_asset_selection.py`

**Interfaces:**
- Produces: per-biome role names consumed by `build_assets.py`.
- Consumes: only source files whose provenance/license records are present.

- [ ] **Step 1: Add a failing vocabulary/provenance test**

Add:

```python
STRUCTURAL_ROLES = {
    "top_left", "top", "top_right", "left", "center", "right",
    "bottom_left", "bottom", "bottom_right",
    "slope_45_left", "slope_45_right",
    "slope_gentle_left", "slope_gentle_right",
    "slope_steep_left", "slope_steep_right",
    "round_convex_tl", "round_convex_tr",
    "round_concave_tl", "round_concave_tr",
    "capsule_left", "capsule_right",
    "arch_left", "arch_mid", "arch_right", "arch_keystone",
    "half_top", "half_bottom", "pillar", "ledge",
}

DECOR_ROLES = {
    "hazard", "detail_1", "detail_2", "crack", "surface_wear",
    "vine", "chain", "banner", "window", "torch",
}

def test_every_biome_has_the_maximalist_role_contract():
    selection = json.loads(Path("assets/source-selection.json").read_text())
    for biome, regions in selection["biomes"].items():
        assert STRUCTURAL_ROLES <= regions.keys(), biome
        assert DECOR_ROLES <= regions.keys(), biome

def test_every_selected_file_has_a_declared_approved_source():
    selection = json.loads(Path("assets/source-selection.json").read_text())
    selected = {
        region["file"]
        for regions in selection["biomes"].values()
        for region in regions.values()
        if "file" in region
    }
    approved = {record["file"] for record in selection["approved_sources"]}
    assert selected <= approved
```

- [ ] **Step 2: Run the test and confirm missing roles**

```bash
rtk .venv/bin/python -m pytest tests/test_asset_selection.py -q
```

Expected: failure listing the new slope/round/arch/decor roles.

- [ ] **Step 3: Separate cropped roles from derived roles**

Represent derived roles explicitly:

```json
{
  "slope_45_left": {"derive": "mask:slope_45_left", "base": "center"},
  "round_convex_tl": {"derive": "mask:round_convex_tl", "base": "center"},
  "capsule_left": {"derive": "mask:capsule_left", "base": "center"}
}
```

Cropped props continue to use:

```json
{
  "torch": {
    "file": "castle_tileset_part2.png",
    "x": 64,
    "y": 512,
    "w": 16,
    "h": 16
  }
}
```

Do not invent crop coordinates: verify every rectangle against the source image
contact sheets before adding it.

- [ ] **Step 4: Add explicit approved-source records**

Use:

```json
{
  "approved_sources": [
    {
      "file": "castle_tileset_part2.png",
      "source_record": "assets/sources/castle-tileset/SOURCE.md",
      "license": "CC0-1.0"
    },
    {
      "file": "castle_tileset_part3.png",
      "source_record": "assets/sources/castle-tileset/SOURCE.md",
      "license": "CC0-1.0"
    },
    {
      "file": "winter_tileset.png",
      "source_record": "assets/sources/castle-tileset/SOURCE.md",
      "license": "CC0-1.0"
    },
    {
      "file": "gothic_tileset.png",
      "source_record": "assets/sources/castle-tileset/SOURCE.md",
      "license": "CC0-1.0"
    }
  ]
}
```

If documentary evidence does not support one of these records, remove that file
from selection instead of weakening the test.

- [ ] **Step 5: Run the asset selection test**

```bash
rtk .venv/bin/python -m pytest tests/test_asset_selection.py -q
```

Expected: role and provenance tests pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add assets/source-selection.json assets/sources/castle-tileset/SOURCE.md \
  THIRD_PARTY_ASSETS.md tests/test_asset_selection.py
rtk git commit -m "feat(assets): define maximalist tile vocabulary"
```

### Task 2: Generate deterministic slope, round, capsule, and arch tiles

**Files:**
- Create: `tools/shape_tiles.py`
- Create: `tests/test_shape_tiles.py`
- Modify: `tools/build_assets.py`
- Modify: `tools/requirements-assets.txt`

**Interfaces:**
- Produces: `derive_structural_tile(base, role, edge_color)`.
- Consumes: 16×16 RGBA biome base tiles.

- [ ] **Step 1: Write exact mask tests**

Create:

```python
from PIL import Image

from tools.shape_tiles import derive_structural_tile, structural_mask


def test_slope_45_left_mask_has_diagonal_solid_area():
    mask = structural_mask("slope_45_left", 16)
    assert mask.getpixel((0, 15)) == 255
    assert mask.getpixel((15, 0)) == 255
    assert mask.getpixel((0, 0)) == 0
    assert sum(mask.getdata()) // 255 == 136


def test_round_convex_quadrants_are_mirrors():
    left = structural_mask("round_convex_tl", 16)
    right = structural_mask("round_convex_tr", 16)
    assert left.transpose(Image.Transpose.FLIP_LEFT_RIGHT).tobytes() == right.tobytes()


def test_derivation_is_byte_stable():
    base = Image.new("RGBA", (16, 16), (80, 100, 70, 255))
    a = derive_structural_tile(base, "capsule_left", (180, 200, 150, 255))
    b = derive_structural_tile(base, "capsule_left", (180, 200, 150, 255))
    assert a.tobytes() == b.tobytes()
```

- [ ] **Step 2: Run and verify missing module**

```bash
rtk .venv/bin/python -m pytest tests/test_shape_tiles.py -q
```

Expected: import failure for `tools.shape_tiles`.

- [ ] **Step 3: Implement integer-only masks**

Create `tools/shape_tiles.py` with:

```python
from __future__ import annotations

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps


def structural_mask(role: str, size: int = 16) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    last = size - 1
    if role == "slope_45_left":
        draw.polygon([(0, last), (last, 0), (last, last)], fill=255)
    elif role == "slope_45_right":
        draw.polygon([(0, 0), (last, last), (0, last)], fill=255)
    elif role == "slope_gentle_left":
        draw.polygon([(0, last), (last, size // 2), (last, last)], fill=255)
    elif role == "slope_gentle_right":
        draw.polygon([(0, size // 2), (last, last), (0, last)], fill=255)
    elif role == "slope_steep_left":
        draw.polygon([(size // 2, last), (last, 0), (last, last)], fill=255)
    elif role == "slope_steep_right":
        draw.polygon([(0, 0), (size // 2, last), (0, last)], fill=255)
    elif role == "round_convex_tl":
        draw.ellipse((0, 0, size * 2 - 1, size * 2 - 1), fill=255)
    elif role == "round_convex_tr":
        return ImageOps.mirror(structural_mask("round_convex_tl", size))
    elif role == "round_concave_tl":
        return ImageChops.invert(structural_mask("round_convex_tl", size))
    elif role == "round_concave_tr":
        return ImageOps.mirror(structural_mask("round_concave_tl", size))
    elif role == "capsule_left":
        draw.pieslice((0, 0, size * 2 - 1, size - 1), 90, 270, fill=255)
    elif role == "capsule_right":
        draw.pieslice((-size, 0, size - 1, size - 1), -90, 90, fill=255)
    else:
        raise ValueError(f"unknown structural mask role: {role}")
    return mask


def derive_structural_tile(
    base: Image.Image, role: str, edge_color: tuple[int, int, int, int]
) -> Image.Image:
    mask = structural_mask(role, base.width)
    result = Image.new("RGBA", base.size)
    result.paste(base.convert("RGBA"), mask=mask)
    inner = mask.filter(ImageFilter.MinFilter(3))
    edge = ImageChops.subtract(mask, inner)
    result.paste(Image.new("RGBA", base.size, edge_color), mask=edge)
    return result
```

Extend tests to every declared derived role.

- [ ] **Step 4: Teach the atlas builder to resolve derived regions**

Add:

```python
def resolve_region_tile(
    regions: dict[str, dict], name: str, sources: SourceImages
) -> Image.Image:
    spec = regions[name]
    if "derive" not in spec:
        return fitted_tile(sources.crop(spec))
    operation, role = spec["derive"].split(":", 1)
    if operation != "mask":
        raise ValueError(f"unsupported derive operation: {operation}")
    base = fitted_tile(resolve_region_tile(regions, spec["base"], sources))
    edge_color = tuple(spec.get("edge_color", [210, 220, 180, 255]))
    return derive_structural_tile(base, role, edge_color)
```

Replace `fitted_tile(sources.crop(regions[name]))` in `build_castle`.

- [ ] **Step 5: Run derivation tests**

```bash
rtk .venv/bin/python -m pytest tests/test_shape_tiles.py \
  tests/test_asset_selection.py -q
```

Expected: all mask counts, symmetry, and determinism checks pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add tools/shape_tiles.py tools/build_assets.py \
  tests/test_shape_tiles.py tools/requirements-assets.txt
rtk git commit -m "feat(assets): derive curved structural tiles"
```

### Task 3: Expand the atlas and publish tile animation metadata

**Files:**
- Modify: `tools/build_assets.py`
- Modify: `assets/source-selection.json`
- Modify: `assets/generated/castle.png`
- Modify: `assets/generated/manifest.json`
- Modify: `tests/assets_manifest_test.cpp`
- Modify: `tools/verify_assets.py`

**Interfaces:**
- Produces: manifest schema v3 `tile_roles` and `tile_animations`.
- Consumes: Task 1 vocabulary and Task 2 derivation.

- [ ] **Step 1: Write a failing manifest contract test**

```cpp
TEST_CASE("manifest exposes structural roles and deterministic tile animations") {
    const AssetCatalog catalog = AssetCatalog::load(
        std::filesystem::path{JUMPCASTLE_SOURCE_DIR} /
        "assets/generated/manifest.json");
    for (const Biome biome : {
             Biome::pixel_adventure, Biome::kenney, Biome::kings_and_pigs}) {
        CHECK(catalog.tile_role(biome, "slope_45_left") > 0u);
        CHECK(catalog.tile_role(biome, "round_convex_tl") > 0u);
        CHECK(catalog.tile_role(biome, "arch_keystone") > 0u);
    }
    const auto* torch = catalog.tile_animation("torch");
    REQUIRE(torch != nullptr);
    CHECK(torch->fps > 0);
    CHECK(torch->gids.size() >= 2);
}
```

- [ ] **Step 2: Define manifest records**

Emit:

```json
{
  "schema_version": 3,
  "atlases": {
    "castle": {
      "columns": 36,
      "rows": 12,
      "tile_size": 16,
      "biomes": {
        "courtyard": {
          "tile_roles": {"slope_45_left": 1, "round_convex_tl": 2}
        }
      },
      "tile_animations": {
        "torch": {"fps": 8, "gids": [301, 302, 303, 304]}
      }
    }
  }
}
```

Compute columns/rows from the generated image; never hard-code `36×12` in C++.
GIDs are one-based global atlas cells.

- [ ] **Step 3: Make layout assignment deterministic**

Use a constant ordered tuple:

```python
STRUCTURAL_ROLE_ORDER = (
    "top_left", "top", "top_right", "left", "center", "right",
    "bottom_left", "bottom", "bottom_right",
    "slope_45_left", "slope_45_right",
    "slope_gentle_left", "slope_gentle_right",
    "slope_steep_left", "slope_steep_right",
    "round_convex_tl", "round_convex_tr",
    "round_concave_tl", "round_concave_tr",
    "capsule_left", "capsule_right",
    "arch_left", "arch_mid", "arch_right", "arch_keystone",
    "half_top", "half_bottom", "pillar", "ledge",
    "hazard", "detail_1", "detail_2", "crack", "surface_wear",
    "vine", "chain", "banner", "window", "torch",
)
```

Allocate biome blocks in fixed `BIOME_ORDER`; derive all GIDs from final atlas
coordinates.

- [ ] **Step 4: Regenerate and verify byte stability**

```bash
rtk .venv/bin/python tools/build_assets.py \
  --downloads assets/sources/downloads \
  --selection assets/source-selection.json \
  --output assets/generated \
  --manifest assets/generated/manifest.json
rtk shasum -a 256 assets/generated/castle.png assets/generated/manifest.json
rtk .venv/bin/python tools/build_assets.py \
  --downloads assets/sources/downloads \
  --selection assets/source-selection.json \
  --output assets/generated \
  --manifest assets/generated/manifest.json
rtk shasum -a 256 assets/generated/castle.png assets/generated/manifest.json
```

Expected: both pairs of hashes are identical.

- [ ] **Step 5: Run asset verification**

```bash
rtk .venv/bin/python tools/verify_assets.py assets/generated/manifest.json
rtk .venv/bin/python tools/verify_asset_pixels.py assets/generated/manifest.json
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[assets]"
```

Expected: manifest, pixel, and catalog tests pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add tools/build_assets.py assets/source-selection.json \
  assets/generated/castle.png assets/generated/manifest.json \
  tests/assets_manifest_test.cpp tools/verify_assets.py
rtk git commit -m "feat(assets): publish maximalist terrain atlas"
```

### Task 4: Load tile roles and animations in `AssetCatalog`

**Files:**
- Modify: `include/jumpcastle/assets.hpp`
- Modify: `src/assets.cpp`
- Modify: `tests/assets_test.cpp`

**Interfaces:**
- Produces: `TileAnimation`, `AssetCatalog::tile_role`,
  `tile_animation_for_gid`, `tile_animation`.
- Consumes: manifest schema v3 from Task 3.

- [ ] **Step 1: Write failing catalog lookup tests**

```cpp
TEST_CASE("catalog resolves roles and animation frames by GID") {
    const AssetCatalog catalog = AssetCatalog::load(manifest_path());
    const std::uint32_t slope =
        catalog.tile_role(Biome::pixel_adventure, "slope_45_left");
    REQUIRE(slope > 0u);
    const TileAnimation* animation = catalog.tile_animation("torch");
    REQUIRE(animation != nullptr);
    REQUIRE(animation->gids.size() >= 2);
    CHECK(catalog.tile_animation_for_gid(animation->gids.front()) == animation);
}
```

- [ ] **Step 2: Add public catalog types**

```cpp
struct TileAnimation {
    std::string name;
    int fps{};
    std::vector<std::uint32_t> gids;
};

[[nodiscard]] std::uint32_t tile_role(
    Biome biome, std::string_view role) const;
[[nodiscard]] const TileAnimation* tile_animation(
    std::string_view name) const noexcept;
[[nodiscard]] const TileAnimation* tile_animation_for_gid(
    std::uint32_t gid) const noexcept;
```

Store per-biome `std::unordered_map<std::string, std::uint32_t>` and an ordered
animation vector plus GID-to-animation index map.

- [ ] **Step 3: Parse and validate manifest metadata**

Reject zero/out-of-range GIDs, empty frames, nonpositive FPS, duplicate
animation names, and a GID assigned to multiple animations. Preserve schema v2
loading by leaving the new collections empty.

- [ ] **Step 4: Run asset tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[assets]"
```

Expected: old atlas/animation tests and new tile metadata tests pass.

- [ ] **Step 5: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/assets.hpp src/assets.cpp tests/assets_test.cpp
rtk git commit -m "feat(assets): expose tile roles and animations"
```

### Task 5: Render named layers and deterministic animated tiles

**Files:**
- Modify: `include/jumpcastle/tile_view.hpp`
- Modify: `include/jumpcastle/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `src/game.cpp`
- Modify: `tests/tile_view_test.cpp`
- Modify: `tests/presentation_test.cpp`

**Interfaces:**
- Consumes: Plan 1 `TileLayers`, Task 4 `TileAnimation`.
- Produces: `animated_tile_gid`, correct runtime render order.

- [ ] **Step 1: Write pure animation frame tests**

```cpp
TEST_CASE("animated_tile_gid advances on deterministic fixed time") {
    const TileAnimation animation{
        .name = "torch", .fps = 4, .gids = {10, 11, 12, 13}};
    CHECK(animated_tile_gid(animation, 0.00) == 10u);
    CHECK(animated_tile_gid(animation, 0.24) == 10u);
    CHECK(animated_tile_gid(animation, 0.25) == 11u);
    CHECK(animated_tile_gid(animation, 1.00) == 10u);
}
```

- [ ] **Step 2: Implement pure GID animation**

```cpp
[[nodiscard]] inline std::uint32_t animated_tile_gid(
    const TileAnimation& animation, const double seconds) noexcept {
    if (animation.fps <= 0 || animation.gids.empty()) return 0u;
    const auto frame = static_cast<std::size_t>(
        std::floor(seconds * static_cast<double>(animation.fps)));
    return animation.gids[frame % animation.gids.size()];
}
```

- [ ] **Step 3: Extend renderer signatures**

```cpp
void draw(
    const CampaignWorld& world,
    const CameraBand& camera,
    const PlayerState& player,
    const CampaignState& campaign,
    float respawn_animation_time,
    double presentation_time,
    bool debug_enabled) const;
```

Pass `campaign.elapsed_seconds` from `Game::run`. For completion freeze avoidance,
add `double presentation_time_` to `Game` and increment it by each consumed fixed
tick; pass that value instead of wall-clock frame time.

- [ ] **Step 4: Resolve animation inside tile drawing**

Update `draw_tile_layer`:

```cpp
std::uint32_t drawn_gid = gid;
if (const TileAnimation* animation =
        catalog.tile_animation_for_gid(gid & 0x1FFFFFFFu)) {
    const std::uint32_t frame = animated_tile_gid(*animation, presentation_time);
    drawn_gid = (gid & 0xE0000000u) | frame;
}
const TileCell cell =
    tile_source_cell(drawn_gid, atlas_columns, atlas_tile_size);
```

Preserve horizontal/vertical Tiled flip bits across animation frames.

- [ ] **Step 5: Split screen-layer drawing around the player**

Use:

```cpp
draw_parallax_background(...);
draw_tile_layer(screen->tiles.background, ..., presentation_time);
draw_tile_layer(screen->tiles.terrain, ..., presentation_time);
draw_tile_layer(screen->tiles.decor, ..., presentation_time);
draw_goal(...);
draw_player(...);
draw_tile_layer(screen->tiles.foreground, ..., presentation_time);
```

If `terrain` is empty, draw the legacy polygon fallback between background and
decor. Make `draw_editor` use the same named-layer function.

- [ ] **Step 6: Run tile and renderer tests**

```bash
rtk cmake --build build/cmake --parallel
rtk ./build/cmake/jumpcastle_tests "[tile_view]"
rtk ctest --test-dir build/cmake -R "presentation|render|asset" \
  --output-on-failure
```

Expected: deterministic frame resolution and existing pixel presentation pass.

- [ ] **Step 7: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/tile_view.hpp include/jumpcastle/renderer.hpp \
  src/renderer.cpp src/game.cpp tests/tile_view_test.cpp \
  tests/presentation_test.cpp
rtk git commit -m "feat(render): draw animated named tile layers"
```

### Task 6: Round-trip v3 layers and semantic shapes through Tiled

**Files:**
- Modify: `tools/tiled_convert.py`
- Modify: `tests/test_tiled_convert.py`
- Modify: `assets/levels/tiled/castle.tsx`
- Create: `assets/levels/tiled/templates/solid-circle.tx`
- Create: `assets/levels/tiled/templates/solid-capsule.tx`
- Create: `assets/levels/tiled/templates/slope-left.tx`
- Create: `assets/levels/tiled/templates/spawn.tx`
- Create: `assets/levels/tiled/templates/goal.tx`

**Interfaces:**
- Consumes/produces: schema v3 JSON from Plan 1.
- Produces: stable `.tmj` layer order and semantic geometry.

- [ ] **Step 1: Write failing four-layer and shape conversion tests**

```python
def test_v3_round_trip_preserves_layers_and_shapes():
    screen = make_v3_screen(
        colliders=[
            {"id": 4, "type": "solid", "geometry": "circle",
             "tag": "round_platform", "center": [8, 9], "radius": 2},
            {"id": 5, "type": "solid", "geometry": "capsule",
             "tag": "bridge", "a": [4, 12], "b": [16, 12], "radius": 1},
        ],
        tiles={
            "background": grid_with_gid(10),
            "terrain": grid_with_gid(20),
            "decor": grid_with_gid(30),
            "foreground": grid_with_gid(40),
        },
    )
    tiled = screen_map_to_tiled(screen)
    assert [layer["name"] for layer in tiled["layers"]] == [
        "background", "terrain", "decor", "foreground",
        "collision", "entities",
    ]
    assert tiled_to_screen_map(tiled) == screen


def test_non_square_tiled_ellipse_is_rejected():
    tiled = make_tmj_with_collision_object({
        "id": 7, "ellipse": True, "x": 32, "y": 32,
        "width": 64, "height": 32, "class": "solid",
    })
    with pytest.raises(ValueError, match="object 7.*use a capsule template"):
        tiled_to_screen_map(tiled)
```

- [ ] **Step 2: Emit four tile layers in fixed order**

Use:

```python
VISUAL_LAYERS = ("background", "terrain", "decor", "foreground")

layers = [
    _tilelayer(index + 1, name, width, height,
               screen.get("tiles", {}).get(name, []))
    for index, name in enumerate(VISUAL_LAYERS)
]
layers.extend([
    collision_object_layer(screen["colliders"]),
    entity_object_layer(screen["entities"]),
])
```

- [ ] **Step 3: Convert Tiled objects to semantic geometry**

Rules:

- `ellipse: true`, equal width/height → circle center/radius in tile units.
- `ellipse: true`, unequal dimensions → explicit conversion error.
- object property `geometry == "capsule"` plus properties `ax`, `ay`, `bx`,
  `by`, `radius` → capsule.
- rectangle/polygon → polygon geometry.
- preserve object `id`, behavior class, and `tag`.

Implement the inverse mapping so circle emits a Tiled ellipse and capsule emits a
template-compatible object with the same properties.

- [ ] **Step 4: Add Terrain Sets and templates**

Update `castle.tsx` with one Terrain Set per biome for nine-slice cells and
explicit tile properties:

```xml
<tile id="72" class="structural">
 <properties>
  <property name="role" value="slope_45_left"/>
  <property name="biome" value="courtyard"/>
 </properties>
</tile>
```

The circle/capsule/slope `.tx` templates must use stable class/property names
matching the converter. Do not embed collision physics in the tileset.

Use these exact object definitions inside Tiled `<template>` roots:

```xml
<!-- solid-circle.tx -->
<object name="Solid Circle" class="solid" width="32" height="32">
 <properties><property name="tag" value="round_platform"/></properties>
 <ellipse/>
</object>

<!-- solid-capsule.tx -->
<object name="Solid Capsule" class="solid" width="48" height="16">
 <properties>
  <property name="geometry" value="capsule"/>
  <property name="ax" type="float" value="8"/>
  <property name="ay" type="float" value="8"/>
  <property name="bx" type="float" value="40"/>
  <property name="by" type="float" value="8"/>
  <property name="radius" type="float" value="8"/>
  <property name="tag" value="capsule_platform"/>
 </properties>
</object>

<!-- slope-left.tx -->
<object name="Slope Left" class="solid" width="32" height="16">
 <properties><property name="tag" value="slope_left"/></properties>
 <polygon points="0,16 32,0 32,16"/>
</object>

<!-- spawn.tx -->
<object name="Player Spawn" class="spawn">
 <point/>
</object>

<!-- goal.tx -->
<object name="Goal" class="trigger" width="32" height="16">
 <properties><property name="tag" value="goal"/></properties>
 <polygon points="0,16 32,16 32,0 0,0"/>
</object>
```

Each file also includes the XML declaration and `<template>` wrapper. Tiled
authors may move/resize the instance and change `tag`; converter tests must
prove that the instantiated values, not the template defaults, are exported.

- [ ] **Step 5: Run converter tests and round-trip all committed maps**

```bash
rtk .venv/bin/python -m pytest tests/test_tiled_convert.py -q
rtk .venv/bin/python tools/tiled_convert.py to-tiled \
  --input-dir assets/levels/screens \
  --output-dir build/tiled-roundtrip
rtk .venv/bin/python tools/tiled_convert.py from-tiled \
  --input-dir build/tiled-roundtrip \
  --output-dir build/screens-roundtrip
rtk diff -ru assets/levels/screens build/screens-roundtrip
```

Expected: tests pass; normalized JSON content is equivalent. If formatting
differs, compare parsed JSON in a test instead of weakening data equality.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add tools/tiled_convert.py tests/test_tiled_convert.py \
  assets/levels/tiled/castle.tsx assets/levels/tiled/templates
rtk git commit -m "feat(tiled): round-trip shape maps and visual layers"
```

### Task 7: Add visual-collision validation and real runtime capture tests

**Files:**
- Create: `tools/validate_shape_map.py`
- Create: `tests/test_validate_shape_map.py`
- Modify: `src/main.cpp`
- Modify: `tests/test_render_asset_smoke.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: CLI validator and `--smoke-all-screens DIR`.
- Consumes: solver trace landing anchors, map layers/colliders, real renderer.

- [ ] **Step 1: Write failing validator tests**

```python
def test_validator_reports_structural_gap_over_circle():
    screen = make_circle_screen()
    screen["tiles"]["terrain"] = empty_grid()
    errors = validate_screen(screen, atlas_tile_count=432, landing_points=[])
    assert any("collider 4 structural coverage gap" in error for error in errors)


def test_validator_reports_foreground_over_landing():
    screen = make_flat_screen()
    screen["tiles"]["foreground"][10][8] = 25
    errors = validate_screen(
        screen, atlas_tile_count=432,
        landing_points=[{"position": [8.5, 10.5], "collider_id": 2}],
    )
    assert any("foreground obscures landing" in error for error in errors)
```

- [ ] **Step 2: Implement deterministic raster checks**

Rasterize authored polygon/circle/capsule geometry at 4 subcells per tile.
Classify nonzero structural GIDs using manifest role metadata. Report:

- collider area lacking structural visual coverage;
- structural terrain with no solid/hazard collider;
- foreground alpha coverage over landing/player rectangles;
- GID/atlas mismatch;
- unknown source/license record.

Use exact CLI:

```bash
rtk .venv/bin/python tools/validate_shape_map.py \
  --screens assets/levels/screens \
  --manifest assets/generated/manifest.json \
  --trace assets/levels/campaign-route.json
```

- [ ] **Step 3: Add a real all-screen capture mode**

Extend `src/main.cpp`:

```cpp
// --smoke-all-screens DIR renders screen-00.png through screen-17.png
for (int screen = 0; screen < world.screen_count(); ++screen) {
    const float center_y =
        static_cast<float>(screen * world.screen_height) +
        static_cast<float>(world.screen_height) * 0.5F;
    PlayerState player{.position = {world.spawn.x, center_y}};
    const CameraBand camera = select_camera_band(world, center_y);
    Image frame = renderer.capture_screen(world, camera, player);
    ExportImage(frame, destination.string().c_str());
    UnloadImage(frame);
}
```

Use the real `Renderer::capture_screen`, not the Pillow composition helper.

- [ ] **Step 4: Assert runtime capture quality**

Update `tests/test_render_asset_smoke.py` to run the built binary, load all 18
PNGs, assert exact `448×576` size, nontrivial color count, nonblank alpha, and
different hashes for every adjacent screen. Retain the three-biome distinctness
assertion.

- [ ] **Step 5: Register and run full gates**

```bash
rtk cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON
rtk cmake --build build/cmake --parallel
rtk .venv/bin/python -m pytest tests/test_validate_shape_map.py \
  tests/test_render_asset_smoke.py -q
rtk ctest --test-dir build/cmake --output-on-failure
```

Expected: validator unit tests, 18-screen runtime captures, and full suite pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add tools/validate_shape_map.py tests/test_validate_shape_map.py \
  src/main.cpp tests/test_render_asset_smoke.py CMakeLists.txt
rtk git commit -m "test(render): validate shape visuals and runtime captures"
```

## Plan 3 Completion Gate

- Every biome exposes the approved structural/decor vocabulary.
- Atlas build and manifest are byte-stable and license-gated.
- Named/animated layers render in the approved order through raylib.
- Tiled round-trips every layer and shape without loss.
- Visual-collision and foreground validators pass.
- Real runtime capture produces valid images for all 18 screens.
- Full CTest/pytest asset and converter suites pass.
