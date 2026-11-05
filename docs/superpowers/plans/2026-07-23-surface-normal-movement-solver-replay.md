# Surface-Normal Movement, Solver, and Replay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make grounded movement and charged jumps follow polygon/circle/capsule support normals, then certify the behavior through surface-aware solver search and replay schema v2.

**Architecture:** `CollisionWorld` supplies a deterministic `Contact`; `PlayerState` persists its support frame and expresses ground movement/launch velocity in normal/tangent coordinates. A separate surface-sampling module converts world colliders into stable solver anchors, while solver transitions and replay traces carry collider IDs and normals and continue to simulate every jump through `step_world`.

**Tech Stack:** C++20, existing fixed-step physics, Catch2, nlohmann/json, existing reachability solver/replay.

## Global Constraints

- Depends on completed Plan 1 interfaces: `Contact`, `WorldCollider`,
  `CollisionWorld::colliders_for_screen`, and contact-aware `ResolveResult`.
- Keep fixed simulation at 120 Hz and keep current charge duration/speed curves.
- On flat ground, launch velocity must match the pre-change values within float tolerance.
- Walkable contact remains `normal.y <= -0.5F`.
- No separate approximate flight integrator; solver and replay call `step_world`.
- Stable ordering is `(screen, collider_id, piece_index, sample_index)`.
- Preserve dirty-worktree changes; commit only with explicit user authorization.

---

## File Structure

- Modify `include/jumpcastle/player.hpp` and `src/player.cpp`: persistent support frame and normal-relative movement/launch.
- Modify `include/jumpcastle/collision_world.hpp` and `src/collision_world.cpp`: support-probe API returning `Contact`.
- Create `include/jumpcastle/surface_samples.hpp` and `src/surface_samples.cpp`: deterministic polygon/arc/capsule launch-anchor extraction.
- Modify `include/jumpcastle/solver.hpp` and `src/solver.cpp`: normal-aware nodes, transitions, and result jumps.
- Modify `include/jumpcastle/replay.hpp` and `src/replay.cpp`: trace schema v2 and normal/collider verification.
- Modify `src/solver_main.cpp`: print and write v2 route metadata.
- Modify `tests/player_test.cpp`, `tests/collision_world_test.cpp`,
  `tests/solver_test.cpp`, `tests/replay_test.cpp`,
  `tests/world_reachability_test.cpp`, and `CMakeLists.txt`.

### Task 1: Persist deterministic support contacts in player state

**Files:**
- Modify: `include/jumpcastle/player.hpp`
- Modify: `include/jumpcastle/collision_world.hpp`
- Modify: `src/collision_world.cpp`
- Modify: `src/player.cpp`
- Test: `tests/player_test.cpp`

**Interfaces:**
- Produces: `PlayerState::ground_normal`, `ground_point`,
  `ground_collider_id`, `ground_piece_index`.
- Produces: `CollisionWorld::support_contact(Aabb)`.
- Consumes: Plan 1 `Contact`.

- [ ] **Step 1: Write failing support-state tests**

Add:

```cpp
TEST_CASE("landing persists the selected support frame") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders.push_back({
        .id = 21,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{{{4,20},{12,16},{12,20}}},
        .tag = "slope",
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    PlayerState player{.position = {10.0F, 14.0F}, .velocity = {0.0F, 4.0F}};

    for (int tick = 0; tick < 240 && !player.on_ground; ++tick) {
        step_player(player, world, {}, config::fixed_delta);
    }

    REQUIRE(player.on_ground);
    CHECK(player.ground_collider_id == 21);
    CHECK(length(player.ground_normal) == Approx(1.0F));
    CHECK(player.ground_normal.x < 0.0F);
    CHECK(player.ground_normal.y <= -0.5F);
}

TEST_CASE("player defaults to a flat deterministic support frame") {
    const PlayerState player{};
    CHECK(player.ground_normal == Vec2{0.0F, -1.0F});
    CHECK(player.ground_collider_id == -1);
    CHECK(player.ground_piece_index == 0);
}
```

