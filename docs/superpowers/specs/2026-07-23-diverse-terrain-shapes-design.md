# Diverse Terrain Shapes, Tiles, and Campaign Redesign

- **Date:** 2026-07-23
- **Status:** Approved design
- **Scope:** Collision, player movement, solver/replay, rendering, Tiled authoring,
  asset pipeline, and all 18 campaign screens
- **Supersedes:** `2026-07-23-sloped-terrain-design.md`
- **Extends:** `2026-07-20-tiled-wysiwyg-tile-painting-design.md`

## 1. Vision

JumpCastle's terrain should look like a sculpted fantasy castle rather than a
collection of axis-aligned rectangles. Every screen will use a richer mixture
of slopes, triangles, circles, capsules, arches, half-blocks, pillars, hazards,
and biome-specific decoration.

The visual direction is **Fantasy Maximalist**: dense architecture and props,
strong silhouettes, and visibly different challenge rooms. Visual density must
not hide landing zones, hazards, the player, or the relationship between art and
collision.

The redesign is not cosmetic. All 18 screens will use the new shapes on the
required route, and the launch direction will rotate with the surface normal.
The full campaign must remain solver-certified and replayable through the same
production physics used by the game.

## 2. Approved Decisions

| Area | Decision |
|---|---|
| Visual direction | Fantasy Maximalist |
| Campaign scope | Redesign all 18 screens |
| Difficulty structure | Challenge-room rhythm; difficulty may rise and fall by room |
| Shape strategy | Exact polygon, circle, and capsule geometry in the existing engine |
| Physics library strategy | Extend the deterministic in-house physics; do not replace it with Box2D |
| Surface behavior | Ground movement and launch velocity use the current contact normal |
| Tiled role | Source-of-truth authoring environment for tiles, shapes, entities, and layers |
| Solver requirement | Every mandatory shape and all 18 screens are production-physics certified |
| Asset usage | Use verified-license source packs only; unverified downloads are excluded |

## 3. Current State

The existing runtime already provides:

- Convex-polygon collision using SAT.
- Concave collider decomposition into convex pieces.
- Fixed-step simulation at 120 Hz.
- Shared production physics for game, replay, and solver.
- Tiled round-tripping for a terrain tile layer, polygon collision objects, and
  entities.
- One demonstrative triangular slope on screens 00–16.
- Painter recognition for colliders tagged `shape: "slope"`.

The missing pieces are:

- True diagonal and round art. Existing slopes currently fall back to a flat,
  stair-stepped top tile.
- Exact circle and capsule collision/contact data.
- Persistent ground contact normals in player state.
- Solver surface sampling for line segments, arcs, and capsules.
- Multiple visual tile layers and runtime animated tiles.
- Tiled templates and terrain brushes for the expanded vocabulary.
- A complete 18-screen route built around the new shape mechanics.

## 4. Goals and Guardrails

### 4.1 Goals

- Author polygon, circle, and capsule collision directly in Tiled. A Tiled
  ellipse is accepted as a circle only when its width equals its height;
  elongated round geometry uses a capsule template.
- Preserve semantic shapes through `.tmj ↔ .map.json` round trips.
- Produce exact contact point, normal, and penetration depth for every collision.
- Rotate grounded movement and charged launch velocity into the support
  surface's local frame.
- Render background, terrain, decor, foreground, and animated tiles from the
  shared generated atlas.
- Supply biome-native tile variants for every structural shape family.
- Redesign all 18 screens as distinct challenge rooms.
- Keep map generation and asset generation deterministic and byte-stable.
- Detect tile/collision mismatch before a map reaches runtime.

### 4.2 Guardrails

- Fixed-step simulation remains `1 / 120` seconds.
- Runtime terrain and collision remain static; moving platforms are outside this
  design.
- No runtime randomness is added to map painting, physics, or animation timing.
- The player remains an AABB. This project adds new static world shapes, not a
  new player body type.
- Walkable surfaces remain limited to normals with `normal.y <= -0.5`, equivalent
  to the current approximately 60-degree grounding limit.
- Foreground art may not obscure required landing zones or the player.
- No downloaded asset enters the generated atlas until its license and source
  record are verified.

