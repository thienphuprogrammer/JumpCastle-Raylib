# Map Redesign — Brutal Tower & Richer Visuals

**Date:** 2026-07-18
**Status:** Design approved — awaiting spec review before implementation planning
**Author:** brainstormed with the JumpCastle developer

## 1. Goal

Rebuild the campaign tower so it is (a) **brutally hard from the very first screen**
in the spirit of Jump King, and (b) **visually alive**, drawing on the large asset
library that the runtime currently ignores.

Two root causes drive the current state:

- **"Assets nhiều mà dùng ít":** the runtime only ever loads ONE tileset.
  `renderer.cpp:312` hardcodes `castle_texture_ = biome(Biome::pixel_adventure).atlas`
  and reuses it for every biome; biomes differ only by background tint. The zip asset
  library is never baked into a runtime atlas.
- **"Map chán":** the campaign format (`world_loader.cpp`) accepts only `.`/`#` in its
  single `[collision]` section — no hazards, props, or decoration. `narrow_campaign.py`
  emits uniform 3-row-spaced, 5-wide platforms, producing monotony.

## 2. Scope & Non-Goals

**In scope**
- Difficulty increase via **geometry only**.
- Visual richness: per-biome tilesets, parallax backgrounds, decorative props.

**Non-goals (explicit)**
- No new gameplay mechanics: no spikes, traps, moving platforms, or instant-death
  hazards. The `WorldMap` collision model stays `solid`/`empty`.
- No checkpoint system (the format has none; the tower remains one continuous fall).
- Decorative props are **visual only** — they never collide.
- Keep world dimensions: **28 × 648**, **18 screens**, **3 biomes**, spawn (3,646),
  goal (24,1).

## 3. Hard Constraints

### 3.1 The solver is the safety net
CI fails only when the campaign is **UNREACHABLE** (`solver_main.cpp:79` returns
`result.reachable ? 0 : 1`). `tolerance_passed` is computed but **not** enforced by
CI. Therefore brutal geometry is permitted, provided the solver still finds a route.

### 3.2 Charge-jump physics envelope
Derived from `game_config.hpp` + `charged_jump_velocity`:

| Quantity | Value | Meaning |
|----------|-------|---------|
| Full-charge horizontal speed | 8 tiles/s | — |
| Full-charge launch (up) | 15 tiles/s | — |
| Gravity | 30 tiles/s² | — |
| **Max rise** | **3.75 tiles** | vertical ceiling of a single jump |
| **Max flat reach** | **8 tiles** | horizontal ceiling at same height |
| Reach when climbing 3 tiles | ~5.8 tiles | (current map jumps 6 → already near-threshold) |

Every authored jump must stay inside this envelope AND remain solver-reachable.

## 4. Phased Plan

| Phase | Name | Files | Format change | Risk |
|-------|------|-------|---------------|------|
| 1 | Brutal geometry | `tools/narrow_campaign.py`, `tests/test_narrow_campaign.py`, regen `assets/levels/campaign.level` | No | Low |
| 2 | Per-biome tilesets | `tools/build_assets.py`, `assets/generated/manifest.json`, `src/assets.cpp`, `src/renderer.cpp` | No (manifest only) | Medium |
| 3 | Parallax backgrounds | `src/renderer.cpp` (+ baked bg layers) | No | Medium |
| 4 | Decorative props | `src/world_loader.cpp`, `src/world.*`, `src/renderer.cpp`, `tools/narrow_campaign.py` | **Yes** (`[decoration]`) | High |

Each phase ends CI-green and is independently shippable. Stopping after Phase 1 still
yields a meaningfully harder map.

## 5. Phase 1 — Brutal Geometry (detailed)

**Philosophy:** brutal through *consequences and precision*, not randomness. The main
route keeps passing `tolerant_jump` (±0.1 tile, ±2 ticks) so it is humanly executable;
missing a jump is punished harshly by long falls.

### 5.1 Difficulty levers (all measured by the solver)
1. **Shrink landing platforms.** Route platforms drop from 5 tiles to **2–3 tiles**,
   with **1-tile "pin" landings** in the hardest Crown Spire chambers. Biggest,
   cheapest difficulty gain.
2. **Diversify jump gaps.** Mix, within the physics envelope:
   - *Near-vertical* (climb 3, horizontal ≤2) — forces near-full charge.
   - *Near-horizontal* (climb 1, reach ~7) — near the 8-tile reach ceiling.
   - *Rebound* — jump into a wall (bounce 0.45) to reach an offset platform.
