# Editing maps in Tiled

JumpCastle screens can be authored in [Tiled](https://www.mapeditor.org/) and
converted to the engine's `screen-NN.map.json` format (and back). This is an
alternative to the in-game editor (`F1`); the two share the same on-disk maps,
so you can mix them.

Tiled authoring is **WYSIWYG**: the tiles you paint on the `terrain` layer are
exactly what the game draws at runtime. Collision is a separate object layer —
either draw it by hand, or auto-generate a first pass from your painted tiles
with `seed-collision` and refine from there.

## One-time setup

1. Install Tiled (1.9 or newer).
2. The 18 screens are pre-exported to `assets/levels/tiled/screen-NN.tmj`.
   Open any of them in Tiled — the shared **`castle`** tileset (defined in
   `assets/levels/tiled/castle.tsx`, backed by `assets/generated/castle.png`)
   loads automatically in the Tilesets panel.

Each map is 28×36 tiles at 16 px, with three layers:

| Layer       | Kind          | Contents                                     |
|-------------|---------------|-----------------------------------------------|
| `terrain`   | tile layer    | painted tiles from the `castle` tileset — this is what the game renders |
| `collision` | object layer  | rectangles / polygons, Class in `solid`, `oneway`, `hazard` |
| `entities`  | object layer  | points, Class in `spawn`, `checkpoint`, `goal` |

> The **Class** field (top of the object properties panel) is what the
> converter reads for `collision`/`entities` objects — not the object name or
> the layer name. An object with no class, or an unrecognised class, is
> ignored on import.

## Authoring tiles (WYSIWYG terrain)

- Select the **`terrain`** layer and paint with tiles from the `castle`
  tileset in the Tilesets panel — stamp, fill, whatever Tiled's tools support.
- What you paint is what renders: the game reads this layer's GIDs directly
  (including horizontal/vertical flips) and draws them with the same shared
  atlas, tinted per-biome at runtime.
- A screen with an all-empty `terrain` layer (every cell GID 0) falls back to
  the older procedural polygon texturing — painting even one tile switches
  that screen over to WYSIWYG rendering on the next `from-tiled` export.

## Hand-painting a screen (step by step)

This is the recommended day-to-day recipe for hand-painting a screen's
`terrain` layer using the hand-paint support kit: a per-biome **Terrain
Brush** in `castle.tsx`, a labeled **palette cheat-sheet**, and a per-screen
**collision underlay guide**. All three are authoring aids only — they don't
change collision, `.map.json`, or game code.

1. Open `assets/levels/tiled/screen-NN.tmj` in Tiled.
2. Keep `docs/tiled-guides/palette-cheatsheet.png` and
   `docs/tiled-guides/screen-NN-collision.png` open as reference (a second
   monitor or image viewer works well).
3. Select the `terrain` layer. Use the **Terrain Brush** — the biome's Wang
   set in the Terrain Sets panel (called "Wang Sets" in Tiled versions before
   1.9) — to drag across platforms; Tiled auto-places edges and corners for
   you. Match the shapes in the collision underlay so painted tiles sit on
   the real platforms rather than floating or clipping into geometry. If
   `castle.tsx` was already open before this kit was added, reload the
   tileset (or reopen the map) so the new Terrain Sets appear.
4. Add variety by hand from the cheat-sheet: `detail_1` / `detail_2`,
   `pillar`, `ledge`, `platform_left` / `platform_mid` / `platform_right`,
   and `hazard` / `spike` aren't part of the Terrain Brush fill (they're
   decoration/hazard tiles, not structural edges/corners) — stamp them
   individually where the collision guide and your own judgment call for
   them.
5. Convert back to the engine format:

   ```bash
   .venv/bin/python tools/tiled_convert.py from-tiled \
     --input-dir assets/levels/tiled --output-dir assets/levels/screens
   ```

6. Run the game to see it (see "Convert Tiled → game" below for the rebuild
   command).
7. Ask the assistant to review — it renders a preview and checks the painted
   tiles against the collision layer.

### Biome → screen map

Verified against the `"biome"` field in each `assets/levels/screens/screen-NN.map.json`:

| Biome          | Screens |
|----------------|---------|
| `crown_spire`  | 00–05   |
| `frosted_keep` | 06–11   |
| `courtyard`    | 12–17   |

### Regenerating the kit