- [ ] **Step 2: Run and verify missing state fields**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
```

Expected: compilation fails for missing ground-support fields.

- [ ] **Step 3: Extend `PlayerState`**

Add:

```cpp
Vec2 ground_normal{0.0F, -1.0F};
Vec2 ground_point{};
int ground_collider_id{-1};
int ground_piece_index{};
```

- [ ] **Step 4: Replace the boolean foot probe with a contact probe**

Declare:

```cpp
[[nodiscard]] std::optional<Contact> support_contact(Aabb box) const noexcept;
```

Implement it with the same neighboring-screen broad phase and narrow-phase
dispatch as `resolve`, ignoring hazards and accepting only contacts with
`normal.y <= -0.5F`. Apply the same deterministic selection comparator:

```cpp
bool better_ground_contact(const Contact& candidate, const Contact& current) noexcept {
    constexpr float epsilon = 1.0e-5F;
    if (candidate.normal.y < current.normal.y - epsilon) return true;
    if (candidate.normal.y > current.normal.y + epsilon) return false;
    if (candidate.depth > current.depth + epsilon) return true;
    if (candidate.depth < current.depth - epsilon) return false;
    return std::tie(candidate.collider_id, candidate.piece_index) <
           std::tie(current.collider_id, current.piece_index);
}
```

- [ ] **Step 5: Persist and clear support in `step_player`**

Use:

```cpp
void apply_ground_contact(PlayerState& player, const Contact& contact) noexcept {
    player.on_ground = true;
    player.ground_normal = contact.normal;
    player.ground_point = contact.point;
    player.ground_collider_id = contact.collider_id;
    player.ground_piece_index = contact.piece_index;
}

void clear_ground_contact(PlayerState& player) noexcept {
    player.on_ground = false;
    player.ground_normal = {0.0F, -1.0F};
    player.ground_point = {};
    player.ground_collider_id = -1;
    player.ground_piece_index = 0;
}
```

Apply `ResolveResult::ground_contact` after resolution. If absent and the
player is falling/resting, query the existing foot AABB through
`world.support_contact(feet)`. Clear support whenever the player becomes
airborne.

- [ ] **Step 6: Run focused player/collision tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[player]"
rtk ./build/cmake/jumpcastle_tests "[collision_world]"
```

Expected: support IDs/normals persist without grounded flicker.

- [ ] **Step 7: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/player.hpp include/jumpcastle/collision_world.hpp \
  src/player.cpp src/collision_world.cpp tests/player_test.cpp
rtk git commit -m "feat(player): persist ground contact frames"
```

### Task 2: Rotate walking and charged launch into the surface frame

**Files:**
- Modify: `include/jumpcastle/player.hpp`
- Modify: `src/player.cpp`
- Test: `tests/player_test.cpp`

**Interfaces:**
- Produces: `surface_tangent_right(Vec2)`.
- Changes: `charged_jump_velocity` accepts `ground_normal`.

- [ ] **Step 1: Write flat, slope, and circle launch tests**

```cpp
TEST_CASE("surface-relative jump preserves flat-ground behavior") {
    const Vec2 legacy = charged_jump_velocity(0.5F, 1.0F);
    const Vec2 framed = charged_jump_velocity(0.5F, 1.0F, {0.0F, -1.0F});
    CHECK(framed.x == Approx(legacy.x));
    CHECK(framed.y == Approx(legacy.y));
}

TEST_CASE("neutral jump follows slope normal") {
    const Vec2 normal = normalized(Vec2{-1.0F, -1.0F});
    const Vec2 velocity = charged_jump_velocity(0.5F, 0.0F, normal);
    CHECK(normalized(velocity).x == Approx(normal.x).margin(1e-5));
    CHECK(normalized(velocity).y == Approx(normal.y).margin(1e-5));
}

TEST_CASE("right input adds right-facing surface tangent") {
    const Vec2 normal = normalized(Vec2{0.6F, -0.8F});
    const Vec2 tangent = surface_tangent_right(normal);
    const Vec2 neutral = charged_jump_velocity(0.5F, 0.0F, normal);
    const Vec2 right = charged_jump_velocity(0.5F, 1.0F, normal);
    CHECK(dot(right - neutral, tangent) > 0.0F);
}

TEST_CASE("ground movement follows the support tangent") {
    PlayerState player{};
    player.mode = PlayerMode::grounded;
    player.on_ground = true;
    player.ground_normal = normalized(Vec2{-1.0F, -1.0F});
    simulate_ground_movement(player, {.right = true}, 0.1F);
    CHECK(dot(player.velocity, surface_tangent_right(player.ground_normal)) > 0.0F);
    CHECK(std::abs(dot(player.velocity, player.ground_normal)) < 1e-5F);
}
```

- [ ] **Step 2: Run and observe old world-axis behavior**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[player]"
```

