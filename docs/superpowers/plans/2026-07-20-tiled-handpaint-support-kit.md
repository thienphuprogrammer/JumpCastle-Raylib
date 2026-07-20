# Tiled Hand-Paint Support Kit — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or executing-plans. This supports the USER hand-painting 17 screens in Tiled — deliverables are authoring aids, not gameplay code. Steps use `- [ ]`.

**Goal:** Give the user four aids so they can hand-paint diverse terrain in Tiled easily: (1) a Terrain Brush (Wang sets) in `castle.tsx` that autotiles edges/corners, (2) a labeled palette cheat-sheet image, (3) a per-screen collision-underlay image for all 18 screens, (4) an updated workflow doc.

**Do NOT touch:** `assets/levels/tiled/screen-17.tmj` (user's reference painting) or `screen-00.tmj` (user's WIP). No changes to collision, `.map.json`, or game code.

**Tech Stack:** Python 3 + PIL via `.venv/bin/python`; Tiled `.tsx` XML (Wang sets); atlas is `assets/generated/castle.png` 384×128, 24 cols, 3 biomes × 8×8 (courtyard x=0, frosted_keep x=128, crown_spire x=256). Per-biome named tile regions are in `assets/generated/manifest.json` under `atlases.castle.biomes.<biome>.regions` (each has `x,y,width,height` in atlas px).

