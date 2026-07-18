# JumpCastle C++20 Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert JumpCastle from a single-file Windows-only C project into a tested, cross-platform C++20 raylib project authored by Thien Phu.

**Architecture:** Build a pure `jumpcastle_core` library for tilemap, collision, and player simulation, then layer the raylib renderer and `Game` orchestration into the `jumpcastle` executable. Use typed value objects and explicit input snapshots so gameplay can be tested without opening a graphics window.

**Tech Stack:** C++20, CMake 3.24+, raylib 5.5, Catch2 3.8.1, CTest, GitHub Actions.

## Global Constraints

- Application code uses C++20 with compiler extensions disabled.
- Support MSVC, AppleClang/Clang, and GCC on Windows, macOS, and Linux.
- Pin raylib to `5.5` and Catch2 to `v3.8.1` through CMake `FetchContent`.
- Preserve the existing level layout, pixel-art rendering, controls, and charge-jump behavior.
- Use `Thien Phu (@thienphuprogrammer)` for all current project attribution.
- Use the MIT License with copyright assigned to Thien Phu.
- Do not commit generated binaries, downloaded dependencies, build directories, or CodeGraph data.
- Follow red-green-refactor for all logic changes: each production behavior requires a test that was observed failing first.

---

## Target File Map

- `CMakeLists.txt`: dependency resolution, targets, warnings, asset copy, install rules, and tests.
- `include/jumpcastle/game_config.hpp`: typed gameplay and viewport constants.
- `include/jumpcastle/tilemap.hpp`, `src/tilemap.cpp`: level data, tile lookup, and screen selection.
- `include/jumpcastle/collision.hpp`, `src/collision.cpp`: box overlap, collision query, and resolution.
- `include/jumpcastle/player.hpp`, `src/player.cpp`: deterministic player state and input-driven simulation.
- `include/jumpcastle/renderer.hpp`, `src/renderer.cpp`: move-only raylib resource ownership and drawing.
- `include/jumpcastle/game.hpp`, `src/game.cpp`: window lifetime, loop ordering, input sampling, and debug state.
- `src/main.cpp`: fatal-error boundary and process exit status.
- `tests/*.cpp`: behavior-focused Catch2 tests for the core library.
- `.github/workflows/ci.yml`: cross-platform configure, build, and CTest matrix.
- `README.md`, `LICENSE`, `.gitignore`: user documentation and repository metadata.

---

### Task 1: Establish the C++20 Build and Test Harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/jumpcastle/game_config.hpp`
- Create: `src/main.cpp`
- Create: `tests/smoke_test.cpp`
- Modify: `.gitignore`
- Move: `build/player.png` to `assets/player.png`
- Move: `build/tilemap.png` to `assets/tilemap.png`

**Interfaces:**
- Produces: CMake targets `jumpcastle_core`, `jumpcastle`, and `jumpcastle_tests`.
- Produces: `jumpcastle::config` typed constants consumed by every later task.
- Produces: `JUMPCASTLE_BUILD_TESTS` option and CTest registration.

- [ ] **Step 1: Move the two runtime assets without changing their bytes**

Run:

```bash
mkdir -p assets
git mv build/player.png assets/player.png
git mv build/tilemap.png assets/tilemap.png
```

Expected: both PNGs are staged as renames; backup files remain untouched until repository cleanup.

- [ ] **Step 2: Add the initial Catch2 smoke test**

Create `tests/smoke_test.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("test harness starts") {
    CHECK(true);
}
```

- [ ] **Step 3: Add typed game constants**

Create `include/jumpcastle/game_config.hpp`:

```cpp
#pragma once

#include "raylib.h"

namespace jumpcastle::config {

inline constexpr int tilemap_width = 16;
inline constexpr int tilemap_height = 12;
inline constexpr int tile_pixels = 16;
inline constexpr int view_width = tilemap_width * tile_pixels;
inline constexpr int view_height = tilemap_height * tile_pixels;
inline constexpr Vector2 player_half_size{0.3F, 0.4F};
inline constexpr float gravity = 30.0F;
inline constexpr float movement_acceleration = 200.0F;
inline constexpr float jump_strength = 15.0F;
inline constexpr float horizontal_bounce = 0.45F;
inline constexpr float maximum_speed = 25.0F;
inline constexpr Color background_color{15, 5, 45, 255};

}  // namespace jumpcastle::config
```

- [ ] **Step 4: Add the root CMake project**

Create `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.24)
project(JumpCastle VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_library(jumpcastle_core INTERFACE)
target_include_directories(jumpcastle_core INTERFACE include)
target_link_libraries(jumpcastle_core INTERFACE raylib)

add_executable(jumpcastle src/main.cpp)
target_link_libraries(jumpcastle PRIVATE jumpcastle_core raylib)

function(jumpcastle_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

jumpcastle_enable_warnings(jumpcastle)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(JUMPCASTLE_BUILD_TESTS_DEFAULT ON)
else()
  set(JUMPCASTLE_BUILD_TESTS_DEFAULT OFF)
endif()
option(JUMPCASTLE_BUILD_TESTS "Build JumpCastle tests" ${JUMPCASTLE_BUILD_TESTS_DEFAULT})

if(JUMPCASTLE_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  add_executable(jumpcastle_tests tests/smoke_test.cpp)
  target_link_libraries(jumpcastle_tests PRIVATE jumpcastle_core Catch2::Catch2WithMain)
  jumpcastle_enable_warnings(jumpcastle_tests)
  include(Catch)
  catch_discover_tests(jumpcastle_tests)
endif()

add_custom_command(TARGET jumpcastle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:jumpcastle>/assets
)

install(TARGETS jumpcastle RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)
```

Create the initial `src/main.cpp` harness entry point:

```cpp
int main() {
    return 0;
}
```

Task 2 converts `jumpcastle_core` from an interface target to a static library. Task 5 replaces
this entry-point harness with the real application error boundary.

- [ ] **Step 5: Configure and run the harness**

Run:

```bash
cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

Expected: configure and build succeed; CTest reports `test harness starts` passed.

- [ ] **Step 6: Expand `.gitignore`**

Ensure `.gitignore` contains:

```gitignore
.codegraph/
.cache/
.idea/
.vscode/
build/
cmake-build-*/
out/
intermediate/
*.sln
*.vcxproj*
*.user
```

- [ ] **Step 7: Commit the build foundation**

```bash
git add CMakeLists.txt .gitignore assets include/jumpcastle/game_config.hpp src/main.cpp tests/smoke_test.cpp
git commit -m "build: Add cross-platform C++20 foundation"
```

---

### Task 2: Implement Typed Tilemaps and Screen Selection

**Files:**
- Create: `include/jumpcastle/tilemap.hpp`
- Create: `src/tilemap.cpp`
- Create: `tests/tilemap_test.cpp`
- Modify: `CMakeLists.txt`
- Delete: `tests/smoke_test.cpp`

**Interfaces:**
- Produces: `enum class Tile : char`.
- Produces: `class Tilemap` with `tile_at`, `solid_at`, and `solid_for_render_at`.
- Produces: `ScreenSelection select_screen(float world_y)`.
- Produces: `const Tilemap& screen(std::size_t index)` and `starting_player_position()`.

- [ ] **Step 1: Write failing tile lookup and screen selection tests**

Create `tests/tilemap_test.cpp`:

```cpp
#include "jumpcastle/tilemap.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("tilemap applies gameplay boundary policies") {
    const auto& map = screen(starting_screen_index);
    CHECK(map.tile_at(0, 0) == Tile::solid);
    CHECK(map.tile_at(-1, 0) == Tile::solid);
    CHECK(map.tile_at(config::tilemap_width, 0) == Tile::solid);
    CHECK(map.tile_at(0, -1) == Tile::empty);
    CHECK(map.tile_at(0, config::tilemap_height) == Tile::empty);
    CHECK(map.solid_for_render_at(-1, 0));
    CHECK(map.solid_for_render_at(0, -1));
}