Expected: slope-normal and tangent tests fail.

- [ ] **Step 3: Add frame helpers and overload**

Declare:

```cpp
[[nodiscard]] Vec2 surface_tangent_right(Vec2 normal) noexcept;
[[nodiscard]] Vec2 charged_jump_velocity(
    float hold_time, float horizontal_input, Vec2 ground_normal) noexcept;
```

Keep the two-argument overload as a compatibility wrapper:

```cpp
Vec2 charged_jump_velocity(
    const float hold_time, const float horizontal_input) noexcept {
    return charged_jump_velocity(
        hold_time, horizontal_input, {0.0F, -1.0F});
}
```

Implement:

```cpp
Vec2 surface_tangent_right(const Vec2 normal) noexcept {
    return {-normal.y, normal.x};
}

Vec2 charged_jump_velocity(
    const float hold_time,
    const float horizontal_input,
    const Vec2 ground_normal) noexcept {
    const float raw_charge = std::clamp(
        (hold_time - config::minimum_charge_seconds) /
            (config::maximum_charge_seconds - config::minimum_charge_seconds),
        0.0F, 1.0F);
    const float charge = raw_charge * raw_charge * (3.0F - 2.0F * raw_charge);
    const float input = std::clamp(horizontal_input, -1.0F, 1.0F);
    const float horizontal_speed = 4.5F + charge * 3.5F;
    const float vertical_speed =
        config::jump_strength * (0.55F + charge * 0.45F);
    const Vec2 normal = normalized(ground_normal);
    return normal * vertical_speed +
        surface_tangent_right(normal) * (input * horizontal_speed);
}
```

- [ ] **Step 4: Use the frame for walk and release**

For grounded walk:

```cpp
const float direction =
    (input.right ? 1.0F : 0.0F) - (input.left ? 1.0F : 0.0F);
player.velocity = surface_tangent_right(player.ground_normal) *
    (direction * config::movement_acceleration * fixed_delta);
```

For charged release:

```cpp
player.velocity = charged_jump_velocity(
    player.jump_hold_time, direction, player.ground_normal);
```

Capture the normal before calling `clear_ground_contact`.

- [ ] **Step 5: Run player and fixed-step regressions**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[player]"
rtk ./build/cmake/jumpcastle_tests "frame chunking produces identical fixed steps"
```

Expected: flat values remain unchanged; surface-relative cases pass.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/player.hpp src/player.cpp tests/player_test.cpp
rtk git commit -m "feat(player): launch along support normals"
```

### Task 3: Generate deterministic solver samples for segments, circles, and capsules

**Files:**
- Create: `include/jumpcastle/surface_samples.hpp`
- Create: `src/surface_samples.cpp`
- Create: `tests/surface_samples_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `SurfaceSample` and `sample_walkable_surfaces`.
- Consumes: Plan 1 `WorldCollider`.

- [ ] **Step 1: Write failing sample-order tests**

```cpp
TEST_CASE("circle samples only its walkable upper arc") {
    const WorldCollider collider{
        .id = 7,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{{10.0F, 10.0F}, 3.0F},
        .aabb = {{7.0F, 7.0F}, {13.0F, 13.0F}},
    };
    const auto samples = sample_walkable_surfaces(collider, 0.5F);
    REQUIRE_FALSE(samples.empty());
    for (const SurfaceSample& sample : samples) {
        CHECK(sample.normal.y <= -0.5F);
        CHECK(sample.collider_id == 7);
    }
}

TEST_CASE("surface samples are stable and ordered") {
    const WorldCollider collider{
        .id = 9,
        .type = ColliderType::solid,
        .geometry = CapsuleGeometry{{4.0F, 8.0F}, {12.0F, 8.0F}, 1.5F},
        .aabb = {{2.5F, 6.5F}, {13.5F, 9.5F}},
    };
    CHECK(sample_walkable_surfaces(collider, 0.25F) ==
          sample_walkable_surfaces(collider, 0.25F));
}
```

- [ ] **Step 2: Declare the sampler**

```cpp
struct SurfaceSample {
    Vec2 position{};
    Vec2 normal{0.0F, -1.0F};
    int collider_id{-1};
    int piece_index{};
    int sample_index{};

