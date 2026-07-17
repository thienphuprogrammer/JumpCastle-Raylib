# Reachable Campaign and Multi-Pack Asset Design

## Objective

Replace JumpCastle's five prototype rooms and temporary art with a twelve-room, hand-authored
vertical campaign. Use three freely redistributable asset packs as distinct visual biomes, add
spikes and checkpoints, and prove that every required jump is achievable through a solver that
runs the production player physics and collision code.

The campaign must feel deliberately designed rather than procedurally assembled. Solver success
is necessary but not sufficient: each accepted route must also include a human-tolerance margin
so a jump is not valid only at one exact frame or sub-pixel launch coordinate.

## Asset Sources and License Policy

Use the following official sources:

1. Pixel Adventure by Pixel Frog
   - Source: <https://pixelfrog-assets.itch.io/pixel-adventure-1>
   - Role: rooms 01-04, outdoor approach and beginner movement language.
   - License: Creative Commons Zero 1.0 Universal (CC0).
2. Pixel Platformer by Kenney
   - Source: <https://kenney.nl/assets/pixel-platformer>
   - Role: rooms 05-08, bright mechanical tower and intermediate challenges.
   - Native tile size: 18 by 18 pixels.
   - License: Creative Commons Zero 1.0 Universal (CC0).
3. Kings and Pigs by Pixel Frog
   - Source: <https://pixelfrog-assets.itch.io/kings-and-pigs>
   - Role: rooms 09-12, castle interior, final gauntlet, and the player character.
   - Contents used: King Human animation frames, castle terrain, spikes or hazards, checkpoint
     props, background elements, and selected decorations.
   - License: Creative Commons Zero 1.0 Universal (CC0).

All three official pages explicitly permit redistribution, remixing, adaptation, and commercial
use without required attribution. The repository will still credit Pixel Frog and Kenney as a
professional courtesy.

Only files used by the game will be committed. Each source pack receives an
`assets/sources/<pack>/SOURCE.md` containing the official URL, creator, license, retrieval date,
original archive name, and a list of derived runtime files. A copy of the CC0 legal text will be
stored at `assets/sources/CC0-1.0.txt`.

## Visual Direction

The packs will not be mixed randomly inside one room. Each group of four rooms has a coherent
palette and environment language:

| Rooms | Biome | Pack | Difficulty |
| --- | --- | --- | --- |
| 01-04 | Outer Grounds | Pixel Adventure | Tutorial to easy |
| 05-08 | Clockwork Tower | Kenney Pixel Platformer | Medium |
| 09-12 | Crown Keep | Kings and Pigs | Medium-hard to finale |

The King Human character from Kings and Pigs remains the player in every biome. This single
silhouette and animation set connects the three environments and prevents the campaign from
feeling like three unrelated demos.

Every biome provides:

- a terrain atlas with faces, edges, corners, and isolated platforms;
- a background or background color treatment;
- a spike sprite;
- one checkpoint visual;
- a small set of non-colliding decorations; and
- palette-appropriate entrance and exit markers.

Pixel art uses nearest-neighbor filtering. Kenney's 18-pixel tiles and larger Pixel Frog sprites
will be normalized into a committed runtime atlas using a reproducible asset preparation script.
The gameplay grid remains 16 by 16 pixels per tile, while the King may draw larger than one tile
without changing the existing collider dimensions.

## Asset Preparation Pipeline

Add `tools/build_assets.py` and `tools/requirements-assets.txt`. The script consumes locally
downloaded source archives from `assets/sources/downloads/`, extracts only an allowlisted set of
files, and emits deterministic PNG atlases under `assets/generated/` using nearest-neighbor
resampling. Downloaded archives remain ignored and are never committed.

Generated atlases are committed so normal CMake builds do not need Python or Pillow. The script
also emits `assets/generated/manifest.json`, which records each sprite region, animation frame,
biome mapping, original source file, source dimensions, runtime dimensions, and SHA-256 digest.

`tools/verify_assets.py` validates that:

- every manifest source points to one of the three approved packs;
- every runtime PNG exists and matches its recorded digest;
- every sprite region lies within its atlas;
- player animations have at least idle, run, charge, rise, fall, and death/respawn frames; and
- no unapproved source or downloaded archive is tracked by Git.

The runtime parses only the committed manifest and generated atlases. If the manifest or a
required atlas is missing or invalid, startup reports the exact asset path and exits safely.

## Level File Format

Store one UTF-8 file per room at `assets/levels/room-01.level` through
`assets/levels/room-12.level`. Each file contains metadata followed by exactly twelve rows of
sixteen gameplay cells:

```text
name=First Steps
biome=pixel_adventure
difficulty=1
---
################
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#......S.......#
#.....###......#
################
```

The gameplay alphabet is:

| Token | Meaning |
| --- | --- |
| `#` | Solid terrain |
| `^` | Spike hazard; non-solid but lethal on overlap |
| `S` | Campaign spawn; exactly one across all rooms |
| `C` | Checkpoint; updates the respawn location |
| `E` | Campaign exit; exactly one across all rooms |
| `.` | Empty space |

Room 01 contains the only `S`; room 12 contains the only `E`. Checkpoints appear at the campaign
start and biome boundaries, specifically rooms 01, 05, and 09. The upper edge of each room must
connect spatially to the lower edge of the next room so the twelve files form one continuous
world. The sentinel invalid room remains an internal fallback and is not a level file.

`LevelRepository` loads all twelve rooms during startup. It rejects unknown metadata, unknown
tokens, incorrect dimensions, duplicate or missing campaign markers, invalid biome names, and
room-boundary mismatches with errors containing the filename and line number.