TEST_CASE("initial position selects the documented starting screen") {
    const Vector2 start = starting_player_position();
    const ScreenSelection selected = select_screen(start.y);
    CHECK(selected.index == starting_screen_index);
    CHECK(selected.tilemap == &screen(starting_screen_index));
}

TEST_CASE("screen selection never returns an invalid index") {
    CHECK(select_screen(10000.0F).index == invalid_screen_index);
    CHECK(select_screen(-10000.0F).index == invalid_screen_index);
}
```

- [ ] **Step 2: Run the tests and verify RED**

Run:

```bash
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

Expected: compilation fails because `jumpcastle/tilemap.hpp` does not exist.

- [ ] **Step 3: Define the tilemap API**

Create `include/jumpcastle/tilemap.hpp`:

```cpp
#pragma once

#include "jumpcastle/game_config.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace jumpcastle {

enum class Tile : char { empty = ' ', solid = '#' };

class Tilemap {
public:
    using Row = std::array<Tile, config::tilemap_width>;
    using Grid = std::array<Row, config::tilemap_height>;

    constexpr explicit Tilemap(Grid grid) noexcept : grid_{grid} {}

    [[nodiscard]] Tile tile_at(int x, int y) const noexcept;
    [[nodiscard]] bool solid_at(int x, int y) const noexcept;
    [[nodiscard]] bool solid_for_render_at(int x, int y) const noexcept;

private:
    Grid grid_{};
};

struct ScreenSelection {
    std::size_t index;
    const Tilemap* tilemap;
    float vertical_offset;
};

inline constexpr std::size_t invalid_screen_index = 0;
inline constexpr std::size_t starting_screen_index = 5;

[[nodiscard]] const Tilemap& screen(std::size_t index) noexcept;
[[nodiscard]] ScreenSelection select_screen(float world_y) noexcept;
[[nodiscard]] Vector2 starting_player_position() noexcept;

}  // namespace jumpcastle
```

- [ ] **Step 4: Implement tilemaps and selection**

In `src/tilemap.cpp`, convert each existing ASCII level row into `Tilemap::Grid` through a
`consteval make_tilemap(std::array<std::string_view, config::tilemap_height>)` helper. Store the
sentinel plus five current screens in `constexpr std::array<Tilemap, 6> screens` without changing
any row text from `source/main.c`.

Implement boundaries and selection as:

```cpp
Tile Tilemap::tile_at(const int x, const int y) const noexcept {
    if (x < 0 || x >= config::tilemap_width) return Tile::solid;
    if (y < 0 || y >= config::tilemap_height) return Tile::empty;
    return grid_[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

bool Tilemap::solid_at(const int x, const int y) const noexcept {
    return tile_at(x, y) == Tile::solid;
}

bool Tilemap::solid_for_render_at(const int x, const int y) const noexcept {
    if (x < 0 || x >= config::tilemap_width || y < 0 || y >= config::tilemap_height) return true;
    return solid_at(x, y);
}

const Tilemap& screen(const std::size_t index) noexcept {
    return screens[index < screens.size() ? index : invalid_screen_index];
}

Vector2 starting_player_position() noexcept {
    return {config::tilemap_width / 2.0F, -config::tilemap_height / 2.0F};
}
```

Calculate the screen number from negative world Y, map it from the starting screen toward lower
array indices as the player climbs, and return the sentinel when outside `[1, 5]`:

```cpp
ScreenSelection select_screen(const float world_y) noexcept {
    const int height_index = static_cast<int>(
        std::floor(-world_y / static_cast<float>(config::tilemap_height)));
    const int index = static_cast<int>(starting_screen_index) - height_index;
    if (index <= static_cast<int>(invalid_screen_index) ||
        index >= static_cast<int>(screens.size())) {
        return {invalid_screen_index, &screens[invalid_screen_index], 0.0F};
    }

    const float offset =
        -static_cast<float>((height_index + 1) * config::tilemap_height);
    return {static_cast<std::size_t>(index),
            &screens[static_cast<std::size_t>(index)],
            offset};
}
```

