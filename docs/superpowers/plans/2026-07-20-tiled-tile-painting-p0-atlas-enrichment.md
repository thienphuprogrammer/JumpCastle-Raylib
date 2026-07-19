# Tiled Tile Painting — P0: Enrich the Terrain Atlas — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans. This phase is VISUAL — several steps require looking at rendered images (Read the PNGs) and iterating; treat "pick clean cells" and "verify the atlas" as real work with a self-check loop, not one-shot.

**Goal:** Expand `assets/generated/castle.png` from ~14 unique tiles/biome to ~24–30 clean orthogonal tiles/biome (9-slice + inner corners + platforms + hazard + decor), rebuilt deterministically by the existing pipeline, and update every consumer of the atlas geometry in lockstep.

**Architecture:** Enrichment flows through the existing pipeline: expand `assets/source-selection.json` (more named slots per biome, pointing at clean orthogonal source cells) and redesign the layout in `tools/build_assets.py` to place them, then regenerate `castle.png` + `manifest.json`. The atlas grows from 21×6 (336×96) to **24×8 (384×128)**, so `castle.tsx`, the converter's atlas-width constant, and the P3 tests that hardcode `columns=21` must all move to the new geometry together.

**Tech Stack:** Python 3 + PIL via `.venv/bin/python`; C++ game (AssetCatalog reads `manifest.json`); Catch2 + pytest.

## Global Constraints

- **New atlas geometry: 24 columns × 8 rows = 384×128 px, tile 16.** Three biomes side-by-side, each an **8-col × 8-row** region (courtyard cols 0–7, frosted_keep cols 8–15, crown_spire cols 16–23).
- **Keep the manifest contract the game already relies on:** each biome still exposes a `terrain_grid` (x = biome_index·8·16, y=0, tile_size=16, columns=8, rows=8) whose **top-left 3×3 (cols 0–2, rows 0–2) is the 9-slice** (so `terrain_fill_region` = cell (1,1) still returns a solid fill), plus the `spike`, `checkpoint`, `exit`, `background` regions. Adding NEW named regions is fine; removing/renaming the existing four is NOT.
- **Orthogonal, tileable cells only.** Sources per biome (all under `assets/sources/downloads/`):
  - `courtyard`: keep the currently-working clean brick 9-slice cells from `castle_tileset_part1.png` if they read clean; ADD stone variants + wooden **platform** tiles from `castle_tileset_part3.png` (clean stone block is bottom-right; wooden platform beams are right side) and a **spike/hazard** from `castle_tileset_part2.png` (horizontal spike strips). Do NOT use part1's isometric diamond cells.
  - `frosted_keep`: `winter_tileset.png` (already a full 3×3 body + corners + pillars; add a platform/ledge + a hazard if a clean one exists).
  - `crown_spire`: `gothic_tileset.png` (ground + edges; ADD real inner-corner variants + a platform; borrow a spike from part2 if gothic has none).
- **Determinism:** `build_assets.py` output must be byte-identical on re-run.
- Run pytest via `.venv/bin/python -m pytest`; C++ via `cmake --build build/cmake` (tests default OFF — configure `-DJUMPCASTLE_BUILD_TESTS=ON`); `rtk` may cache — bypass with `rtk proxy`/`env` or logs.
- Commits allowed on `feat/tiled-tile-painting`. Conventional Commits. NO "Co-Authored-By" trailer. No push.

### Regeneration command (exact)
```
.venv/bin/python tools/build_assets.py \
  --downloads assets/sources/downloads \
  --selection assets/source-selection.json \
  --output assets/generated \
  --manifest assets/generated/manifest.json
```

---

## File Structure

- `tools/slice_preview.py` — NEW throwaway helper: given a source PNG + optional region, write a 3× upscaled copy with a labeled 16px grid (col,row numbers) so exact cell coordinates can be read by eye.
- `assets/source-selection.json` — expand: repoint courtyard off isometric part1 where needed; add named slots per biome.
- `tools/build_assets.py` — redesign `terrain_layout()` + `build_castle` for the 24×8 geometry; place the new tiles; record every named region in the manifest.
- `assets/generated/castle.png`, `assets/generated/manifest.json` — REGENERATED (committed).
- `assets/levels/tiled/castle.tsx` — update to `columns=24 tilecount=192`, image `width=384 height=128`.
- `tools/tiled_convert.py` — `TILESET_IMAGE_WIDTH = 384`, `TILESET_IMAGE_HEIGHT = 128`.
- `tests/test_tiled_convert.py` — update the fixtures/asserts that hardcode `columns=21` → `24`, and the `castle.tsx` asserts → new dims.
- (P1 `ScreenMap.tileset_columns{21}` default may stay — it is only a fallback; the converter/renderer derive the real value. Leave it or bump to 24 for tidiness; either passes.)

---

### Task 1: Slice-preview helper (to pick exact coordinates)

- [ ] **Step 1: Write `tools/slice_preview.py`** — a small argparse script: `--src PATH [--x --y --w --h] --out PATH --scale 3`. Loads the source (PIL, RGBA), optionally crops the region, scales ×`scale` NEAREST, draws grid lines every `16*scale` px and a small col/row index label in each cell (use PIL ImageDraw; a default bitmap font is fine). Save to `--out`.
- [ ] **Step 2: Generate labeled grids** for the regions you will pull from:
  - `castle_tileset_part3.png` full (locate the stone block + wooden platforms; note their col,row).
  - `castle_tileset_part2.png` region with the horizontal spike strips.
  - `winter_tileset.png` full, `gothic_tileset.png` full.
  Write each to the scratchpad and **Read (view) them** to record exact cell coordinates.
