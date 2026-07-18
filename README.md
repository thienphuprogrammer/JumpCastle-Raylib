# JumpCastle

JumpCastle is a vertical charge-jumping platformer built with C++20 and
[raylib](https://www.raylib.com/). Climb one continuous 18-screen castle across three visual
biomes using committed, no-air-control jumps. The full route is verified by a headless solver
running the same production physics as the game.

Created and maintained by [Thien Phu](https://github.com/thienphuprogrammer)
(`@thienphuprogrammer`).

## Campaign

- 18 fixed-camera screens in one uninterrupted 32 by 324 tile world.
- Three six-screen regions: Castle Courtyard, Frosted Keep, and Crown Spire.
- Missing a landing can drop the knight through every previous screen; there are no checkpoints.
- Reaching the goal on screen 18 completes the run and shows elapsed time and falls.
- Press Enter on the completion screen to restart the full climb.

The level solver accepts only routes below 86% charge and replays the resulting JSON trace through
production physics. See [Level Design](docs/LEVEL_DESIGN.md) for the format and guarantees.

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

The always-run `jumpcastle_assets` CMake target synchronizes `assets/generated/` and
`assets/levels/` beside the executable. Deleted runtime images are restored even when the C++
executable does not need relinking.

At startup the game searches for a valid asset root — a directory containing both
`generated/manifest.json` and `levels/campaign.level` — in this order: an explicit override, the
`assets/` directory beside the executable, then the installed data root. Point it at any tree with
`--asset-root PATH` (or `--asset-root=PATH`); the `JUMPCASTLE_ASSET_ROOT` environment variable is
used when the flag is absent. If no valid root is found, startup fails with every searched path
listed.

## Development and Verification

Tests are enabled by default when JumpCastle is configured as the top-level project:

```bash
cmake -S . -B build/cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DJUMPCASTLE_BUILD_TESTS=ON
cmake --build build/cmake --parallel
ctest --test-dir build/cmake --output-on-failure
```

The suite covers parsing, collision, committed-jump physics, continuous falls, completion, asset
integrity, fixed camera bands, and the full campaign. Run the solver directly for a human-readable
route or a JSON trace:

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
  --selection assets/source-selection.json \
  --output assets/generated \
  --manifest assets/generated/manifest.json
python3 tools/verify_assets.py assets/generated/manifest.json
out/assets-venv/bin/python tools/verify_asset_pixels.py assets/generated/manifest.json
```

## Art Sources

All third-party art is CC0 1.0 and may be remixed or used commercially without attribution. Source
metadata and the legal text are stored under `assets/sources/`.

- [Pixel Art Castle Tileset by rubberduck](https://opengameart.org/content/pixel-art-castle-tileset)
- [Gloomy Knight by loveOS](https://loveosstudio.itch.io/gloomy-knight-16x16)
- [UI Pack - Pixel Adventure by Kenney](https://kenney.nl/assets/ui-pack-pixel-adventure)

The playable character is the Gloomy Knight. These credits document the actual third-party art;
JumpCastle game and source-code authorship belongs to Thien Phu (`@thienphuprogrammer`).

## Project Structure

```text
.
├── assets/
│   ├── generated/                # Deterministic runtime atlases and manifest
│   ├── levels/                   # Continuous 18-screen campaign and legacy room archive
│   └── sources/                  # CC0 provenance; downloaded ZIPs are ignored
├── include/jumpcastle/           # Public C++ module interfaces
├── src/                          # Core simulation, solver, rendering, and game loop
├── tests/                        # Catch2 and manifest verification tests
├── tools/                        # Reproducible asset build and verification scripts
└── CMakeLists.txt
```

`jumpcastle_core` contains world parsing, campaign state, collision, committed-jump simulation,
asset catalog, replay, and reachability solver. The `jumpcastle` executable adds raylib resource
ownership, rendering, input, and the fixed-step game loop. Both the playable game and solver call
the same production physics functions.

## License

JumpCastle source code is available under the [MIT License](LICENSE). Third-party generated art is
covered by [CC0 1.0](assets/sources/CC0-1.0.txt) as documented above.