3. **Punishing falls ("no-net").** Hard chambers deliberately thin out the catch
   platforms and stagger them so a miss falls **through several screens**. This is the
   Jump King feel; fairness is preserved because the *upward* route stays in tolerance.
4. **Low ceilings / overhangs.** Force reduced, precise charges (a full charge hits the
   ceiling). Expands the existing "Low-Ceiling Hall" / "Overhang Reversal" chambers.

### 5.2 Difficulty curve
**Brutal from screen 1 — no teaching screens** (explicit user decision). All 18 screens
sit near the physics threshold. A light *structural* ramp remains only in the sense that
Crown Spire adds 1-tile pins and longer no-net falls on top of an already-hard base.

| Biome | Screens | Character |
|-------|---------|-----------|
| Courtyard | 1–6 | Brutal from the first jump: 2–3-tile platforms, mixed gaps immediately |
| Frosted Keep | 7–12 | 2-tile platforms, rebounds + overhangs, first long no-net falls |
| Crown Spire | 13–18 | 1-tile pins, chained rebounds, misses drop 3–5 screens, near-max every jump |

### 5.3 Acceptance criteria
- **Required:** `jumpcastle_solver --campaign` returns `reachable` (exit 0).
- **Brutality target:** solver-reported `max charge %` ≥ **90%**.
- **Fairness:** the main route keeps passing `tolerant_jump`.
- `tests/test_narrow_campaign.py` updated to the new invariants (dimensions, chamber
  count, bounds checks, no overlaps, new platform-width policy).
- The committed replay trace still verifies against the regenerated map.

## 6. Phase 2 — Per-Biome Tilesets

The data model already supports this: `BiomeAssets.atlas` (`assets.hpp:36`) is per-biome;
the renderer simply ignores it and hardcodes one texture. This phase is mostly wiring.

- `build_assets.py`: bake two additional atlases and register them in the manifest.
- **Proposed source-pack → biome mapping** (confirm during review):
  - Courtyard → **castle** (pixel_adventure) — keep.
  - Frosted Keep → **Sunnyland Winter Forest**.
  - Crown Spire → **Gothicvania Swamp** (alternative: **Cold Corridors**).
- `renderer.cpp`: replace the single `castle_texture_` with a per-biome texture set,
  selected by `camera.biome` inside `draw()` / `capture_screen()`.

## 7. Phase 3 — Parallax Backgrounds

- Tall Forest ships `far`/`middle`/`back` layers; bake per-biome background layers.
- Replace `draw_background` (currently a single sprite tiled at 12% opacity) with a
  **vertical parallax** pass: as the camera changes band while climbing, the `far` layer
  scrolls slowest and `back` fastest, creating depth.

## 8. Phase 4 — Decorative Props (format change)

- Add an **optional** `[decoration]` section to `campaign.level`, after `[collision]`,
  with the same grid dimensions. Glyphs map to prop sprites (torch, banner, plant, rock);
  props **do not collide**.
- Backward compatible: level files without the section still parse (old files unaffected).
- Touches: `world_loader.cpp` (parser — relax the "unexpected content after collision
  grid" check at lines 213–218), `world.*` (store decoration), `renderer.cpp` (draw
  props), `narrow_campaign.py` (emit per-biome decoration).

## 9. Testing Strategy

- **Phase 1:** regenerate map, run `jumpcastle_solver --campaign` (reachable + max
  charge %), update & run `test_narrow_campaign.py`, verify the replay trace.
- **Phase 2:** `asset_manifest` / `asset_pixels` / `asset_selection` tests; headless
  `--smoke-screens` capture to confirm three visually distinct biomes.
- **Phase 3:** smoke-screen capture shows layered backgrounds; no regression in
  `asset_render_smoke`.
- **Phase 4:** parser unit tests for the new section (present, absent, malformed);
  smoke capture shows props; backward-compat test loads a decoration-less file.

## 10. Risks

- **Phase 1 over-tightening:** a jump becomes unreachable → solver catches it (exit 1),
  iterate. Low risk because the solver is authoritative.
- **Phase 4 parser change:** the only format change; a mistake breaks map loading for
  every level. Mitigated by keeping the section optional and by dedicated parser tests.
- **Asset licensing:** confirm the chosen packs' licenses permit redistribution before
  baking them into the runtime atlas (`assets/sources/*/SOURCE.md`, CC0 files present).
