# Charge-proportional (impact-scaled) wall bounce + solver re-verify

**Date:** 2026-07-19
**Status:** Approved (brainstorm), pending implementation plan
**Supersedes:** the fixed-coefficient wall-bounce description in
`2026-07-17-cpp20-modernization-design.md`, `2026-07-18-map-redesign-design.md`
(the `bounce 0.45` Rebound lever), and `docs/LEVEL_DESIGN.md` for the bounce
coefficient only. The reachability-solver contract (v2: 120 Hz, `step_world`,
90% charge budget, single route across all screens) is unchanged.

## Problem

The wall rebound is a single fixed constant. In the live continuous-collision
path (`src/collision_world.cpp`, used by `step_world` and therefore shared by
gameplay, the reachability solver, and the replay verifier) an airborne player
striking a vertical wall retains `config::wall_bounce = 0.8` of the into-wall
velocity, reversed. A second, divergent constant `config::horizontal_bounce =
0.6` lives in the legacy grid resolver (`src/collision.cpp`) that is reached
only by tests. Design docs still specify `0.45`; the code has drifted to
`0.6`/`0.8`.

We want the wall bounce to reward charge: a harder-charged jump should rebound
*proportionally bouncier*, not merely faster. Today the rebound is already
proportional to impact speed with a **constant** restitution fraction, so
"charge-proportional bounce" is the new requirement that the restitution
*fraction itself* rises with a harder hit.

## Goals

1. Make the wall restitution fraction scale with **impact speed** (the
   into-wall component of velocity at the moment of the hit), capped at `1.0`.
2. Keep the reachability solver, replay verifier, and live gameplay in exact
   physics parity — achieved for free by editing only the shared
   `step_world` collision path and adding no player state.
3. Re-certify the campaign under the new physics: regenerate the proof trace
   and get the whole verify pipeline (tests + solver prove step + trace replay)
   green.
4. Collapse to a single bounce path and reconcile the docs.

## Non-goals (explicitly out of scope)

- **Speeding up `solve_campaign`** (~7–8 min on the 18-screen campaign). We
  accept the current solve time for now.
- **Tightening the full-route replay test.** `tests/replay_test.cpp` continues
  to verify only the opening (`executed_jumps >= 12`) of the committed route,
  as it does today, because open-loop replay accumulates polygon drift.
- Level/layout redesign. If reachability breaks, we tune bounce constants, not
  the level.

## Design

### Bounce driver: impact-speed-scaled restitution

Chosen over launch-charge-scaled restitution because impact speed is already
local at the collision site (`velocity_along_normal`, `collision_world.cpp:113`)
and needs no persisted player state, so solver/gameplay/replay parity is
automatic. Impact speed is strongly charge-correlated (horizontal launch speed
spans ~4.5→8.0 from min to full charge), so a harder-charged jump hits the wall
faster and, under the new curve, rebounds proportionally bouncier.

**Ceiling = 1.0 (energy-preserving).** Restitution is never allowed above 1.0,
so a wall can never add kinetic energy. This is a solver-fairness invariant:
the exhaustive solver would otherwise be able to certify routes that exploit
free energy a fair human could not chain.

### Restitution mapping

A small pure function, single source of truth for both collision paths:

```cpp
// game_config.hpp (or a collision helper) — pure, testable
float wall_bounce_restitution(float impact_speed) {
    const float t = std::clamp(
        (impact_speed - wall_bounce_impact_lo) /
            (wall_bounce_impact_hi - wall_bounce_impact_lo),
        0.0f, 1.0f);
    return std::lerp(wall_bounce_min, wall_bounce_max, t);  // wall_bounce_max <= 1.0
}
```

Properties (to be unit-tested): monotonic non-decreasing in `impact_speed`;
equals `wall_bounce_min` at/below `impact_lo`; equals `wall_bounce_max` at/above
`impact_hi`; never exceeds `1.0`.

### Config constants (replace `wall_bounce`, remove `horizontal_bounce`)

Initial anchors in `include/jumpcastle/game_config.hpp`, **tuned empirically by
the re-solve** (§ Re-verify):

| constant | initial | meaning |
|---|---|---|
| `wall_bounce_min` | `0.55` | restitution for a slow / min-charge wall hit |
| `wall_bounce_max` | `1.0`  | energy-preserving ceiling at a hard hit |
| `wall_bounce_impact_lo` | `4.5` | impact speed at/below which restitution = min (≈ min-charge horizontal launch) |
| `wall_bounce_impact_hi` | `8.0` | impact speed at/above which restitution = max (≈ full-charge horizontal launch) |