## Gameplay Additions

### Spikes and Respawn

The player's existing box collider is tested against spike cells after solid collision
resolution. On overlap, the player returns to the most recently activated checkpoint. Position,
velocity, jump charge, animation time, and grounded state reset deterministically. The same
checkpoint may immediately be used again without repeated side effects.

Spikes cannot occupy a spawn, checkpoint, exit, or mandatory landing surface. Every checkpoint
has a safe standing area at least two tiles wide and one tile of overhead clearance.

### Checkpoints

Touching a checkpoint activates it and changes its rendered state. Activation is immediate and
does not pause the game. The active checkpoint belongs to campaign state rather than player
physics so respawn behavior can be tested independently.

### Campaign Exit

Touching `E` after room 12 marks the campaign complete and displays a simple completion overlay
with elapsed time and death count. Restart returns to room 01 and clears campaign progress.

Moving platforms, enemies, combat, collectibles, and procedural room variants are explicitly
outside this iteration.

## Hand-Authored Difficulty Progression

Rooms are designed manually around one or more intended routes:

- Rooms 01-02 teach short and medium charge while providing wide landing areas.
- Rooms 03-04 introduce direction changes and alternating walls without lethal precision.
- Rooms 05-06 introduce narrower platforms and controlled gaps over spikes.
- Rooms 07-08 combine charge selection with one mid-air wall bounce per route.
- Rooms 09-10 require planned left-right alternation and checkpoint recovery.
- Room 11 is the hardest traversal room but retains the solver's tolerance margin.
- Room 12 is a readable final climb and exit sequence rather than the narrowest room.

Difficulty comes from route reading, charge selection, and direction changes. It must not depend
on invisible collision edges, one-frame input windows, or landing on less than half a tile.

## Reachability Solver

Add a development-only `jumpcastle_level_solver` library and CLI. It links
`jumpcastle_core` and calls the production `update_player`, collision resolution, hazard query,
checkpoint handling, and room-selection logic. The solver must not contain a second simplified
physics model.

The solver explores landing states. A state records room, position, velocity, current checkpoint,
grounded surface, and completion status. From each stable surface it samples launch positions,
three directions (left, neutral, right), and charge durations. Each candidate action runs the
production simulation at a fixed 60 Hz until it lands, dies, completes, or reaches a three-second
timeout. Equivalent landing states are quantized and deduplicated before breadth-first search.

The search performs both:

1. per-room validation from the documented lower entry band to the upper exit band; and
2. full-campaign validation from `S`, through all required biome checkpoints, to `E`.

A nominal route is accepted only when:

- no jump uses more than 85 percent of maximum charge;
- no mandatory landing surface is narrower than one full tile;
- collision clearance remains at least 0.1 world tile from unintended solid corners; and
- replaying each jump with charge varied by plus or minus two frames and launch position varied
  by plus or minus 0.1 tile succeeds for at least three of the five perturbation cases.

On failure, the CLI prints the room, starting landing state, attempted direction and charge,
failure reason, and the nearest reached surface. On success, it can write a JSON solution trace
for debugging; solution traces are generated artifacts and remain ignored.

## Tests and Continuous Integration

Extend Catch2 and CTest with:

- level parser dimension, metadata, token, marker, and boundary tests;
- all twelve real room files loading successfully;
- spike overlap and deterministic respawn tests;
- checkpoint activation and campaign reset tests;
- exit/completion tests;
- asset manifest schema, bounds, and digest tests;
- one reachability test per room;
- one full-campaign reachability test;
- tolerance replay tests for every jump in the accepted campaign route; and
- a guard that rejects untracked source archives or non-CC0 pack metadata.

The reachability suite runs in the existing Windows, macOS, and Linux CI matrix without opening a
window. The solver uses only deterministic core logic and committed level/manifest files.

## Runtime Data Flow

1. `Game` loads the generated asset manifest and twelve room files.
2. `CampaignState` records the spawn checkpoint, active checkpoint, deaths, and completion state.
3. Each frame selects the active room from player height.
4. Player simulation and solid collision run unchanged through `jumpcastle_core`.
5. Hazard and checkpoint queries update `CampaignState` or reset `PlayerState`.
6. `Renderer` selects the biome atlas and background from the active room metadata.
7. Exit overlap displays completion UI and allows a clean campaign restart.

The CLI solver loads the same manifest-independent level repository and runs steps 2-5 without
constructing a raylib window or renderer.

## Documentation

Update the README with campaign description, three biome screenshots, controls, checkpoint/spike
rules, asset credits, and solver commands. Add `docs/LEVEL_DESIGN.md` describing the level format,
human-tolerance rules, how to run a focused room validation, and how to interpret a failed route.

Although attribution is not required by CC0, the README and source metadata credit:

- Pixel Frog for Pixel Adventure and Kings and Pigs; and
- Kenney for Pixel Platformer.

## Success Criteria

The iteration is complete when:

1. The game loads exactly twelve hand-authored rooms across the three approved biomes.
2. The King Human character and all three environment packs are visible in the running game.
3. Spikes respawn the player at the latest activated checkpoint.
4. Room 12 exit produces campaign completion state.
5. The solver proves every room and the full campaign reachable with the defined tolerance margin.
6. Tests fail when a required platform is moved beyond the verified jump envelope.
7. Runtime asset atlases are reproducible and traceable to CC0 source metadata.
8. Clean CMake build and CTest pass on the local platform.
9. Existing CI continues to cover Windows, macOS, and Linux.
10. CodeGraph indexes the new level, campaign, asset, and solver call paths.