    friend bool operator==(const SurfaceSample&, const SurfaceSample&) = default;
};

[[nodiscard]] std::vector<SurfaceSample> sample_walkable_surfaces(
    const WorldCollider& collider, float spacing);
```

- [ ] **Step 3: Implement polygon edge sampling**

For every edge whose outward normal has `y <= -0.5F`, emit endpoints and
interior positions at `ceil(length / spacing)` intervals. Preserve polygon point
order and increment `sample_index` monotonically.

```cpp
void sample_segment(
    std::vector<SurfaceSample>& out,
    const Vec2 a,
    const Vec2 b,
    const Vec2 normal,
    const WorldCollider& collider,
    const float spacing) {
    const int intervals = std::max(
        1, static_cast<int>(std::ceil(length(b - a) / spacing)));
    for (int i = 0; i <= intervals; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(intervals);
        out.push_back({
            a + (b - a) * t, normal, collider.id, collider.piece_index,
            static_cast<int>(out.size()),
        });
    }
}
```

- [ ] **Step 4: Implement circle and capsule arc sampling**

Circle walkable angles are those whose normal satisfies `sin(angle) <= -0.5`.
With y-down coordinates:

```cpp
constexpr float start = 7.0F * std::numbers::pi_v<float> / 6.0F;
constexpr float end = 11.0F * std::numbers::pi_v<float> / 6.0F;
const float arc_length = circle.radius * (end - start);
const int intervals = std::max(
    1, static_cast<int>(std::ceil(arc_length / spacing)));
for (int i = 0; i <= intervals; ++i) {
    const float angle = std::lerp(start, end,
        static_cast<float>(i) / static_cast<float>(intervals));
    const Vec2 normal{std::cos(angle), std::sin(angle)};
    emit(circle.center + normal * circle.radius, normal);
}
```

For capsules, sample the two end semicircles using the segment's local normal
frame and sample the offset walkable side segment when its normal passes the
walkable threshold. Sort the final vector by position.y, position.x, then
sample index only after preserving stable collider/piece identity.

- [ ] **Step 5: Run sampler tests**

```bash
rtk cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[surface_samples]"
```

Expected: all samples are stable, normalized, and walkable.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/surface_samples.hpp src/surface_samples.cpp \
  tests/surface_samples_test.cpp CMakeLists.txt
rtk git commit -m "feat(solver): sample curved support surfaces"
```

### Task 4: Make solver nodes and transitions normal-aware

**Files:**
- Modify: `include/jumpcastle/solver.hpp`
- Modify: `src/solver.cpp`
- Modify: `tests/solver_test.cpp`

**Interfaces:**
- Consumes: `SurfaceSample`.
- Produces: `SolverJump` with launch/landing normals and collider IDs.

- [ ] **Step 1: Add a curved-route solver test**

```cpp
TEST_CASE("solver finds a route that requires a circle-normal launch") {
    CampaignWorld world = make_shape_solver_world();
    SolverConfig config;
    config.charge_ticks = {42, 54, 66};
    config.launch_sample_spacing = 0.20F;

    const SolverResult result = ReachabilitySolver{world, config}.solve_campaign();

    REQUIRE(result.reachable);
    REQUIRE_FALSE(result.jumps.empty());
    CHECK(std::ranges::any_of(result.jumps, [](const SolverJump& jump) {
        return jump.start_collider_id == 20 &&
               std::abs(jump.start_normal.x) > 0.1F;
    }));
}
```

Add `make_shape_solver_world` to `tests/test_world_factory.hpp` with a spawn
floor, a required circle platform, a capsule landing, and a goal reachable only
through the curved launch.

- [ ] **Step 2: Extend solver result fields**

```cpp
struct SolverJump {
    Vec2 start{};
    Vec2 start_normal{0.0F, -1.0F};
    int start_collider_id{-1};
    Vec2 landing{};
    Vec2 landing_normal{0.0F, -1.0F};
    int landing_collider_id{-1};
    JumpDirection direction{JumpDirection::neutral};
    int charge_ticks{};
    int start_screen{};
    int landing_screen{};
};
```

- [ ] **Step 3: Replace horizontal `Surface` extraction**

Delete the current `Surface { y, start_x, end_x, screen }` model. Gather
`SurfaceSample` values from every solid collider in every screen:

```cpp
std::vector<SurfaceSample> samples;
for (int screen = 0; screen < world.screen_count(); ++screen) {
    const auto* colliders = world.collision.colliders_for_screen(screen);
    if (colliders == nullptr) continue;
    for (const WorldCollider& collider : *colliders) {
        if (collider.type != ColliderType::solid) continue;
        auto current = sample_walkable_surfaces(
            collider, config.launch_sample_spacing);
        samples.insert(samples.end(), current.begin(), current.end());
    }
}
```

Quantize node identity using position, normal, collider ID, and piece index.

- [ ] **Step 4: Simulate every candidate from the support frame**

Initialize:

```cpp
PlayerState player{
    .position = sample.position + sample.normal * config::player_half_size.y,
    .mode = PlayerMode::grounded,
    .on_ground = true,
    .ground_normal = sample.normal,
    .ground_point = sample.position,
    .ground_collider_id = sample.collider_id,
    .ground_piece_index = sample.piece_index,
};
```

Charge and release through `step_world` exactly as the current solver does.
Read landing normal/ID from `PlayerState` when grounded. Do not infer landing
from nearest surface geometry.

- [ ] **Step 5: Preserve deterministic BFS ordering**

Order launch directions `left, neutral, right`, charge ticks in ascending array
order, and samples by `(screen, collider_id, piece_index, sample_index)`.
Include normal quantization in visited keys:

```cpp
const int qnx = static_cast<int>(std::lround(normal.x / config.state_quantization));
const int qny = static_cast<int>(std::lround(normal.y / config.state_quantization));
```

- [ ] **Step 6: Run solver tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[solver]"
```

Expected: the synthetic curved route is reachable and deterministic across two
successive runs.

- [ ] **Step 7: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/solver.hpp src/solver.cpp \
  tests/solver_test.cpp tests/test_world_factory.hpp
rtk git commit -m "feat(solver): search surface-normal launch anchors"
```

### Task 5: Upgrade trace serialization and verification to replay schema v2

**Files:**
- Modify: `include/jumpcastle/replay.hpp`
- Modify: `src/replay.cpp`
- Modify: `tests/replay_test.cpp`
- Modify: `src/solver_main.cpp`

**Interfaces:**
- Consumes: extended `SolverJump`.
- Produces: backward-readable `SolverTrace` schema v2.

- [ ] **Step 1: Write v2 round-trip and mismatch tests**

```cpp
TEST_CASE("trace v2 round-trips launch and landing support frames") {
    SolverTrace trace;
    trace.schema_version = 2;
    trace.jumps.push_back({
        .launch_position = {4.0F, 10.0F},
        .launch_normal = normalized(Vec2{-1.0F, -1.0F}),
        .launch_collider_id = 8,
        .direction = JumpDirection::right,
        .charge_ticks = 54,
        .expected_landing = {12.0F, 6.0F},
        .expected_landing_normal = {0.0F, -1.0F},
        .expected_landing_collider_id = 9,
        .expected_screen = 1,
    });

    const SolverTrace parsed = parse_trace(serialize_trace(trace), "trace-v2");
    REQUIRE(parsed.schema_version == 2);
    REQUIRE(parsed.jumps.size() == 1);
    CHECK(parsed.jumps[0].launch_collider_id == 8);
    CHECK(parsed.jumps[0].launch_normal.x == Approx(-0.707106F));
    CHECK(parsed.jumps[0].expected_landing_collider_id == 9);
}

TEST_CASE("replay reports launch normal mismatch") {
    CampaignWorld world = make_shape_solver_world();
    SolverTrace trace = known_shape_trace();
    trace.jumps.front().launch_normal = {1.0F, 0.0F};
    const ReplayResult result = verify_trace(world, trace);
    CHECK_FALSE(result.completed);
    CHECK(result.failure.find("launch normal mismatch") != std::string::npos);
}
```

- [ ] **Step 2: Extend trace data**

```cpp
struct TraceJump {
    Vec2 launch_position{};
    Vec2 launch_normal{0.0F, -1.0F};
    int launch_collider_id{-1};
    JumpDirection direction{JumpDirection::neutral};
    int charge_ticks{};
    Vec2 expected_landing{};
    Vec2 expected_landing_normal{0.0F, -1.0F};
    int expected_landing_collider_id{-1};
    int expected_screen{};
};

struct SolverTrace {
    int schema_version{2};
    float fixed_delta{config::fixed_delta};
    std::vector<TraceJump> jumps;
};
```

