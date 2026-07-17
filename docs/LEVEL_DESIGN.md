# JumpCastle Level Design

JumpCastle loads exactly twelve room files, named `room-01.level` through `room-12.level`, in
bottom-to-top order. A committed campaign is accepted only when its schema and production-physics
route both pass the test suite.

## File format

Every room starts with three metadata fields, a separator, and exactly twelve rows of sixteen
tokens:

```text
name=First Steps
biome=pixel_adventure
difficulty=1
---
................
#..######......#
...
################
```

Metadata keys are required once and unknown keys are rejected.

| Field | Allowed value |
| --- | --- |
| `name` | Non-empty display name |
| `biome` | `pixel_adventure`, `kenney`, or `kings_and_pigs` |
| `difficulty` | Integer from 1 through 4 |

## Tile tokens

| Token | Meaning |
| --- | --- |
| `.` | Empty space |
| `#` | Solid terrain |
| `^` | Lethal spike |
| `S` | Campaign spawn |
| `C` | Checkpoint |
| `E` | Campaign exit |

Only room 1 may contain the single `S`; only room 12 may contain the single `E`. Rooms 1, 5, and 9
each contain one checkpoint. The biome sequence is four Pixel Adventure rooms, four Kenney rooms,
then four Kings and Pigs rooms.

Closed side walls prevent leaving a room horizontally. Top and bottom openings must align with the
vertical campaign path. Place checkpoints on safe platforms at least two tiles wide, and do not put
spikes inside the only required landing envelope.

## Designing legitimate jumps

The King occupies 0.8 by 0.8 world tiles. A solid row two grid rows above another leaves only one
empty row, so overlapping platforms can block the King's head. For close vertical steps, stagger
the ledges horizontally so the jump can pass around the upper edge before landing.

Use generous platform widths for mandatory jumps. Difficulty should come from charge selection,
direction changes, and safe spike placement rather than single-pixel precision.

## Solver contract

The headless solver samples launch positions every 0.25 tile, charge lengths at 60 Hz, and left,
neutral, or right releases. It then advances the same `simulate_step` function used by the game for
up to 180 airborne frames.

A route is accepted only when:

1. Every jump uses at most 85% of the production charge range.
2. The nominal jump reaches the intended landing surface.
3. At least three of five replays reach the same surface: nominal, charge -2 frames, charge +2
   frames, launch X -0.1 tile, and launch X +0.1 tile.
4. Every room and the combined twelve-room campaign have a route.

Build and inspect a single room:

```bash
./build/cmake/jumpcastle_level_solver --levels assets/levels --room 5
```

Verify the complete campaign and save the route:

```bash
./build/cmake/jumpcastle_level_solver \
  --levels assets/levels \
  --campaign \
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
./build/cmake/jumpcastle_level_solver --levels assets/levels --campaign
```

The tests also validate marker counts, biome order, spike/checkpoint behavior, and generated asset
integrity. Do not commit source ZIP archives from `assets/sources/downloads/`.
