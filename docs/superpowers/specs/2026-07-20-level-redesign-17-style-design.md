# Campaign Level Redesign in the Screen-17 Style — Design Spec

- **Date:** 2026-07-20
- **Status:** Approved by user (approach C3, biome batches, entities fixed, screen-17 untouched)
- **Author:** pair session (design approved in Vietnamese; spec in English per repo convention)
- **Related:** `docs/superpowers/specs/2026-07-20-tiled-wysiwyg-tile-painting-design.md`,
  `tools/gen_terrain.py`, `tools/tiled_convert.py`, `src/solver.cpp`,
  `assets/levels/screens/*.map.json`

## 1. Overview

The user hand-painted `screen-17` (the spawn screen) in Tiled and wants the rest
of the campaign rebuilt "like map 17": dense, chunky, carved-stone screens with
real thick collision masses — replacing the current skeletal auto-generated
look (thin `ledge` outlines over floating planks).

This project redesigns **collision layout AND tiles** for screens 00–16 in the
visual and structural language of screen-17 — leaving screen-17 itself
untouched (§7) — while keeping the campaign provably completable via the
existing reachability solver after every single screen change.

Style fingerprint measured from the user's screen-17 vs the auto screens:

| | screen-17 (reference) | auto screens 00–15 |
|---|---|---|
| painted cells | 507/1008 (≈50%) | 70–96 (≈7–9%) |
| structure | thick walls, solid towers with climbing notches, bridges | 1-tile floating planks |
| unique tiles | 6 (fill 191, trims 64/67, column 161, accents 92/114) | 1–9, mostly `ledge` |
| palette | crown-stone cells used on a courtyard screen (cross-biome, deliberate) | biome named tiles |

## 2. Goals / Non-Goals

### Goals
- Redesign collision layouts for screens 00–16: thick solid masses, side walls,
  bridges, pillars — obeying the jump grammar (§4) so the solver certifies the
  full campaign after each screen is swapped in.
- Repaint all redesigned screens with a deterministic painter tool (§6) that
  reproduces screen-17's visual grammar (§5).
- Screen-17 needs no climbability fix anymore: commit cd8dada (merged via PR #4)
  restored the solver-provable collision while keeping the painted tiles. The
  redesign leaves screen-17 untouched by default; §7 covers the optional
  art/collision reconciliation.
- Keep every `spawn` / `checkpoint` / `goal` entity **byte-identical**; each
  must have a supporting surface in the new layout.
- Preserve each screen's hazard intent (same hazard collider count ±0, may be
  reshaped to fit the new masses).
- Deliver in three user-approved batches with rendered previews:
  courtyard 12–16 → frosted_keep 06–11 → crown_spire 00–05.
- Finish with a regenerated `assets/levels/campaign-route.json` and the full
  C++/Python suites green, with tests 73 and 80 staying green at every batch
  boundary (route regen per batch, §8).

### Non-Goals (YAGNI)
- No atlas/art changes; the enriched 24×8 atlas is used as-is.
- No `background` decor tile layer; tiles are painted only where collision (or
  a hazard) exists — no misleading "ghost stone".
- No entity moves, no difficulty redesign beyond grammar compliance.
- (Resolved upstream: CI now solves the screens directory directly — commit
  3f91d44 — so the stale `campaign.level` concern is gone.)
- No changes to solver physics or `SolverConfig`.

## 3. Grounding (measured, not assumed)

- **Jump physics (from the certified 8bed0c4 route + `SolverConfig`):** highest
  certifiable charge is 99 ticks = 97.06% (cap `max_charge_ratio` 0.98). A full
  charge directional jump nets ≈ **+3.06 tiles rise** with ≈ **5.4–5.8 tiles of
  lateral travel**. There is no high-rise/low-lateral jump: charge couples the
  vertical and horizontal impulse. Neutral jumps rise straight up but cannot
  mount a ledge directly overhead (the body collides with its underside).
- **Solver surface model (`src/solver.cpp`):** standable surfaces are merged
  upward-facing edges (normal.y ≤ −0.9); landings are rejected if the player
  centre ends past the surface's x-span; support tolerance is 0.13 tiles;
  launch positions are sampled every 0.15 tiles inside
  `[start_x + half + 0.05, end_x − half − 0.05]`.