Verify the starting point maps to `starting_screen_index` before proceeding.

- [ ] **Step 5: Add sources to CMake and verify GREEN**

Convert the interface core target to a static library and update the test source list:

```cmake
add_library(jumpcastle_core STATIC src/tilemap.cpp)
target_include_directories(jumpcastle_core PUBLIC include)
target_link_libraries(jumpcastle_core PUBLIC raylib)
jumpcastle_enable_warnings(jumpcastle_core)
target_sources(jumpcastle_tests PRIVATE tests/tilemap_test.cpp)
```

Remove the smoke test from disk and the test target. Run:

```bash
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

Expected: all tilemap tests pass.

- [ ] **Step 6: Commit the tilemap module**

```bash
git add CMakeLists.txt include/jumpcastle/tilemap.hpp src/tilemap.cpp tests
git commit -m "ref(tilemap): Add typed level and screen selection"
```

---

### Task 3: Extract and Test Collision Physics

**Files:**
- Create: `include/jumpcastle/collision.hpp`
- Create: `src/collision.cpp`
- Create: `tests/collision_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `TileRange overlapped_tiles(Vector2 center, Vector2 half_size)`.
- Produces: `bool collides_with_tilemap(...)`.
- Produces: `void resolve_tilemap_collision(...)`.

- [ ] **Step 1: Write failing collision tests**

Create `tests/collision_test.cpp` with tests that construct a `Tilemap` containing a solid floor
and wall, then assert the public API:

```cpp
#include "jumpcastle/collision.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

namespace {

Tilemap::Grid empty_grid() {
    Tilemap::Grid grid{};
    for (auto& row : grid) row.fill(Tile::empty);
    return grid;
}

Tilemap test_map_with_floor_at(const int y) {
    auto grid = empty_grid();
    grid.at(static_cast<std::size_t>(y)).fill(Tile::solid);
    return Tilemap{grid};
}

Tilemap test_map_with_wall_at(const int x) {
    auto grid = empty_grid();
    for (auto& row : grid) row.at(static_cast<std::size_t>(x)) = Tile::solid;
    return Tilemap{grid};
}

}  // namespace

TEST_CASE("overlapped tiles include both crossed boundaries") {
    const TileRange range = overlapped_tiles({2.5F, 3.5F}, {0.6F, 0.6F});
    CHECK(range.start_x == 1);
    CHECK(range.end_x == 3);
    CHECK(range.start_y == 2);
    CHECK(range.end_y == 4);
}

TEST_CASE("landing clips the player to the floor and stops downward velocity") {
    const Tilemap map = test_map_with_floor_at(6);
    Vector2 center{4.5F, 5.8F};
    Vector2 velocity{0.0F, 10.0F};
    resolve_tilemap_collision(map, 0.0F, center, velocity, config::player_half_size);
    CHECK(center.y == Approx(5.6F));
    CHECK(velocity.y == Approx(0.0F));
}

TEST_CASE("wall collision reflects horizontal velocity") {
    const Tilemap map = test_map_with_wall_at(6);
    Vector2 center{5.8F, 4.5F};
    Vector2 velocity{10.0F, 0.0F};
    resolve_tilemap_collision(map, 0.0F, center, velocity, config::player_half_size);
    CHECK(center.x == Approx(5.7F));
    CHECK(velocity.x == Approx(-4.5F));
}

TEST_CASE("collision query distinguishes solid and empty regions") {
    const Tilemap map = test_map_with_floor_at(6);
    CHECK(collides_with_tilemap(map, 0.0F, {4.5F, 5.8F}, config::player_half_size));
    CHECK_FALSE(collides_with_tilemap(map, 0.0F, {4.5F, 2.0F}, config::player_half_size));
}
```