## Global Constraints
- GID for a biome region: `local = (region.y // 16) * 24 + (region.x // 16)`; Tiled tileid = `local` (0-based); painting GID = `local + 1` (firstgid 1).
- Commit per task on a new branch `feat/tiled-handpaint-kit` off main (do NOT commit on main; the working tree has the user's uncommitted screen-00/17.tmj — leave them untouched and unstaged).
- Conventional Commits, NO "Co-Authored-By". No push. pytest/scripts via `.venv/bin/python`.

---

### Task 0: Branch
- [ ] `git checkout -b feat/tiled-handpaint-kit` (leaves the user's uncommitted .tmj edits in place, untouched).

---

### Task 1: Terrain Brush — corner-Wang sets in `castle.tsx`

**File:** `assets/levels/tiled/castle.tsx` (additive — append `<wangsets>` before `</tileset>`).

Add one **corner-type** wangset per biome, each with a single wangcolor "solid". For each biome, resolve the tileid of each named region from the manifest, then emit a `<wangtile tileid="..." wangid="T,TR,R,BR,B,BL,L,TL"/>` using this corner map (8 slots; only corner slots TR=idx1, BR=idx3, BL=idx5, TL=idx7 are set; edges T/R/B/L stay 0):

| region | wangid (T,TR,R,BR,B,BL,L,TL) | meaning (which corners are solid) |
|---|---|---|
| `center` | `0,1,0,1,0,1,0,1` | all 4 |
| `top` | `0,0,0,1,0,1,0,0` | BR,BL |
| `bottom` | `0,1,0,0,0,0,0,1` | TR,TL |
| `left` | `0,1,0,1,0,0,0,0` | TR,BR |
| `right` | `0,0,0,0,0,1,0,1` | BL,TL |
| `top_left` | `0,0,0,1,0,0,0,0` | BR |
| `top_right` | `0,0,0,0,0,1,0,0` | BL |
| `bottom_left` | `0,1,0,0,0,0,0,0` | TR |
| `bottom_right` | `0,0,0,0,0,0,0,1` | TL |
| `inner_corner_tl` | `0,1,0,1,0,1,0,0` | all but TL |
| `inner_corner_tr` | `0,0,0,1,0,1,0,1` | all but TR |
| `inner_corner_bl` | `0,1,0,1,0,0,0,1` | all but BL |
| `inner_corner_br` | `0,1,0,0,0,1,0,1` | all but BR |

Skip regions not in this table (platforms/decor/hazard aren't part of the terrain fill — they stay manual tiles).

- [ ] **Step 1:** Write a small script `tools/gen_wangsets.py` that reads the manifest (JSON), computes each biome's region tileids, builds the `<wangsets>` XML block as a STRING, and inserts it before `</tileset>` via plain text replace (do NOT XML-parse `castle.tsx` — it's a known trusted format; string insert avoids a defusedxml dependency and stdlib-XML lints). Idempotent: if a `<wangsets>` block already exists, replace it. Use the exact wangid table above. wangcolor colors: courtyard `#b06a3a`, frosted_keep `#8aa0c8`, crown_spire `#6a8a4a`.
- [ ] **Step 2:** Validate with text/grep (no XML parser): `grep -c '<wangset ' castle.tsx` == 3 and `grep -c '<wangtile ' castle.tsx` == 39, and the file still ends with `</tileset>` exactly once. Print the biome→tileid→region mapping for a human sanity check.
- [ ] **Step 3:** Commit: `feat(tiled): add per-biome terrain Wang sets to castle.tsx for autotiling`.

Note in the report: the terrain brush must be visually confirmed by the user in Tiled (headless validation only checks structure).

---

### Task 2: Labeled palette cheat-sheet

**File:** `tools/tiled_cheatsheet.py` (new), output `docs/tiled-guides/palette-cheatsheet.png`.

- [ ] Render `castle.png` scaled ×6 (nearest). For each biome (label the 3 columns "courtyard / frosted_keep / crown_spire"), overlay on each named-region cell: the short role name (e.g. `top_left`, `ledge`, `hazard`, `detail_1`) and its painting GID (`local+1`). Use legible outlined/contrasting text. Save the PNG.
- [ ] Run it; the script must also print the biome→region→GID table (text fallback).
- [ ] Commit: `docs(tiled): add labeled palette cheat-sheet + generator`.

---

### Task 3: Per-screen collision guides

**File:** `tools/tiled_collision_guide.py` (new), outputs `docs/tiled-guides/screen-NN-collision.png` for NN=00..17.

- [ ] For each `assets/levels/screens/screen-NN.map.json`: draw a `width*16 × height*16` image (dark bg), fill each collider polygon semi-transparent + outline by type (solid=grey, oneway=blue, hazard=red), and mark entities (spawn=green dot, checkpoint=cyan, goal=gold). Label the screen index + biome. This is the "paint-by-numbers" underlay showing where platforms are.
- [ ] Run for all 18; confirm 18 PNGs written.
- [ ] Commit: `docs(tiled): add per-screen collision underlay guides + generator`.

---

### Task 4: Workflow doc

**File:** `docs/tiled-workflow.md` (extend).

- [ ] Add a "Hand-painting a screen (step by step)" section:
  1. Open `assets/levels/tiled/screen-NN.tmj` in Tiled.
  2. Keep `docs/tiled-guides/palette-cheatsheet.png` + `docs/tiled-guides/screen-NN-collision.png` open as reference.
  3. Select the `terrain` layer. Use the **Terrain Brush** (the biome's Wang set) to drag platforms — Tiled auto-places edges/corners. Match the collision underlay so tiles sit on the real platforms.
  4. Add variety by hand: `detail_1/2`, `pillar`, `ledge`, `platform_*`, `hazard`/`spike` from the cheat-sheet.
  5. `.venv/bin/python tools/tiled_convert.py from-tiled --input-dir assets/levels/tiled --output-dir assets/levels/screens`
  6. Run the game to see it.
  7. Ask the assistant to review — it renders a preview + checks tiles vs collision.
  - Include the biome→screen map read from the `.map.json`: crown_spire = screens 00–05, frosted_keep = 06–11, courtyard = 12–17 (verify against the files).
- [ ] Commit: `docs(tiled): document the hand-painting workflow with the support kit`.

---

## Self-Review
- Wang sets: corner-type, 1 color/biome, 13 wangtiles each, tileids from manifest → Task 1. ✅
- Cheat-sheet + collision guides + doc → Tasks 2–4. ✅
- User's screen-00/17.tmj untouched; no game/collision/.map.json changes. ✅
- Verification is structural (headless) for the Wang set; the user confirms the brush feel in Tiled (noted).

## Execution Handoff
Subagent-driven: one implementer builds all four on `feat/tiled-handpaint-kit`, then the controller verifies (views the cheat-sheet + one collision guide, validates castle.tsx parses + wangtile counts). Then the user paints; assistant reviews per screen.