- [ ] **Step 3: Serialize and parse v2 fields**

Use JSON keys:

```json
{
  "launch": {"position": [0,0], "normal": [0,-1], "collider_id": 1},
  "direction": "right",
  "charge_ticks": 54,
  "landing": {
    "position": [1,1],
    "normal": [0,-1],
    "collider_id": 2,
    "screen": 1
  }
}
```

Accept schema v1 by defaulting normals to `{0,-1}` and collider IDs to `-1`.
Reject versions other than 1 and 2.

- [ ] **Step 4: Verify support before charge and after landing**

Before walking/charging:

```cpp
if (trace.schema_version >= 2) {
    if (player.ground_collider_id != jump.launch_collider_id) {
        return replay_failure(executed, "jump " + std::to_string(index + 1) +
            " launch collider mismatch");
    }
    if (length(player.ground_normal - jump.launch_normal) > 0.03F) {
        return replay_failure(executed, "jump " + std::to_string(index + 1) +
            " launch normal mismatch");
    }
}
```

Perform equivalent checks after landing. Walking to the launch anchor uses
surface tangent movement and must remain on the expected collider.

- [ ] **Step 5: Run replay and solver trace tests**

```bash
rtk cmake --build build/cmake --target jumpcastle_tests --parallel
rtk ./build/cmake/jumpcastle_tests "[replay]"
rtk ./build/cmake/jumpcastle_tests "solver trace replays through production physics"
```

Expected: v1 regression remains readable; v2 retains and validates support data.

- [ ] **Step 6: Commit only with explicit authorization**

```bash
rtk git add include/jumpcastle/replay.hpp src/replay.cpp \
  tests/replay_test.cpp src/solver_main.cpp
rtk git commit -m "feat(replay): verify surface-normal trace v2"
```

### Task 6: Prove deterministic curved physics end to end

**Files:**
- Modify: `tests/world_reachability_test.cpp`
- Modify: `tests/simulation_test.cpp`
- Modify: `assets/levels/campaign-route.json` only after Plan 4 redesign

**Interfaces:**
- Validates all prior tasks; produces no new production API.

- [ ] **Step 1: Add repeated-solve determinism test**

```cpp
TEST_CASE("curved campaign solve is deterministic") {
    const CampaignWorld world = make_shape_solver_world();
    const SolverResult first = ReachabilitySolver{world}.solve_campaign();
    const SolverResult second = ReachabilitySolver{world}.solve_campaign();
    REQUIRE(first.reachable);
    REQUIRE(second.reachable);
    CHECK(serialize_trace(make_trace(first)) ==
          serialize_trace(make_trace(second)));
}
```

- [ ] **Step 2: Add fixed-step split regression on a slope launch**

Drive two identical players with equivalent accumulated frame chunks through
`FixedStepClock`; apply the same charged input sequence and compare position,
velocity, support normal, mode, and collider ID after every produced tick.

```cpp
CHECK(a.position == b.position);
CHECK(a.velocity == b.velocity);
CHECK(a.ground_normal == b.ground_normal);
CHECK(a.ground_collider_id == b.ground_collider_id);
CHECK(a.mode == b.mode);
```

- [ ] **Step 3: Run all core and reachability tests**

```bash
rtk cmake --build build/cmake --parallel
rtk ctest --test-dir build/cmake -R "player|collision|simulation|solver|replay|reachable" \
  --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 4: Run unchanged campaign proof**

```bash
rtk ./build/cmake/jumpcastle_solver \
  --levels assets/levels/screens --campaign \
  --trace build/campaign-route-surface-normal.json
```

Expected: the current all-polygon campaign still solves before content redesign.

- [ ] **Step 5: Review and commit only with explicit authorization**

```bash
rtk git diff --check
rtk git status --short
rtk git add tests/world_reachability_test.cpp tests/simulation_test.cpp
rtk git commit -m "test(physics): prove curved-route determinism"
```

## Plan 2 Completion Gate

- Flat-ground movement and jump values remain compatible.
- Slope/circle/capsule movement follows support tangent/normal.
- Solver searches deterministic curved surface anchors.
- Replay v2 validates launch and landing normals/IDs.
- Current campaign and synthetic curved campaign solve and replay.
- Focused and full regression suites pass.