These test helpers exercise real production objects without mocks.

- [ ] **Step 2: Run and verify RED**

Expected: compilation fails because `jumpcastle/collision.hpp` does not exist.

- [ ] **Step 3: Add the collision API and implementation**

Create `include/jumpcastle/collision.hpp`:

```cpp
#pragma once

#include "jumpcastle/tilemap.hpp"

namespace jumpcastle {

struct TileRange { int start_x; int start_y; int end_x; int end_y; };

[[nodiscard]] TileRange overlapped_tiles(Vector2 center, Vector2 half_size) noexcept;
[[nodiscard]] bool collides_with_tilemap(
    const Tilemap& tilemap, float tilemap_offset_y, Vector2 center, Vector2 half_size) noexcept;
void resolve_tilemap_collision(
    const Tilemap& tilemap,
    float tilemap_offset_y,
    Vector2& center,
    Vector2& velocity,
    Vector2 half_size) noexcept;

}  // namespace jumpcastle
```

Port the existing collision algorithm from `source/main.c` into `src/collision.cpp`. Use
`std::floor`, `std::abs`, `std::min`, and `std::max`; preserve the current least-penetration axis
choice and `config::horizontal_bounce`. Use references instead of nullable output pointers.

- [ ] **Step 4: Add CMake sources and verify GREEN**

Add `src/collision.cpp` to `jumpcastle_core` and `tests/collision_test.cpp` to
`jumpcastle_tests`. Run the focused test and full suite:

```bash
ctest --test-dir build/cmake -C Release -R "collision|overlapped|landing|wall" --output-on-failure
ctest --test-dir build/cmake -C Release --output-on-failure
```

Expected: focused and full suites pass.

- [ ] **Step 5: Commit collision extraction**

```bash
git add CMakeLists.txt include/jumpcastle/collision.hpp src/collision.cpp tests/collision_test.cpp
git commit -m "ref(physics): Extract tested tile collision logic"
```

---

### Task 4: Implement Deterministic Player Simulation

**Files:**
- Create: `include/jumpcastle/player.hpp`
- Create: `src/player.cpp`
- Create: `tests/player_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `PlayerState` with deterministic member initializers.
- Produces: `PlayerInput` snapshot independent of raylib input calls.
- Produces: `void update_player(PlayerState&, ..., PlayerInput, float)`.

- [ ] **Step 1: Write failing player-state and movement tests**

Create `tests/player_test.cpp`:

```cpp
#include "jumpcastle/player.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("player state starts deterministically") {
    const PlayerState player{};
    CHECK(player.velocity.x == 0.0F);
    CHECK(player.velocity.y == 0.0F);
    CHECK(player.jump_hold_time == 0.0F);
    CHECK(player.animation_time == 0.0F);
    CHECK_FALSE(player.on_ground);
    CHECK(player.facing_right);
}

TEST_CASE("ground input changes facing direction") {
    PlayerState player{};
    player.on_ground = true;
    simulate_ground_movement(player, PlayerInput{.left = true}, 0.1F);
    CHECK(player.velocity.x < 0.0F);
    CHECK_FALSE(player.facing_right);
}

TEST_CASE("long jump charge is clamped") {
    const Vector2 short_jump = charged_jump_velocity(0.1F, 0.0F);
    const Vector2 long_jump = charged_jump_velocity(10.0F, 0.0F);
    CHECK(long_jump.y < short_jump.y);
    CHECK(long_jump.y == Approx(-config::jump_strength));
}

TEST_CASE("simulation caps player speed") {
    PlayerState player{};
    player.velocity = {100.0F, 100.0F};
    integrate_player(player, 1.0F / 60.0F);
    CHECK(Vector2Length(player.velocity) == Approx(config::maximum_speed));
}

