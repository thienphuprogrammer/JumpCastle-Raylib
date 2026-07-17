# JumpCastle

JumpCastle is a vertical charge-jumping platformer built with C++20 and
[raylib](https://www.raylib.com/). Climb a twelve-room castle across three visual biomes, activate
checkpoints, avoid spikes, and reach the crown. Every committed room is verified by a headless
solver running the same production physics as the game.

Created and maintained by [Thien Phu](https://github.com/thienphuprogrammer)
(`@thienphuprogrammer`).

## Campaign

- 12 hand-authored rooms in three four-room regions: garden, clockworks, and royal castle.
- Checkpoints in rooms 1, 5, and 9 become the next respawn position.
- Spike contact respawns the King and increments the death counter.
- The exit in room 12 completes the run and shows elapsed time and deaths.
- Press Enter on the completion screen to restart the full campaign.

The level solver accepts only routes below 85% charge and replays each jump with timing and launch
position perturbations. See [Level Design](docs/LEVEL_DESIGN.md) for the format and guarantees.

## Controls

| Action | Input |
| --- | --- |
| Aim left | `A` or Left Arrow |
| Aim right | `D` or Right Arrow |
| Charge and release a jump | Hold, then release Space |
| Restart after completion | Enter |
| Toggle debug overlay | `I` |
| Move one room in debug mode | Page Up / Page Down |
| Exit | Close the window |

Holding Space longer increases jump strength. Choose left or right while releasing Space to set the
launch direction.

## Requirements

- CMake 3.24 or newer
- Git
- A C++20 compiler:
  - Visual Studio 2022 on Windows
  - AppleClang/Xcode Command Line Tools on macOS
  - GCC or Clang on Linux

CMake downloads raylib 5.5, Catch2 3.8.1, and nlohmann/json 3.12.0. Linux also needs raylib's
standard X11, OpenGL, and ALSA development packages.

For Ubuntu/Debian:

```bash
sudo apt-get install build-essential cmake git \
  libasound2-dev libgl1-mesa-dev libglu1-mesa-dev \
  libx11-dev libxcursor-dev libxi-dev libxinerama-dev libxrandr-dev
```

## Build and Run

### macOS and Linux

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DJUMPCASTLE_BUILD_TESTS=OFF
cmake --build build/cmake --parallel
./build/cmake/jumpcastle
```

### Windows with Visual Studio

```powershell
cmake -S . -B build/cmake -G "Visual Studio 17 2022" -A x64 `
  -DJUMPCASTLE_BUILD_TESTS=OFF
cmake --build build/cmake --config Release --parallel
.\build\cmake\Release\jumpcastle.exe
```

CMake copies only `assets/generated/` and `assets/levels/` beside the executable, so the game can
run directly from its output directory without source archives or asset tooling.

## Development and Verification

Tests are enabled by default when JumpCastle is configured as the top-level project:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build build/cmake --parallel
ctest --test-dir build/cmake --output-on-failure
```

The suite covers parsing, collision, charge physics, spikes, checkpoints, completion, asset
integrity, every room's tolerant route, and the full campaign. Run the solver directly for a
human-readable route or a JSON trace:

```bash
./build/cmake/jumpcastle_level_solver \
  --levels assets/levels \
  --campaign \
  --trace build/campaign-route.json
```

### Rebuild generated atlases

Official ZIP archives belong in the ignored `assets/sources/downloads/` directory. Runtime PNGs
and their manifest are deterministic and committed, so Pillow is not required for a normal build.

```bash
python3 -m venv out/assets-venv
out/assets-venv/bin/pip install -r tools/requirements-assets.txt
out/assets-venv/bin/python tools/build_assets.py \
  --downloads assets/sources/downloads \
  --output assets/generated \
  --manifest assets/generated/manifest.json
python3 tools/verify_assets.py assets/generated/manifest.json
```

## Art Sources

All third-party art is CC0 1.0 and may be remixed or used commercially without attribution. Source
metadata and the legal text are stored under `assets/sources/`.

- [Pixel Adventure by Pixel Frog](https://pixelfrog-assets.itch.io/pixel-adventure-1)
- [Pixel Platformer by Kenney](https://kenney.nl/assets/pixel-platformer)
- [Kings and Pigs by Pixel Frog](https://pixelfrog-assets.itch.io/kings-and-pigs)

The playable King is the King Human animation set from Kings and Pigs. Attribution above is kept as
project documentation even though CC0 does not require it.

## Project Structure

```text
.
├── assets/
│   ├── generated/                # Deterministic runtime atlases and manifest
│   ├── levels/                   # Twelve strict 16x12 room files
│   └── sources/                  # CC0 provenance; downloaded ZIPs are ignored
├── include/jumpcastle/           # Public C++ module interfaces
├── src/                          # Core simulation, solver, rendering, and game loop
├── tests/                        # Catch2 and manifest verification tests
├── tools/                        # Reproducible asset build and verification scripts
└── CMakeLists.txt
```

`jumpcastle_core` contains the level repository, campaign state, collision, player simulation,
asset catalog, and reachability solver. The `jumpcastle` executable adds raylib resource ownership,
rendering, input, and the game loop. Both the playable game and solver call the same
`simulate_step` function.

## License

JumpCastle source code is available under the [MIT License](LICENSE). Third-party generated art is
covered by [CC0 1.0](assets/sources/CC0-1.0.txt) as documented above.
