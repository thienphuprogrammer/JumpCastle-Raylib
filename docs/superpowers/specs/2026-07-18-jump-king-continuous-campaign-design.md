# Jump King-Style Continuous Campaign Design

## Status

Approved through interactive design review on 2026-07-18. This document supersedes the
twelve-room campaign, checkpoint, spike, and three-pack visual direction described in
`2026-07-18-reachable-campaign-assets-design.md` for the next implementation iteration.

## Objective

Transform JumpCastle into a focused, Jump King-style precision platformer while keeping its own
identity, code, world, characters, and art. The result is one continuous vertical castle with 18
fixed-camera screens, charge-and-release jumping, meaningful loss of height after a missed jump,
and no rescue checkpoints.

The campaign remains hand-authored. A solver using the exact production physics must prove that a
complete route exists and replay that route through the same simulation core used by the game.
The solver is a verification tool, not a procedural level generator.

The implementation also replaces the current runtime art and fixes the asset pipeline defects
that allow images to disappear or produce nearly transparent special markers.

## Product Direction

The intended experience is deliberately close to Jump King's core tension:

- hold the jump input to charge, then release to leap;
- commit to the launch without airborne steering;
- use walls, ceilings, and platform edges to alter the trajectory;
- climb a single tall world shown through fixed camera screens; and
- lose real vertical progress when a jump fails.

This is inspiration at the mechanics and pacing level only. JumpCastle must not copy Jump King
level geometry, sprites, backgrounds, audio, names, text, or other copyrighted content.

The following alternatives were considered and rejected:

1. A forgiving vertical campaign with biome checkpoints. It weakens the tension created by a
   fall and conflicts with the selected faithful direction.
2. A broad branching castle with exploration routes. It uses the larger viewport but dilutes the
   precise upward objective.
3. Procedurally generated rooms. A solver can prove reachability, but generated layouts do not
   provide the authored rhythm, fall routes, or memorable screen silhouettes required here.

## Campaign Shape

### World Dimensions

The world uses a 16-pixel gameplay tile and a 512 by 288 logical viewport. One camera screen is
therefore 32 tiles wide and 18 tiles tall. Eighteen screens form a single 32 by 324 tile world,
or 512 by 5,184 logical pixels.

The desktop window defaults to 1,536 by 864 pixels, a three-times integer scale of the logical
viewport. The window is resizable. Rendering chooses the largest integral scale that fits and
letterboxes the remaining space so pixel art stays sharp. A smaller display may use a fitted
non-integral scale only as a last resort, with nearest-neighbor texture filtering retained.

### Camera

The camera is aligned to 288-pixel vertical bands. Crossing the top or bottom boundary changes the
active band, but player position, velocity, charge state, and collision continue in world space.
The camera must never teleport the player or reload a screen.

Transitions should be immediate or use a very short visual slide that does not pause simulation.
Falling can cross several previous screens in one continuous trajectory.

### Failure and Recovery

There are no campaign checkpoints and no spike-based teleportation. A missed landing falls toward
the previously traversed geometry. The only full reset occurs after the player drops below the
bottom boundary of the world; that reset returns to the single campaign spawn.

Fall routes are first-class level geometry. They must:

- lead to a lower valid standing surface or the bottom reset boundary;
- contain no enclosed pocket from which the player cannot charge a useful jump;
- avoid invisible blockers and one-way visual tricks; and
- remain readable enough that a player can understand why progress was lost.

## Biomes and Difficulty Curve

The 18 screens are divided into three connected six-screen biomes. The biomes change palette,
background silhouettes, decoration density, and platform construction, but they share one collision
language and one player physics model.

### Screens 01-06: Courtyard

Visual direction: moss, warm stone, fading daylight, trees and low castle walls.

Screen archetypes:

1. Training Yard: short and medium charges with broad recovery ledges.
2. Long-Gap Court: the first committed horizontal jump.
3. Rebound Alley: an obvious single-wall rebound.
4. Low-Ceiling Hall: teaches head collision and reduced arcs.
5. Central Tower: route wraps around a central solid structure.
6. Gatehouse Exam: combines the biome's mechanics without a new rule.

Most misses lose one or two screens.

### Screens 07-12: Frosted Keep

Visual direction: blue stone, cold windows, restrained fog, metal and frozen-looking decoration.
Ice is visual only; it does not secretly change friction.

Screen archetypes:

7. Split Shaft: choose a launch side before entering a vertical gap.
8. Window Steps: small offset landings with open fall routes.
9. Crossing Chamber: two long trajectories cross the middle of the screen.
10. Reversal Climb: alternating wall and platform rebounds.
11. Narrow Gallery: constrained arcs under ceilings.
12. Bell-Tower Exam: a longer sequence combining the biome's patterns.

Severe misses can lose two to four screens.

### Screens 13-18: Crown Spire

Visual direction: purple-black masonry, gold metal, moonlight, banners and throne architecture.

Screen archetypes:

13. Broken Bridge: high-commitment horizontal launch.
14. Crown Chamber: platforms orbit a large central silhouette.
15. Vertical Chimney: controlled rebounds between two walls.
16. Overhang Reversal: ceiling collision sets up the landing direction.
17. Fall Funnel: difficult ascent with deliberate lower rescue ledges.
18. Throne Leap: readable final sequence ending at the crown goal.

The largest misses can lose three to six screens, but no screen may rely on a single frame or an
unreadable collision edge.

## Player Controls and Physics

### Input State Machine

1. While grounded, left and right move the player slowly to select a launch position.
2. Holding jump enters charge state. Direction can be selected or changed while charging.
3. Releasing jump converts charge duration and direction into a launch velocity.
4. The launch vector is committed until the next landing. There is no airborne steering.
5. Solid wall and ceiling collisions resolve physically and may redirect or cancel velocity.

The initial tuning range is 0.12 to 0.85 seconds of charge. Launch strength uses a smooth,
monotonic curve so low and medium charges have useful resolution. Exact constants are tuning data,
not duplicated literals.

### Deterministic Simulation

Production gameplay, solver search, replay verification, and tests use one fixed-step simulation
core at 120 Hz. Collision uses swept bounds or an equivalent continuous test so high-charge jumps
cannot tunnel through thin platforms. Rendering may run at a different frame rate and interpolate
visual position without changing authoritative state.

Raylib types and window state must not appear in the authoritative physics API. The core owns its
own small vector and bounds types or neutral value structs.

## Campaign File Format

Replace `room-01.level` through `room-12.level` as runtime inputs with one UTF-8 file:
`assets/levels/campaign.level`.

The format is line-oriented and diff-friendly:

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
....########....................
...
```

The collision grid contains exactly 324 rows of 32 cells. At minimum, `.` means empty and `#`
means solid. Spawn and goal live in metadata instead of masquerading as collision tiles.
Decorations use a separate semantic section or sidecar containing named props and tile positions.

The parser must reject:

- unsupported or duplicate version, dimension, spawn, goal, biome, or section declarations;
- unknown biome names or collision tokens;
- a row with the wrong width or an incorrect row count;
- spawn or goal outside the world, inside solid terrain, or without a usable standing surface;
- overlapping or incomplete biome screen ranges; and
- decoration identifiers absent from the asset manifest.

Every error includes the file path, line and column when applicable, offending value, and a useful
reason. The format is documented in `docs/LEVEL_DESIGN.md`.

The current locally modified `room-01.level` and any other dirty legacy room file must be preserved
under a non-runtime legacy directory before the old runtime path is removed. Git history alone is
not sufficient protection for uncommitted user work.

## Hand-Authored and Solver-Verified Workflow

Level construction follows this order:

1. Author collision geometry and intended fall routes in `campaign.level`.
2. Run the solver with production physics to find a complete route from spawn to goal.
3. Write the accepted route to a JSON trace containing launch positions, directions, charge
   durations, collision events, landings, camera bands, and final completion state.
4. Replay that trace through the production simulation without a renderer.
5. Add autotiles and decorations only after the collision route passes.
6. Run render smoke tests and a manual play check.

The search state includes player position and velocity, grounded surface, charge state, and camera
band. Search actions include walking to sampled launch positions, selecting left/neutral/right,
and sampling charge duration. Equivalent stable landings are quantized and deduplicated.

An accepted campaign must satisfy all of the following:

- the replay reaches the goal through all 18 screens;
- no mandatory jump requires more than 90 percent of maximum charge;
- mandatory landing width is at least one full gameplay tile;
- no state in the accepted route penetrates or tunnels through solid geometry;
- modest launch-position and charge perturbations preserve at least one viable route on teaching
  screens, while later screens may be less tolerant but never single-frame; and
- every stable landing reachable during the accepted route has at least one legal outgoing action
  or an intentional fall route.

The solver proves feasibility, not quality. Human review remains responsible for rhythm,
readability, visual composition, and whether failure feels fair.

## Asset Sources

The implementation uses only assets whose source pages identify them as Creative Commons Zero.
No Jump King art is included.

