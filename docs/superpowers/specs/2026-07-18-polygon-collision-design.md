# Polygon Collision + SAT + In-Game Editor — Design

**Date:** 2026-07-18
**Status:** Design approved (brainstorm) — pending implementation plan
**Related:** [map-redesign](2026-07-18-map-redesign-design.md), [narrow-vertical-tower](2026-07-18-narrow-vertical-tower-design.md), [jump-king-continuous-campaign](2026-07-18-jump-king-continuous-campaign-design.md)

## 1. Motivation

The map is currently an ASCII grid (`#`/`.`/`S`/`C`/`^`/`E`), parsed by `src/level.cpp` (per-room `Tilemap`) and assembled into a vertical tower `WorldMap` by `src/world_loader.cpp`. Collision (`src/collision.cpp`) is tile-based: broadphase via `overlapped_tiles()`, then per-tile AABB tests, resolving by *guessing* the separation axis.

The grid's smallest unit is one tile. This blocks: slopes, half-tile edges, one-way platforms, and precise ledge geometry — all of which a precise Jump-King-style platformer wants. The user's goal: **more precise/expressive collision** *and* **easier map editing**.

## 2. Chosen Approach

- **World geometry:** arbitrary **convex polygons** (concave shapes are split into convex pieces at author/load time).
- **Player:** stays an **AABB** (AABB-vs-convex SAT is simpler and more stable than polygon-vs-polygon and is sufficient for the player).
- **Resolution:** **sub-stepping + discrete SAT MTV** (reuses the sub-stepping already in `resolve_world_collision`). Swept-SAT (time-of-impact) is a deliberate future upgrade, not MVP.
- **Format:** full replacement of ASCII `.level` with JSON `.map.json`, **one file per screen** (mirrors the old per-`.level` layout). Reuses `nlohmann::json` already used in `src/assets.cpp`.
- **Authoring:** an **in-game editor** (draw/delete polygons, drag vertices, snap-to-grid, place entities).

Rejected alternatives: discrete MTV without sub-stepping (tunnels at high fall speed); full swept-SAT (more complex/bug-prone than needed now); Tiled/TMX (extra parser + external tool dependency); keeping the grid (defeats the precision goal).

## 3. Map Format (`.map.json`, one per screen)

```jsonc
{
  "schema_version": 1,
  "screen": { "index": 5, "width": 16.0, "height": 15.0 },
  "biome": "courtyard",
  "colliders": [
    {
      "id": 1,
      "type": "solid",                 // solid | oneway | hazard
      "points": [[0,15],[6,15],[6,13],[3,11],[0,11]]  // convex, CCW, screen-local tile units
    }
  ],
  "entities": [
    { "type": "spawn",      "pos": [2.5, 14.0] },
    { "type": "checkpoint", "pos": [8.0, 9.0] },
    { "type": "goal",       "pos": [12.0, 1.0] }
  ]
}
```

Rules & invariants (enforced by loader):
- Coordinates are floats in **screen-local tile units**, origin top-left, **y-down** (matches `WorldMap`). Renderer multiplies by `tile_size` when drawing.
- `points`: >= 3, **convex**, consistent **CCW** winding, non-self-intersecting. Concave input is auto-split into convex pieces (fan / ear-clipping) at load; genuinely invalid input throws.
- `type`: `solid` (blocks all directions), `oneway` (blocks only when landing from above), `hazard` (overlap = respawn; replaces `spike`).
- Entities are separate from colliders. Validation keeps the existing `WorldMap` invariant: spawn/goal must be empty and stand above a solid collider.
- The tower stitches per-screen files vertically (existing `screen_height` / `screen_for_y` / `world_loader` machinery); biome stays a per-screen attribute.

## 4. Collision Runtime

Data structures:
```cpp
enum class ColliderType { solid, oneway, hazard };

struct ConvexPolygon {
    std::vector<Vec2> points;        // CCW, screen-local
    std::vector<Vec2> edge_normals;  // precomputed at load
    Rect  aabb;                      // precomputed at load (broadphase)
    ColliderType type;
};

struct ScreenColliders {             // one .map.json => one of these
    std::vector<ConvexPolygon> polygons;
    float vertical_offset;           // screen position within the tower
};
```

Per-frame pipeline (replaces `resolve_world_collision`):
1. **Screen selection:** player at screen `k` => gather colliders from screens `k` and `k±1` (catches boundary contacts). Add `vertical_offset` to lift polygons into world space.
2. **Sub-stepping:** split displacement into steps <= `min_feature` (start 0.2 tile, matching current code). Per step:
   - **Broadphase:** reject polygons whose AABB does not overlap the player AABB.
   - **Narrowphase SAT (AABB-vs-convex):** candidate axes = `{(1,0),(0,1)} ∪ edge_normals`. Compute **MTV** (least-penetration axis). No overlap => skip.
   - **Resolve:** push player out along MTV; **kill velocity component along the normal** (solid). On slopes, **project velocity onto the surface** (natural slide).
