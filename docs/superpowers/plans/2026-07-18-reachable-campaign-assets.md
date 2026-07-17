# Reachable Campaign and Multi-Pack Assets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the five-room prototype with a twelve-room, three-biome campaign whose assets are traceable to CC0 sources and whose complete route is proven reachable by production physics.

**Architecture:** Move level data into validated external room files, centralize campaign rules in a headless simulation layer shared by game and solver, then hand-author rooms against a landing-state BFS. Generate committed runtime atlases from three official source packs while keeping source archives ignored and normal CMake builds Python-free.

**Tech Stack:** C++20, CMake 3.24+, raylib 5.5, Catch2 3.8.1, Python 3.10+, Pillow 12.3.0, nlohmann/json 3.12.0, CTest.

## Global Constraints

- Keep the gameplay grid at 16 by 12 cells and 16 pixels per tile.
- Load exactly twelve rooms ordered bottom-to-top as rooms 01 through 12.
- Use Pixel Adventure for rooms 01-04, Kenney Pixel Platformer for rooms 05-08, and Kings and Pigs for rooms 09-12.
- Use the Kings and Pigs King Human as player in all rooms.
- Use only official CC0 downloads; commit source metadata and CC0 text while ignoring source archives.
- Add spikes, checkpoints in rooms 01, 05, and 09, and one exit in room 12.
- Keep moving platforms, enemies, combat, collectibles, and procedural generation out of scope.
- The solver must call production simulation at 60 Hz and must not contain alternate physics.
- Valid routes use at most 85 percent charge and pass charge/position perturbation tolerance.
- Apply red-green-refactor to parser, campaign, simulation, solver, and manifest behavior.
- Preserve C++20 portability across MSVC, AppleClang/Clang, and GCC.

---

## Target File Map

- `assets/levels/room-01.level` ... `room-12.level`: campaign metadata and collision cells.
- `assets/generated/manifest.json` and PNG atlases: committed runtime art.
- `assets/sources/*/SOURCE.md`: creator, source URL, CC0 license, retrieval, and derivation notes.
- `tools/build_assets.py`: deterministic extraction, nearest-neighbor normalization, and atlas packing.
- `tools/verify_assets.py`: manifest bounds, digest, source, animation, and Git hygiene validation.
- `include/jumpcastle/level.hpp`, `src/level.cpp`: parser, repository, and room selection.
- `include/jumpcastle/campaign.hpp`, `src/campaign.cpp`: checkpoint, respawn, completion, restart.
- `include/jumpcastle/simulation.hpp`, `src/simulation.cpp`: shared deterministic production step.
- `include/jumpcastle/solver.hpp`, `src/solver.cpp`: landing-state reachability and tolerance replay.
- `src/solver_main.cpp`: headless validation CLI.
- `include/jumpcastle/assets.hpp`, `src/assets.cpp`: runtime atlas/animation catalog.
- `tests/*_test.cpp`: parser, campaign, solver, asset, and real-campaign coverage.

---

### Task 1: Add the Validated Twelve-Room Level Model

**Files:**
- Create: `include/jumpcastle/level.hpp`
- Create: `src/level.cpp`
- Create: `tests/level_test.cpp`
- Modify: `include/jumpcastle/tilemap.hpp`
- Modify: `src/tilemap.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `Biome`, `RoomMetadata`, `Room`, `RoomSelection`, and `LevelRepository`.
- Produces: `Room parse_room(std::string_view, std::string_view)`.
- Produces: `std::optional<RoomSelection> LevelRepository::select(float) const`.

- [ ] **Step 1: Write parser and selection tests first**

```cpp
TEST_CASE("room parser reads metadata and markers") {
    const Room room = parse_room(valid_room_text(), "valid-room.level");
    CHECK(room.metadata.name == "First Steps");
    CHECK(room.metadata.biome == Biome::pixel_adventure);
    CHECK(room.metadata.difficulty == 1);
    CHECK(room.tilemap.tile_at(7, 9) == Tile::spawn);
}

TEST_CASE("room parser reports line for invalid width") {
    CHECK_THROWS_WITH(parse_room(invalid_width_text(), "broken.level"),
                      Catch::Matchers::ContainsSubstring("broken.level:5"));
}