- **Replay-test contract (`tests/replay_test.cpp`):** the committed route must
  contain **144–234 jumps total** (8–13 per screen) and its opening must replay
  in-engine.
- **Screens:** 18 maps, 28×36 tiles, schema v2. Biomes: crown_spire 00–05,
  frosted_keep 06–11, courtyard 12–17. Screen 17 is the bottom/spawn band;
  screen 0 holds the goal. `CampaignWorld::load(assets/levels/screens)` is what
  both the game and the tests consume; the solver runs against the same dir.

## 4. Jump grammar (hard constraints for every layout)

1. Every consecutive pair of route surfaces satisfies: rise ≤ 3 tiles with
   lateral offset 2.0–5.5 tiles, **or** walkable (same height, overlapping/adjacent
   spans), **or** a drop (any height).
2. Never place the only continuation ledge directly above a launch surface with
   lateral offset < 2 (body-block). Ladders are left–right zigzags, like the
   original screen-17 opening (spans at x1–5 ↔ x7–11, 3-tile steps).
3. Landing surfaces on the intended route are ≥ 2 tiles wide.
4. Headroom ≥ 2 tiles of clear air above every standable surface; ascent
   corridors keep a ≥ 2-tile-wide clear channel.
5. All collider coordinates are integers (grid-snapped by construction).
6. Each screen's route segment needs 8–13 jumps (replay-test bound). Aim ~12,
   the current average (218/18).
7. Each entity keeps its exact position and gains a supporting surface: an
   upward edge whose y equals the entity's feet within 0.13 and whose x-span
   contains it.
8. Hazards: keep per-screen hazard collider count; hazards must not sit on the
   sole certified route (the solver refuses hazardous landings automatically —
   a screen that only solves through a hazard fails the gate).
9. Screen boundaries: the full-campaign solve after each screen swap is the
   arbiter that inter-screen crossings still work; no separate rule needed.

## 5. Visual grammar and palettes

Painter roles (all collision-backed; classification per solid cell from its
8-neighborhood in the merged solid grid):

