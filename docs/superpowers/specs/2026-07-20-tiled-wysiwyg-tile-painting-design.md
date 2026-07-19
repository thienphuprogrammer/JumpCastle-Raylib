# Tiled WYSIWYG Tile Painting — Design Spec

- **Date:** 2026-07-20
- **Status:** Draft (awaiting user review)
- **Author:** pair session
- **Related:** `tools/tiled_convert.py`, `src/renderer.cpp`, `src/map_format.cpp`,
  `include/jumpcastle/map_format.hpp`, `assets/levels/tiled/*.tmj`

## 1. Overview

Today the game derives its terrain art *procedurally*: `draw_world_polygons`
([src/renderer.cpp:260](../../../src/renderer.cpp)) takes each collision polygon
and tiles a single "fill" brick across it. The map files
(`assets/levels/screens/screen-NN.map.json`) store **only** colliders and
entities — no tile data. Consequently, when a screen is opened in Tiled the user
sees only collision outlines, and any tiles they paint are discarded by the
converter (`tiled_convert.py` comment, lines 44–48: *"a painted tile layer … is
ignored"*).

This spec turns Tiled into a **true WYSIWYG level editor**: the user paints real
tiles onto a grid in Tiled, and the game renders **exactly** those tiles. It adds
a painted tile layer to the map format and a tile-layer renderer to the game,
while **preserving** the existing polygon+SAT collision system as a separate,
authoritative collision layer.

## 2. Goals / Non-Goals

### Goals
- Paint a tile grid in Tiled → the game draws precisely those tiles (same image,
  same tile indices, no runtime tint or "auto-derivation").
- Two-way conversion round-trips tile data losslessly (`.map.json ↔ .tmj`).
- Keep the existing polygon collision system (solid / one-way / hazard, incl.
  slopes) as the authoritative runtime collision source.
- Provide an **auto-seed** authoring tool that pre-populates the Tiled collision
  object layer from solid tiles, which the user then refines.
- Backward compatible: the 18 existing v1 screens keep working with no tiles
  (procedural fallback), and can be migrated incrementally.

### Non-Goals (YAGNI)
- No per-tile runtime collision (collision stays polygon-based; tiles are visual).
- No animated/auto-tiling tiles at runtime (paint explicitly; auto-tile variety
  is the user's choice at paint time).
- No diagonal-flip (anti-diagonal) tile support in v1 — horizontal/vertical flip
  only (covers >95% of painting needs).
- No multi-tileset-per-map in v1 — one shared atlas image, one `firstgid`. (The
  atlas already contains all three biomes side-by-side, so a screen still paints
  biome-appropriate tiles by choosing that region of the palette.)
- No separate per-biome tileset *files* in v1 — all biomes stay in the one
  enriched atlas. Splitting them out is deferred (§13).
- No hand-drawn new art — enrichment only *curates additional cells* from the
  existing CC0 source sheets via the build pipeline.

## 3. Current State (grounding)

- **Map schema v1** (`ScreenMap`, [map_format.hpp:27](../../../include/jumpcastle/map_format.hpp)):
  `index, width, height, biome, polygons[], entities[]`. On disk the JSON key is
  `colliders` (with `points`); `parse_screen_map` splits concave→convex.
- **Rendering:** `draw()` always uses `castle_texture_` (the
  `Biome::pixel_adventure` atlas = `assets/generated/castle.png`, 336×96 = 21×6
  tiles @16px, three biomes side-by-side). `draw_world_polygons` textures only
  axis-aligned rects with the biome's *center fill* cell, recolored per biome via
  `biome_terrain_tint`.
- **Converter:** `screen_map_to_tiled` writes only `collision` + `entities`
  object layers plus a *reference* tileset (so Tiled's palette shows the bricks);
  `tiled_to_screen_map` reads only object layers.

## 4. Design Overview

```
  Tiled (paint tiles + refine collision)
        │  screen-NN.tmj  (tile layer + object layers)
        ▼
  tools/tiled_convert.py   ◄── from-tiled / to-tiled (two-way)
        │  screen-NN.map.json  (schema_version 2: + tiles)
        ▼
  Game (parse_screen_map → ScreenMap.tiles → draw_tile_layer)
        └── renders EXACTLY the painted tiles (no tint, no derivation)
```

**Core WYSIWYG invariant:** Tiled and the game reference the **same tileset image
with the same layout** (tile size + column count + `firstgid`). Painting tile GID
`N` in Tiled makes the game draw the identical source cell. This is guaranteed by
an external Tiled tileset file (`.tsx`) shared by all maps and a matching
GID→source-rect computation in the renderer.

## 5. Tiled Project Structure

Each `screen-NN.tmj` has three layers, authored in this order (bottom→top):

| Layer name  | Tiled type   | Purpose                                             | Runtime use            |
|-------------|--------------|----------------------------------------------------|------------------------|
| `terrain`   | tile layer   | The solid/visible tiles. **You paint here.**       | Drawn; seeds collision |
| `collision` | object layer | Polygon solid/oneway/hazard (refined by hand)      | **Authoritative** physics |
| `entities`  | object layer | spawn / checkpoint / goal points                    | Spawn/goal/checkpoints |

Optional (v1.1, not required): a `background` tile layer for non-solid decor,
drawn behind `terrain` and **never** seeding collision.

**Shared tileset:** one external `assets/levels/tiled/castle.tsx` referencing
`../../generated/castle.png` (`firstgid = 1`, `tilewidth = tileheight = 16`,
`columns` = atlas width ÷ 16). All 18 maps reference this `.tsx` so GIDs are
consistent. `columns` is the **single source of truth** for GID math — it must
match the atlas image width and `ScreenMap.tileset_columns`; the renderer derives
it from the loaded texture so there is no hard-coded magic number to drift.

**Tileset choice — the enriched generated atlas (no file promotion):**
v1 paints from `assets/generated/castle.png`, the game's own curated 3-biome
orthogonal atlas. This file is **built by the existing pipeline**
(`build_assets.py` ← `source-selection.json`) and is **git-tracked and shipped**,
so nothing under the git-ignored `assets/sources/downloads/` needs promoting.
Rationale (superseding an earlier draft that reached for
`castle_tileset_part1.png`): that raw sheet is *isometric* and mostly empty —
unusable on an orthogonal grid. `generated/castle.png` is precisely the clean,
tileable extract of the good cells and is the correct palette for painting.

The atlas is intentionally **enriched first** (§5.1) so the painting palette is
rich enough to build attractive screens before any WYSIWYG work begins.

### 5.1 Prerequisite: enrich the terrain atlas

Today `terrain_layout()` emits a 7×5 grid per biome but only ~14 *unique* tiles
(rows repeat for auto-tiling). Enrichment expands the palette **through the
existing pipeline**, not by hand-editing images.

**Per-biome sources (finalized 2026-07-20 asset research).** No single sheet
covers all three biomes; each biome draws from its best-fit orthogonal source
(all under `assets/sources/downloads/`):

| Biome | Source sheet(s) | Notes |
|-------|-----------------|-------|
| `courtyard` | `castle_tileset_part3.png` (ortho stone floor block + wooden platform beams) + `castle_tileset_part2.png` (spikes/hazards, chains, ladders, decor) | **Repoints away from `castle_tileset_part1.png`** — that sheet is isometric and its current nine-slice in `source-selection.json` is **degenerate** (inner-corner / bottom / isolated collapse onto duplicate pixels). This is the single most important fix. Terrain/platform cells sit among multi-tile props at arbitrary offsets → they must be located precisely (cropped) when picking rects. |
| `frosted_keep` | `winter_tileset.png` (ansimuz) | Already the most structurally complete: full 3×3 body incl. corners + vertical pillars. Clean 16px grid, no spacing/margin. Enrich with hazards if a clean source exists. |
| `crown_spire` | `gothic_tileset.png` (ansimuz) | Clean, on-theme mossy stone; currently only ground + edge caps + a decor row. Add corner variants + a platform/ledge; hazards may borrow from `castle_tileset_part2.png`. |

**Critical:** fix the `courtyard` degenerate nine-slice first (correctness), then
enrich. Steps:

1. **Catalog clean cells:** write a throwaway helper that slices each source sheet
   above into a labeled 16-px grid image; visually pick clean, orthogonal,
   tileable cells. (`winter`/`gothic` are exact 16px grids; the `castle_part2/3`
   sheets mix grid tiles with props, so isolate carefully.)
2. **Rewrite `source-selection.json` per biome:** repoint `courtyard` to the
   ortho castle sources; keep `frosted_keep`/`crown_spire` on winter/gothic. Add
   new named slots — `inner_corner_*` (real, non-degenerate), `platform`,
   `platform_left/right`, `ledge`, `pillar`, `crack`, `moss`, `detail_1/2`,
   `hazard` (spikes) — each a `{file, x, y, w, h}` source rect.
   Target ≈ 20–30 unique tiles per biome (from ~14). Where a biome's clean-tile
   supply is thin, cap it there rather than forcing bad cells.
3. **Extend `build_assets.py`:** grow the atlas layout to place every unique tile
   in a clean grid (drop wasteful duplicates), and record each named region in
   `manifest.json` so the game (procedural fallback + props) still resolves them.
4. **Regenerate + verify:** rebuild `castle.png`/`manifest.json` deterministically
   (byte-stable output), then screenshot the atlas to confirm the new tiles read
   correctly. Existing biome/terrain regions used by the game must keep working.

Enrichment changes the atlas width → `columns` changes. Because the renderer and
`.tsx` derive `columns` from the image, this is absorbed automatically; the only
coupled value is `ScreenMap.tileset_columns`, written by the converter from the
atlas at export time.

## 6. Map Format v2

Additive, versioned change. `schema_version` bumps `1 → 2`.

```jsonc
{
  "schema_version": 2,
  "screen": { "index": 5, "width": 28, "height": 36 },
  "biome": "courtyard",
  "tileset": { "name": "castle", "columns": 21, "tile_size": 16 },  // columns = atlas width / 16 (updated after enrichment)
  "tiles": {
    "terrain": [
      [0, 0, 0, 0, ...],        // row 0: width entries, row-major, y-down
      [0, 23, 24, 25, 0, ...],  // GID 0 = empty; N>0 = tile (N-1) in the tileset
      ...                       // exactly `height` rows
    ]
  },
  "colliders": [ { "id": 1, "type": "solid", "points": [[x,y], ...] } ],
  "entities":  [ { "type": "spawn", "pos": [x, y] } ]
}
```

- **`tiles.terrain`**: a `height × width` array of **GIDs**. GID `0` = empty.
  The top 3 bits are Tiled flip flags (0x80000000 H, 0x40000000 V, 0x20000000
  anti-diagonal); the low 29 bits are the raw id. The tileset cell is
  `local = (gid & 0x1FFFFFFF) - firstgid` (with `firstgid = 1`, that is
  `id - 1`) — **flip bits must be masked off before indexing**. H/V flip flags
  are **preserved** through the pipeline and honored by the renderer; the
  anti-diagonal flag is stripped in v1.
- **`tileset`**: descriptive metadata so the renderer can compute GID→cell
  without re-reading the image header. `columns` and `tile_size` must match the
  loaded tileset texture. (Single global tileset in v1.)
- **`colliders` / `entities`**: unchanged from v1.

### Versioning / fallback rules
- Loader accepts `schema_version` 1 **or** 2.
- v1 file (no `tiles`) → `ScreenMap.terrain` empty → renderer falls back to the
  current procedural `draw_world_polygons` texturing. Nothing breaks.
- v2 file with a `terrain` layer → renderer draws tiles; procedural texturing of
  collider rects is **suppressed** (tiles are the visuals now). Goal sprite +
  optional collision-debug outlines still draw.
- A v2 file with an **empty** `terrain` grid behaves like v1 (procedural
  fallback), so migration can proceed screen-by-screen.

### C++ type change (`map_format.hpp`)
```cpp
struct TileLayer {
    int columns{};
    int rows{};
    std::vector<uint32_t> gids;   // row-major, size == columns*rows; 0 == empty
};

struct ScreenMap {
    int index{};
    float width{};
    float height{};
    std::string biome;
    std::vector<ConvexPolygon> polygons;
    std::vector<MapEntity> entities;
    TileLayer terrain;            // NEW; empty when absent (v1 fallback)
    int tileset_columns{21};      // NEW; from "tileset.columns"; renderer re-derives
    int tileset_tile_size{16};    // NEW;   from the loaded atlas width (post-enrich)
};
```
`parse_screen_map` reads `tiles.terrain` into `terrain.gids` (validating
`rows*columns == width*height`); `serialize_screen_map` writes it back. Both
tolerate its absence.

## 7. Runtime: Renderer + Assets

### Tileset texture
Reuse the already-loaded `castle_texture_` — it *is* `assets/generated/castle.png`,
the enriched atlas. The tile layer draws from it **without tint** (true WYSIWYG);
the same texture is still used tinted by the v1 procedural fallback and the
parallax backdrop. `tileset_columns` is derived once as
`castle_texture_.get().width / tile_size` and must equal `ScreenMap.tileset_columns`
(assert/log on mismatch). No new `TextureResource` is required.

### `draw_tile_layer` (new, in the anonymous namespace of `renderer.cpp`)
```cpp
// GID -> source cell in a `columns`-wide tileset (firstgid == 1).
//   id  = gid & 0x1FFFFFFF          // mask off the 3 flip bits
//   if (id == 0) skip               // empty cell
//   local = id - firstgid           // 0-based tile index
//   col = local % columns ; row = local / columns
//   srcRect = { col*ts, row*ts, ±ts, ±ts }   // signs from H/V flip bits
void draw_tile_layer(const TileLayer& layer, int tileset_columns,
                     int tile_size, const Texture2D& tileset,
                     float world_top);
```
- Iterate `row∈[0,rows) × col∈[0,cols)`. Skip GID 0.
- Strip/read flip flags; build a raylib source `Rectangle` (negative width/height
  flips, matching the existing `sprite_source_rectangle` trick).
- Destination = `world_to_screen({col, row}, world_top)` scaled by
  `config::tile_pixels`; draw with `DrawTexturePro`, tint `WHITE` (true WYSIWYG).

### Wiring in `Renderer::draw` and `draw_editor`
```cpp
draw_parallax_background(...);
const TileLayer& terrain = /* current screen's ScreenMap.terrain */;
if (!terrain.gids.empty())
    draw_tile_layer(terrain, tileset_cols, tile_size,
                    biome_texture, world_top);   // biome_texture == castle_texture_, no tint
else
    draw_world_polygons(world, camera, assets, biome_texture);  // v1 fallback
// goal sprite + (debug) collision outlines still draw regardless
```
The editor preview (`draw_editor`) uses the same path, so the in-game F1 editor
shows painted tiles too. `CampaignWorld` must expose the per-screen `TileLayer`
(add a `const TileLayer* tiles_for_screen(int) const;` alongside the existing
`polygons_for_screen`).

## 8. Converter: `tools/tiled_convert.py`

### `screen_map_to_tiled` (JumpCastle → Tiled)
- Emit a real `tilelayer`:
  ```jsonc
  { "type": "tilelayer", "name": "terrain", "width": W, "height": H,
    "data": [ ...W*H GIDs, row-major... ], "visible": true, "opacity": 1,
    "x": 0, "y": 0 }
  ```
  from `screen["tiles"]["terrain"]` (flatten rows). Empty/missing → all-zero data.
- Reference the shared external tileset: `"tilesets": [{ "firstgid": 1,
  "source": "castle.tsx" }]` (replaces the current embedded reference tileset).
- Keep writing `collision` + `entities` object layers unchanged.

### `tiled_to_screen_map` (Tiled → JumpCastle)
- **Read** the `terrain` tile layer's `data` back into
  `tiles.terrain` as a `height × width` 2D array (currently ignored). Preserve
  raw GIDs incl. flip flags.
- Keep reading colliders/entities from object layers exactly as today.
- Set `schema_version: 2` and the `tileset` block.

### `seed-collision` (new CLI subcommand)
`tiled_convert.py seed-collision --input-dir assets/levels/tiled [--force]`
- For each `.tmj`: read the `terrain` layer, treat every non-zero cell as solid,
  **greedy-merge** contiguous solid cells into maximal rectangles, and write them
  as `type: "solid"` rectangle objects into the `collision` object layer.
- Skip maps whose `collision` layer already has objects unless `--force` (never
  silently clobber hand-authored collision).
- Output is written **into the `.tmj`** so the user opens Tiled, sees the seeded
  boxes over their tiles, and refines (delete under decor, add slopes/hazards).
- This is an **authoring-time** convenience only; runtime collision always comes
  from the object layer via the normal `from-tiled` pass.

## 9. Migration of the 18 Existing Screens

1. Re-run `to-tiled` so each `.tmj` references `castle.tsx` and gains an (empty)
   `terrain` layer. Existing collision/entities are untouched → screens still
   play (procedural fallback while `terrain` is empty).
2. **Optional** one-time seed: generate a starter `terrain` grid from current
   collider rects (a `seed-tiles` helper, or paint by hand). This gives each
   screen a "pre-painted" look matching the old bricks, which the user then
   improves. Recommended but not blocking.
3. Paint/refine screen-by-screen. Each converted-back file becomes
   `schema_version: 2`.

Order chosen so **nothing is ever in a broken state**: fallback keeps every
unpainted screen rendering as before.

## 10. Testing

### Python (`tests/test_tiled_convert.py`)
- Round-trip a screen **with tiles**: `screen_map_to_tiled → tiled_to_screen_map`
  preserves the terrain grid exactly (incl. a flipped GID).
- `terrain` tile layer emitted with correct `width*height` data length and
  row-major order.
- Tiled → screen reads a hand-written `tilelayer` into `tiles.terrain`.
- `seed-collision`: a small solid tile blob → expected merged rectangle
  object(s); respects the no-clobber rule (existing objects untouched sans
  `--force`).
- Existing v1 behavior unchanged (no-tiles map still round-trips).

### C++ (`tests/map_format_test.cpp` + `tests/editor_test.cpp`)
- `parse_screen_map` reads a v2 JSON `tiles.terrain` into `ScreenMap.terrain`
  with correct dimensions and GIDs; rejects a grid whose size ≠ width*height.
- `serialize_screen_map` round-trips a `TileLayer` (parse∘serialize == identity).
- v1 JSON (no `tiles`) parses with an empty `terrain` (fallback path).
- GID→source-rect math (extract as a pure `tile_source_cell(gid, columns,
  tile_size)` free function so it is unit-testable without raylib).

### Asset pipeline (P0 enrichment)
- `build_assets.py` regenerates `castle.png` + `manifest.json` **deterministically**
  (re-running produces byte-identical output).
- `manifest.json` exposes every named region the game reads (existing biome
  terrain grids + props still resolve); a smoke check asserts the enriched atlas
  contains the expected unique-tile count per biome.

### Visual (advisory, per project standards)
- Screenshot the enriched atlas and a painted screen (via the existing headless
  `capture_screen` smoke path); confirm new tiles read correctly and painted
  tiles appear where authored.

## 11. Acceptance Criteria

1. Painting tiles on the `terrain` layer in Tiled and running `from-tiled`
   produces a `schema_version: 2` `.map.json` whose `tiles.terrain` matches.
2. Launching the game shows **exactly** those tiles at those cells (no tint, no
   procedural override) for painted screens.
3. Unpainted / v1 screens render identically to today (procedural fallback).
4. `.map.json ↔ .tmj` round-trips tile data losslessly (Python tests green).
5. `seed-collision` fills the Tiled collision layer from solid tiles and never
   clobbers existing collision without `--force`.
6. All existing C++ (Catch2) and Python (pytest) suites remain green; new tests
   added per §10.
7. **Enrichment (P0):** the regenerated `castle.png` is orthogonal, has a
   noticeably richer palette (target ≈20–30 unique tiles/biome), regenerates
   deterministically, and the game loads it with all existing regions intact.

## 12. Rollout Phases (detailed by writing-plans)

- **P0 — Enrich the atlas (§5.1):** catalog clean cells; expand
  `source-selection.json` + `build_assets.py`; regenerate `castle.png` +
  `manifest.json`; visual-verify; game still loads. Author
  `assets/levels/tiled/castle.tsx` (external → `../../generated/castle.png`,
  `columns` = atlas width ÷ 16). Deliverable: a richer, still-orthogonal palette.
- **P1 — Format & fallback:** `TileLayer` in `map_format`; parse/serialize v2;
  C++ tests; game still renders (empty tiles → fallback). No visible change yet.
- **P2 — Renderer:** `draw_tile_layer` + pure `tile_source_cell`; wire into `draw`
  and `draw_editor` (reusing `castle_texture_`); `CampaignWorld::tiles_for_screen`.
  A hand-authored v2 screen now shows painted tiles from the enriched atlas.
- **P3 — Converter:** tile layer read/write + reference `castle.tsx` +
  `tileset_columns` written from the atlas + Python tests; full two-way pipeline.
- **P4 — Auto-seed + migration:** `seed-collision`; re-export the 18 maps;
  document the workflow (update `docs/tiled-workflow.md`).

## 13. Open Questions / Future

- **Per-biome tilesets (fast-follow):** build distinct enriched atlases per biome
  (same pipeline, from `winter_tileset.png` / `gothic_tileset.png`), one `.tsx`
  per biome, and select the tileset by the screen's `biome`. Requires either a
  per-map tileset reference (multi-`firstgid`) or a global-offset scheme. The v1
  format already carries `biome`, so this is additive.
- **Further atlas variety:** beyond the P0 enrichment, more decorative/animated
  tiles can be added the same way (source-selection + build_assets) as needed.
- `background` decor tile layer — additive, when needed.