1. Pixel Art Castle Tileset by rubberduck
   - Source: <https://opengameart.org/content/pixel-art-castle-tileset>
   - License: CC0.
   - Role: 16-pixel castle stone, brick, pillars, metals, wood, flags, doors, windows and flames.
   - Multiple source palettes support the three biomes without mixing unrelated tile languages.
2. Gloomy Knight by loveOS
   - Source: <https://loveosstudio.itch.io/gloomy-knight-16x16>
   - License: CC0 1.0 Universal.
   - Role: replacement player sprite with idle, walk, jump and fall animation sources.
   - A charge pose and JumpCastle-specific gold/blue palette may be derived and committed under
     the same CC0 terms.
3. UI Pack - Pixel Adventure by Kenney
   - Source: <https://kenney.nl/assets/ui-pack-pixel-adventure>
   - License: CC0.
   - Role: menu panels, buttons and input prompts. The in-game HUD remains minimal.

Project and source-code authorship is presented as `Thien Phu (@thienphuprogrammer)`. Third-party
source and license facts remain in `THIRD_PARTY_ASSETS.md` and per-pack source metadata. Replacing
old project credits must never remove required license notices or misrepresent third-party art as
original work, even when attribution is not legally required by CC0.

## Visual Rules

Collision and presentation are separate layers:

- collision geometry is authoritative and never inferred from a decorative sprite;
- solid geometry is autotiled from collision neighbors and active biome;
- every standable top edge has a consistent high-contrast rim;
- decoration is always non-colliding unless explicitly represented in collision data;
- no decoration may look like a safe platform if it cannot support the player;
- textures use nearest-neighbor filtering and pixel-snapped destination coordinates; and
- background layers use biome-colored parallax silhouettes rather than stretched raster images.

Courtyard uses warm stone and green rims, Frosted Keep uses blue stone and pale rims, and Crown
Spire uses purple stone with gold or rose highlights. These are palette variations of a shared
construction language, so the player can read collision consistently throughout the campaign.

## Asset Pipeline and Missing-Image Fix

The current asset failures have two known design causes:

1. runtime assets are copied only by a post-build action attached to the executable, so deleting a
   runtime image is not repaired when the executable target is already up to date; and
2. special markers are derived by selecting arbitrary early non-empty cells from source sheets,
   which can produce sprites containing only a few rows of opaque pixels.

The replacement pipeline uses explicit, deterministic inputs:

- raw source files, source page, retrieval date, archive name and checksum are stored or described
  under `assets/sources/`;
- an asset manifest maps each semantic sprite name to an exact file and rectangle;
- the atlas builder validates source dimensions, rectangle bounds, frame counts and minimum alpha
  coverage before producing an atlas;
- generated atlases and their SHA-256 hashes are committed for reproducible normal builds; and
- no code chooses a sprite because it happens to be the first non-empty cell.

CMake defines a dedicated `jumpcastle_assets` target included in `ALL`. It copies runtime assets
independently of whether `jumpcastle` needs relinking, using `cmake -E copy_if_different` or an
equivalent explicit file list. Deleting a copied runtime PNG and rebuilding must restore it.

At runtime, `AssetCatalog` owns textures through RAII and searches only documented asset roots:

1. an explicit command-line or environment override for development;
2. `assets/` next to the executable; and
3. the installed shared-data path configured by CMake.

Every required texture is checked for successful decode and positive dimensions. A missing or
invalid optional texture renders a visible magenta checkerboard and logs the resolved path. A
missing required atlas or manifest stops startup with a clear error screen and non-zero exit
instead of silently rendering nothing.

## Target C++ and CMake Structure

The target graph becomes:

- `jumpcastle_core`: pure C++20 physics, collision, player state, world state, camera bands and
  replay types;
- `jumpcastle_content`: campaign parser, validation, biome metadata, autotile rules and semantic
  asset manifest;
- `jumpcastle_raylib`: input adapter, renderer, audio, window and RAII resource wrappers;
- `jumpcastle`: interactive executable;
- `jumpcastle_solver`: headless route search and trace writer, with a `--verify-trace` subcommand
  for production-physics replay;
- `jumpcastle_tests`: unit and integration tests; and
- `jumpcastle_assets`: deterministic build-tree asset synchronization.

The interactive game and tools depend inward on core/content. Core must not depend on Raylib,
filesystem layout, atlas coordinates, or UI state.

## Runtime Data Flow