The Terrain Brush, cheat-sheet, and collision guides are all generated —
re-run their scripts after the source data changes (e.g. the atlas is
regenerated, or a screen's collision is reworked):

```bash
.venv/bin/python tools/gen_wangsets.py          # castle.tsx Wang sets (idempotent)
.venv/bin/python tools/tiled_cheatsheet.py       # docs/tiled-guides/palette-cheatsheet.png
.venv/bin/python tools/tiled_collision_guide.py  # docs/tiled-guides/screen-NN-collision.png (all 18)
```

## Authoring collision

Two ways to fill the `collision` layer, and they compose:

1. **By hand**: draw a **Rectangle** (or **Polygon** for slopes and odd
   shapes) on the `collision` layer, set its Class to `solid` (or `oneway` /
   `hazard`). Enable **View → Snapping → Snap to Grid** (or Fine Grid for
   quarter-tile) to keep coordinates clean.
2. **Auto-seed from painted tiles**: after painting `terrain`, run

   ```bash
   python3 tools/tiled_convert.py seed-collision --input-dir assets/levels/tiled
   ```

   This greedy-merges every non-zero terrain cell into maximal solid
   rectangles and writes them into the `collision` layer, **in place**. It is
   **no-clobber**: a screen whose `collision` layer already has objects is
   left untouched (your hand-authored/refined collision is never silently
   replaced). Pass `--force` to regenerate anyway and discard what's there:

   ```bash
   python3 tools/tiled_convert.py seed-collision --input-dir assets/levels/tiled --force
   ```

   Treat the seeded rectangles as a first draft — open the map in Tiled
   afterwards and refine (merge, retype to `oneway`/`hazard`, add slopes with
   polygons, etc.) before exporting.

## Authoring entities

- Place a **Point** on the `entities` layer and set its Class
  (`spawn` / `checkpoint` / `goal`).

Coordinates convert by tile size: `map.json tile = Tiled pixel / 16`. You
never type tile units in Tiled — just draw/paint on the 16 px grid.

## Convert Tiled → game

```bash
python3 tools/tiled_convert.py from-tiled \
  --input-dir assets/levels/tiled \
  --output-dir assets/levels/screens
```

Then rebuild so the dev-run assets are re-synced and the world reloads:

```bash
cmake --build build/cmake --target jumpcastle
```

Run the game — screens with painted tiles render exactly what you drew
(schema v2); screens with an empty `terrain` layer keep falling back to the
procedural polygon look (schema v1).

The screen index and biome travel as Tiled **map custom properties**
(`index`: int, `biome`: string); if a map has no `index` property the converter
falls back to the `screen-NN` filename.

## Convert game → Tiled (refresh from the current maps)

If you edited screens in-game (or via `narrow_campaign.py`) and want to pull
them back into Tiled:

```bash
python3 tools/tiled_convert.py to-tiled \
  --input-dir assets/levels/screens \
  --output-dir assets/levels/tiled
```

**This regenerates the `.tmj` from scratch** from `assets/levels/screens/*.map.json`
— it is the source of truth. Any Tiled-side edit (painted tiles, hand-drawn
collision, a stray click) that hasn't been exported yet via `from-tiled` is
overwritten and lost. Always `from-tiled` first to save your Tiled work
before running `to-tiled` from another source.

The round trip is lossless for collider geometry, entities, screen dimensions,
biome, and painted terrain tiles (including flips) — verified across all 18
committed screens. A tile-less screen round-trips back to an identical schema
v1 `.map.json` (no spurious `tiles`/`tileset` keys are introduced).

## Notes and limits

- **Runtime collision always comes from the object layer**, never from
  painted tiles directly — `seed-collision` is an authoring convenience that
  writes into that same object layer, not a separate code path.
- **No live feel test in Tiled.** Use the in-game editor (`F1`) or just play
  to check jump feel; Tiled is best for bulk layout and painting.
- **Concave shapes are fine.** The engine splits concave colliders into convex
  pieces on load, so a re-export may show more pieces than you drew.
- **Known limitation**: the `frosted_keep` biome's tiles are placeholder-ish
  (the source is scatter-style ice/snow art, not a clean structural set) —
  swap its source rects in `assets/source-selection.json` and regenerate the
  atlas (see the asset pipeline docs) for crisper ice tiles.
- Run the converter's tests with
  `.venv/bin/python -m pytest tests/test_tiled_convert.py` (plain `python3`/
  `pytest` may not have the test dependencies on your machine).

## CLI reference

| Command | Direction | `--output-dir` | `--force` |
|---------|-----------|:--------------:|:---------:|
| `to-tiled`       | `.map.json` → `.tmj` | required | n/a |
| `from-tiled`     | `.tmj` → `.map.json` | required | n/a |
| `seed-collision` | `.tmj` → same `.tmj` (in place) | n/a | overwrite existing collision objects |

All three accept `--input-dir` (required) and `--tile-size` (default `16`).
