# Tiled Tile Painting — P3: Converter Two-Way Tile Layer + `.tsx` — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `tools/tiled_convert.py` carry the painted `terrain` tile layer in BOTH directions (`.map.json ↔ .tmj`) and reference one shared external Tiled tileset (`castle.tsx`), so tiles painted in Tiled reach the game and vice-versa — losslessly.

**Architecture:** `screen_map_to_tiled` gains a real `tilelayer` (named `terrain`, flat row-major GID `data`) built from `screen["tiles"]["terrain"]`, and swaps the embedded reference tileset for an external `{firstgid:1, source:"castle.tsx"}`. `tiled_to_screen_map` reads that layer back into a 2-D `tiles.terrain`, treating an all-zero grid as "no tiles" so the 18 existing tile-less screens round-trip unchanged (schema v1). A static `assets/levels/tiled/castle.tsx` points every map at `generated/castle.png`.

**Tech Stack:** Python 3 (stdlib json/xml), pytest via the repo `.venv` (`.venv/bin/python -m pytest`).

## Global Constraints

- Painted tile layer name is exactly `terrain`; Tiled `data` is a FLAT, row-major array of length `width*height`; `.map.json` stores it as a 2-D `height×width` array. Preserve GIDs verbatim incl. flip bits (values may exceed 2^31 — plain Python `int`).
- Emit `schema_version: 2` + a `tileset` block ONLY when the screen actually has non-zero tiles; otherwise emit v1 with no `tiles`/`tileset` (keeps existing screens byte-stable through a round trip).
- External tileset reference is exactly `{"firstgid": 1, "source": "castle.tsx"}`. `castle.tsx` → `../../generated/castle.png`, `columns=21`, `tilecount=126`, tile 16, image 336×96.
- Column count for the `.map.json` `tileset` block = `TILESET_IMAGE_WIDTH // tile_size` (= 21) — one source of truth.
- Object classification (colliders/entities) is UNCHANGED from today.
- Run tests with `.venv/bin/python -m pytest tests/test_tiled_convert.py -q` (the venv has pytest + PIL; bare `python3`/`pytest` do not, and `rtk` intercepts the literal `pytest`).
- Commits allowed on `feat/tiled-tile-painting` (user-authorized). Conventional Commits. NO "Co-Authored-By"/attribution trailer. No push.

---

## File Structure

- `assets/levels/tiled/castle.tsx` — NEW static Tiled external tileset (XML).
- `tools/tiled_convert.py` — add `_tilelayer` helper + `TILESET_SOURCE` const; rewrite `screen_map_to_tiled` (emit terrain layer + external tileset); rewrite `tiled_to_screen_map` (read terrain layer, all-zero→none); remove the now-unused `_reference_tileset`.
- `tests/test_tiled_convert.py` — add terrain-emit, terrain-read, all-zero→none, tiles round-trip, and castle.tsx tests. Existing tests must still pass.

---

### Task 1: `castle.tsx` external tileset + test

**Files:**
- Create: `assets/levels/tiled/castle.tsx`
- Modify: `tests/test_tiled_convert.py`

**Interfaces:**
- Produces: the shared tileset every `.tmj` references (`source: "castle.tsx"`). Consumed by Task 2's tileset reference.

- [ ] **Step 1: Write the failing test**

Add to `tests/test_tiled_convert.py` (text assertions — no XML parser needed for a
tiny trusted static file, and it avoids stdlib-XML security lints):

```python
def test_castle_tsx_matches_the_atlas():
    tsx = (
        Path(__file__).resolve().parents[1]
        / "assets" / "levels" / "tiled" / "castle.tsx"
    ).read_text(encoding="utf-8")
    assert 'name="castle"' in tsx
    assert 'columns="21"' in tsx
    assert 'tilecount="126"' in tsx
    assert 'tilewidth="16"' in tsx and 'tileheight="16"' in tsx
    assert 'source="../../generated/castle.png"' in tsx
    assert 'width="336"' in tsx and 'height="96"' in tsx
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py::test_castle_tsx_matches_the_atlas -q`
Expected: FAIL (file `castle.tsx` does not exist → ET.parse raises).

- [ ] **Step 3: Create the tileset file**

Create `assets/levels/tiled/castle.tsx` with EXACTLY:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.10.2" name="castle" tilewidth="16" tileheight="16" tilecount="126" columns="21">
 <image source="../../generated/castle.png" width="336" height="96"/>
