# Jump King Ansimuz Re-theme Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-skin JumpCastle as a Jump King style medieval ascent using one cohesive Ansimuz CC0 asset set — an armoured hero across all twelve rooms and three ascent-zone biomes (forest, castle, frozen peak) — without changing gameplay, levels, or the reproducible-atlas architecture.

**Architecture:** Keep the existing CC0 pipeline (`build_assets.py` → committed atlases + `manifest.json` → `AssetCatalog` → renderer). Only the extraction inputs and generated art change. The hero reuses the feet-on-floor sprite registration from commit `97aca8f`. The `Biome` enum values are untouched; each slot simply points at new Ansimuz art.

**Tech Stack:** C++20, CMake 3.24+, raylib 5.5, Catch2 3.8.1, nlohmann/json 3.12.0, Python 3.10+, Pillow 12.3.0, CTest.

## Global Constraints

- Use only Ansimuz (Luis Zuno) packs and only sources whose itch page license reads **Creative Commons Zero (CC0-1.0)**; if a candidate is not CC0, substitute another CC0 pack of the same theme.
- Keep source archives in `assets/sources/downloads/` (gitignored); never stage a ZIP.
- Keep gameplay grid 16×12 at 16 px/tile; do not edit `assets/levels/*.level` or solver/physics.
- Keep `Biome` enum values `pixel_adventure`, `kenney`, `kings_and_pigs` (slots), and the six `PlayerAnimation` states `idle, run, charge, rise, fall, respawn`.
- Regenerated atlases must be byte-for-byte reproducible and pass `tools/verify_assets.py`.
- All existing tests must stay green (37/37), including reachability and `player_view`.
- Preserve C++20 portability (MSVC, AppleClang/Clang, GCC).

---

### Task 1: Acquire Ansimuz CC0 Archives and Record Sources

**Files:**
- Create: `assets/sources/gothicvania/SOURCE.md`
- Create: `assets/sources/sunnyland-forest/SOURCE.md`
- Create: `assets/sources/sunnyland-winter/SOURCE.md`
- Modify: `assets/sources/CC0-1.0.txt` (reuse existing legal text; no change if present)

**Interfaces:**
- Produces: three archives in `assets/sources/downloads/` and one `SOURCE.md` per pack consumed by Task 2.

- [ ] **Step 1: Download the three packs into ignored storage**

Download from itch.io (via "No thanks, just take me to the downloads"), saving into `assets/sources/downloads/`:
- Gothicvania (hero + castle tileset) — <https://ansimuz.itch.io/>
- SunnyLand Tall Forest Environment — <https://ansimuz.itch.io/>
- SunnyLand Winter Forest — <https://ansimuz.itch.io/>

- [ ] **Step 2: Confirm CC0 and inspect contents**

On each pack's itch page, confirm the License field says "Creative Commons Zero v1.0 Universal". Then:

```bash
unzip -l "assets/sources/downloads/<archive>.zip"
```

Record the exact archive filename, the hero animation sheet member paths (idle/run/ground-or-crouch/jump/fall/hurt-or-dead), and the terrain/background member paths. These exact names feed Task 2.

- [ ] **Step 3: Write one SOURCE.md per pack**

Each file records: creator (Luis Zuno / Ansimuz), official URL, `CC0-1.0`, retrieval date `2026-07-18`, archive filename, and the generated outputs it feeds. Mirror the format of the existing `assets/sources/*/SOURCE.md`.

- [ ] **Step 4: Commit source metadata (no archives)**

```bash
git status --short   # confirm no .zip is staged
git add assets/sources/gothicvania assets/sources/sunnyland-forest assets/sources/sunnyland-winter
git commit -m "docs(assets): Record Ansimuz CC0 source metadata"
```

---

### Task 2: Regenerate Atlases from the Ansimuz Packs

**Files:**
- Modify: `tools/build_assets.py`
- Modify: `assets/generated/manifest.json` (regenerated)
- Modify: `assets/generated/player.png`, `pixel-adventure.png`, `kenney.png`, `kings-and-pigs.png` (regenerated; filenames kept so `manifest.json`/renderer paths are stable)

**Interfaces:**
- Consumes: the three archives and member names recorded in Task 1.
- Produces: regenerated atlases + manifest consumed by `AssetCatalog` (unchanged loader) and `verify_assets.py`.

- [ ] **Step 1: Point the SOURCES table and extractors at the Ansimuz packs**

In `tools/build_assets.py`, update the `SOURCES` tuple ids/urls/archives to the three Ansimuz packs, and rewrite `pixel_adventure_atlas` (→ forest), `kenney_atlas` (→ castle), and `kings_atlas` (→ frozen) so each opens its pack's terrain sheet and special tiles by the exact member names from Task 1 Step 2. Keep each function's return shape (`build_biome_atlas(terrain, specials, path)`), the 7×5 terrain grid, and the four special regions (`spike`, `checkpoint`, `exit`, `background`).

