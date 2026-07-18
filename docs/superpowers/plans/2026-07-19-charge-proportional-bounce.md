# Charge-proportional (impact-scaled) wall bounce — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the wall-bounce restitution fraction scale with impact speed (capped at 1.0), unify to a single bounce formula, and re-certify the campaign so the whole verify pipeline is green.

**Architecture:** One pure `config::wall_bounce_restitution(impact_speed)` function is the single source of truth. The live polygon collision path (`collision_world.cpp`, shared by gameplay + solver + replay via `step_world`) and the legacy grid path (`collision.cpp`) both call it. Because the solver *is* the game physics, the solver becomes charge-proportional automatically; the cost is regenerating the proof trace via a re-solve.

**Tech Stack:** C++20, CMake, Catch2 (`catch_discover_tests`), nlohmann_json; Python for asset ctests. Reachability solver CLI `jumpcastle_solver`.

## Global Constraints

- C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`).
- Restitution ceiling ≤ `1.0` (energy-preserving; solver-fairness invariant). Never allow the wall to add energy.
- Solver/gameplay/replay physics parity: edit ONLY shared code + `game_config.hpp`; add NO new `PlayerState` fields.
- Re-solve acceptance: `reachable == true` and `maximum_charge_ratio <= 0.98` (`SolverConfig` default).
- Do NOT edit the campaign level to fix reachability — tune the four bounce constants only.
- Commit attribution is disabled globally — no `Co-Authored-By` trailer.
- All paths are under `/Users/thienphuprogrammer/Workspaces/Projects/JumpCastle-Raylib` (referred to below as repo root); commands run from repo root.
- A configured Release build dir already exists at `build/`; reuse it for fast incremental builds. CI uses a separate `build/cmake` dir.

---

## Task 1: Restitution model (config constants + pure function)

**Files:**
- Modify: `include/jumpcastle/game_config.hpp` (remove `horizontal_bounce` line 21; replace `wall_bounce` block lines 22-25; add includes)
- Test: `tests/collision_world_test.cpp` (add a `TEST_CASE`; already includes `game_config.hpp` and Catch2)

**Interfaces:**
- Produces: `float jumpcastle::config::wall_bounce_restitution(float impact_speed) noexcept` — monotonic non-decreasing, returns `wall_bounce_min` at/below `wall_bounce_impact_lo`, `wall_bounce_max` at/above `wall_bounce_impact_hi`, never > 1.0. New constants `wall_bounce_min`, `wall_bounce_max`, `wall_bounce_impact_lo`, `wall_bounce_impact_hi`.
- Removes: `config::horizontal_bounce`, `config::wall_bounce` (Tasks 2 and 3 update the two call sites that referenced them).

- [ ] **Step 1: Write the failing test** — append to `tests/collision_world_test.cpp`:

```cpp
TEST_CASE("wall_bounce_restitution scales with impact speed and caps at 1.0") {
    using config::wall_bounce_restitution;

    // Clamps to the minimum at/below the low anchor.
    REQUIRE(wall_bounce_restitution(0.0F) == Approx(config::wall_bounce_min));
    REQUIRE(wall_bounce_restitution(config::wall_bounce_impact_lo) ==
            Approx(config::wall_bounce_min));

    // Clamps to the maximum at/above the high anchor, never exceeding 1.0.
    REQUIRE(wall_bounce_restitution(config::wall_bounce_impact_hi) ==
            Approx(config::wall_bounce_max));
    REQUIRE(wall_bounce_restitution(100.0F) == Approx(config::wall_bounce_max));
    REQUIRE(wall_bounce_restitution(100.0F) <= 1.0F);

    // Monotonic ramp strictly between the anchors.
    const float mid = wall_bounce_restitution(
        0.5F * (config::wall_bounce_impact_lo + config::wall_bounce_impact_hi));
    REQUIRE(mid > config::wall_bounce_min);
    REQUIRE(mid < config::wall_bounce_max);
}
```

- [ ] **Step 2: Run test to verify it fails (does not compile)**

Run: `cmake --build build --target jumpcastle_tests`
Expected: FAIL — `wall_bounce_restitution` / `wall_bounce_min` are not members of `config`.

- [ ] **Step 3: Implement the model** in `include/jumpcastle/game_config.hpp`. Change the include block at the top from:

```cpp
#pragma once