</tileset>
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py::test_castle_tsx_matches_the_atlas -q`
Expected: PASS.

- [ ] **Step 5: Commit** (user-authorized)

```bash
git add assets/levels/tiled/castle.tsx tests/test_tiled_convert.py
git commit -m "feat(tiled): add shared castle.tsx external tileset"
```

---

### Task 2: `screen_map_to_tiled` emits the terrain tilelayer + external tileset

**Files:**
- Modify: `tools/tiled_convert.py`
- Modify: `tests/test_tiled_convert.py`

**Interfaces:**
- Consumes: `screen["tiles"]["terrain"]` (2-D grid) + `screen["screen"]["width"/"height"]`.
- Produces: a `.tmj` dict whose `layers` are `[terrain(tilelayer), collision(objectgroup), entities(objectgroup)]` and whose `tilesets` is `[{"firstgid":1,"source":"castle.tsx"}]`.

- [ ] **Step 1: Write the failing test**

```python
def test_screen_to_tiled_emits_terrain_tilelayer():
    screen = {
        "schema_version": 2,
        "screen": {"index": 2, "width": 3, "height": 2},
        "biome": "courtyard",
        "tileset": {"name": "castle", "columns": 21, "tile_size": 16},
        "tiles": {"terrain": [[0, 1, 0], [2, 0, 3]]},
        "colliders": [],
        "entities": [],
    }
    tmj = screen_map_to_tiled(screen)
    terrain = next(l for l in tmj["layers"] if l["name"] == "terrain")
    assert terrain["type"] == "tilelayer"
    assert terrain["width"] == 3 and terrain["height"] == 2
    assert terrain["data"] == [0, 1, 0, 2, 0, 3]  # row-major flatten
    assert tmj["tilesets"] == [{"firstgid": 1, "source": "castle.tsx"}]
    # collision/entities object layers are still present
    assert {l["name"] for l in tmj["layers"]} == {"terrain", "collision", "entities"}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py::test_screen_to_tiled_emits_terrain_tilelayer -q`
Expected: FAIL — no terrain layer; `tilesets` is the embedded reference tileset.

- [ ] **Step 3: Add the `_tilelayer` helper + `TILESET_SOURCE` constant**

In `tools/tiled_convert.py`, add near the other constants (after `TILESET_IMAGE_HEIGHT`):

```python
TILESET_SOURCE = "castle.tsx"  # external tileset shared by every screen .tmj
```

Add this helper next to `_objectgroup`:

```python
def _tilelayer(
    layer_id: int, name: str, width: int, height: int, grid: list[list[int]]
) -> dict[str, Any]:
    """A Tiled tile layer with flat row-major GID data (all zeros if grid empty)."""
    if grid:
        data = [int(gid) for row in grid for gid in row]
    else:
        data = [0] * (width * height)
    return {
        "id": layer_id,
        "name": name,
        "type": "tilelayer",
        "width": width,
        "height": height,
        "visible": True,
        "opacity": 1,
        "x": 0,
        "y": 0,
        "data": data,
    }
```

- [ ] **Step 4: Rewrite the tail of `screen_map_to_tiled`**

Keep the collider/entity object-building loops above exactly as they are; only the `screen_meta`/`return` tail changes to:

```python
    screen_meta = screen.get("screen", {})
    width = int(screen_meta.get("width", 0))
    height = int(screen_meta.get("height", 0))
    tiles = screen.get("tiles") or {}
    terrain_grid = tiles.get("terrain", []) if isinstance(tiles, dict) else []

    return {
        "type": "map",
        "version": "1.10",
        "tiledversion": "1.10.2",
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "infinite": False,
        "width": width,
        "height": height,
        "tilewidth": tile_size,
        "tileheight": tile_size,
        "nextlayerid": 4,
        "nextobjectid": next_object_id,
        "tilesets": [{"firstgid": 1, "source": TILESET_SOURCE}],
        "properties": [
            {"name": "index", "type": "int", "value": int(screen_meta.get("index", 0))},
            {"name": "biome", "type": "string", "value": screen.get("biome", DEFAULT_BIOME)},
        ],
        "layers": [
            _tilelayer(1, "terrain", width, height, terrain_grid),
            _objectgroup(2, "collision", collider_objects),
            _objectgroup(3, "entities", entity_objects),
        ],
    }
