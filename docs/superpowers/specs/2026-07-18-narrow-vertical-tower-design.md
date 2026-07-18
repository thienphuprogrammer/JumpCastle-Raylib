# Narrow Vertical Tower Campaign Design

## Status

Approved through interactive design review on 2026-07-18. This document refines the continuous
campaign direction with a narrower, taller viewport and a vertical sequence of authored challenge
chambers.

## Objective

Rebuild the campaign around a 28 by 36 tile camera band so the character and terrain occupy less
screen space while the level reads as a tall, narrow castle. Keep all 18 fixed-camera bands, the
charge-and-release movement model, continuous falling between bands, and solver verification.

Difficulty must come from readable geometry and committed jumps rather than tiny collision targets,
hidden rules, or single-frame inputs. Each band should feel like a distinct vertical challenge room
within one continuous tower.

## Selected Direction

The campaign uses a vertical spine with authored challenge chambers. Most progress moves upward
through a clear primary route. Individual chambers introduce horizontal commitment only when it
serves a mechanic such as a wall rebound, ceiling deflection, overhang reversal, or recovery path.

The following alternatives were considered and rejected:

1. A uniformly narrow zigzag shaft. It emphasizes height but becomes visually and mechanically
   repetitive.
2. Two parallel routes through most of the tower. It adds replay value but spends too much width and
   weakens the focused upward objective.
3. A wider 48 by 27 landscape viewport. It creates more visible area but does not deliver the
   requested narrow-tower composition.

## World and Viewport

- One camera band is 28 tiles wide and 36 tiles high.
- Gameplay tiles remain 16 logical pixels so existing collision units and 16-pixel source art keep
  their native grid.
- The logical render target is 448 by 576 pixels.
- Eighteen bands form a 28 by 648 tile campaign.
- The camera remains aligned to band boundaries. Player position, velocity, charge state, and
  collision continue in world space when crossing a boundary.
- The authoritative physics constants remain unchanged initially. Geometry is tuned against the
  existing movement model before any physics adjustment is considered.

The desktop window selects the largest integer scale that fits the current display work area. A
display with enough vertical space uses 896 by 1,152 pixels at 2x scale. Smaller displays use the
448 by 576 logical size or a fitted nearest-neighbor fallback. Fullscreen centers the portrait game
area and letterboxes the unused horizontal space.

Window sizing must never change the number of visible gameplay tiles or the authoritative camera
band. Rendering scale and input simulation remain independent.

## Chamber Structure

Each band contains one recognizable challenge-room idea and approximately eight to twelve committed
jumps on its intended route. The usable corridor may vary between 20 and 28 tiles wide, but collision
geometry must not create large decorative horizontal fields that distract from the climb.

Every chamber contains:

- a readable entry landing from the band below;
- one primary upward route;
- at least one meaningful launch-position decision;
- a deliberate miss path leading to a lower standing surface or the bottom reset boundary; and
- an exit trajectory that composes cleanly with the next band.

Optional short route splits are allowed inside individual chambers, but they must rejoin within the
same band and must not turn the campaign into a branching exploration game.

## Difficulty Curve

### Screens 01-06: Courtyard Foundations

Teach the new visual scale and taller camera band with broad recovery ledges. Introduce medium
charges, alternating landings, one obvious wall rebound, and a low-ceiling correction. Most misses
lose part of one band or one complete band.

### Screens 07-12: Frosted Keep Mechanisms

Combine offset platforms, split shafts, crossing trajectories, constrained arcs, and reversal
climbs. Landing widths decrease gradually, while fall routes remain visible. Serious misses may lose
two or three bands.

### Screens 13-18: Crown Spire Trials

Use narrow chimneys, overhang reversals, long committed gaps, fall funnels, and multi-step rebound
sequences. The final band ends with a readable throne leap rather than an arbitrary precision check.
Late misses may lose three to five bands, but every recovery state must retain a legal route upward.

Across all biomes, difficulty comes from combining known mechanics. No late screen introduces an
undocumented collision rule, physics modifier, invisible platform, or mandatory blind jump.