## 5. System Architecture

```text
Tiled
  ├── background tile layer
  ├── terrain tile layer
  ├── decor tile layer
  ├── foreground tile layer
  ├── collision object layer
  │     ├── polygon
  │     ├── circle / ellipse
  │     └── capsule
  └── entities object layer
          │
          ▼
tools/tiled_convert.py
          │
          ▼
screen-NN.map.json (schema v3)
          │
          ├── Map loader and validator
          ├── ShapeCollider world
          ├── Multi-layer renderer
          └── Solver/replay surface data
```

Runtime collision uses an explicit shape variant:

```cpp
using ColliderGeometry = std::variant<ConvexPolygon, Circle, Capsule>;

struct ShapeCollider {
    int id;
    ColliderType type;
    ColliderGeometry geometry;
    Aabb aabb;
    std::string tag;
};
```

`ColliderType` continues to describe behavior (`solid`, `oneway`, or `hazard`).
Geometry describes the mathematical shape. `tag` records an authoring/gameplay
role such as `slope`, `arch`, or `round_platform`.

This separation prevents the current `shape: "slope"` authoring tag from
conflicting with the new geometry discriminator.

## 6. Map Schema v3

### 6.1 Shape representation

Polygon:

```json
{
  "id": 12,
  "type": "solid",
  "geometry": "polygon",
  "tag": "slope",
  "points": [[6, 20], [12, 17], [12, 20]]
}
```

Circle:

```json
{
  "id": 13,
  "type": "solid",
  "geometry": "circle",
  "tag": "round_platform",
  "center": [14, 18],
  "radius": 3
}
```

Capsule:

```json
{
  "id": 14,
  "type": "solid",
  "geometry": "capsule",
  "tag": "capsule_platform",
  "a": [7, 12],
  "b": [18, 12],
  "radius": 1.5
}
```

Capsules use a center segment from `a` to `b` plus `radius`. Horizontal,
vertical, and diagonal capsules use the same representation.

### 6.2 Visual layers

Schema v3 stores named tile layers:

```json
{
  "tiles": {
    "background": [[0]],
    "terrain": [[0]],
    "decor": [[0]],
    "foreground": [[0]]
  }
}
```

Every present layer must contain exactly `screen.height` rows and
`screen.width` GIDs per row. Missing optional layers behave as empty layers.
`terrain` remains the structural WYSIWYG layer.

### 6.3 Backward compatibility

- Schema v1 and v2 remain loadable.
- A v1/v2 collider without `geometry` is interpreted as a polygon.
- Existing `shape: "slope"` is migrated to
  `geometry: "polygon", tag: "slope"` by the converter.
- A v1 map without painted terrain keeps the procedural polygon-render fallback.
- A v2 map keeps its single `terrain` layer and gains empty optional layers in
  memory.
- Serializing a schema v3 map never silently downgrades circle or capsule
  geometry.

## 7. Collision and Contact Manifold

Every narrow-phase collision returns:

```cpp
struct Contact {
    int collider_id;
    ColliderType type;
    Vec2 point;
    Vec2 normal;
    float depth;
};
```

`normal` points outward from the world shape toward the player. All returned
normals must be finite and normalized.

### 7.1 Dispatch

The existing screen-neighbor lookup and AABB broad phase remain. Narrow phase
dispatches based on `ColliderGeometry`:

- Player AABB versus convex polygon: existing SAT path.
- Player AABB versus circle: closest-point test with a deterministic inside-box
  fallback normal.
- Player AABB versus capsule: exact closest pair between the capsule center
  segment and the AABB, followed by the capsule-radius overlap test.

The current maximum movement sub-step of `0.2` tile remains. Map validation
requires circle/capsule radius to be at least `0.5` tile so the existing
sub-stepping cannot tunnel across a complete curved collider in one tick.

### 7.2 Deterministic contact selection

When multiple walkable contacts exist:

1. Prefer the contact with the most upward normal, meaning the lowest
   `normal.y`.
2. If normals are equal within the comparison epsilon, prefer greater
   penetration depth.
3. If still tied, prefer the lowest stable collider ID.