TEST_CASE("world height selects bottom-to-top room") {
    const LevelRepository repository = twelve_room_fixture();
    CHECK(repository.select(-6.0F)->index == 0);
    CHECK(repository.select(-18.0F)->index == 1);
    CHECK_FALSE(repository.select(6.0F));
    CHECK_FALSE(repository.select(-150.0F));
}
```

Test helpers return complete 16-by-12 strings and twelve real `Room` objects, not mocks.

- [ ] **Step 2: Run RED**

```bash
cmake --build out/verify --config Release --parallel
```

Expected: compile failure because `jumpcastle/level.hpp` does not exist.

- [ ] **Step 3: Extend tile types and define interfaces**

```cpp
enum class Tile : char {
    empty = '.', solid = '#', spike = '^', spawn = 'S', checkpoint = 'C', exit = 'E'
};

enum class Biome { pixel_adventure, kenney, kings_and_pigs };
struct RoomMetadata { std::string name; Biome biome; int difficulty; };
struct Room { RoomMetadata metadata; Tilemap tilemap; };
struct RoomSelection { std::size_t index; const Room* room; float vertical_offset; };

class LevelRepository {
public:
    static constexpr std::size_t room_count = 12;
    explicit LevelRepository(std::array<Room, room_count> rooms);
    static LevelRepository load(const std::filesystem::path& directory);
    [[nodiscard]] const Room& room(std::size_t index) const;
    [[nodiscard]] std::optional<RoomSelection> select(float world_y) const noexcept;
    [[nodiscard]] Vector2 campaign_spawn() const;
private:
    std::array<Room, room_count> rooms_;
};
```

- [ ] **Step 4: Implement strict parsing and selection**

Require `name`, `biome`, `difficulty`, separator `---`, and exactly 12 rows of 16 known tokens.
Errors use `<filename>:<line>: <reason>`. Validate exactly one campaign spawn/exit, checkpoints in
rooms 01/05/09, and biome groups 4/4/4. Select room with:

```cpp
const int index = static_cast<int>(
    std::floor(-world_y / static_cast<float>(config::tilemap_height)));
if (index < 0 || index >= static_cast<int>(room_count)) return std::nullopt;
return RoomSelection{static_cast<std::size_t>(index), &rooms_[index],
    -static_cast<float>((index + 1) * config::tilemap_height)};
```

- [ ] **Step 5: Reach GREEN and commit**

```bash
cmake -S . -B out/verify -DCMAKE_BUILD_TYPE=Release -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release -R "room|world height" --output-on-failure
git add CMakeLists.txt include/jumpcastle/level.hpp include/jumpcastle/tilemap.hpp src/level.cpp src/tilemap.cpp tests/level_test.cpp
git commit -m "feat(level): Add validated external room model"
```

---

### Task 2: Centralize Campaign Simulation, Hazards, and Checkpoints

**Files:**
- Create: `include/jumpcastle/campaign.hpp`
- Create: `include/jumpcastle/simulation.hpp`
- Create: `src/campaign.cpp`
- Create: `src/simulation.cpp`
- Create: `tests/campaign_test.cpp`
- Create: `tests/simulation_test.cpp`
- Modify: `src/game.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CampaignState`, `CampaignEvent`, `respawn_player`, `restart_campaign`.
- Produces: `simulate_step`, used by both runtime and solver.

- [ ] **Step 1: Write checkpoint, spike, and completion tests**

```cpp
TEST_CASE("checkpoint changes deterministic respawn") {
    CampaignState campaign{.respawn_position = {8.5F, -6.5F}};
    PlayerState player{};
    activate_checkpoint(campaign, {3.5F, -18.5F}, 4);
    player.velocity = {4.0F, 7.0F};
    respawn_player(campaign, player);
    CHECK(player.position.x == 3.5F);
    CHECK(player.position.y == -18.5F);
    CHECK(player.velocity.x == 0.0F);
    CHECK(campaign.deaths == 1);
}

TEST_CASE("spike overlap respawns through production step") {
    auto fixture = campaign_fixture_with_spike_under_player();
    CHECK(simulate_step(fixture.player, fixture.campaign, fixture.level,
                        PlayerInput{}, 1.0F / 60.0F) == CampaignEvent::respawned);
    CHECK(fixture.campaign.deaths == 1);
}
```

- [ ] **Step 2: Run RED**

Expected: missing campaign/simulation headers.

- [ ] **Step 3: Define and implement the shared step**

```cpp
enum class CampaignEvent { none, checkpoint_activated, respawned, completed };
struct CampaignState {
    Vector2 respawn_position{};
    std::size_t checkpoint_room{};
    int deaths{};
    float elapsed_seconds{};
    bool complete{};
};
CampaignEvent simulate_step(
    PlayerState&, CampaignState&, const LevelRepository&, PlayerInput, float delta) noexcept;
