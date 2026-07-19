# Tiled Tile Painting — P4: Auto-seed Collision + Migrate Screens + Docs — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Add a `seed-collision` CLI that generates collision rectangles from painted solid tiles into a `.tmj` (author-time convenience, no-clobber), migrate the 18 existing screen `.tmj` files to the P3 format (castle.tsx + terrain layer), and document the full WYSIWYG workflow.

**Architecture:** `seed-collision` reads a `.tmj`'s `terrain` tilelayer, treats every non-zero cell as solid, greedy-merges contiguous solids into maximal rectangles, and writes them as `type:"solid"` rectangle objects into the `collision` objectgroup — in place, skipping maps whose collision layer already has objects unless `--force`. Runtime collision is unchanged (it still reads the object layer via `from-tiled`); this only pre-populates Tiled for hand-refinement. Migration re-runs `to-tiled` so every `.tmj` gains the terrain layer + external tileset (and normalizes the stray `screen-17.tmj`).

**Tech Stack:** Python 3 (stdlib json), pytest via `.venv/bin/python`.

## Global Constraints

- Solid = any terrain GID whose masked id (`gid & 0x1FFFFFFF`) is non-zero.
- Greedy-merge is deterministic: scan row-major; for each unused solid cell extend right while solid+unused, then down while the whole row-span is solid+unused; mark used; emit `(x,y,w,h)` in TILE units → object rect in PIXELS.
- `seed-collision` is IN-PLACE on `--input-dir` `.tmj` files. NEVER overwrite a collision layer that already has objects unless `--force` (protects hand-authored collision).
- Migration `to-tiled` regenerates `.tmj` from the `.map.json` — the 18 screens currently have no tiles, so their terrain layers are empty (all-zero), which is correct (paintable, and `from-tiled` treats all-zero as no-tiles → they stay v1).
- Commits allowed on `feat/tiled-tile-painting`. Conventional Commits. NO "Co-Authored-By". No push. Run pytest via `.venv/bin/python -m pytest`.

---

## File Structure

- `tools/tiled_convert.py` — add `_solid_rects_from_terrain`, `seed_collision`, `convert_seed_collision`; wire `seed-collision` + `--force` into `main()`.
- `tests/test_tiled_convert.py` — add greedy-merge, no-clobber, and force tests.
- `assets/levels/tiled/screen-00.tmj` … `screen-17.tmj` — REGENERATED via `to-tiled` (migration).
- `docs/tiled-workflow.md` — update to the WYSIWYG workflow.

---

### Task 1: `seed-collision` algorithm + CLI + tests

**Files:** `tools/tiled_convert.py`, `tests/test_tiled_convert.py`

**Interfaces:**
- Produces: `seed_collision(tmj, *, tile_size=TILE_SIZE, force=False) -> tuple[bool, int]` (changed?, rect_count), used by `convert_seed_collision(input_dir, tile_size, force) -> int`.

- [ ] **Step 1: Write the failing tests**

```python
def test_seed_collision_merges_solid_tiles_into_rects():
    from tiled_convert import seed_collision
    tmj = {
        "width": 4, "height": 3, "nextobjectid": 1,
        "layers": [
            {"type": "tilelayer", "name": "terrain", "width": 4, "height": 3,
             "data": [0, 0, 0, 0,
                      1, 1, 1, 0,
                      1, 1, 1, 0]},
            {"type": "objectgroup", "name": "collision", "objects": []},
        ],
    }
    changed, count = seed_collision(tmj)
    assert changed is True
    assert count == 1  # the 3x2 solid block merges to ONE rectangle
    collision = next(l for l in tmj["layers"] if l["name"] == "collision")
    obj = collision["objects"][0]
    assert obj["type"] == "solid"
    # (col1,row1) .. 3 wide x 2 tall @16px
    assert (obj["x"], obj["y"], obj["width"], obj["height"]) == (0, 16, 48, 32)


def test_seed_collision_is_no_clobber_without_force():
    from tiled_convert import seed_collision
    tmj = {
        "width": 2, "height": 1, "nextobjectid": 5,
        "layers": [
            {"type": "tilelayer", "name": "terrain", "width": 2, "height": 1, "data": [1, 1]},
            {"type": "objectgroup", "name": "collision",
             "objects": [{"id": 1, "type": "solid", "x": 0, "y": 0, "width": 16, "height": 16}]},
        ],
    }
    changed, count = seed_collision(tmj)                 # existing objects -> skip
    assert changed is False
    assert len(next(l for l in tmj["layers"] if l["name"] == "collision")["objects"]) == 1
    changed2, _ = seed_collision(tmj, force=True)         # force -> replace
    assert changed2 is True
```