#include "jumpcastle/math.hpp"
```
to:
```cpp
#pragma once

#include "jumpcastle/math.hpp"

#include <algorithm>
#include <cmath>
```

Then delete line 21 (`inline constexpr float horizontal_bounce = 0.6F;`) and replace the `wall_bounce` block (lines 22-25):

```cpp
// Jump King-style wall rebound: fraction of horizontal speed retained (and
// reversed) when an airborne player strikes a vertical wall in the polygon
// collision path. Floors and ceilings do not bounce.
inline constexpr float wall_bounce = 0.8F;
```
with:
```cpp
// Charge-proportional (impact-scaled) wall rebound. When an airborne player
// strikes a vertical wall in the polygon collision path, the restitution
// fraction ramps linearly with impact speed between the anchors below and is
// clamped to [wall_bounce_min, wall_bounce_max]. The <= 1.0 ceiling keeps the
// wall from adding kinetic energy (a solver-fairness invariant). Floors and
// ceilings do not bounce (their restitution stays 0 at the call site).
inline constexpr float wall_bounce_min = 0.55F;
inline constexpr float wall_bounce_max = 1.0F;
inline constexpr float wall_bounce_impact_lo = 4.5F;
inline constexpr float wall_bounce_impact_hi = 8.0F;

[[nodiscard]] inline constexpr float wall_bounce_restitution(
    const float impact_speed) noexcept {
    const float span = wall_bounce_impact_hi - wall_bounce_impact_lo;
    const float t = std::clamp((impact_speed - wall_bounce_impact_lo) / span,
                               0.0F, 1.0F);
    return std::lerp(wall_bounce_min, wall_bounce_max, t);
}
```

> **Learning-mode contribution point:** the `wall_bounce_restitution` body (the curve shape) is the one genuinely creative/tunable piece. During execution I will offer you the chance to author or adjust it (e.g. smoothstep instead of linear) before I proceed — the linear version above is the working reference.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target jumpcastle_tests && ./build/jumpcastle_tests "wall_bounce_restitution scales with impact speed and caps at 1.0"`
Expected: PASS (1 test case).

- [ ] **Step 5: Commit**

```bash
git add include/jumpcastle/game_config.hpp tests/collision_world_test.cpp
git commit -m "feat: add impact-scaled wall_bounce_restitution model"
```

---

## Task 2: Apply the model to the live polygon path

**Files:**
- Modify: `src/collision_world.cpp:131-136` (restitution computation)
- Test: `tests/collision_world_test.cpp:87` (update the wall-rebound assertion)

**Interfaces:**
- Consumes: `config::wall_bounce_restitution` (Task 1).

- [ ] **Step 1: Update the wall-rebound assertion** — `tests/collision_world_test.cpp:87`, change:

```cpp
    REQUIRE(player.velocity.x == Approx(-5.0F * config::wall_bounce).margin(1e-3));
```
to:
```cpp
    REQUIRE(player.velocity.x ==
            Approx(-5.0F * config::wall_bounce_restitution(5.0F)).margin(1e-3));
```

(The into-wall speed is 5.0, so the reflected x-velocity is `-5.0 * restitution(5.0)`; this stays correct under any curve tuning.)

- [ ] **Step 2: Run test to verify it fails (does not compile)**

Run: `cmake --build build --target jumpcastle_tests`
Expected: FAIL — `config::wall_bounce` no longer exists.

- [ ] **Step 3: Update the restitution computation** in `src/collision_world.cpp`, replace lines 131-136:

```cpp
                    const bool is_wall =
                        std::abs(mtv.normal.x) > std::abs(mtv.normal.y);
                    const float restitution =
                        (is_wall && player.mode == PlayerMode::airborne)
                            ? config::wall_bounce
                            : 0.0F;
```
with:
```cpp
                    const bool is_wall =
                        std::abs(mtv.normal.x) > std::abs(mtv.normal.y);
                    float restitution = 0.0F;
                    if (is_wall && player.mode == PlayerMode::airborne) {
                        const float impact_speed = -velocity_along_normal;
                        restitution = config::wall_bounce_restitution(impact_speed);
                    }
```

(`velocity_along_normal < 0` here, so `impact_speed > 0`. The reflection line 137-138 and the `is_wall && airborne` gate are unchanged.)

- [ ] **Step 4: Run the collision_world suite to verify it passes**

Run: `cmake --build build --target jumpcastle_tests` then run the four affected cases:
`./build/jumpcastle_tests "airborne player rebounds off a vertical wall like Jump King" "landing on a floor does not bounce the player back up" "bonking a ceiling absorbs upward velocity without a wall rebound" "grounded player does not bounce off a wall it walks into"`
Expected: PASS (4 cases). Floors/ceilings/grounded still get restitution 0.

- [ ] **Step 5: Commit**

```bash
git add src/collision_world.cpp tests/collision_world_test.cpp
git commit -m "feat: impact-scaled restitution in polygon wall bounce"
```

---

## Task 3: Unify the legacy grid path onto one formula

**Files:**
- Modify: `src/collision.cpp:36,41` (`resolve_world_x`) and `src/collision.cpp:165,170` (`resolve_tilemap_collision`)
- Test: `tests/collision_test.cpp:67` (update the reflected-velocity assertion)

**Interfaces:**
- Consumes: `config::wall_bounce_restitution` (Task 1). `<cmath>` (`std::abs`) and `config` are already available in `collision.cpp`.

- [ ] **Step 1: Update the grid-path assertion** — `tests/collision_test.cpp:67`, change:

```cpp
    CHECK(velocity.x == Approx(-10.0F * config::horizontal_bounce));
```
to:
```cpp
    CHECK(velocity.x == Approx(-10.0F * config::wall_bounce_restitution(10.0F)));
```

- [ ] **Step 2: Run test to verify it fails (does not compile)**

Run: `cmake --build build --target jumpcastle_tests`
Expected: FAIL — `config::horizontal_bounce` no longer exists (this and the four `src/collision.cpp` references).

- [ ] **Step 3: Replace all four `horizontal_bounce` uses** in `src/collision.cpp`.

`resolve_world_x` line 36:
```cpp
                    player.velocity.x = -player.velocity.x * config::horizontal_bounce;
```
→
```cpp
                    player.velocity.x = -player.velocity.x *
                        config::wall_bounce_restitution(std::abs(player.velocity.x));
```

`resolve_world_x` line 41: identical replacement (same two lines).

`resolve_tilemap_collision` line 165:
```cpp
                        velocity.x = -velocity.x * config::horizontal_bounce;
```
→
```cpp
                        velocity.x = -velocity.x *
                            config::wall_bounce_restitution(std::abs(velocity.x));
```

`resolve_tilemap_collision` line 170: identical replacement.

- [ ] **Step 4: Run the collision suite to verify it passes**

Run: `cmake --build build --target jumpcastle_tests` then:
`./build/jumpcastle_tests "wall collision reflects horizontal velocity" "landing clips the player to the floor and stops downward velocity" "swept world collision cannot tunnel through one tile ceiling"`
Expected: PASS.

- [ ] **Step 5: Confirm no stray references remain**

Run: `grep -rnE 'horizontal_bounce|config::wall_bounce\b' src include tests`
Expected: NO matches (all usages migrated to `wall_bounce_restitution`).

- [ ] **Step 6: Commit**

```bash
git add src/collision.cpp tests/collision_test.cpp
git commit -m "refactor: unify grid bounce onto wall_bounce_restitution, drop horizontal_bounce"
```

---

## Task 4: Fix the broken CI "Prove campaign route" step

**Files:**
- Modify: `.github/workflows/ci.yml:73-83`

- [ ] **Step 1: Replace the step body.** Change lines 73-83 from:

```yaml
      - name: Prove campaign route
        shell: bash
        run: |
          solver="build/cmake/jumpcastle_level_solver"
          if [[ "$RUNNER_OS" == "Windows" ]]; then
            solver="build/cmake/Release/jumpcastle_level_solver.exe"
          fi
          "$solver" \
            --levels assets/levels \
            --campaign \
            --trace build/campaign-route.json
```
to:
```yaml
      - name: Prove campaign route
        shell: bash
        run: |
          solver="build/cmake/jumpcastle_solver"
          if [[ "$RUNNER_OS" == "Windows" ]]; then
            solver="build/cmake/Release/jumpcastle_solver.exe"
          fi
          "$solver" \
            --level assets/levels/campaign.level \
            --campaign \
            --trace build/campaign-route.json
          "$solver" \
            --level assets/levels/campaign.level \
            --verify-trace build/campaign-route.json
```

(Fixes: binary `jumpcastle_level_solver`→`jumpcastle_solver`; flag `--levels DIR`→`--level assets/levels/campaign.level`; adds the `--verify-trace` replay gate on the freshly emitted trace.)

- [ ] **Step 2: Sanity-check the invocation locally** (proves the fixed command works before it reaches CI). This runs the ~7–8 min solve:

Run:
```bash
./build/jumpcastle_solver --level assets/levels/campaign.level --campaign --trace /tmp/ci-check-route.json
./build/jumpcastle_solver --level assets/levels/campaign.level --verify-trace /tmp/ci-check-route.json
```
Expected: first command prints `reachable: ... jumps, max charge ...%, highest screen ...` and exits 0; second exits 0 (trace replays its verified portion).

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: fix Prove campaign route (jumpcastle_solver, --level, + verify-trace)"
```

---

## Task 5: Re-solve and regenerate the proof trace

**Files:**
- Modify (regenerate): `assets/levels/campaign-route.json`
- Modify (only if the certified numbers shift): `tests/world_reachability_test.cpp`

**Interfaces:**
- Consumes: the rebuilt `jumpcastle_solver` (Tasks 2–3 changed the physics it links).

- [ ] **Step 1: Rebuild the solver with the new physics**

Run: `cmake --build build --target jumpcastle_solver`
Expected: builds clean.

- [ ] **Step 2: Re-solve to regenerate the committed trace**

Run:
```bash
./build/jumpcastle_solver --level assets/levels/campaign.level --campaign \
    --trace assets/levels/campaign-route.json