This selected contact supplies the player's grounded normal and point.

### 7.3 Surface classification

- `normal.y <= -0.5`: walkable ground.
- Other contacts: wall or underside.
- `hazard`: records a lethal contact but does not resolve the player's position.
- `oneway`: resolves only when approached from the permitted side and the
  previous support point was outside the blocking half-space.

Circle and capsule geometry may be `solid` or `hazard`. One-way curved geometry
is not supported in schema v3; one-way colliders remain polygons so their
blocking side is unambiguous.

### 7.4 Wall response

Airborne wall response generalizes the existing wall bounce to any
non-walkable normal:

```text
v_out = v_in - normal × dot(v_in, normal) × (1 + restitution)
```

Restitution continues to use the current impact-speed curve and remains capped
at `1.0`.

## 8. Player Movement on Surface Normals

`PlayerState` gains persistent support data:

```cpp
Vec2 ground_normal{0.0F, -1.0F};
Vec2 ground_point{};
int ground_collider_id{-1};
```

Ground support is refreshed every fixed tick by a contact query. A boolean foot
overlap probe is insufficient because slope and round movement require the
current contact normal.

Define the right-facing surface tangent as:

```text
tangent_right = {-ground_normal.y, ground_normal.x}
```

### 8.1 Ground movement

While grounded and not charging, left/right movement follows
`±tangent_right`. This lets the player walk naturally along slopes and around
the walkable upper arc of circles/capsules.

### 8.2 Charged launch

The existing charge curves and speed ranges remain. Only the coordinate frame
changes:

```text
velocity =
    ground_normal × vertical_speed(charge)
  + tangent_right × horizontal_input × horizontal_speed(charge)
```

On flat ground, `ground_normal = {0, -1}` and
`tangent_right = {1, 0}`, producing the current launch behavior. On slopes and
round surfaces, the complete launch vector rotates with the surface.

Neutral release launches along the surface normal. Left/right release adds the
corresponding tangent component.

### 8.3 Losing support

If no walkable contact remains after ground movement, the player becomes
airborne immediately. Charging may continue only while a walkable support
contact exists.

## 9. Solver and Replay

### 9.1 Surface model

The solver replaces horizontal-only `Surface` records with:

```cpp
struct SurfaceSample {
    Vec2 position;
    Vec2 normal;
    int collider_id;
};
```

Samples are generated deterministically:

- Polygon: walkable edge endpoints plus regularly spaced interior samples.
- Circle: samples across the walkable upper arc.
- Capsule: samples across its walkable segment and end arcs.

Sampling density derives from the existing launch sample spacing. Stable
collider ID and sample order make solver output deterministic.

### 9.2 Search

Each launch candidate contains position, surface normal, charge ticks, and
direction. Candidate flight is simulated with `step_world`; the solver does not
use a separate approximate trajectory integrator.

### 9.3 Replay schema v2

Each trace jump records:

- Launch position.
- Expected launch normal.
- Expected launch collider ID.
- Direction and charge ticks.
- Expected landing position, normal, collider ID, and screen.

Replay verifies both position and normal tolerance before launching the next
jump. The fixed-step value remains embedded and validated.

## 10. Tile and Asset Vocabulary

Every biome provides the same structural roles with biome-native material:

### 10.1 Slopes

- 45-degree left/right slopes.
- Gentle 1:2 left/right slopes.
- Steep 2:1 left/right slopes.
- Inverted ceiling variants for visual completeness.
- Matching fill, edge, and inner-corner transitions.

### 10.2 Round geometry

- Convex quarter-circle corners.
- Concave quarter-circle corners.
- Circle quadrants.
- Horizontal and vertical capsule ends.
- Pillar caps and round platform caps.

Large circles and capsules are composed from edge/corner tiles plus biome fill,
so they are not restricted to one tile size.

### 10.3 Architecture

- Arch bases, sides, curved segments, and keystones.
- Half-blocks in four orientations.
- Bridges, ledges, pillars, isolated blocks, and inner corners.

### 10.4 Hazards and decoration

- Spikes and thorns.
- Cracks, moss, snow, and surface wear.
- Vines, chains, banners, windows, torches, foliage, and biome props.