1. Startup resolves the asset root, validates the manifest, and loads `campaign.level`.
2. `WorldMap` stores collision, spawn, goal, biome ranges and semantic decoration records.
3. Input is translated into walk, charge, direction and release commands.
4. The fixed-step core advances player and collision state in world coordinates.
5. `CameraController` selects the current 288-pixel band from player position.
6. Renderer autotiles visible collision geometry, draws decorations and player frames, then scales
   the 512 by 288 render target into the desktop window.
7. Crossing the goal marks completion and shows elapsed time and fall statistics.
8. Falling below the world resets to the initial spawn and increments the fall counter.

The solver performs steps 1-4 and goal evaluation without constructing a Raylib window.

## Error Handling

- Campaign parse errors name the file, line, column, screen and invalid value.
- Solver failure names the highest reached screen, nearest stable surface and action envelope that
  failed.
- Asset validation names the semantic asset, source rectangle and failed constraint.
- Runtime load failures include every searched asset root.
- Unexpected exceptions are caught at the executable boundary, logged once and presented as a
  concise fatal error rather than an empty window.

## Test Strategy

### Unit Tests

- charge state transitions and launch-curve boundaries;
- fixed-step determinism across different render-frame chunking;
- swept collision against floors, walls, ceilings and corners;
- camera-band selection at exact boundaries;
- campaign v2 parsing, dimensions, biome ranges, metadata and diagnostics;
- autotile neighbor selection; and
- asset manifest bounds, required animation frames and alpha thresholds.

### Integration Tests

- load the real 32 by 324 campaign;
- solve and replay a complete spawn-to-goal route;
- reject a deliberately unreachable platform mutation;
- verify every accepted landing has an outgoing action or fall route;
- delete a copied runtime atlas, rebuild the asset target, and verify restoration;
- resolve assets beside the executable and from an install tree; and
- load/unload all textures repeatedly without leaks or double-unload.

### Visual and Runtime Tests

- render at least one deterministic snapshot per biome and assert non-empty pixel/alpha coverage;
- render every required player state: idle, walk, charge, rise and fall;
- launch the interactive game and verify default size, resize, letterboxing and nearest filtering;
- replay the solver route visibly through all camera transitions; and
- manually test a multi-screen fall, bottom reset, pause/restart and final completion.

### Platform Verification

A clean CMake configure, build and CTest run must pass locally. Existing Windows, macOS and Linux
CI remains supported. Asset tooling must be reproducible from documented Python dependencies but
normal compilation uses committed generated atlases and does not download from the network.

After implementation, refresh the repository's CodeGraph index and confirm the graph exposes the
new campaign parser, shared physics path, solver/replay path, asset catalog and renderer flow.

## Migration Sequence

1. Preserve dirty legacy room edits outside the new runtime path.
2. Add characterization tests around current movement and collision behavior.
3. Separate pure simulation types from Raylib and introduce fixed-step world physics.
4. Add campaign v2 parser and the continuous camera model.
5. Hand-author the 18-screen collision skeleton.
6. extend the solver to full-world search and production replay until the skeleton passes.
7. Import the approved CC0 sources, replace the atlas manifest and fix CMake asset synchronization.
8. Add biome autotiling, parallax backgrounds, decorations, player animation and UI.
9. Remove obsolete room/checkpoint/spike runtime paths after new coverage is green.
10. Run clean build, all tests, asset-deletion recovery, visible replay and manual runtime checks.
11. Update README, level-design documentation, asset sources, screenshots and CodeGraph.

Each step should leave the branch buildable. Old code is removed only after its replacement has
passing tests.

## Definition of Done

The redesign is complete only when:

1. The game runs as one continuous 18-screen, three-biome vertical world.
2. The 1,536 by 864 default window renders a sharp 512 by 288 logical viewport and resizes safely.
3. Charge-and-release movement, committed aerial control and wall/ceiling rebounds feel consistent.
4. Missing a jump can fall through previous camera screens without teleportation or checkpoints.
5. The solver finds a complete route and the production core replays it to the goal.
6. All 18 screens have distinct silhouettes and intentional fall routes rather than repeated
   zigzags.
7. The approved CC0 castle, knight and UI assets replace the prior runtime art.
8. Deleting a runtime image followed by a normal build restores it even when the executable was
   already up to date.
9. No required sprite is empty, nearly transparent, out of bounds or silently missing at runtime.
10. Clean build, CTest, asset validation, three-biome snapshots and manual runtime checks pass.
11. Project authorship is consistently `Thien Phu (@thienphuprogrammer)`, with accurate third-party
    CC0 source metadata retained.
12. Documentation and CodeGraph match the implemented architecture.
