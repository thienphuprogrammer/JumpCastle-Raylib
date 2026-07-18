# Phase 1 — Brutal Geometry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Rework `tools/narrow_campaign.py` so the regenerated `campaign.level` is brutally hard from screen 1 (precise landings, diverse gaps, punishing "no-net" falls) while remaining solver-reachable.

**Architecture:** Keep the proven route topology (per-screen `ROUTE_Y` climbs + `ROUTE_X` columns) as a reachable backbone, but (a) shrink route platform widths, (b) escalate hardest gaps toward max charge, (c) rename chambers away from "training". The C++ solver is the arbiter of reachability; iterate against it.

**Tech Stack:** Python 3 (`narrow_campaign.py`, pytest), prebuilt `out/campaign-final/jumpcastle_solver`.

## Global Constraints
- World stays **28 × 648**, **18 screens** of 36 rows, spawn (3,646), goal (24,1), biomes `courtyard 1-6 / frosted_keep 7-12 / crown_spire 13-18`.
- Collision grid emits only `.`/`#` (no format change this phase).
- **Baseline measured:** current map reachable, `max charge 85.3%`, hardest jump 87/93 ticks.
- **Acceptance:** `jumpcastle_solver --campaign` returns reachable (exit 0) AND `max charge % >= 90`. Update `tests/test_narrow_campaign.py` to new invariants. Regenerated replay verifies.

---

### Task 1: Brutal platform-width + gap policy in `narrow_campaign.py`

**Files:**
- Modify: `tools/narrow_campaign.py`
- Test (arbiter): `out/campaign-final/jumpcastle_solver`

**Interfaces:**
- Produces: `CHAMBERS` (18 `Chamber`), `render_campaign(CHAMBERS) -> str`, per-chamber `.mechanic`, `.route_jump_count`.

- [ ] **Step 1:** Introduce a per-biome width policy: Courtyard route platforms width 3, Frosted Keep width 2, Crown Spire width 1-2 "pins". Replace the hardcoded `5`/`6` in `_base_chamber`'s route builder with a `route_width(screen)` helper.
- [ ] **Step 2:** Rename chamber mechanics away from "training_*" (brutal from screen 1). New Courtyard mechanics reflect immediate difficulty, e.g. `opening_gauntlet, long_gap, wall_rebound, low_ceiling, central_tower, gatehouse_exam`.
- [ ] **Step 3:** Escalate Crown Spire: add 1-wide pins + one or two lengthened gaps so the hardest required jump climbs toward 90-95% charge; thin redundant catch platforms to create no-net falls.
- [ ] **Step 4:** Regenerate + solve. Run:
  ```bash
  python3 tools/narrow_campaign.py --output assets/levels/campaign.level
  ./out/campaign-final/jumpcastle_solver --level assets/levels/campaign.level --campaign | head -1
  ```
  Expected: `reachable: ... max charge >=90%, highest screen 18`. If unreachable, widen the offending platform by 1 and repeat. If max charge < 90, lengthen a hardest-chamber gap and repeat.
- [ ] **Step 5:** Commit once green:
  ```bash
  git add tools/narrow_campaign.py assets/levels/campaign.level
  git commit -m "feat(level): brutal narrow-tower geometry, solver-verified >=90% charge"
  ```

### Task 2: Update authoring invariants in `tests/test_narrow_campaign.py`

**Files:**
- Modify: `tests/test_narrow_campaign.py`

- [ ] **Step 1:** Update the mechanic-name assertions (courtyard/frosted/crown sets) to the renamed mechanics from Task 1.
- [ ] **Step 2:** Replace the `8 <= route_jump_count <= 12` bound if the new per-screen platform count changed; add a width-policy assertion (e.g. Crown Spire has >=1 width-1 route platform).
- [ ] **Step 3:** Run:
  ```bash
  python3 -m pytest tests/test_narrow_campaign.py -q
  ```
  Expected: all pass.
- [ ] **Step 4:** Commit:
  ```bash
  git add tests/test_narrow_campaign.py
  git commit -m "test(level): update narrow-tower invariants for brutal geometry"
  ```

### Task 3: Regenerate & verify the replay trace

**Files:**
- Modify: the committed solver trace consumed by `verify-trace`.

- [ ] **Step 1:** Locate the committed trace file (grep CI + docs for `--verify-trace` / `--trace`).
- [ ] **Step 2:** Regenerate it from the new map:
  ```bash
  ./out/campaign-final/jumpcastle_solver --level assets/levels/campaign.level --campaign --trace <trace_path>
  ```
- [ ] **Step 3:** Verify it replays:
  ```bash
  ./out/campaign-final/jumpcastle_solver --level assets/levels/campaign.level --verify-trace <trace_path>
  ```
  Expected: `replay complete: goal reached in N jumps`.
- [ ] **Step 4:** Commit:
  ```bash
  git add <trace_path>
  git commit -m "test(level): regenerate solver replay for brutal geometry"
  ```

### Task 4: Full authoring test sweep

- [ ] **Step 1:** Run the Python authoring/asset tests that don't need a fresh C++ build:
  ```bash
  python3 -m pytest tests/test_narrow_campaign.py tests/test_asset_selection.py -q
  ```
  Expected: pass. Fix any regressions before proceeding to Phase 2.
