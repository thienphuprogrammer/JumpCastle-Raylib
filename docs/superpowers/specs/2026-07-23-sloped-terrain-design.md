# Sloped Terrain (Diverse Shapes, Phase 1: Slopes) — Design Spec

- **Date:** 2026-07-23
- **Status:** Draft for review (scope approved: slopes first, not circles; spec before build)
- **Related:** `src/collision_world.cpp`, `src/solver.cpp`,
  `include/jumpcastle/collision_world.hpp`, `tools/paint_screen.py`,
  `assets/levels/screens/*.map.json`, `assets/generated/manifest.json`,
  memory `shapes-feature-feasibility`.

## 1. Overview

The campaign is built entirely from axis-aligned rectangular colliders. The
user wants richer geometry — **sloped (diagonal) surfaces** — so screens read
less like stacked bricks. This spec covers **slopes only**; circles/curves are
a later phase (they need new SAT-vs-circle collision and are out of scope).

Grounding measured from the engine (feasibility investigation, 2026-07-23):

| Layer | Slope support today | Work needed |
|---|---|---|
| Runtime collision (`collision_world.cpp`) | **Already works** — SAT over arbitrary `ConvexPolygon`; `kGroundNormalY = -0.5` treats surfaces up to ~60° as floor | none for basic walk/land; verify slide/bounce feel |
| Solver certification (`solver.cpp`) | **Blocked** — `Surface{y, start_x, end_x}` is flat-only (`normal.y ≤ −0.9`); slopes are invisible to reachability | new sloped-surface model **or** keep slopes off the certified route |
| Tile art (`castle.png` atlas) | none — no diagonal tiles | add 45° slope tiles per biome (or accept collision-only, no bespoke art) |
| Painter (`tools/paint_screen.py`) | grid-orthogonal roles only | add a slope role + tile mapping |

## 2. Goals / Non-Goals

### Goals
- Author **triangular slope colliders** in `.map.json` and have the player walk
  up/down them and land on them in-engine (collision already supports this).
- Decide and implement how slopes interact with the **reachability solver** so
  the campaign stays provably completable (§4 — the core design decision).
- Render slopes with appropriate tiles (§5).
- Keep the existing 18-screen campaign solving and all suites green.

### Non-Goals (this phase)
- Circles / curved colliders (needs new collision math — separate phase).
- Moving/rotating platforms.
- Changing jump physics or `SolverConfig`.
- Re-theming; reuse the current atlas + biomes.

## 3. Collider schema

Add a slope as a 3-point polygon collider (the format already stores free-form
`points`, so no schema version bump is strictly required — but add an explicit
`shape` tag for clarity and tooling):

```jsonc
{ "id": 7, "type": "solid", "shape": "slope",
  "points": [[6,20],[12,20],[12,17]] }   // rises 3 tiles over 6, left->right up
```

- `shape` defaults to `"rect"` when absent (all existing colliders).
- Points remain integer, grid-snapped (keep the §4.5-style invariant).
- Collision reads `points` exactly as today; `shape` is advisory for the
  painter/validator.

## 4. Solver decision (THE core choice — pick one before building)

The solver only routes across flat `Surface`s. Two viable paths:

- **Option A — Slopes as decoration, off the certified route.** The reachability
  route stays on flat ledges; slopes are added as extra terrain the player *can*
  use but the solver ignores. Cheapest, zero solver risk, but slopes never
  gate/shape the intended path. Validator must ensure every slope has a flat
  alternative so it can't become the *only* way up.
- **Option B — First-class sloped surfaces.** Extend the solver's surface
  extraction to emit sloped `Surface`s (store a slope/normal, not just `y`),
  and teach landing/launch sampling to accept a foothold anywhere along the
  incline. Correct and expressive, but touches the certified core and the
  144–234-jump replay contract; needs new `world_test`/solver tests and a route
  re-solve. Expensive.

**Recommendation:** ship **Option A** first (immediate visible payoff, no
solver risk), then evaluate B as its own project. This spec is written so A is
a strict subset of B.

## 5. Art & painter

- **Tiles:** add `slope_up`, `slope_down` (45°) per biome to the atlas +
  `manifest.json` regions, OR (MVP) paint slope cells with the existing `top`
  edge tile and accept a stair-stepped look until art lands.
- **Painter (`paint_screen.py`):** add `ROLE_SLOPE`; classify a solid cell as
  slope when it lies under a `shape:"slope"` collider; map to the slope tile.
  Keep determinism. Extend `check_layout` for slopes: integer points, and
  (Option A) presence of a flat alternative.

## 6. Rollout phases

- **P0 — Collision proof:** author one slope collider on a scratch screen, run
  the game/solver, confirm the player walks/lands on it. No art. Establishes the
  feel and confirms the "already works" claim end-to-end.
- **P1 — Authoring + validator:** slope `shape` tag, painter `ROLE_SLOPE`,
  `check_layout` slope rules, tests. Slopes render (even if MVP stair tiles).
- **P2 — Option A integration:** add slopes to real screens as decoration off
  the route; full-campaign solve stays green; previews.
- **P3 (optional, separate approval) — Option B:** sloped surfaces in the
  solver.

## 7. Testing

- Collision: a `world_test` case — player released above a slope ends grounded
  on the incline, not clipped/through.
- Painter: `ROLE_SLOPE` classification + slope-tile mapping + determinism.
- Campaign: full solve stays at 218 jumps (Option A must not change the route).
- Suites: `PYTHONPATH=. pytest` and C++ `ctest` green.

## 8. Open questions for the reviewer

1. Option A (decoration) or commit to Option B (solver slopes) now?
2. Bespoke 45° slope art this phase, or MVP stair-step tiles first?
3. Only 45°, or arbitrary slope angles (collision allows any; art/painter get
   harder)?