3. **Contact classification from normal** (y-down): normal points **up** => `on_ground = true` (floor/standable slope); **down** => ceiling (cut upward velocity); **|nx| dominant** => wall.
4. **hazard:** any overlap triggers respawn (no positional resolve).

`oneway` rule: collide **only if both** hold — (a) MTV normal points **up** (landing from above), and (b) the player's **previous-step bottom was above** the polygon's top edge. Otherwise (moving up, side contact, coming from below) => ignore entirely. Sub-stepping makes (b) reliable at high fall speed.

Performance: precompute `edge_normals` + `aabb` at load, never per frame. Local `k±1` gathering serves as spatial broadphase without a global spatial hash.

## 5. In-Game Editor

- Toggle with `F1`; physics **pauses** in editor. Edits **one screen at a time** (`PageUp`/`PageDown` to switch), camera pan (WASD) + zoom.
- Tools: **Draw polygon** (click vertices, `Enter` close, `Esc` cancel; key cycles type solid→oneway→hazard) · **Drag vertex** (click-select + drag; double-click edge = insert vertex; `Del` = delete vertex) · **Delete polygon** (click inside + `Del`) · **Place entity** (pick spawn/checkpoint/goal, click) · **Snap-to-grid** at 0.25 tile (hold `Alt` to disable temporarily).
- Validation feedback: non-convex / self-intersecting draft rendered red with a warning; on **Save**, concave is auto-split or rejected.
- **Save** serializes the current screen back to its `.map.json` (colliders + entities). Load uses the normal loader.
- Rendering: filled translucent polygons + outlines, vertex handles, entity icons, grid overlay.

## 6. Architecture & Data Flow

```
.map.json (per screen)
   -> map_format (parse + validate + concave-split + precompute)
   -> ScreenColliders / WorldMap (polygon-backed)
   -> collision_world (broadphase + SAT resolve)  <- called each frame by physics
   -> renderer (debug colliders + editor overlay + visual tiles/props)
```

New, independently testable units (high cohesion, low coupling):
- `sat.hpp/.cpp` — pure math: AABB-vs-convex overlap + MTV. No engine deps.
- `convex.hpp/.cpp` — polygon validation, concave→convex split, edge-normal/AABB precompute.
- `map_format.hpp/.cpp` — `.map.json` parse + serialize (round-trippable).
- `collision_world.hpp/.cpp` — screen gathering, broadphase, sub-stepping resolve.
- `editor.hpp/.cpp` — editor state/UI, isolated from runtime collision.

Existing files to change:
- `src/level.cpp`, `src/world_loader.cpp` — replace ASCII parsing with `.map.json` loading.
- `include/jumpcastle/world.hpp`, `src/world.cpp` — `WorldMap` backed by polygons (keep spawn/goal/biome/screen API where callers depend on it).
- `src/collision.cpp`, `include/jumpcastle/collision.hpp` — SAT resolve replaces grid resolve.
- `src/renderer.cpp`, `include/jumpcastle/renderer.hpp` — draw polygon debug + editor overlay.
- `include/jumpcastle/tilemap.hpp`, `src/tilemap.cpp` — deprecate/remove the ASCII grid path (full replacement).
- `src/main.cpp` / `src/game.cpp` — wire editor toggle + pause.

Reconciling the two existing systems (`Tilemap` per-room grid vs `WorldMap` vertical tower): full replacement removes the ASCII grid path; the polygon world is the single source of collision truth. The implementation plan sequences this so the game stays runnable between stages.

## 7. Testing

Unit:
- `sat`: MTV correctness for known overlaps and normals (floor, ceiling, wall, slope).
- `convex`: convex validation accept/reject; concave-split correctness; winding normalization.
- `map_format`: round-trip parse→serialize→parse equality; invalid-input rejection.
- `oneway`: approach from above (blocks), from below (passes), from side (passes).
- slope slide: velocity projected onto surface, no sticking.

Integration:
- Load a hand-authored 2-screen `.map.json`; simulate a falling player landing on floor, sliding on a slope, passing up through / landing on a one-way platform, and dying on a hazard. Reuse the existing headless smoke pattern (`tools/render_asset_smoke.py`, `tests/test_render_asset_smoke.py`).

Migration:
- Author fresh `.map.json` test screens (do not depend on converting old ASCII). Optionally a one-off ASCII→polygon converter for existing rooms, but not required for MVP.

## 8. Build Order (for the implementation plan)

Although this is one spec, staged so the game stays runnable:
1. `sat` + `convex` (pure math, fully unit-tested, no engine wiring).
2. `map_format` + loader swap; author one polygon test screen.
3. `collision_world` + physics integration replacing grid resolve; renderer debug draw.
4. Remove ASCII grid path; convert/author real campaign screens.
5. In-game editor (draw/delete, drag, snap, entities, save).

## 9. Out of Scope (YAGNI)

Swept/time-of-impact collision; polygon-vs-polygon player; moving/rotating platforms; editor undo-history and multi-select; external editor/TMX import.