- [ ] **Step 2: Run to verify they fail**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py -k seed_collision -q`
Expected: FAIL (`seed_collision` does not exist).

- [ ] **Step 3: Implement the functions**

In `tools/tiled_convert.py`, add (before the CLI section):

```python
def _solid_rects_from_terrain(
    terrain: dict[str, Any], width: int, height: int
) -> list[tuple[int, int, int, int]]:
    """Greedy-merge non-zero terrain cells into maximal (x, y, w, h) tile rects."""
    data = terrain.get("data", []) or []
    solid = [
        [(int(data[r * width + c]) & 0x1FFFFFFF) != 0 for c in range(width)]
        for r in range(height)
    ]
    used = [[False] * width for _ in range(height)]
    rects: list[tuple[int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            if not solid[y][x] or used[y][x]:
                continue
            w = 1
            while x + w < width and solid[y][x + w] and not used[y][x + w]:
                w += 1
            h = 1
            while y + h < height and all(
                solid[y + h][x + i] and not used[y + h][x + i] for i in range(w)
            ):
                h += 1
            for yy in range(y, y + h):
                for xx in range(x, x + w):
                    used[yy][xx] = True
            rects.append((x, y, w, h))
    return rects


def seed_collision(
    tmj: dict[str, Any], *, tile_size: int = TILE_SIZE, force: bool = False
) -> tuple[bool, int]:
    """Fill the collision objectgroup from solid terrain tiles (in place).

    Returns (changed, rect_count). No-op if there is no terrain/collision layer,
    or if the collision layer already has objects and force is False.
    """
    width = int(tmj.get("width", 0))
    height = int(tmj.get("height", 0))
    terrain = None
    collision = None
    for layer in tmj.get("layers", []):
        if layer.get("type") == "tilelayer" and layer.get("name") == "terrain":
            terrain = layer
        elif layer.get("type") == "objectgroup" and layer.get("name") == "collision":
            collision = layer
    if terrain is None or collision is None or width <= 0 or height <= 0:
        return (False, 0)
    if collision.get("objects") and not force:
        return (False, 0)

    rects = _solid_rects_from_terrain(terrain, width, height)
    obj_id = int(tmj.get("nextobjectid", 1))
    objects: list[dict[str, Any]] = []
    for (x, y, w, h) in rects:
        objects.append(
            {
                "id": obj_id,
                "name": "",
                "type": "solid",
                "x": x * tile_size,
                "y": y * tile_size,
                "width": w * tile_size,
                "height": h * tile_size,
                "rotation": 0,
                "visible": True,
            }
        )
        obj_id += 1
    collision["objects"] = objects
    tmj["nextobjectid"] = obj_id
    return (True, len(rects))


def convert_seed_collision(input_dir: Path, tile_size: int, force: bool) -> int:
    count = 0
    for tmj_path in sorted(input_dir.glob("screen-*.tmj")):
        tmj = json.loads(tmj_path.read_text(encoding="utf-8"))
        changed, _ = seed_collision(tmj, tile_size=tile_size, force=force)
        if changed:
            _write_json(tmj_path, tmj)
            count += 1
    return count
```

- [ ] **Step 4: Wire into `main()`**

Change the `direction` argument choices to include `seed-collision`, add `--force`, relax `--output-dir`, and dispatch:

```python
    parser.add_argument(
        "direction",
        choices=("to-tiled", "from-tiled", "seed-collision"),
        help="to-tiled / from-tiled / seed-collision (fill collision from solid tiles, in place)",
    )
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=False)
    parser.add_argument("--tile-size", type=int, default=TILE_SIZE)
    parser.add_argument("--force", action="store_true",
                        help="seed-collision: overwrite an existing collision layer")
    args = parser.parse_args()

    if args.direction in ("to-tiled", "from-tiled"):
        if args.output_dir is None:
            parser.error(f"{args.direction} requires --output-dir")
    if args.direction == "to-tiled":
        written = convert_to_tiled(args.input_dir, args.output_dir, args.tile_size)
    elif args.direction == "from-tiled":
        written = convert_from_tiled(args.input_dir, args.output_dir, args.tile_size)
    else:  # seed-collision
        written = convert_seed_collision(args.input_dir, args.tile_size, args.force)
    print(f"{args.direction}: wrote {written} file(s)")
```
(Replace the existing arg definitions + dispatch tail with the above; keep the `argparse.ArgumentParser(description=__doc__)` line.)

- [ ] **Step 5: Run tests**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py -q`
Expected: all green (existing 13 + 2 new).

- [ ] **Step 6: Commit**

```bash
git add tools/tiled_convert.py tests/test_tiled_convert.py
git commit -m "feat(tiled): add seed-collision (solid tiles -> collision rects, no-clobber)"
```

---

### Task 2: Migrate the 18 screen `.tmj` files

**Files:** `assets/levels/tiled/screen-*.tmj` (regenerated)

- [ ] **Step 1: Regenerate all 18 `.tmj` from the `.map.json`**

Run:
```
.venv/bin/python tools/tiled_convert.py to-tiled \
  --input-dir assets/levels/screens \
  --output-dir assets/levels/tiled
```
Expected: `to-tiled: wrote 18 file(s)`. Each `.tmj` now has an (empty) `terrain` tilelayer + `tilesets:[{firstgid:1,source:"castle.tsx"}]`. This also normalizes the stray `screen-17.tmj` local edit.

- [ ] **Step 2: Sanity-check one file**

Run: `.venv/bin/python -c "import json;d=json.load(open('assets/levels/tiled/screen-00.tmj'));print([l['name'] for l in d['layers']], d['tilesets'])"`
Expected: `['terrain', 'collision', 'entities'] [{'firstgid': 1, 'source': 'castle.tsx'}]`

- [ ] **Step 3: Confirm round-trip stability** (no accidental `.map.json` change)

Run: `.venv/bin/python tools/tiled_convert.py from-tiled --input-dir assets/levels/tiled --output-dir assets/levels/screens && git status --short assets/levels/screens/`
Expected: no changes to `assets/levels/screens/` (tile-less screens round-trip to identical v1 `.map.json`). If any changed, investigate before committing.

- [ ] **Step 4: Commit the migrated `.tmj`**

```bash
git add assets/levels/tiled/screen-*.tmj
git commit -m "chore(tiled): migrate 18 screen .tmj to terrain layer + castle.tsx"
```

---

### Task 3: Document the workflow

**Files:** `docs/tiled-workflow.md`

- [ ] **Step 1: Rewrite/extend `docs/tiled-workflow.md`** to cover the now-complete WYSIWYG loop. Include:
  - Open `assets/levels/tiled/screen-NN.tmj` in Tiled; the `castle` tileset (from `castle.tsx`) appears in the Tilesets panel.
  - Paint on the **`terrain`** tile layer — this is what the game draws, exactly.
  - (Optional) `tiled_convert.py seed-collision --input-dir assets/levels/tiled` to auto-generate collision boxes from your solid tiles, then refine them by hand on the `collision` object layer (add slopes/oneway/hazard). `--force` overwrites existing collision.
  - Export back: `tiled_convert.py from-tiled --input-dir assets/levels/tiled --output-dir assets/levels/screens`.
  - Run the game — painted screens render your tiles (v2); unpainted screens fall back to procedural (v1).
  - Note the three layers (`terrain` tiles / `collision` objects / `entities` points) and that runtime collision comes from the object layer only.
  - Note the known limitation: `frosted_keep` tiles are placeholder-ish (source is scatter art) — swap its source in `source-selection.json` + regenerate for crisper ice.

- [ ] **Step 2: Commit**

```bash
git add docs/tiled-workflow.md
git commit -m "docs: document the Tiled WYSIWYG tile-painting workflow"
```

---

## Self-Review

**Spec coverage (design spec §8 seed-collision, §9 migration, §12 P4):**
- `seed-collision` greedy-merge + no-clobber + `--force` → Task 1. ✅
- Runtime collision unchanged (object layer only); seed is author-time → by design (Task 1 writes into the .tmj, `from-tiled` reads objects as before). ✅
- Migrate 18 `.tmj` (terrain layer + castle.tsx), normalize screen-17, round-trip-stable `.map.json` → Task 2. ✅
- Workflow docs → Task 3. ✅

**Placeholder scan:** none — full function bodies, exact commands.

**Type consistency:** `seed_collision(tmj, *, tile_size, force) -> (bool, int)` and `_solid_rects_from_terrain(terrain, width, height) -> list[(x,y,w,h)]` match their call sites; `convert_seed_collision(input_dir, tile_size, force)` matches the `main()` dispatch.

---

## Execution Handoff

Subagent-driven: one implementer (cohesive: converter code + tests + regenerate + docs), then an independent review of the diff. This is the final phase — after it, a whole-branch review + merge is appropriate. Commits per task on `feat/tiled-tile-painting`.