TEST_CASE("position integration is stable across equivalent frame splits") {
    PlayerState one_step{};
    PlayerState two_steps{};
    one_step.velocity = {3.0F, -4.0F};
    two_steps.velocity = one_step.velocity;

    integrate_player(one_step, 1.0F / 30.0F);
    integrate_player(two_steps, 1.0F / 60.0F);
    integrate_player(two_steps, 1.0F / 60.0F);

    CHECK(one_step.position.x == Approx(two_steps.position.x));
    CHECK(one_step.position.y == Approx(two_steps.position.y));
}
```

- [ ] **Step 2: Run and verify RED**

Expected: compilation fails because `jumpcastle/player.hpp` does not exist.

- [ ] **Step 3: Define the player API**

Create `include/jumpcastle/player.hpp`:

```cpp
#pragma once

#include "jumpcastle/collision.hpp"

namespace jumpcastle {

struct PlayerInput {
    bool left{};
    bool right{};
    bool jump_down{};
    bool jump_released{};
};

struct PlayerState {
    Vector2 position{starting_player_position()};
    Vector2 velocity{};
    float jump_hold_time{};
    float animation_time{};
    bool on_ground{};
    bool facing_right{true};
};

[[nodiscard]] Vector2 charged_jump_velocity(float hold_time, float horizontal_input) noexcept;
void simulate_ground_movement(PlayerState& player, PlayerInput input, float delta) noexcept;
void integrate_player(PlayerState& player, float delta) noexcept;
void update_player(
    PlayerState& player,
    const Tilemap& tilemap,
    float tilemap_offset_y,
    PlayerInput input,
    float delta) noexcept;

}  // namespace jumpcastle
```

- [ ] **Step 4: Implement simulation while preserving charge-jump behavior**

In `src/player.cpp`:

- add gravity before the ground probe;
- detect ground through `collides_with_tilemap` using the existing narrow probe;
- accumulate charge only while Space is held on the ground;
- create jump velocity when Space is released;
- normalize and scale the jump direction exactly once;
- reset charge in the air;
- cap vector length at `config::maximum_speed`; and
- integrate position using `position += velocity * delta`.

Extract `charged_jump_velocity`, `simulate_ground_movement`, and `integrate_player` exactly as
declared so individual behavior remains testable without a window.

- [ ] **Step 5: Add CMake sources and verify GREEN**

Add the player implementation and test files to their targets. Run:

```bash
ctest --test-dir build/cmake -C Release -R "player|jump|simulation" --output-on-failure
ctest --test-dir build/cmake -C Release --output-on-failure
```

Expected: all tests pass with no project compiler warnings.

- [ ] **Step 6: Commit player simulation**

```bash
git add CMakeLists.txt include/jumpcastle/player.hpp src/player.cpp tests/player_test.cpp
git commit -m "ref(player): Add deterministic testable simulation"
```

---

### Task 5: Add RAII Rendering and the Game Loop

**Files:**
- Create: `include/jumpcastle/renderer.hpp`
- Create: `include/jumpcastle/game.hpp`
- Create: `src/renderer.cpp`
- Create: `src/game.cpp`
- Create: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Delete: `source/main.c`

**Interfaces:**
- Produces: move-only `TextureResource` and `RenderTargetResource`.
- Produces: `Renderer::draw(...)`.
- Produces: `Game::run()` and the final executable entry point.

- [ ] **Step 1: Add a failing compile-time ownership test**

Create `tests/resource_traits_test.cpp`:

```cpp
#include "jumpcastle/renderer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace jumpcastle;