```
Expected: exits 0 and prints `reachable: <N> jumps, max charge <R>%, highest screen <S>`. **Record N, R, S** — Step 4 needs them.

- [ ] **Step 3: Decision gate — reachable & fair?**
  - If exit 0, `reachable`, and `R <= 98.0`: proceed to Step 4.
  - If NOT reachable, or `R > 98.0`: tune the four constants in `include/jumpcastle/game_config.hpp` (raise `wall_bounce_min`/`wall_bounce_max` or narrow `impact_lo..impact_hi` to make walls carry more speed and open routes; do NOT edit the level), rebuild the solver (Step 1), and re-solve (Step 2). Repeat until the gate passes. Note each iteration is ~7–8 min.

- [ ] **Step 4: Reconcile the reachability test if numbers moved.** Open `tests/world_reachability_test.cpp` and compare its asserted `highest_screen` and charge-ratio bound against the recorded S/R. The CLI prints `highest_screen + 1`, so the test's `highest_screen` assertion is `S - 1`. If they differ from what the test asserts, update the assertion(s) to the new certified values (charge bound stays `<= SolverConfig{}.max_charge_ratio`, i.e. `0.98`). If they match, make no change.

- [ ] **Step 5: Verify the regenerated trace replays**

Run: `./build/jumpcastle_solver --level assets/levels/campaign.level --verify-trace assets/levels/campaign-route.json`
Expected: exit 0.

- [ ] **Step 6: Commit**

```bash
git add assets/levels/campaign-route.json tests/world_reachability_test.cpp
git commit -m "chore: re-solve campaign under charge-proportional bounce; regenerate proof trace"
```

---

## Task 6: Reconcile docs to the impact-scaled model

**Files:**
- Modify: `docs/LEVEL_DESIGN.md` (line ~71), `docs/superpowers/specs/2026-07-18-map-redesign-design.md` (line ~83 `bounce 0.45`), `docs/superpowers/specs/2026-07-17-cpp20-modernization-design.md` (lines ~75/103/142/176), `docs/superpowers/plans/2026-07-18-continuous-campaign-core.md` (line ~422)

- [ ] **Step 1: Update each fixed-coefficient bounce mention.** In each location above, replace the fixed-coefficient wording (e.g. "wall bounce (0.45)", "multiply incoming horizontal velocity by `-config::horizontal_bounce`") with the impact-scaled description: the airborne wall rebound uses `config::wall_bounce_restitution(impact_speed)` — restitution ramps from `wall_bounce_min` to `wall_bounce_max` (≤ 1.0) with impact speed, so harder-charged jumps rebound proportionally bouncier; there is a single bounce formula shared by the polygon and grid paths. Reference the design spec `2026-07-19-charge-proportional-bounce-design.md`.

- [ ] **Step 2: Confirm no doc still cites the retired constant as current**

Run: `grep -rnE 'horizontal_bounce|bounce 0.45|wall_bounce = 0.8' docs`
Expected: no matches presenting the retired constants as current (the THIS-change spec/plan may name them historically — that's fine).

- [ ] **Step 3: Commit**

```bash
git add docs
git commit -m "docs: describe impact-scaled wall bounce and single bounce path"
```

---

## Task 7: Full verify — everything green

**Files:** none (verification only)

- [ ] **Step 1: Build all targets**

Run: `cmake --build build`
Expected: `jumpcastle`, `jumpcastle_solver`, `jumpcastle_tests` all build clean.

- [ ] **Step 2: Run the fast C++ suites** (exclude the ~8-min reachability solve to fail fast on regressions first)

Run: `ctest --test-dir build --output-on-failure -E 'reachab|campaign'`
Expected: all pass (collision, collision_world, solver, replay, player, simulation, world, etc.).

- [ ] **Step 3: Run the slow reachability test**

Run: `ctest --test-dir build --output-on-failure -R 'reachab'`
Expected: PASS (~7–8 min). This asserts the regenerated campaign is reachable with charge ≤ 0.98.

- [ ] **Step 4: Run the exact CI prove + verify commands**

Run:
```bash
./build/jumpcastle_solver --level assets/levels/campaign.level --campaign --trace /tmp/final-route.json
./build/jumpcastle_solver --level assets/levels/campaign.level --verify-trace /tmp/final-route.json
diff <(jq -S . /tmp/final-route.json) <(jq -S . assets/levels/campaign-route.json) && echo "trace is deterministic ✓"
```
Expected: both solver runs exit 0; the `diff` is empty (the committed trace matches a fresh solve — deterministic proof).

- [ ] **Step 5: Final commit (only if Step 4 revealed a trace drift to re-commit)**

```bash
git add -A
git commit -m "test: verify charge-proportional bounce pipeline green end-to-end"
```

---

## Self-Review

- **Spec coverage:** impact-scaled restitution + cap 1.0 (Tasks 1–2) ✓; no new player state / shared-code parity (Task 2 edits only the shared `step_world` path) ✓; one bounce path + drop `horizontal_bounce` (Task 3) ✓; fix CI binary/flag + add verify-trace (Task 4) ✓; re-solve & regenerate trace with `<=0.98` gate (Task 5) ✓; docs reconciliation (Task 6) ✓; non-goals honored (no solver speedup; replay test untouched) ✓.
- **Placeholders:** none — every code step shows the exact before/after; the only "iterate" is the empirical constant-tuning loop in Task 5 Step 3, which is inherent to certifying physics and has a concrete pass/fail gate.
- **Type consistency:** `wall_bounce_restitution(float) -> float` used identically in Tasks 1–3; removed symbols `horizontal_bounce`/`wall_bounce` are updated at all four+two known sites (verified by the grep steps).