With these anchors a typical mid-charge impact (~6.25) yields restitution
≈ `0.78` — close to today's flat `0.8` — while min-charge hits are less bouncy
(`0.55`) and full-charge hits reach `1.0`, giving a clear charge gradient.

### Collision call site (`src/collision_world.cpp:112-128`)

Only the `restitution` computation changes; the reflection expression
`player.velocity -= mtv.normal * (velocity_along_normal * (1 + restitution))`
and the `is_wall && airborne` gating are unchanged:

```cpp
float restitution = 0.0f;
if (is_wall && player.mode == PlayerMode::airborne) {
    const float impact_speed = -velocity_along_normal;   // > 0 (moving into surface)
    restitution = config::wall_bounce_restitution(impact_speed);
}
```

### One bounce path

Remove the divergent `config::horizontal_bounce` constant. Route the legacy
grid resolver (`src/collision.cpp` `resolve_world_x` / `resolve_tilemap_collision`)
through the same `wall_bounce_restitution` (using `std::abs(velocity.x)` as the
impact speed), so there is exactly one bounce formula in the codebase.

**Blast-radius decision (during implementation):** if `src/collision.cpp` /
the `WorldMap` grid path is confirmed reachable only from tests (not gameplay,
solver, or replay), prefer **deleting** the grid bounce path and its tests over
unifying it. The chosen outcome is recorded back into this spec.

### Docs reconciliation

Update the bounce description in `2026-07-17-cpp20-modernization-design.md`,
`2026-07-18-map-redesign-design.md`, `docs/LEVEL_DESIGN.md`, and the relevant
plan files to describe the impact-scaled model and the single bounce path.

## Re-verify (the solver-verify half)

### Fix CI "Prove campaign route" (`.github/workflows/ci.yml`)

Currently broken two ways and has never passed:
- binary `jumpcastle_level_solver` → the real target is `jumpcastle_solver`
  (`CMakeLists.txt:81`);
- flag `--levels assets/levels` (a directory) → the CLI only accepts
  `--level FILE`; the current invocation exits `2` before solving.

Fix both, point `--level` at `assets/levels/campaign.level`, and **add a
follow-up `--verify-trace` run** on the emitted trace so the walk-in replay
fairness gate is actually exercised in CI.

### Re-solve and regenerate the proof

Because gameplay/solver/replay share `step_world`, the bounce change
invalidates the committed `assets/levels/campaign-route.json` (218 jumps).
Regenerate it:

```
jumpcastle_solver --level assets/levels/campaign.level --campaign \
    --trace assets/levels/campaign-route.json
```

**Acceptance:** exit 0, `reachable`, and `max_charge_ratio <= 0.98`
(`SolverConfig` default; the reachability test also asserts this). If the new
curve makes the campaign unreachable or pushes charge over budget, tune the
four bounce constants (never the level) and re-solve until it certifies.

### Tests to reconcile

- `tests/collision_world_test.cpp`: the wall-bounce cases assert
  `-5.0 * config::wall_bounce`; update to the impact-scaled expected value
  `-5.0 * wall_bounce_restitution(5.0)` and add cases pinning the mapping's
  monotonicity, clamps, and the `<= 1.0` ceiling.
- `tests/world_reachability_test.cpp`: if the regenerated route changes
  `highest_screen` or the charge ratio, update the assertions to the new
  certified values (still `<= 0.98`).
- Legacy grid collision tests: update or delete per the blast-radius decision.

## Testing strategy

1. TDD `wall_bounce_restitution` (pure function): min/max clamps, monotonicity,
   ceiling.
2. Update/extend `collision_world_test` for the new restitution at representative
   impact speeds and confirm floors/ceilings/grounded still get restitution 0.
3. Full `ctest` (C++ + Python asset ctests), the solver `--campaign` prove step,
   and `--verify-trace` on the regenerated trace.

## Risks

- **Re-solve loop is slow (~8 min/iteration).** Tuning the four constants to
  keep the campaign beatable/fair is an iterative, slow loop — the accepted
  "significant cost / solver-verify may still have bugs" risk.
- **Reachability regression.** A materially different bounce curve can break the
  route the previous flat 0.8 constant enabled; mitigated by tuning constants
  and re-solving, bounded by the `<= 0.98` charge budget.
- **Impact-speed anchors are estimates.** `impact_lo/hi ≈ 4.5/8.0` assume the
  into-wall component tracks horizontal launch speed; validated empirically by
  inspecting solver output during the re-solve.

## Open items resolved during implementation

- Exact final values of the four bounce constants (tuned via re-solve).
- Whether the legacy grid bounce path is unified or deleted (blast-radius check).
