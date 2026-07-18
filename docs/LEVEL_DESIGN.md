# JumpCastle Continuous Campaign

JumpCastle loads one continuous world from `assets/levels/campaign.level`. The campaign is 32
tiles wide and 324 tiles tall: eighteen fixed-camera screens of 32 by 18 tiles. Screen numbers are
zero-based from bottom to top at runtime, so falling through a screen boundary remains a physical
fall into the previous screen. Only falling below the complete world resets the player.

## File format

The version 2 file contains strict metadata, a separator, a collision section, and exactly 324
rows of 32 collision tokens:

```text
version 2
tile_size 16
size 32 324
screen_height 18
spawn 4 322
goal 27 2
biome 1 6 courtyard
biome 7 12 frosted_keep
biome 13 18 crown_spire
---
[collision]
................................
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

## Designing legitimate jumps

The King occupies 0.8 by 0.8 world tiles. A solid row two grid rows above another leaves only one
empty row, so overlapping platforms can block the King's head. For close vertical steps, stagger
the ledges horizontally so the jump can pass around the upper edge before landing.

Use generous platform widths for mandatory jumps. Difficulty should come from charge selection,
direction changes, and safe spike placement rather than single-pixel precision.

## Solver contract

The headless solver samples launch positions and charge ticks at the same authoritative 120 Hz used
by the game. Solver and replay both call `step_world`, so a verified route uses production
collision, committed jump direction, wall bounce, and continuous cross-screen falls.

A route is accepted only when:

1. Every jump uses at most 85% of the production charge range.
2. The nominal jump reaches the intended landing surface.
3. At least three of five replays reach the same surface: nominal, charge -2 frames, charge +2
   frames, launch X -0.1 tile, and launch X +0.1 tile.
4. The single route reaches the goal across all eighteen screens.

Build and inspect a single room:

```bash
./build/cmake/jumpcastle_campaign_solver --level assets/levels/campaign.level
```

Verify the complete campaign and save the route:

```bash
./build/cmake/jumpcastle_campaign_solver \
  --level assets/levels/campaign.level \
  --trace build/campaign-route.json
```

Exit code 0 means a tolerant route exists, 1 means valid data with no acceptable route, and 2 means
invalid input or level data. A “nearest surface reached” failure usually means the next ledge is
too high, too far, too narrow, blocked from below, or unsafe under the tolerance replays. Adjust
geometry before considering any physics change; do not weaken the 85% or three-of-five limits.

## Required checks

After editing levels, run:

```bash
cmake --build build/cmake --parallel
ctest --test-dir build/cmake --output-on-failure
./build/cmake/jumpcastle_campaign_solver --level assets/levels/campaign.level
```

The tests validate the campaign shape, biome order, continuous-world reachability, replay trace, and
generated asset integrity. Legacy room files are archival only and are not runtime inputs.