## Geometry Rules

- Mandatory landing surfaces are at least one full gameplay tile wide.
- No mandatory action requires more than 90 percent of maximum charge.
- Teaching chambers tolerate modest launch-position and charge-duration errors.
- Later chambers may narrow tolerances but never require a single simulation frame.
- Solid pockets must not trap a grounded player without a useful outgoing jump.
- Ceilings and walls used for rebounds must be visually continuous with their collision geometry.
- A fall may cross several camera bands without teleporting, pausing, or resetting intermediate
  state.
- The campaign contains no checkpoints or spike-triggered rescue teleportation.

## Rendering and Assets

The renderer continues to draw from semantic atlas regions and uses nearest-neighbor filtering.
Terrain, goal markers, and player frames remain anchored to the 16-pixel gameplay grid. The new
portrait target changes composition and output scale, not atlas coordinates.

Backgrounds should reinforce the vertical tower with masonry, windows, pillars, banners, and distant
silhouettes. Decorative elements may occupy the letterboxed presentation area only if they are
clearly non-interactive and never obscure the gameplay target.

Existing asset recovery and validation behavior remains required. Missing runtime art must produce
a clear startup error or be restored by the configured asset synchronization target; it must never
silently render an invisible gameplay object.

## Campaign Data and Solver Workflow

`assets/levels/campaign.level` remains the single runtime campaign source. Its metadata changes to:

```text
tile_size 16
size 28 648
screen_height 36
```

Spawn, goal, biome ranges, and all 648 collision rows are re-authored for the new dimensions. The
accepted solver trace is regenerated after geometry is complete.

The workflow is:

1. Author one chamber at a time and document its intended mechanic and miss path.
2. Run the production-physics solver from every stable entry landing.
3. Verify a complete route from the campaign spawn to the goal.
4. Replay the accepted route through the same fixed-step simulation used by the game.
5. Perturb launch positions and charge durations on teaching screens to measure tolerance.
6. Add visual decoration only after collision reachability passes.
7. Perform a manual play check at the smallest and largest supported presentation scales.

The solver proves feasibility, not quality. Human review remains responsible for pacing, visual
clarity, route memorability, and whether lost progress feels fair.

## Code Boundaries

- `game_config` owns logical tile and viewport dimensions.
- `WorldMap` and the campaign parser own dynamic world dimensions and band validation.
- `CameraBand` maps continuous world coordinates to one 36-row render band.
- `Renderer` owns the portrait render target, integer presentation scaling, and letterboxing.
- The solver and replay system consume `WorldMap` and production simulation APIs without renderer
  dependencies.
- Tests and level tooling derive dimensions from campaign metadata or shared configuration rather
  than duplicating 28, 36, or 648 as unrelated literals.

This work must not absorb unrelated asset-root, CMake recovery, or locally modified legacy room
changes. Overlapping files are reconciled deliberately and existing user edits are preserved.

## Validation and Acceptance Criteria

The redesign is complete only when all of the following hold:

- the game builds through CMake on the current supported desktop environment;
- the campaign parser accepts exactly 28 columns and 648 rows for the committed campaign;
- all 18 camera bands have distinct collision silhouettes and readable entries and exits;
- the production solver reaches the goal through every band;
- the accepted trace replays to completion without collision penetration or tunneling;
- mandatory charges and landing widths satisfy the geometry rules;
- camera transitions preserve continuous position and velocity in both directions;
- portrait rendering is sharp at supported integer scales and correctly letterboxed;
- player, terrain, and goal textures remain visible when facing either direction;
- asset manifest, selection, and pixel-integrity tests pass; and
- the full automated test suite passes with no unrelated local changes included in the commit.

## Out of Scope

- New player abilities or airborne steering.
- Procedural level generation.
- Online leaderboards, multiplayer, or save synchronization.
- A general-purpose visual level editor.
- Physics retuning unless solver evidence shows the existing model cannot support the approved
  chamber geometry.