```

- [ ] **Step 5: Remove the now-unused `_reference_tileset`**

Delete the `def _reference_tileset(...)` function (its embedded-tileset output is replaced by the external reference). Keep the `TILESET_IMAGE_WIDTH`/`TILESET_IMAGE_HEIGHT` constants — Task 3 uses them for the column count. (If `TILESET_IMAGE` becomes unused after Task 3, leave it; it documents the atlas path.)

- [ ] **Step 6: Run tests**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py -q`
Expected: the new test PASSES; ALL existing tests still pass. If an existing test asserted the old embedded `tilesets` shape or an exact layer count of 2, update it to reflect the terrain layer + external tileset (the geometry/collider/entity assertions must not change).

- [ ] **Step 7: Commit** (user-authorized)

```bash
git add tools/tiled_convert.py tests/test_tiled_convert.py
git commit -m "feat(tiled): export terrain tile layer + reference castle.tsx"
```

---

### Task 3: `tiled_to_screen_map` reads the terrain layer (all-zero → none)

**Files:**
- Modify: `tools/tiled_convert.py`
- Modify: `tests/test_tiled_convert.py`

**Interfaces:**
- Consumes: a `.tmj` dict with a `terrain` tilelayer (flat `data`).
- Produces: `.map.json` dict with `schema_version` 2 + `tiles.terrain` (2-D) + `tileset` block when tiles are non-zero; else v1 with neither.

- [ ] **Step 1: Write the failing tests**

```python
def test_tiled_to_screen_reads_terrain_layer():
    tmj = {
        "width": 3,
        "height": 2,
        "layers": [
            {"type": "tilelayer", "name": "terrain", "width": 3, "height": 2,
             "data": [0, 1, 0, 2, 0, 3]},
        ],
    }
    screen = tiled_to_screen_map(tmj, index=2)
    assert screen["schema_version"] == 2
    assert screen["tiles"]["terrain"] == [[0, 1, 0], [2, 0, 3]]
    assert screen["tileset"]["columns"] == 21
    assert screen["tileset"]["tile_size"] == 16


def test_all_zero_terrain_layer_yields_no_tiles():
    tmj = {
        "width": 3,
        "height": 2,
        "layers": [
            {"type": "tilelayer", "name": "terrain", "width": 3, "height": 2,
             "data": [0, 0, 0, 0, 0, 0]},
        ],
    }
    screen = tiled_to_screen_map(tmj, index=0)
    assert screen["schema_version"] == 1
    assert "tiles" not in screen
    assert "tileset" not in screen
```

- [ ] **Step 2: Run to verify they fail**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py::test_tiled_to_screen_reads_terrain_layer tests/test_tiled_convert.py::test_all_zero_terrain_layer_yields_no_tiles -q`
Expected: FAIL — current `tiled_to_screen_map` ignores tile layers and always writes `schema_version: 1` with no `tiles`.

- [ ] **Step 3: Rewrite `tiled_to_screen_map`**

Replace the whole function with:

```python
def tiled_to_screen_map(
    tmj: dict[str, Any],
    *,
    index: int,
    biome: str | None = None,
    tile_size: int = TILE_SIZE,
) -> dict[str, Any]:
    """Convert a parsed Tiled map into a JumpCastle screen-map dict."""
    resolved_biome = biome or _map_property(tmj, "biome") or DEFAULT_BIOME
    width = int(tmj.get("width", 0))
    height = int(tmj.get("height", 0))
    colliders: list[dict[str, Any]] = []
    entities: list[dict[str, Any]] = []
    terrain_grid: list[list[int]] | None = None

    for layer in tmj.get("layers", []):
        layer_type = layer.get("type")
        if layer_type == "tilelayer" and layer.get("name") == "terrain":
            data = layer.get("data", []) or []
            if width > 0 and any(int(gid) != 0 for gid in data):
                terrain_grid = [
                    [int(gid) for gid in data[row * width:(row + 1) * width]]
                    for row in range(height)
                ]
            continue
        if layer_type != "objectgroup":
            continue
        for obj in layer.get("objects", []):
            cls = object_class(obj)
            if obj.get("point") and cls in ENTITY_CLASSES:
                entities.append(
                    {
                        "type": cls,
                        "pos": [
                            float(obj.get("x", 0.0)) / tile_size,
                            float(obj.get("y", 0.0)) / tile_size,
                        ],
                    }
                )
                continue
            if cls not in COLLIDER_CLASSES:
                continue
            points = _object_tile_points(obj, tile_size)
            if points is None:
                continue
            colliders.append(
                {"id": len(colliders) + 1, "type": cls, "points": points}
            )

    result: dict[str, Any] = {
        "schema_version": 2 if terrain_grid is not None else 1,
        "screen": {"index": index, "width": float(width), "height": float(height)},
        "biome": resolved_biome,
    }
    if terrain_grid is not None:
        result["tileset"] = {
            "name": "castle",
            "columns": TILESET_IMAGE_WIDTH // tile_size,
            "tile_size": tile_size,
        }
        result["tiles"] = {"terrain": terrain_grid}
    result["colliders"] = colliders
    result["entities"] = entities
    return result
