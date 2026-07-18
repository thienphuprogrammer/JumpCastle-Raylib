# JumpCastle Narrow Vertical Tower

JumpCastle loads one continuous world from `assets/levels/campaign.level`. The campaign is 28
tiles wide and 648 tiles tall: eighteen fixed-camera screens of 28 by 36 tiles. Screen numbers are
zero-based from bottom to top at runtime, so falling through a screen boundary remains a physical
fall into the previous screen. Only falling below the complete world resets the player.

## File format

The version 2 file contains strict metadata, a separator, a collision section, and exactly 648
rows of 28 collision tokens:

```text
version 2
tile_size 16
size 28 648
screen_height 36
spawn 3 646
goal 24 1
biome 1 6 courtyard
biome 7 12 frosted_keep
biome 13 18 crown_spire
---
[collision]
............................
...
```

Metadata keys are required once and unknown or duplicate keys are rejected. Spawn and goal use
zero-based tile coordinates and must be empty cells with solid terrain immediately below.

The three biome ranges must cover all screens exactly once: `courtyard`, `frosted_keep`, then
`crown_spire`.

## Tile tokens

| Token | Meaning |
| --- | --- |
| `.` | Empty space |
| `#` | Solid terrain |

There are no spikes, checkpoints, rescue teleports, or procedural platforms in the collision
source. Side boundaries are solid outside the world; authored rebound walls are used sparingly.

`tools/narrow_campaign.py` is the deterministic authoring representation for the committed file.
It contains eighteen explicit `Chamber` records and uses no randomness. Regenerate the runtime
collision file after changing those coordinates:

```bash
rtk proxy out/assets-venv/bin/python tools/narrow_campaign.py \
  --output assets/levels/campaign.level
```

Local chamber row `y` maps to the global top-down collision row with
`(18 - screen_number) * 36 + y`, where `screen_number` is one-based from the bottom.

## Designing legitimate jumps

The King occupies 0.8 by 0.8 world tiles. A solid row two grid rows above another leaves only one
empty row, so overlapping platforms can block the King's head. For close vertical steps, stagger
the ledges horizontally so the jump can pass around the upper edge before landing.

Each chamber contains eight to twelve primary jumps. Use readable platform widths for mandatory
jumps. Difficulty comes from charge selection, direction changes, wall rebounds, constrained
ceilings, overhangs, and fall recovery rather than single-pixel precision.

## Solver contract

The headless solver samples launch positions and charge ticks at the same authoritative 120 Hz used
by the game. Solver and replay both call `step_world`, so a verified route uses production
collision, committed jump direction, wall bounce, and continuous cross-screen falls. The wall
bounce is charge-proportional: an airborne wall strike rebounds with a restitution that scales
with impact speed via `config::wall_bounce_restitution` (smoothstep-eased from `wall_bounce_min`
up to a 1.0 energy-preserving cap), so a harder-charged jump rebounds proportionally bouncier.
See `docs/superpowers/specs/2026-07-19-charge-proportional-bounce-design.md`.

A route is accepted only when:

1. Every jump uses at most 90% of the production charge range.
2. The nominal jump reaches the intended landing surface.
3. At least three of five replays reach the same surface: nominal, charge -2 frames, charge +2
   frames, launch X -0.1 tile, and launch X +0.1 tile.
4. The single route reaches the goal across all eighteen screens.

Build and solve the complete campaign:

```bash
rtk cmake --build out/campaign-final --target jumpcastle_solver --parallel
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
```

Verify the complete campaign and save the route:

```bash
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign \
  --trace assets/levels/campaign-route.json
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level \
  --verify-trace assets/levels/campaign-route.json
```

Exit code 0 means a tolerant route exists, 1 means valid data with no acceptable route, and 2 means
invalid input or level data. A “nearest surface reached” failure usually means the next ledge is
too high, too far, too narrow, blocked from below, or unsafe under the tolerance replays. Adjust
geometry before considering any physics change; do not weaken the 90% or three-of-five limits.

## Required checks

After editing levels, run:

```bash
rtk cmake --build out/campaign-final --parallel
rtk ctest --test-dir out/campaign-final --output-on-failure
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level --campaign
rtk proxy ./out/campaign-final/jumpcastle_solver \
  --level assets/levels/campaign.level \
  --verify-trace assets/levels/campaign-route.json
```

The tests validate the campaign shape, biome order, continuous-world reachability, replay trace, and
generated asset integrity. Legacy room files are archival only and are not runtime inputs.