- [ ] **Step 2: Point the hero extractor at the Gothicvania hero**

Rewrite `king_player_atlas` state→file mapping to the Gothicvania hero sheets (idle/run/crouch-or-ground/jump/fall/hurt-or-dead) using the exact member names and frame width/height from Task 1. Keep `KING_FRAME`, `union_box`, and `register_king_frames` unchanged so the hero is centred with feet on the cell floor (renderer anchor stays valid).

- [ ] **Step 3: Regenerate and prove reproducibility**

```bash
out/assets-venv/bin/python tools/build_assets.py --downloads assets/sources/downloads --output assets/generated --manifest assets/generated/manifest.json
shasum -a 256 assets/generated/* > out/a1.sha
out/assets-venv/bin/python tools/build_assets.py --downloads assets/sources/downloads --output assets/generated --manifest assets/generated/manifest.json
shasum -a 256 assets/generated/* > out/a2.sha
diff -u out/a1.sha out/a2.sha            # must be identical
python3 tools/verify_assets.py assets/generated/manifest.json   # exit 0
```

Expected: identical digests and `verify_assets.py` prints "verified 4 atlases, 6 King animations, and 3 CC0 sources".

- [ ] **Step 4: Confirm asset tests still pass**

```bash
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release -R "catalog|asset_manifest" --output-on-failure
```

Expected: catalog and `asset_manifest` tests PASS (frame counts read from the new manifest).

- [ ] **Step 5: Commit regenerated art**

```bash
git status --short   # no .zip staged
git add tools/build_assets.py assets/generated
git commit -m "feat(assets): Re-theme atlases to Ansimuz Jump King set"
```

---

### Task 3: Tune the Hero Render Scale and Verify Runtime

**Files:**
- Modify: `include/jumpcastle/player_view.hpp` (only if the hero's proportions need a different `player_sprite_display`)
- Test: `tests/player_view_test.cpp` (adjust the expected display size if the constant changes)

**Interfaces:**
- Consumes: regenerated `player.png` + manifest from Task 2.
- Produces: on-screen hero at correct scale, feet on ground, flip-symmetric.

- [ ] **Step 1: Decide the display size from the hero's cell**

Measure the hero content height within its 48 px cell:

```bash
out/assets-venv/bin/python - <<'PY'
from PIL import Image
img = Image.open('assets/generated/player.png').convert('RGBA')
b = img.crop((0, 0, 48, 48)).getbbox()
print('idle f0 content', b, 'height', b[3]-b[1])
PY
```

If the hero looks too large/small at `player_sprite_display = 3 * tile_pixels`, choose a new multiple (e.g. `2.5F * tile_pixels`) so the hero is roughly 1.5–2 tiles tall.

- [ ] **Step 2: Update the constant and its test together (only if changed)**

If you change `player_sprite_display`, update the `CHECK(dest.width == Catch::Approx(dest.height))` sizing expectations in `tests/player_view_test.cpp` to the new value, then:

```bash
cmake --build out/verify --config Release --parallel
ctest --test-dir out/verify -C Release --output-on-failure
```

Expected: 37/37 (or 37+ if a case was added) PASS.

- [ ] **Step 3: Runtime smoke test**

```bash
(cd out/verify && ./jumpcastle)
```

Verify: hero renders in rooms 01/05/09/12, feet sit on the ground, flipping left/right does not shift the body, and each biome shows its forest/castle/frozen terrain and background. Trigger a spike death to confirm the hurt/dead (`respawn`) animation plays.

- [ ] **Step 4: Commit any tuning**

```bash
git add include/jumpcastle/player_view.hpp tests/player_view_test.cpp
git commit -m "fix(rendering): Tune hero sprite scale for Ansimuz set"
```

---

## Final Review Checklist

- [ ] Three Ansimuz packs downloaded, each confirmed CC0-1.0; no ZIP tracked by git.
- [ ] `SOURCE.md` per pack records creator, URL, CC0, retrieval date, archive, outputs.
- [ ] Atlases regenerate byte-for-byte and `verify_assets.py` passes.
- [ ] Hero (armoured, Gothicvania) visible across all twelve rooms with all six states.
- [ ] Forest / castle / frozen terrain and backgrounds render per biome slot.
- [ ] Hero feet on ground, centred, flip-symmetric; scale ~1.5–2 tiles tall.
- [ ] All existing tests green (levels, reachability, campaign, player_view, assets).
- [ ] No changes to level geometry, solver, physics, or `Biome` enum values.