```

`simulate_step` selects room, calls `update_player`, resolves solids, checks spikes first, then
checkpoint and exit markers. Out-of-campaign position respawns. Reset clears velocity, charge,
animation, and grounded state. Elapsed time advances only before completion.

- [ ] **Step 4: Make Game call the shared step**

Load room files from `<application>/assets/levels`. `Game::update` handles input/window/debug only,
calls `simulate_step`, then refreshes room selection for rendering.

- [ ] **Step 5: Reach GREEN and commit**

```bash
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release -R "checkpoint|spike|campaign" --output-on-failure
git add CMakeLists.txt include/jumpcastle/campaign.hpp include/jumpcastle/simulation.hpp src/campaign.cpp src/simulation.cpp src/game.cpp tests/campaign_test.cpp tests/simulation_test.cpp
git commit -m "feat(campaign): Add checkpoints hazards and completion"
```

---

### Task 3: Build the Production-Physics Reachability Solver

**Files:**
- Create: `include/jumpcastle/solver.hpp`
- Create: `src/solver.cpp`
- Create: `src/solver_main.cpp`
- Create: `tests/solver_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `simulate_step`, `LevelRepository`, and player constants.
- Produces: `SolverConfig`, `SolverJump`, `SolverResult`, `ReachabilitySolver`.
- Produces CLI: `jumpcastle_level_solver --room N` and `--campaign`.

- [ ] **Step 1: Write reachable and impossible RED tests**

```cpp
TEST_CASE("solver finds a tolerant route") {
    const SolverResult result = ReachabilitySolver{
        solver_fixture_with_two_platforms()}.solve_room(0);
    REQUIRE(result.reachable);
    CHECK(result.maximum_charge_ratio <= 0.85F);
    CHECK(result.tolerance_passed);
}

TEST_CASE("solver rejects ledge beyond jump envelope") {
    const SolverResult result = ReachabilitySolver{
        solver_fixture_with_unreachable_ledge()}.solve_room(0);
    CHECK_FALSE(result.reachable);
    CHECK(result.failure.find("nearest surface") != std::string::npos);
}
```

- [ ] **Step 2: Define solver types and run RED**

```cpp
enum class JumpDirection { left = -1, neutral = 0, right = 1 };
struct SolverConfig {
    int simulation_hz{60};
    std::array<int, 10> charge_frames{8, 12, 16, 20, 24, 28, 32, 36, 40, 44};
    int maximum_air_frames{180};
    float launch_sample_spacing{0.25F};
    float state_quantization{0.1F};
};
struct SolverJump {
    Vector2 start{};
    Vector2 landing{};
    JumpDirection direction{JumpDirection::neutral};
    int charge_frames{};
    std::size_t start_room{};
    std::size_t landing_room{};
};
struct SolverResult {
    bool reachable{};
    bool tolerance_passed{};
    float maximum_charge_ratio{};
    std::vector<SolverJump> jumps;
    std::string failure;
};
```

Run `cmake --build out/verify --config Release --parallel`; expect failure because
`jumpcastle/solver.hpp` does not exist.

- [ ] **Step 3: Implement surface extraction, BFS, and tolerance replay**

Extract contiguous solid top edges with empty space above; discard widths below one tile. Sample
launch X every 0.25 tile. Charge, release in left/neutral/right, then call `simulate_step` at 1/60
until landing, death, completion, or 180 frames. Deduplicate by room, checkpoint, surface, and X
rounded to 0.1. Reconstruct parents. Replay nominal, charge -2/+2, X -0.1/+0.1; require three of
five to land on the same destination and reject charge above 85 percent.

- [ ] **Step 4: Add CLI and reach GREEN**

```text
jumpcastle_level_solver --levels <directory> --room <1-12> [--trace <json>]
jumpcastle_level_solver --levels <directory> --campaign [--trace <json>]
```

Exit 0 for tolerant route, 1 for unreachable, 2 for invalid input/data.