TEST_CASE("graphics resources cannot be copied") {
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<TextureResource>);
    STATIC_CHECK_FALSE(std::is_copy_assignable_v<TextureResource>);
    STATIC_CHECK(std::is_move_constructible_v<TextureResource>);
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<RenderTargetResource>);
}
```

- [ ] **Step 2: Run and verify RED**

Expected: compilation fails because `jumpcastle/renderer.hpp` does not exist.

- [ ] **Step 3: Define renderer ownership and drawing interfaces**

Create `include/jumpcastle/renderer.hpp` with move-only resource types that expose
`const Texture2D& get() const noexcept` or `const RenderTexture2D& get() const noexcept`, plus:

```cpp
class Renderer {
public:
    explicit Renderer(const std::filesystem::path& asset_directory);
    void draw(
        const Tilemap& tilemap,
        float screen_offset_y,
        const PlayerState& player,
        bool debug_enabled,
        std::size_t screen_index,
        float delta);

private:
    TextureResource player_texture_;
    TextureResource tilemap_texture_;
    RenderTargetResource pixelart_target_;
};
```

Constructors throw `std::runtime_error` with the attempted asset path if raylib returns an
invalid resource. Destructors call `UnloadTexture` or `UnloadRenderTexture` only for valid IDs.
Move constructors transfer the resource and zero the source ID.

- [ ] **Step 4: Port the current rendering behavior**

In `src/renderer.cpp`, port the existing connected-neighbor tileset selection, player animation
selection, sprite flipping, logical render texture, integer scale, letterboxing, and debug
overlay. Replace macros and global variables with the typed modules from Tasks 1-4. Keep the
sprite coordinates and animation frame rates unchanged.

- [ ] **Step 5: Define and implement `Game`**

Create `include/jumpcastle/game.hpp`:

```cpp
#pragma once

#include "jumpcastle/renderer.hpp"

#include <optional>

namespace jumpcastle {

class Game {
public:
    Game();
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    void run();

private:
    [[nodiscard]] static PlayerInput sample_input() noexcept;
    void update(float delta);

    PlayerState player_{};
    bool debug_enabled_{};
    ScreenSelection active_screen_{select_screen(player_.position.y)};
    std::optional<Renderer> renderer_;
};

}  // namespace jumpcastle
```

`Game` calls `SetConfigFlags`, `InitWindow`, `SetTargetFPS(60)`, and `SetExitKey(KEY_NULL)` in its
constructor body, then emplaces `renderer_`. Resolve the executable asset directory with
`std::filesystem::path(GetApplicationDirectory()) / "assets"`. `run()` clamps delta to
`[0.0001F, 0.1F]`, updates, resolves collision, enforces the minimum viewport size, renders, and
handles debug keys in the order defined by the spec. Dereference `active_screen_.tilemap` only
after checking the pointer supplied by `select_screen`. Reset `renderer_` in `Game::~Game()`
before calling `CloseWindow()`.

- [ ] **Step 6: Add the error boundary**

Create `src/main.cpp`:

```cpp
#include "jumpcastle/game.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        jumpcastle::Game game;
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "JumpCastle: fatal error: " << error.what() << '\n';
        return 1;
    }
}
```

- [ ] **Step 7: Wire targets and verify GREEN**

Set executable sources to `src/main.cpp`, `src/game.cpp`, and `src/renderer.cpp`. Add
`tests/resource_traits_test.cpp` to tests. Delete any temporary Task 1 stub and `source/main.c`.
Run:

```bash
cmake -S . -B build/cmake -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

Expected: executable and tests build; all tests pass; the two PNGs exist in the executable's
`assets` directory.

- [ ] **Step 8: Run a local smoke test**

Launch the executable from its generated output directory. Verify the starting screen and player
are visible, short and long charge jumps work, left/right direction changes work, `I` toggles the
overlay, Page Up/Page Down work only in debug mode, resizing keeps integer-scaled pixels, and
closing the window exits normally.

- [ ] **Step 9: Commit the playable C++ application**

```bash
git add CMakeLists.txt include/jumpcastle src tests/resource_traits_test.cpp source/main.c
git commit -m "ref(app): Port JumpCastle runtime to C++20"
```

---

### Task 6: Add CI, Documentation, License, and Repository Cleanup

**Files:**
- Create: `.github/workflows/ci.yml`
- Create: `LICENSE`
- Replace: `README.md`
- Modify: `.gitignore`
- Delete: `JumpCastle.filters`
- Delete: `JumpCastle.sln`
- Delete: `JumpCastle.user`
- Delete: `JumpCastle.vcxproj`
- Delete: `JumpCastle.vcxproj.user`
- Delete: `raylib_win64_msvc16/`
- Delete: `build/player.png~`
- Delete: `build/tileset.png~`

