# Editing maps in Tiled

JumpCastle screens can be authored in [Tiled](https://www.mapeditor.org/) and
converted to the engine's `screen-NN.map.json` format (and back). This is an
alternative to the in-game editor (`F1`); the two share the same on-disk maps,
so you can mix them.

## One-time setup

1. Install Tiled (1.9 or newer).
2. The 18 screens are pre-exported to `assets/levels/tiled/screen-NN.tmj`.
   Open any of them in Tiled.

Each map is 28×36 tiles at 16 px, with two **object layers**:

| Layer       | Objects                        | Object *Class* values           |
|-------------|--------------------------------|---------------------------------|
| `collision` | rectangles / polygons          | `solid`, `oneway`, `hazard`     |
| `entities`  | points                         | `spawn`, `checkpoint`, `goal`   |

> The **Class** field (top of the object properties panel) is what the
> converter reads — not the object name or the layer name. An object with no
> class, or an unrecognised class, is ignored on import.

## Authoring

- **Platforms / walls**: draw a **Rectangle** (or **Polygon** for slopes and
  odd shapes) on the `collision` layer, then set its Class to `solid`
  (or `oneway` / `hazard`).
- **Spawn / goal / checkpoint**: place a **Point** on the `entities` layer and
  set its Class.
- Enable **View → Snapping → Snap to Grid** (or Fine Grid for quarter-tile) to
  keep coordinates clean.

Coordinates convert by tile size: `map.json tile = Tiled pixel / 16`. You never
type tile units in Tiled — just draw in pixels on the 16 px grid.

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

The round trip is lossless for collider geometry, entities, screen dimensions,
and biome (verified across all 18 committed screens).

## Notes and limits

- **Art is auto-applied.** Tiled shows plain object outlines; the game fills
  your polygons with the biome tileset at runtime. You are authoring
  *collision geometry*, not painting tiles.
- **No live feel test in Tiled.** Use the in-game editor (`F1`) or just play to
  check jump feel; Tiled is best for bulk layout.
- **Concave shapes are fine.** The engine splits concave colliders into convex
  pieces on load, so a re-export may show more pieces than you drew.
- Run the converter's tests with `pytest tests/test_tiled_convert.py`.