```bash
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release -R solver --output-on-failure
git add CMakeLists.txt include/jumpcastle/solver.hpp src/solver.cpp src/solver_main.cpp tests/solver_test.cpp
git commit -m "feat(solver): Prove level reachability with production physics"
```

---

### Task 4: Hand-Author and Verify Twelve Campaign Rooms

**Files:**
- Create: `assets/levels/room-01.level` through `room-12.level`
- Create: `tests/campaign_reachability_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: strict room format and solver.
- Produces: runtime campaign geometry and CI reachability evidence.

- [ ] **Step 1: Create twelve exact-dimension room files**

Each file has `name`, `biome`, `difficulty`, `---`, and 12 rows of 16 tokens. Use closed side
walls and matching top/bottom openings. Follow this progression:

| Room | Biome | Difficulty | Required mechanic | Checkpoint |
| --- | --- | ---: | --- | --- |
| 01 | pixel_adventure | 1 | short/medium charge | yes |
| 02 | pixel_adventure | 1 | wide alternating ledges | no |
| 03 | pixel_adventure | 2 | direction reversal | no |
| 04 | pixel_adventure | 2 | safe wall bounce | no |
| 05 | kenney | 2 | spike introduction | yes |
| 06 | kenney | 3 | controlled spike gaps | no |
| 07 | kenney | 3 | full-tile landings | no |
| 08 | kenney | 3 | one planned wall bounce | no |
| 09 | kings_and_pigs | 3 | alternating tower | yes |
| 10 | kings_and_pigs | 4 | charge selection over spikes | no |
| 11 | kings_and_pigs | 4 | hardest tolerant sequence | no |
| 12 | kings_and_pigs | 3 | readable finale and exit | no |

Place sole `S` plus adjacent `C` in room 01, `C` in rooms 05/09, and sole `E` in room 12. Every
checkpoint platform is two or more tiles wide with one tile overhead clearance.

- [ ] **Step 2: Add real-campaign tests**

```cpp
TEST_CASE("all committed rooms satisfy schema") {
    const LevelRepository level = LevelRepository::load(levels_path());
    CHECK(level.room(0).metadata.biome == Biome::pixel_adventure);
    CHECK(level.room(4).metadata.biome == Biome::kenney);
    CHECK(level.room(8).metadata.biome == Biome::kings_and_pigs);
}

TEST_CASE("every room has tolerant route") {
    const LevelRepository level = LevelRepository::load(levels_path());
    for (std::size_t room = 0; room < LevelRepository::room_count; ++room) {
        INFO("room " << room + 1);
        const SolverResult result = ReachabilitySolver{level}.solve_room(room);
        REQUIRE(result.reachable);
        CHECK(result.tolerance_passed);
    }
}