**Interfaces:**
- Produces: documented CMake workflows for contributors and users.
- Produces: three-platform CI verification.
- Produces: MIT licensing and final author identity.

- [ ] **Step 1: Add the CI matrix**

Create `.github/workflows/ci.yml`:

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

permissions:
  contents: read

jobs:
  build-and-test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Install Linux graphics dependencies
        if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
      - name: Configure
        run: cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release -DJUMPCASTLE_BUILD_TESTS=ON
      - name: Build
        run: cmake --build build/cmake --config Release --parallel
      - name: Test
        run: ctest --test-dir build/cmake -C Release --output-on-failure
```

- [ ] **Step 2: Add the MIT license**

Create `LICENSE` using the standard MIT License text beginning with:

```text
MIT License

Copyright (c) 2026 Thien Phu
```

and including the complete permission and warranty paragraphs without modification.

- [ ] **Step 3: Replace the README**

Write a UTF-8 Markdown README containing:

- project description and one screenshot placeholder only if an actual screenshot is added;
- author `Thien Phu (@thienphuprogrammer)`;
- controls table;
- prerequisites: CMake 3.24+, a C++20 compiler, Git, and platform graphics development packages;
- configure/build/run commands for Unix Makefiles/Ninja and Visual Studio;
- test command with `ctest --output-on-failure`;
- concise module tree matching the final repository; and
- MIT license link.

Do not claim CI is passing until the workflow has run on GitHub.

- [ ] **Step 4: Remove obsolete generated and Windows-only files**

Run explicit removals:

```bash
git rm JumpCastle.filters JumpCastle.sln JumpCastle.user JumpCastle.vcxproj JumpCastle.vcxproj.user
git rm -r raylib_win64_msvc16
git rm build/player.png~ build/tileset.png~
```

Confirm the only remaining runtime images are `assets/player.png` and `assets/tilemap.png`.

- [ ] **Step 5: Verify attribution and repository hygiene**

Run:

```bash
rg -n "Thien Phu|thienphuprogrammer" README.md LICENSE CMakeLists.txt src
git status --short
git diff --check
```

Expected: `rg` finds the current author metadata; no generated build output or `.codegraph` entry appears in
`git status`; `git diff --check` is clean.

- [ ] **Step 6: Perform the full clean verification**

Use a fresh build directory:

```bash
cmake -S . -B out/verify -DCMAKE_BUILD_TYPE=Release -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release --output-on-failure
```

Expected: configure, build, and all tests pass with no project warnings. Verify the executable
output contains `assets/player.png` and `assets/tilemap.png`.

- [ ] **Step 7: Refresh CodeGraph and inspect final call paths**

Run:

```bash
codegraph sync
codegraph explore "Game::run update_player resolve_tilemap_collision Renderer::draw"
codegraph callers update_player
```

Expected: CodeGraph indexes the new `.cpp/.hpp` files and reports the final flow from `Game` into
player simulation, collision resolution, and renderer without references to `source/main.c`.

- [ ] **Step 8: Commit repository polish**

```bash
git add .github .gitignore README.md LICENSE CMakeLists.txt assets include src tests
git add -u
git commit -m "meta: Complete JumpCastle C++ modernization"
```

---

## Final Review Checklist

- [ ] `git status --short` contains no unexpected files.
- [ ] `git diff main...HEAD --check` passes.
- [ ] A clean CMake configure and build passes.
- [ ] CTest passes every registered test.
- [ ] The game opens, finds assets, and preserves expected controls and visuals.
- [ ] Project source contains no `.c` application file and no vendored raylib binary.
- [ ] Legacy author credit is absent outside immutable Git history.
- [ ] README, window title, license, and package metadata identify Thien Phu.
- [ ] CodeGraph is synchronized and the final architecture matches this plan.
