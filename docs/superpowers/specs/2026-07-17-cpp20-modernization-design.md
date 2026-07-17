# JumpCastle C++20 Modernization Design

## Objective

Modernize JumpCastle from a single-file, Windows-only C project into a maintainable C++20
raylib game owned and maintained by Thien Phu (`@thienphuprogrammer`). Preserve the existing
pixel-art presentation and charge-jump gameplay while improving portability, correctness,
testability, documentation, and repository hygiene.

## Project Identity

- Use `JumpCastle` as the product and CMake project name.
- Use `Thien Phu (@thienphuprogrammer)` for source headers, documentation, package metadata,
  and the game window title where an author is displayed.
- Replace the existing Team 3/F-Code credit in the working tree. Git history remains intact.
- Distribute the modernized project under the MIT License with copyright assigned to Thien Phu.

## Supported Toolchain

- C++ standard: C++20, with compiler extensions disabled.
- Build system: CMake 3.24 or newer.
- Platforms: Windows, macOS, and Linux.
- Compilers: MSVC, AppleClang/Clang, and GCC.
- Graphics/game library: raylib 5.5, pinned through CMake `FetchContent`.
- Test framework: Catch2 3.8.1, pinned through CMake `FetchContent` when tests are enabled.

The repository will not require users to install raylib or Catch2 globally. CMake will download
the pinned sources during configuration. Tests remain optional through
`JUMPCASTLE_BUILD_TESTS`, enabled by default only when this repository is the top-level project.

## Repository Structure

```text
JumpCastle-Raylib/
├── .github/workflows/ci.yml
├── assets/
│   ├── player.png
│   └── tilemap.png
├── include/jumpcastle/
│   ├── collision.hpp
│   ├── game.hpp
│   ├── game_config.hpp
│   ├── player.hpp
│   ├── renderer.hpp
│   └── tilemap.hpp
├── src/
│   ├── collision.cpp
│   ├── game.cpp
│   ├── main.cpp
│   ├── player.cpp
│   ├── renderer.cpp
│   └── tilemap.cpp
├── tests/
│   ├── collision_test.cpp
│   ├── player_test.cpp
│   └── tilemap_test.cpp
├── CMakeLists.txt
├── LICENSE
├── README.md
└── .gitignore
```

The old Visual Studio solution/project files and the vendored Windows raylib binaries and
headers will be removed. CMake remains the only supported build definition and can generate a
Visual Studio solution when requested with a Visual Studio generator.

## Architecture

All project code lives in the `jumpcastle` namespace. Each module has one responsibility and
communicates through explicit value types or narrow interfaces.

### Configuration

`game_config.hpp` owns compile-time gameplay and rendering constants: tile dimensions, logical
viewport size, gravity, jump strength, movement acceleration, player collider size, bounce
factor, and background color. Constants use typed `inline constexpr` declarations instead of
preprocessor macros.

### Tilemap and Level Selection

`tilemap.hpp/.cpp` defines a strongly typed `enum class Tile`, a fixed-size `Tilemap` backed by
`std::array`, and the built-in level screens. It exposes bounded tile lookup, solid-tile lookup,
and conversion from player world height to the active screen plus its vertical offset.

Screen selection becomes one tested operation returning both the tilemap index and screen
offset. Invalid heights return the sentinel empty screen without out-of-range array access.
The starting position must resolve to the screen marked as the starting screen.

### Player

`player.hpp/.cpp` owns `PlayerState`, `PlayerInput`, and player simulation. `PlayerState` uses
member initializers so position, velocity, animation time, jump charge, facing direction, and
grounded state are always deterministic.

The game translates raylib keyboard state into a `PlayerInput` value. Simulation consumes that
value rather than calling raylib input functions directly, allowing movement and charge-jump
behavior to be unit tested without opening a window.

### Collision

`collision.hpp/.cpp` owns tile overlap calculation, collision queries, and collision resolution.
It operates on a `Tilemap`, world-space box data, and velocity. It does not depend on window,
input, texture, or rendering state. The existing edge-aware resolution and horizontal bounce
behavior are retained, with explicit local/world coordinate conversion.

### Rendering and Resource Ownership