The atlas builder derives structural mask/edge variants deterministically from
approved biome textures. Distinct props are cropped from verified source packs.
Generated PNGs are never edited by hand.

## 11. Animated Tiles

Tiled animation metadata is converted into the runtime manifest. The renderer
supports deterministic looping animation for decorative tiles such as torches,
fire, falling snow, and ambient particles.

Animation time derives from the game's accumulated fixed-step time. Animated
tile frames do not affect collision or solver state.

## 12. Render Order

The Raylib renderer draws:

1. Existing multi-layer parallax background.
2. `background` tile layer.
3. `terrain` tile layer.
4. `decor` tile layer.
5. Entities and player.
6. `foreground` tile layer.
7. Completion UI and debug overlay.

All tile layers use the same generated atlas and GID mapping as Tiled.
Structural tiles draw without runtime tint to preserve WYSIWYG output.

Foreground validation rejects opaque coverage over required landing zones.
Runtime does not dynamically fade foreground art; readability is guaranteed by
the authored layer and validator so runtime captures remain deterministic.

## 13. Tiled Authoring Workflow

Each `.tmj` contains:

- Four named tile layers: `background`, `terrain`, `decor`, `foreground`.
- One `collision` object layer.
- One `entities` object layer.

The shared external tileset provides:

- Wang/Terrain Sets for biome nine-slice terrain.
- Stamp brushes for slopes, arches, circle quadrants, capsules, and hazards.
- Object templates for polygon slopes, circles, capsules, spawn, and goal.
- Tiled custom classes for collider behavior and gameplay tags.

The converter preserves stable object IDs and all recognized custom properties.
A Tiled ellipse whose width and height differ fails conversion with guidance to
use a capsule template. Unknown collision geometry or malformed templates fail
conversion with the map path and object ID.

## 14. Debugging and Validation

### 14.1 Runtime overlay

Debug mode can display:

- Collider outlines by behavior type.
- Shape AABBs.
- Current contact point and normal.
- Grounded collider ID.
- Tile-layer names and visibility.

### 14.2 Authoring validator

The validator checks:

- Screen index, dimensions, and biome.
- Stable, unique collider IDs.
- Finite coordinates.
- Non-degenerate polygon, positive circle radius, and non-zero capsule segment.
- Collider and entity bounds.
- Tile-layer dimensions.
- GID range and atlas column/tile-size agreement.
- Structural tile coverage versus rasterized collision coverage.
- Foreground overlap over solver-certified landing zones.
- Entity support and goal readability.
- License/source record for every atlas input.

Errors include the file, screen, object/layer, ID or coordinate, and violated
rule. Validation never silently drops malformed geometry.

## 15. Campaign Redesign

The player climbs from screen 17 to screen 00.

### 15.1 Castle Courtyard — screens 17 to 12

| Screen | Room | Core geometry | Intensity |
|---:|---|---|:---:|
| 17 | Bastion Threshold | Gentle slope plus flat support | 1/5 |
| 16 | Broken Ramparts | Paired left/right slopes | 2/5 |
| 15 | Moonwell | First large circle and pillar caps | 3/5 |
| 14 | Old Aqueduct | Arches and half-blocks | 2/5 |
| 13 | Banner Yard | Capsule-to-slope normal change | 4/5 |
| 12 | Courtyard Trial | Mixed-shape biome exam | 3/5 |

### 15.2 Frosted Keep — screens 11 to 06

| Screen | Room | Core geometry | Intensity |
|---:|---|---|:---:|
| 11 | Frozen Cistern | Wide circular bowl | 2/5 |
| 10 | Ice Teeth | Zigzag triangular slopes | 4/5 |
| 09 | Split Glacier | Narrow horizontal capsules | 3/5 |
| 08 | Snow Arcade | Arch recovery room | 2/5 |
| 07 | Echo Shaft | Alternating circle launch chain | 5/5 |
| 06 | Cold Labyrinth | Mixed shapes with recovery shafts | 3/5 |

### 15.3 Crown Spire — screens 05 to 00