```

- [ ] **Step 4: Run tests**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py -q`
Expected: the two new tests PASS; ALL existing tests still pass (the old round-trip fixture has no tiles → terrain layer is all-zero → `terrain_grid` stays `None` → v1 output, matching the existing assertions).

- [ ] **Step 5: Commit** (user-authorized)

```bash
git add tools/tiled_convert.py tests/test_tiled_convert.py
git commit -m "feat(tiled): import terrain tile layer (all-zero grid = no tiles)"
```

---

### Task 4: Full tiles round-trip (incl. flip) + v1 no-tiles round-trip

**Files:**
- Modify: `tests/test_tiled_convert.py`

**Interfaces:** none (locks the two-way contract).

- [ ] **Step 1: Write the tests**

```python
def test_round_trip_preserves_tiles_incl_flip():
    screen = {
        "schema_version": 2,
        "screen": {"index": 4, "width": 2, "height": 2},
        "biome": "frosted_keep",
        "tileset": {"name": "castle", "columns": 21, "tile_size": 16},
        "tiles": {"terrain": [[0, 5], [0x80000000 | 6, 3]]},  # one H-flipped GID
        "colliders": [],
        "entities": [],
    }
    tmj = screen_map_to_tiled(screen)
    restored = tiled_to_screen_map(tmj, index=4)
    assert restored["schema_version"] == 2
    assert restored["tiles"]["terrain"] == [[0, 5], [0x80000000 | 6, 3]]


def test_round_trip_tileless_screen_stays_v1():
    screen = {
        "schema_version": 1,
        "screen": {"index": 7, "width": 4, "height": 3},
        "biome": "courtyard",
        "colliders": [{"id": 1, "type": "solid", "points": [[0, 2], [4, 2], [4, 3], [0, 3]]}],
        "entities": [{"type": "spawn", "pos": [1, 1]}],
    }
    tmj = screen_map_to_tiled(screen)
    restored = tiled_to_screen_map(tmj, index=7)
    assert restored["schema_version"] == 1
    assert "tiles" not in restored
    assert restored["colliders"][0]["points"] == [[0, 2], [4, 2], [4, 3], [0, 3]]
    assert restored["entities"] == [{"type": "spawn", "pos": [1, 1]}]
```

- [ ] **Step 2: Run tests**

Run: `.venv/bin/python -m pytest tests/test_tiled_convert.py -q`
Expected: both PASS; entire file green.

- [ ] **Step 3: Commit** (user-authorized)

```bash
git add tests/test_tiled_convert.py
git commit -m "test(tiled): lock two-way tile round-trip incl. flip + v1 stability"
```

---

## Self-Review

**Spec coverage (design spec §8):**
- Emit real `tilelayer` (flat row-major data) + external `castle.tsx` → Tasks 1, 2. ✅
- Read terrain layer back to 2-D `tiles.terrain`; schema v2 + tileset block → Task 3. ✅
- Existing colliders/entities object handling unchanged → Task 3 (same logic). ✅
- Tile-less screens round-trip unchanged (all-zero → none) → Task 3 + Task 4. ✅
- Flip bits preserved through the round trip → Task 4. ✅
- `seed-collision` subcommand is P4, not here (documented).

**Placeholder scan:** none — full function bodies, exact XML, exact commands.

**Type consistency:** `_tilelayer(layer_id, name, width, height, grid)` matches its 1 call site; `TILESET_SOURCE`/`"castle.tsx"` identical in the emit code, the test, and the .tsx path; column count `TILESET_IMAGE_WIDTH // tile_size` (=21) matches P1's `tileset_columns` default and the .tsx `columns="21"`.

---

## Execution Handoff

Subagent-driven: one implementer for the phase (cohesive: one static file + two function rewrites + tests), then an independent review of the diff. Commits per task on `feat/tiled-tile-painting`.