TEST_CASE("full campaign reaches exit through checkpoints") {
    const SolverResult result = ReachabilitySolver{
        LevelRepository::load(levels_path())}.solve_campaign();
    REQUIRE(result.reachable);
    CHECK(result.tolerance_passed);
}
```

- [ ] **Step 3: Run solver and adjust only failed geometry**

Run each room, then campaign:

```bash
./out/verify/jumpcastle_level_solver --levels assets/levels --room 1
./out/verify/jumpcastle_level_solver --levels assets/levels --campaign --trace out/campaign-route.json
```

Repeat room command for 1-12. Move or widen the reported nearest surface on failure; do not weaken
charge or tolerance limits.

- [ ] **Step 4: Copy levels to output, test, and commit**

```bash
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release -R "committed rooms|tolerant route|full campaign" --output-on-failure
git add CMakeLists.txt assets/levels tests/campaign_reachability_test.cpp
git commit -m "feat(level): Add twelve verified campaign rooms"
```

---

### Task 5: Acquire CC0 Sources and Generate Runtime Atlases

**Files:**
- Create: `assets/sources/CC0-1.0.txt`
- Create: `assets/sources/pixel-adventure/SOURCE.md`
- Create: `assets/sources/kenney-pixel-platformer/SOURCE.md`
- Create: `assets/sources/kings-and-pigs/SOURCE.md`
- Create: `assets/generated/manifest.json`
- Create: `assets/generated/player.png`
- Create: `assets/generated/pixel-adventure.png`
- Create: `assets/generated/kenney.png`
- Create: `assets/generated/kings-and-pigs.png`
- Create: `tools/build_assets.py`
- Create: `tools/verify_assets.py`
- Create: `tools/requirements-assets.txt`
- Create: `tests/assets_test.cpp`
- Modify: `.gitignore`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: deterministic committed atlases and manifest consumed by renderer.
- Produces: Python asset verifier registered in CTest.

- [ ] **Step 1: Download official archives into ignored storage**

Ignore `assets/sources/downloads/`. Download official filenames:

- `Pixel Adventure 1.zip` from <https://pixelfrog-assets.itch.io/pixel-adventure-1> via
  “No thanks, just take me to the downloads”.
- `Kings and Pigs.zip` from <https://pixelfrog-assets.itch.io/kings-and-pigs> via the same flow.
- `kenney_pixel-platformer.zip` from
  <https://kenney.nl/media/pages/assets/pixel-platformer/33bb4921eb-1696667883/kenney_pixel-platformer.zip>.

Inspect with `unzip -l`. Archives must never be staged.

- [ ] **Step 2: Write RED manifest checks**

Register CTest `asset_manifest` running `python3 tools/verify_assets.py
assets/generated/manifest.json`. It initially fails because files do not exist. Add Catch2 checks
for three biome IDs and King states `idle`, `run`, `charge`, `rise`, `fall`, `respawn`.

- [ ] **Step 3: Add source metadata and pinned dependency**

`tools/requirements-assets.txt`:

```text
Pillow==12.3.0
```

Each `SOURCE.md` records creator, official URL, `CC0-1.0`, retrieval `2026-07-18`, archive name,
and generated outputs. Store complete CC0 1.0 legal code once.

- [ ] **Step 4: Implement deterministic build and verification scripts**

`build_assets.py` accepts `--downloads`, `--output`, `--manifest`; locates allowlisted source files
case-insensitively; fails on zero/multiple matches; converts RGBA; uses
`Image.Resampling.NEAREST`; packs stable sorted regions; strips timestamps; writes sorted JSON.
Normalize terrain to 16-by-16 while preserving King animation order.

`verify_assets.py` uses stdlib to validate schema version 1, approved sources, atlas names, region
bounds, SHA-256, King states, and absence of tracked ZIP files.

- [ ] **Step 5: Generate twice and prove reproducibility**

```bash
python3 -m venv out/assets-venv
out/assets-venv/bin/pip install -r tools/requirements-assets.txt
out/assets-venv/bin/python tools/build_assets.py --downloads assets/sources/downloads --output assets/generated --manifest assets/generated/manifest.json
shasum -a 256 assets/generated/* > out/assets-first.sha256
out/assets-venv/bin/python tools/build_assets.py --downloads assets/sources/downloads --output assets/generated --manifest assets/generated/manifest.json
shasum -a 256 assets/generated/* > out/assets-second.sha256
diff -u out/assets-first.sha256 out/assets-second.sha256
python3 tools/verify_assets.py assets/generated/manifest.json
```

- [ ] **Step 6: Commit without archives**

```bash
git add .gitignore CMakeLists.txt assets/generated assets/sources tools tests/assets_test.cpp
git status --short
git commit -m "feat(assets): Add reproducible CC0 biome atlases"
```

Confirm no ZIP is staged.

---

### Task 6: Render Three Biomes and Animated King

**Files:**
- Create: `include/jumpcastle/assets.hpp`
- Create: `src/assets.cpp`
- Create: `tests/assets_manifest_test.cpp`
- Modify: `include/jumpcastle/renderer.hpp`
- Modify: `src/renderer.cpp`
- Modify: `src/game.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: manifest, atlases, `Biome`, `CampaignState`.
- Produces: `AssetCatalog`, biome rendering, King animation, and completion overlay.

- [ ] **Step 1: Write AssetCatalog RED tests**

```cpp
TEST_CASE("catalog exposes every biome and King animation") {
    const AssetCatalog catalog = AssetCatalog::load(generated_manifest_path());
    CHECK(catalog.biome(Biome::pixel_adventure).terrain.width > 0);
    CHECK(catalog.biome(Biome::kenney).terrain.width > 0);
    CHECK(catalog.biome(Biome::kings_and_pigs).terrain.width > 0);
    CHECK(catalog.animation(PlayerAnimation::idle).frames.size() >= 1);
    CHECK(catalog.animation(PlayerAnimation::run).frames.size() >= 2);
}
```

- [ ] **Step 2: Define catalog and load strict JSON**

Add FetchContent `nlohmann_json` tag `v3.12.0`. Define `SpriteRegion`, `AnimationClip`,
`BiomeAssets`, `PlayerAnimation`, and `AssetCatalog`. Reject missing keys, duplicates, negative
sizes, out-of-bounds regions, and unknown biome/animation with manifest path in exceptions.

- [ ] **Step 3: Replace fixed textures and map animations**

Renderer owns player plus three biome atlases with `TEXTURE_FILTER_POINT`. Draw background,
terrain, spike, active/inactive checkpoint, exit, and decoration from manifest. Select biome from
room metadata. Map state: charge when holding jump; run grounded/moving; idle grounded/still; rise
with negative Y velocity; fall with positive Y velocity; respawn after death. Flip by facing.

- [ ] **Step 4: Add completion/debug UI and smoke test**

Show room/biome/checkpoint/death count in debug overlay. On completion show elapsed time, deaths,
and “Press Enter to restart”; Enter calls `restart_campaign`. Run and inspect rooms 01/05/09/12
plus one spike/checkpoint cycle.

- [ ] **Step 5: Reach GREEN and commit**

```bash
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release --output-on-failure
git add CMakeLists.txt include/jumpcastle/assets.hpp include/jumpcastle/renderer.hpp src/assets.cpp src/renderer.cpp src/game.cpp tests/assets_manifest_test.cpp
git commit -m "feat(rendering): Add biome art and King animations"
```

---

### Task 7: Document, Integrate, and Verify

**Files:**
- Create: `docs/LEVEL_DESIGN.md`
- Modify: `README.md`
- Modify: `.github/workflows/ci.yml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: contributor documentation and final cross-platform checks.

- [ ] **Step 1: Register headless checks in CI**

Ensure CTest includes parser, campaign, asset verifier, 12 room solver tests, full campaign, and
tolerance replay. Normal builds use committed atlases without Pillow.

- [ ] **Step 2: Document level and source rules**

`docs/LEVEL_DESIGN.md` includes exact metadata, tokens, room ordering, checkpoints, solver CLI,
85-percent limit, perturbation criteria, and failure interpretation. README adds biome source
links, CC0 notice, campaign controls, checkpoint behavior, real screenshots when captured, and
solver command.

- [ ] **Step 3: Run clean verification**

```bash
cmake -S . -B out/campaign-verify -DCMAKE_BUILD_TYPE=Release -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build out/campaign-verify --config Release --parallel
ctest --test-dir out/campaign-verify -C Release --output-on-failure
./out/campaign-verify/jumpcastle_level_solver --levels assets/levels --campaign --trace out/final-route.json
python3 tools/verify_assets.py assets/generated/manifest.json
git diff --check
git status --short
```

- [ ] **Step 4: Refresh CodeGraph and commit**

```bash
codegraph sync
codegraph explore "Game simulate_step ReachabilitySolver AssetCatalog LevelRepository"
codegraph callers simulate_step
git add .github CMakeLists.txt README.md docs/LEVEL_DESIGN.md
git commit -m "docs: Document verified JumpCastle campaign"
```

Expected: both Game and solver call production simulation; index is current.

---

## Final Review Checklist

- [ ] Exactly 12 rooms load bottom-to-top with biome order 4/4/4.
- [ ] Only room 01 has `S`, only room 12 has `E`, rooms 01/05/09 have checkpoints.
- [ ] Spike death, checkpoint activation, respawn, completion, and restart tests pass.
- [ ] Solver proves every room and campaign with production `simulate_step`.
- [ ] Every route remains below 85 percent charge and passes tolerance replay.
- [ ] King Human and all three environment packs are visible at runtime.
- [ ] Atlases reproduce byte-for-byte from ignored official archives.
- [ ] CC0 source metadata is committed and no archive is tracked.
- [ ] Clean CMake build, CTest, solver CLI, and asset verifier pass.
- [ ] Runtime smoke covers rooms 01/05/09/12 and checkpoint respawn.
- [ ] CI retains Windows, macOS, and Linux coverage.
- [ ] README and level documentation match runtime.
- [ ] CodeGraph shows shared production simulation call paths.