`renderer.hpp/.cpp` owns player/tile textures and the logical render target. Small move-only
RAII wrappers release each raylib resource exactly once. Copying resource wrappers is disabled.
The renderer draws the active tilemap and player to the fixed logical viewport, then scales it
to the resizable window using integer scaling and letterboxing.

Texture loading validates `IsTextureValid`. Missing or invalid assets produce a clear exception
that includes the attempted path. Asset paths are built from raylib's cross-platform application
directory plus the `assets` directory; no separator-specific parsing of `argv[0]` is used.

### Game and Entry Point

`game.hpp/.cpp` owns the `Game` class and coordinates initialization, input sampling, fixed
per-frame update order, active-screen selection, collision resolution, debug controls, and
rendering. The order remains:

1. Clamp frame delta.
2. Select the active screen.
3. Sample input and update player simulation.
4. Resolve player collision.
5. Apply debug-only screen movement.
6. Render the world, player, and optional debug overlay.

`main.cpp` constructs the window and `Game`, runs the loop, catches standard exceptions, logs a
clear fatal error, and returns a non-zero exit code. Window shutdown and graphics resource
cleanup happen deterministically through scope exit.

## Gameplay Compatibility

The modernization preserves:

- a 16 by 12 tile logical screen with 16-pixel tiles;
- integer-scaled pixel art and letterboxing;
- `A`/`D` and left/right arrow movement;
- holding and releasing Space to control jump strength;
- airborne gravity, horizontal wall bounce, and the existing speed cap;
- `I` to toggle debug information; and
- Page Up/Page Down screen movement while debug mode is active.

Changes to gameplay are limited to correcting undefined or clearly inconsistent behavior:

- initialize every player field before the first frame;
- guarantee that initial player coordinates select and render the intended starting screen;
- prevent screen index calculations from accessing or wrapping an invalid index; and
- clamp unusually large frame times before simulation.

## Build and Asset Flow

The root CMake project defines these targets:

- `jumpcastle_core`: tilemap, player, and collision logic;
- `jumpcastle`: executable containing the game loop and renderer; and
- `jumpcastle_tests`: Catch2-based unit tests when tests are enabled.

CMake copies `assets/` next to the executable after a successful build. Multi-config generators
copy into the active configuration directory. Install rules place the executable in `bin` and
assets in `bin/assets` so installed and build-tree execution share the same lookup convention.

Compiler warnings are enabled per toolchain (`/W4` for MSVC and `-Wall -Wextra -Wpedantic` for
GCC/Clang). Warnings are not promoted to errors because external compiler versions differ.

## Tests

Catch2 tests cover behavior rather than implementation details:

- tile lookup at valid coordinates and each boundary policy;
- world-height to screen selection, including initial, upper, lower, and invalid positions;
- overlapped-tile ranges for centered and boundary-crossing boxes;
- solid-tile collision detection;
- vertical landing and horizontal bounce resolution;
- deterministic default player state;
- charge-jump strength at short and long hold durations;
- left/right input effects and facing direction; and
- velocity capping and frame-delta-independent integration.

Rendering and OS window behavior are verified by successful application compilation rather than
headless unit tests. The test target links only the logic required by each behavior.

## Continuous Integration

GitHub Actions runs a build matrix on `ubuntu-latest`, `macos-latest`, and `windows-latest`. Each
job configures CMake in Release mode with tests enabled, builds the project, and runs CTest with
failure output enabled. Dependency caches are optional and will not be required for correctness.

## Documentation and Repository Hygiene

The README describes the game, controls, supported platforms, prerequisites, configure/build/run
commands, project structure, development tests, and author/license information. Commands include
single-config and Visual Studio multi-config examples.

`.gitignore` excludes CMake build directories, generated IDE files, compiler output, and local
CodeGraph data. Source assets remain tracked. No generated binaries or downloaded dependencies
remain committed.

## Success Criteria

The modernization is complete when:

1. The repository contains C++20 source only for application code.
2. CMake configures and builds the game without a preinstalled raylib.
3. CTest passes all logic tests.
4. CI defines successful Windows, macOS, and Linux verification.
5. The executable locates assets without platform-specific path parsing.
6. The original controls, visuals, level data, and charge-jump behavior remain recognizable.
7. No Team 3/F-Code credit remains in the working tree.
8. Documentation and license identify Thien Phu (`@thienphuprogrammer`) as the project author.