| Screen | Room | Core geometry | Intensity |
|---:|---|---|:---:|
| 05 | Gothic Gate | Convex/concave arches and capsules | 3/5 |
| 04 | Clockwork Ring | Offset circular platforms | 4/5 |
| 03 | Royal Chimney | Vertical capsules and half-blocks | 2/5 |
| 02 | Thorn Gallery | Slope launches through hazards | 4/5 |
| 01 | Shattered Crown | Circle-slope-capsule precision chain | 5/5 |
| 00 | Crown Ascent | Spiral circle finale and goal | 3/5 |

Each room uses at most two primary structural shape families. Decoration may be
dense, but structural silhouettes and failure paths remain readable. Recovery
rooms deliberately alternate with peak challenge rooms.

## 16. Testing and Quality Gates

### 16.1 Unit tests

- AABB versus polygon, circle, and capsule.
- Circle/capsule inside and zero-distance fallback normals.
- Contact selection and collider-ID tie breaking.
- Grounded, walking, charging, and launching on flat, slope, circle, and capsule.
- Wall bounce on curved non-walkable contacts.
- Map schema v1/v2 compatibility and v3 validation.
- Tiled round-trip for every visual layer and collider geometry.
- GID and animated tile resolution.

### 16.2 Integration tests

- Runtime renderer captures every committed screen through the real Raylib
  renderer.
- Debug-overlay captures confirm visual/collision alignment.
- The complete solver route replays through production physics.
- Landing normal and collider ID match replay expectations.
- Asset and map generation remain byte-stable.

### 16.3 Release gates

- All existing tests remain green.
- Every new test is green.
- The complete 18-screen campaign solves and replays.
- Runtime maintains 60 FPS while simulation remains at 120 Hz.
- No map validator errors remain.
- No unverified asset appears in the manifest.

## 17. Implementation Phases

Each phase is independently buildable and testable.

### Phase 1 — Data contract and validation

- Add schema v3 types and backward-compatible parsing.
- Add stable collider IDs, geometry variants, tags, and named visual layers.
- Add malformed geometry, bounds, GID, and atlas validation.

### Phase 2 — Shape collision

- Add circle/capsule math and contact manifold.
- Generalize collision dispatch, grounding, one-way checks, and curved wall
  response.
- Add collision unit tests and debug shape outlines.

### Phase 3 — Normal-driven movement, solver, and replay

- Persist ground contact data in `PlayerState`.
- Rotate movement and launch velocity into the surface frame.
- Add polygon/arc/capsule solver samples.
- Upgrade traces to replay schema v2.

### Phase 4 — Atlas and renderer

- Add slope, round, capsule, arch, half-block, hazard, and decor vocabulary.
- Add deterministic structural tile derivation and license validation.
- Add named tile layers and animated tile rendering.
- Add real runtime screenshot tests.

### Phase 5 — Tiled tooling

- Round-trip schema v3 shapes and layers.
- Add Terrain Sets, stamp brushes, object templates, and custom classes.
- Add collision/visual alignment and foreground-occlusion validators.

### Phase 6 — Campaign redesign

- Redesign Castle Courtyard and pass render/solver/replay gates.
- Redesign Frosted Keep and pass the same gates.
- Redesign Crown Spire and pass the same gates.
- Regenerate all Tiled maps, screen maps, previews, collision guides, and the
  committed campaign route.

### Phase 7 — Final polish

- Tune challenge-room difficulty and recovery paths.
- Add verified animated decor and maximalist props.
- Run the complete regression, performance, license, and visual-quality gates.

## 18. Acceptance Criteria

- All 18 screens visibly use non-rectangular geometry on the mandatory route.
- Triangle, circle, and capsule contacts are mathematically represented rather
  than faked with rectangular collision.
- Ground movement and launch direction respond to the current surface normal.
- Tiled round-trips every supported shape and visual layer without data loss.
- Each biome has a distinct but complete structural tile vocabulary.
- The real runtime renderer, not a mock renderer, is used for campaign captures.
- Visual tiles and collision pass automated alignment checks.
- Solver and replay certify the full redesigned campaign.
- Existing fixed-step determinism and wall-bounce behavior remain intact.
- All runtime assets have verified provenance and license records.