| Role | Applied to | Courtyard 12–16 (user's 17 palette) | frosted_keep / crown_spire |
|---|---|---|---|
| FILL | interior solid cells | local id **191** | biome `center` |
| EDGE | boundary cells (top/side/bottom + outer & inner corners) | role mapping **learned from screen-17's own grid** (cells 64/67 usage by neighborhood pattern), falling back to biome 9-slice names | biome 9-slice + `inner_corner_*` |
| BRIDGE | 1-tile-thick `oneway` (and 1-thick solid planks) | `platform_left/mid/right` | same |
| COLUMN | 1-wide vertical solid stacks | local id **161** | biome `pillar` |
| ACCENT | sparse detail on wall faces/tops, deterministic hash placement, ≤ 12/screen | local ids **92, 114** | biome `detail_1`, `detail_2` |
| HAZARD | hazard collider cells | biome `hazard`/`spike` | same |

- The courtyard EDGE mapping is extracted programmatically from screen-17:
  for each of the 6 cells the user painted, record the solid-neighborhood
  patterns where it appears; the painter reuses that mapping. This keeps
  batch-1 screens pixel-faithful to the user's hand style instead of guessing.
- Coverage target per screen: **35–55%** painted cells (17 sits at 50%).
- Determinism: no RNG — accent positions come from a fixed hash of
  (screen index, cell x, cell y). Re-running the painter is byte-stable.

## 6. Tool: `tools/paint_screen.py` (new)

CLI (venv python):
```
paint_screen.py --screens 12-16          # paint tiles into the .map.json files
               [--preview DIR]           # also write screen-NN-preview.png (PIL, atlas composite)
               [--check]                 # validate-only: grammar §4 items 3,4,5,7 + coverage + determinism
               [--include-17]            # off by default: screen-17 is never repainted unless asked
```
- Input: `assets/levels/screens/screen-NN.map.json` (colliders, biome,
  entities). Output: `tiles.terrain` GID grid written back (schema v2), then
  `.tmj` refreshed via the existing `tiled_convert.py to-tiled` for the touched
  screens only (temp-dir + copy, same as the snap workflow).
- Preview: composites tiles straight from `assets/generated/castle.png` — no
  engine needed; collision outlines and entities drawn on top faintly (reuses
  the drawing approach of `tools/tiled_collision_guide.py`).
- `--check` is the layout validator run in CI-less workflow: integer coords,
  route-surface widths, headroom, entity support, hazard count vs `main`,
  coverage bounds. (Reachability itself is the solver's job, not this tool's.)
- Module layout: single file, ~300 lines, stdlib + PIL only (matches existing
  tools).

## 7. Screen-17 status (no fix required)

- Commit cd8dada already restored the pre-painting, solver-provable collision
  for screen-17 while keeping the user's painted tiles, and PR #4 merged it.
  The campaign solves end-to-end again.
- Consequence: screen-17's visuals and physics currently disagree in places
  (tiles show bridges/walls where the restored ladder has none). Default for
  this project: **leave screen-17 completely untouched**.
- Optional (user's call at the batch-1 gate): reconcile by repainting
  screen-17 with the painter over the restored collision (`--include-17`),
  trading some of the hand-painting for WYSIWYG accuracy. Not done unless
  explicitly requested.

## 8. Workflow, branches, batches

- Branch: `feat/level-redesign-17-style` off `main` (main already contains
  PR #4 = the suite fixes + screen-17 restoration).
- Per screen: author colliders (integer JSON) → `paint_screen.py --check` →
  full-campaign solve (`out/verify/jumpcastle_solver --level
  assets/levels/screens --campaign`) with the new screen in place → paint +
  preview → commit `feat(level): redesign screen-NN (biome, 17-style)`.
- Batch gates (user approval on preview images before the next batch starts):
  1. **Batch 1:** screens 12–16 (screen-17 untouched, §7). Previews to
     `docs/tiled-guides/previews/`.
  2. **Batch 2:** screens 06–11 (frosted_keep).
  3. **Batch 3:** screens 00–05 (crown_spire).
- Route regen at **every batch boundary** (not just the finale): redesigned
  layouts invalidate the committed trace, so each batch ends with
  `jumpcastle_solver --level assets/levels/screens --campaign --trace
  assets/levels/campaign-route.json` + full suites, keeping the branch green
  at every gate. Finale additionally: update
  `docs/tiled-workflow.md` (painter + workflow section); commit
  `chore: re-solve campaign over redesigned screens; regen proof trace`.
- No pushes; the user merges/PRs when satisfied (repo protocol).

## 9. Testing

- **Existing suites stay green** at every batch boundary: map_format, editor,
  collision/world, converter round-trip (Python), asset checks. Tiles are
  schema-v2 data — no C++ changes are expected at all.
- **Solver gate per screen** (reachability + ≤98% charge) and **route regen at
  the end** turn tests 73/80 green; replay opening robustness comes free from
  `tolerant_jump` certification.
- **Painter unit tests** (`tests/test_paint_screen.py`, pytest): role
  classification on a tiny synthetic collider set (interior/edge/corner/
  bridge/column), determinism (two runs byte-equal), coverage bounds check,
  screen-17 skip behavior, and entity-support validation failure case.
- **Visual:** preview PNG per screen reviewed by the user per batch
  (advisory, per project standards).

## 10. Acceptance criteria

1. All 17 redesigned/painted screens committed (screen-17 untouched unless
   reconciliation was requested); campaign solves end-to-end with ≤ 98% charge
   and 144–234 total jumps.
2. `ctest` 89/89 and `pytest` 24/24 (+ new painter tests) on the branch.
3. Entities byte-identical across all 18 maps (`git diff` on `entities` blocks
   is empty); hazard counts preserved per screen.
4. Painter re-run produces byte-identical `tiles.terrain` (determinism).
5. Coverage per redesigned screen within 35–55%; courtyard batch visually
   consistent with screen-17 (user approves each batch's previews).
6. `.tmj` files stay in sync (from-tiled(to-tiled(map)) round-trips).

## 11. Rollout phases (detailed by writing-plans)

- **P0 — Painter + validator + previews:** `tools/paint_screen.py` with tests;
  extract the screen-17 role mapping; prove determinism on screen-17 (paint →
  identical grid when `--include-17` on a scratch copy).
- **P1 — Batch 1 (courtyard 12–16):** redesign, solve-gate, paint, previews,
  route regen, user approval, commits.
- **P2 — Batch 2 (frosted_keep 06–11):** same.
- **P3 — Batch 3 (crown_spire 00–05):** same.
- **P4 — Finale:** route regen, suites green, workflow doc update.