- [ ] **Step 3: Commit the helper** (it documents how the coords were chosen).
```bash
git add tools/slice_preview.py
git commit -m "chore(assets): add slice_preview helper for tile coord picking"
```

---

### Task 2: Expand `source-selection.json`

- [ ] **Step 1:** For EACH biome, define named slots (use the coords picked in Task 1). Minimum set per biome (≈24 tiles):
  - 9-slice: `top_left, top, top_right, left, center, right, bottom_left, bottom, bottom_right`
  - inner corners: `inner_corner_tl, inner_corner_tr, inner_corner_bl, inner_corner_br` (REAL distinct cells — fix courtyard's current degenerate/duplicate mapping)
  - `isolated` (single free-standing block)
  - platforms: `platform_left, platform_mid, platform_right`
  - `ledge` (thin one-tile top)
  - `pillar`
  - decor: `detail_1, detail_2` (crack / moss / banner-ish, biome-appropriate)
  - `hazard` (spikes)
  Each slot = `{"file": "<sheet>.png", "x": .., "y": .., "width": 16, "height": 16}`. Where a biome genuinely lacks a clean cell for a slot, reuse the nearest sensible clean cell rather than an isometric/transparent one (build_assets rejects transparent crops).
- [ ] **Step 2:** Keep the existing `props` block (torch/banner/crown) working — the special regions depend on it.
- [ ] **Step 3: Validate JSON parses** and every referenced file exists.
- [ ] **Step 4: Commit.**
```bash
git add assets/source-selection.json
git commit -m "feat(assets): expand source-selection with per-biome tile set"
```

---

### Task 3: Redesign `build_assets.py` layout for 24×8

- [ ] **Step 1:** Set `TERRAIN_COLUMNS = 8`, add rows for the new tiles (biome region is 8×8). Rewrite `terrain_layout()` to a deterministic 8×8 arrangement placing the 9-slice at rows 0–2 / cols 0–2, inner corners, platforms, ledge, pillar, isolated, details, and hazard at defined cells. Rewrite `build_castle` so `atlas = Image.new(RGBA, (3*8*16, 8*16))` = 384×128, composites each named slot, and records EACH named region's atlas pixel rect in the manifest under the biome (in addition to `terrain_grid`).
- [ ] **Step 2:** Keep `spike`/`checkpoint`/`exit`/`background` special regions in the manifest (the game reads them). `spike` may now point at the real `hazard` tile.
- [ ] **Step 3: Regenerate** with the exact command above. Then run it AGAIN and confirm byte-identical (`git status` shows the two generated files changed once, and a second run leaves them unchanged).
- [ ] **Step 4: View `assets/generated/castle.png`** — confirm all three biomes read as clean orthogonal tiles (no isometric diamonds, no transparent holes, 9-slice tiles align). Iterate on coords in source-selection.json until it looks right.
- [ ] **Step 5: Commit** the pipeline + regenerated assets.
```bash
git add tools/build_assets.py assets/generated/castle.png assets/generated/manifest.json
git commit -m "feat(assets): enrich terrain atlas to 24x8 (24 tiles/biome)"
```

---

### Task 4: Move every geometry consumer to 24×8

- [ ] **Step 1: `assets/levels/tiled/castle.tsx`** → `columns="24" tilecount="192"`, `<image ... width="384" height="128"/>`.
- [ ] **Step 2: `tools/tiled_convert.py`** → `TILESET_IMAGE_WIDTH = 384`, `TILESET_IMAGE_HEIGHT = 128` (so `tiled_to_screen_map` writes `columns = 384//16 = 24`).
- [ ] **Step 3: `tests/test_tiled_convert.py`** → update `test_castle_tsx_matches_the_atlas` (columns 24, tilecount 192, width 384, height 128) and any test asserting `columns == 21` / `tile_size` fixtures → `24`. Do NOT change collider/entity/geometry assertions.
- [ ] **Step 4: Run pytest** — `.venv/bin/python -m pytest tests/test_tiled_convert.py -q` → all green.
- [ ] **Step 5: Build + run the game** to prove AssetCatalog still loads the new manifest/atlas:
  - `cmake --build build/cmake --target jumpcastle jumpcastle_tests`
  - `./build/cmake/jumpcastle_tests` → C++ suite green (82).
  - Run the headless smoke capture (grep `src/main.cpp` for the flag) and Read one output PNG → the game renders with the new atlas, no crash, biomes look right.
- [ ] **Step 6: Commit.**
```bash
git add assets/levels/tiled/castle.tsx tools/tiled_convert.py tests/test_tiled_convert.py
git commit -m "feat(assets): move castle.tsx + converter + tests to 24x8 atlas geometry"
```

---

## Self-Review

**Spec coverage (design spec §5.1 + §11.7):**
- ~24–30 clean ortho tiles/biome via pipeline (not hand-drawn) → Tasks 2, 3. ✅
- Courtyard repointed off isometric part1; real inner corners → Task 2. ✅
- Deterministic regenerate + game still loads (manifest contract kept) → Task 3, Task 4. ✅
- Geometry ripple (castle.tsx, converter constant, P3 tests) handled in lockstep → Task 4. ✅

**Placeholder note:** exact per-slot coordinates are intentionally NOT in this plan — they are read visually from the Task 1 labeled grids at execution time (asset curation is inherently visual). The slot NAMES, sources, geometry, and all consumer updates are exact.

**Risk:** if a biome lacks enough clean cells for every slot, cap that biome (reuse nearest clean cell) rather than shipping bad tiles — log which slots were reused.

---

## Execution Handoff

Subagent-driven: dispatch ONE capable implementer for the whole phase (it must view images and iterate); it self-verifies by viewing the regenerated atlas and running both test suites, then returns the atlas + status for a final controller review. Commits per task on `feat/tiled-tile-painting`.
