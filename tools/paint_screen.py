#!/usr/bin/env python3
"""Deterministically repaint JumpCastle screens in the hand-painted screen-17 style.

Reads each screen's collision polygons + biome from
``assets/levels/screens/screen-NN.map.json``, classifies every solid cell by
its 8-neighborhood into a visual role (fill / edge / bridge / column / hazard),
and writes a chunky carved-stone ``tiles.terrain`` GID grid back into the
``.map.json`` -- then refreshes the matching ``.tmj`` via ``tiled_convert``.

The look reproduces screen-17's grammar (see
``docs/superpowers/specs/2026-07-20-level-redesign-17-style-design.md`` §5):
thick solid masses instead of thin floating planks. Screen-17 itself is never
repainted unless ``--include-17`` is given (§7).

Usage::

    python3 tools/paint_screen.py --screens 12-16              # paint into .map.json
                                 [--preview docs/tiled-guides/previews]
                                 [--check]                     # validate only
                                 [--include-17]                # allow repainting 17

Determinism: no RNG. Accent placement is a fixed hash of (screen, x, y), so
re-running the painter is byte-stable (acceptance §4).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
SCREENS_DIR = REPO_ROOT / "assets" / "levels" / "screens"
TILED_DIR = REPO_ROOT / "assets" / "levels" / "tiled"
MANIFEST_PATH = REPO_ROOT / "assets" / "generated" / "manifest.json"

ATLAS_COLUMNS = 24
ATLAS_TILE_SIZE = 16
FIRSTGID = 1  # every generated .tmj references the tileset at firstgid 1

SOLID_COLLIDER_TYPES = ("solid", "oneway")
HAZARD_COLLIDER_TYPE = "hazard"

REFERENCE_SCREEN = 17  # the user's hand-painted reference; never repaint by default

# --- roles (structural classification of a solid cell) ----------------------
ROLE_FILL = "fill"
ROLE_EDGE = "edge"
ROLE_BRIDGE = "bridge"
ROLE_COLUMN = "column"
ROLE_HAZARD = "hazard"
ROLE_SLOPE = "slope"

SLOPE_SHAPE = "slope"  # collider `shape` tag for diagonal (triangular) terrain
# Optional diagonal atlas regions (present after the atlas gains slope tiles).
SLOPE_REGIONS = ("slope_ne", "slope_nw", "slope_se", "slope_sw")

# --- courtyard 17-palette (local ids -> GIDs via +FIRSTGID) -----------------
# Measured from the user's screen-17: fill 191, column 161, accents 92 & 114.
FILL_GID = 191 + FIRSTGID       # 192
COLUMN_GID = 161 + FIRSTGID     # 162
ACCENT_GIDS = (92 + FIRSTGID, 114 + FIRSTGID)  # (93, 115)

MAX_ACCENTS = 12                # spec §5: <= 12 accents per screen
ACCENT_PERIOD = 37              # hash modulus tuning accent density
# The 17-style spec targeted 35-55% density, but the Jump-King fall-shaft
# redesign deliberately trades density for open drop-lanes (feel-first), so
# shaft-heavy screens legitimately sit as low as ~26%. COVERAGE_MIN is now a
# "not accidentally empty" sanity floor, not the old style target; the upper
# bound still guards against a screen filling in solid.
COVERAGE_MIN = 0.20
COVERAGE_MAX = 0.55
ENTITY_SUPPORT_TOL = 0.13       # matches solver support tolerance (src/solver.cpp)

# Named atlas regions the painter resolves per biome.
REQUIRED_REGIONS = (
    "top_left", "top", "top_right",
    "left", "center", "right",
    "bottom_left", "bottom", "bottom_right",
    "inner_corner_tl", "inner_corner_tr", "inner_corner_bl", "inner_corner_br",
    "isolated", "pillar", "platform_left", "platform_mid", "platform_right",
    "detail_1", "detail_2", "hazard",
)

BoolGrid = list[list[bool]]
Grid = list[list[int]]


# --- atlas / manifest -------------------------------------------------------

def region_gid(region: dict[str, Any]) -> int:
    """Tiled GID (1-based) for an atlas region, from its top-left pixel."""
    col = region["x"] // ATLAS_TILE_SIZE
    row = region["y"] // ATLAS_TILE_SIZE
    return row * ATLAS_COLUMNS + col + FIRSTGID


def load_manifest() -> dict[str, Any]:
    return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))


def load_biome_gids(biome: str, manifest: dict[str, Any] | None = None) -> dict[str, int]:
    """Resolve every required named region to a GID for one biome."""
    manifest = manifest or load_manifest()
    biomes = manifest["atlases"]["castle"]["biomes"]
    if biome not in biomes:
        raise KeyError(f"biome {biome!r} not found in manifest (have {sorted(biomes)})")
    regions = biomes[biome]["regions"]
    missing = [name for name in REQUIRED_REGIONS if name not in regions]
    if missing:
        raise KeyError(f"biome {biome!r} manifest missing regions: {missing}")
    gids = {name: region_gid(regions[name]) for name in REQUIRED_REGIONS}
    # Slope tiles are optional: present only after the atlas has been rebuilt
    # (or patched) with the diagonal regions. Absent -> ROLE_SLOPE falls back.
    for name in SLOPE_REGIONS:
        if name in regions:
            gids[name] = region_gid(regions[name])
    return gids


# --- rasterization ----------------------------------------------------------

def point_in_polygon(x: float, y: float, points: list[list[float]]) -> bool:
    """Even-odd ray-casting point-in-polygon test (points in tile units)."""
    inside = False
    n = len(points)
    j = n - 1
    for i in range(n):
        xi, yi = points[i]
        xj, yj = points[j]
        if (yi > y) != (yj > y):
            x_intersect = (xj - xi) * (y - yi) / (yj - yi) + xi
            if x < x_intersect:
                inside = not inside
        j = i
    return inside


def rasterize_colliders(
    colliders: list[dict[str, Any]], width: int, height: int
) -> tuple[BoolGrid, BoolGrid]:
    """Rasterize collider polygons onto an H-row x W-col cell grid.

    A cell is set when its center (c+0.5, r+0.5) falls inside any collider
    polygon of the matching kind. Returns (solid_grid, hazard_grid).
    """
    solid_polys = [c["points"] for c in colliders if c.get("type") in SOLID_COLLIDER_TYPES]
    hazard_polys = [c["points"] for c in colliders if c.get("type") == HAZARD_COLLIDER_TYPE]

    solid = [[False] * width for _ in range(height)]
    hazard = [[False] * width for _ in range(height)]
    for r in range(height):
        cy = r + 0.5
        for c in range(width):
            cx = c + 0.5
            if any(point_in_polygon(cx, cy, poly) for poly in solid_polys):
                solid[r][c] = True
            if any(point_in_polygon(cx, cy, poly) for poly in hazard_polys):
                hazard[r][c] = True
    return solid, hazard


def slope_cells(
    colliders: list[dict[str, Any]], width: int, height: int
) -> BoolGrid:
    """Cells whose center falls inside a solid collider tagged ``shape:"slope"``.

    These are the diagonal-terrain cells; the painter gives their walkable
    top a distinct slope tile (spec 2026-07-23-sloped-terrain-design.md §5).
    """
    slope_polys = [
        c["points"] for c in colliders
        if c.get("type") in SOLID_COLLIDER_TYPES and c.get("shape") == SLOPE_SHAPE
    ]
    grid = [[False] * width for _ in range(height)]
    for r in range(height):
        cy = r + 0.5
        for c in range(width):
            cx = c + 0.5
            if any(point_in_polygon(cx, cy, poly) for poly in slope_polys):
                grid[r][c] = True
    return grid


def _is_solid(solid: BoolGrid, width: int, height: int, r: int, c: int) -> bool:
    """Out-of-bounds counts as open (not-solid), per the autotiling model."""
    if r < 0 or r >= height or c < 0 or c >= width:
        return False
    return solid[r][c]


# --- classification (spec §5) ----------------------------------------------

def classify_cell(
    solid: BoolGrid, hazard: BoolGrid, width: int, height: int, r: int, c: int,
    slope: BoolGrid | None = None,
) -> str:
    """Structural role of the solid cell (r, c) from its 4 orthogonal neighbors.

    When a ``slope`` grid is supplied, a slope cell whose top is open air is a
    walkable incline surface and classifies as ``ROLE_SLOPE``.
    """
    if hazard[r][c]:
        return ROLE_HAZARD

    open_up = not _is_solid(solid, width, height, r - 1, c)
    open_down = not _is_solid(solid, width, height, r + 1, c)
    open_left = not _is_solid(solid, width, height, r, c - 1)
    open_right = not _is_solid(solid, width, height, r, c + 1)

    if slope is not None and slope[r][c] and open_up:
        return ROLE_SLOPE

    horiz_thin = open_up and open_down     # nothing above or below -> a plank
    vert_thin = open_left and open_right   # nothing left or right -> a stack

    if horiz_thin:
        # 1-tile-thick horizontal run (incl. isolated single cell) -> bridge.
        return ROLE_BRIDGE
    if vert_thin:
        # 1-wide vertical stack -> column.
        return ROLE_COLUMN
    if open_up or open_down or open_left or open_right:
        return ROLE_EDGE
    return ROLE_FILL


def _edge_region(
    solid: BoolGrid, width: int, height: int, r: int, c: int
) -> str:
    """9-slice region name for a boundary (edge) cell."""
    open_up = not _is_solid(solid, width, height, r - 1, c)
    open_down = not _is_solid(solid, width, height, r + 1, c)
    open_left = not _is_solid(solid, width, height, r, c - 1)
    open_right = not _is_solid(solid, width, height, r, c + 1)

    if open_up and open_left:
        return "top_left"
    if open_up and open_right:
        return "top_right"
    if open_down and open_left:
        return "bottom_left"
    if open_down and open_right:
        return "bottom_right"
    if open_up:
        return "top"
    if open_down:
        return "bottom"
    if open_left:
        return "left"
    if open_right:
        return "right"

    # Fully enclosed orthogonally but a diagonal is open -> concave corner.
    if not _is_solid(solid, width, height, r - 1, c - 1):
        return "inner_corner_tl"
    if not _is_solid(solid, width, height, r - 1, c + 1):
        return "inner_corner_tr"
    if not _is_solid(solid, width, height, r + 1, c - 1):
        return "inner_corner_bl"
    return "inner_corner_br"


def _bridge_region(
    solid: BoolGrid, width: int, height: int, r: int, c: int
) -> str:
    """platform_left / _mid / _right for a horizontal 1-thick plank cell."""
    open_left = not _is_solid(solid, width, height, r, c - 1)
    open_right = not _is_solid(solid, width, height, r, c + 1)
    if open_left and not open_right:
        return "platform_left"
    if open_right and not open_left:
        return "platform_right"
    return "platform_mid"


# --- accents ----------------------------------------------------------------

def _accent_hash(screen_index: int, c: int, r: int) -> int:
    """Fixed, RNG-free hash of a cell for deterministic accent placement."""
    h = (screen_index & 0xFFFF) * 2654435761
    h ^= (c & 0xFFFF) * 73856093
    h ^= (r & 0xFFFF) * 19349663
    return h & 0xFFFFFFFF


def _accent_gid(screen_index: int, c: int, r: int) -> int:
    return ACCENT_GIDS[_accent_hash(screen_index, c, r) % len(ACCENT_GIDS)]


# --- painting ---------------------------------------------------------------

def _gid_for_role(
    role: str, biome: str, gids: dict[str, int],
    solid: BoolGrid, width: int, height: int, r: int, c: int,
    edge_mapping: dict[str, int] | None,
) -> int:
    if role == ROLE_HAZARD:
        return gids["hazard"]
    if role == ROLE_COLUMN:
        return COLUMN_GID if biome == "courtyard" else gids["pillar"]
    if role == ROLE_BRIDGE:
        return gids[_bridge_region(solid, width, height, r, c)]
    if role == ROLE_SLOPE:
        # Demonstrative slopes are all NE-rising right triangles (place_slopes),
        # so render the NE diagonal when the atlas provides it; else fall back to
        # the flat-top tile (stair-stepped) until the atlas gains slope art.
        return gids.get("slope_ne", gids.get("slope", gids["top"]))
    if role == ROLE_FILL:
        return FILL_GID if biome == "courtyard" else gids["center"]
    # ROLE_EDGE
    region = _edge_region(solid, width, height, r, c)
    if edge_mapping and region in edge_mapping:
        return edge_mapping[region]
    return gids[region]


def paint_grid(
    screen: dict[str, Any], index: int, *,
    edge_mapping: dict[str, int] | None = None,
    biome_gids: dict[str, int] | None = None,
) -> Grid:
    """Build the H x W GID grid (0 = empty) for one screen. Deterministic."""
    width = int(screen["screen"]["width"])
    height = int(screen["screen"]["height"])
    biome = screen.get("biome", "courtyard")
    gids = biome_gids if biome_gids is not None else load_biome_gids(biome)

    colliders = screen.get("colliders", [])
    solid, hazard = rasterize_colliders(colliders, width, height)
    slope = slope_cells(colliders, width, height)

    grid: Grid = [[0] * width for _ in range(height)]
    fill_cells: list[tuple[int, int]] = []
    for r in range(height):
        for c in range(width):
            # paint solid cells AND standalone hazard cells (a hazard collider
            # in open air is not solid but still needs a hazard tile).
            if not solid[r][c] and not hazard[r][c]:
                continue
            role = classify_cell(solid, hazard, width, height, r, c, slope=slope)
            grid[r][c] = _gid_for_role(
                role, biome, gids, solid, width, height, r, c, edge_mapping
            )
            if role == ROLE_FILL:
                fill_cells.append((r, c))

    _apply_accents(grid, index, fill_cells, biome, gids)
    return grid


def _apply_accents(
    grid: Grid, index: int, fill_cells: list[tuple[int, int]],
    biome: str, gids: dict[str, int],
) -> None:
    """Sprinkle <= MAX_ACCENTS deterministic accents over interior fill cells."""
    chosen = [
        (r, c) for (r, c) in fill_cells
        if _accent_hash(index, c, r) % ACCENT_PERIOD == 0
    ]
    chosen = chosen[:MAX_ACCENTS]
    for (r, c) in chosen:
        if biome == "courtyard":
            grid[r][c] = _accent_gid(index, c, r)
        else:
            detail = "detail_1" if _accent_hash(index, c, r) % 2 == 0 else "detail_2"
            grid[r][c] = gids[detail]


# --- learning screen-17's edge mapping (spec §5) ----------------------------

def learn_edge_mapping(screen17: dict[str, Any]) -> dict[str, int]:
    """Learn region-name -> GID from screen-17's own painted grid.

    For each boundary (edge) cell in 17's collision, record which GID the user
    actually painted there, keyed by the 9-slice region the cell resolves to.
    The most frequent GID per region wins. Batch-1 courtyard screens reuse this
    so their edges are pixel-faithful to 17 instead of guessed.
    """
    width = int(screen17["screen"]["width"])
    height = int(screen17["screen"]["height"])
    terrain = screen17["tiles"]["terrain"]
    solid, hazard = rasterize_colliders(screen17.get("colliders", []), width, height)

    votes: dict[str, dict[int, int]] = {}
    for r in range(height):
        for c in range(width):
            if not solid[r][c] or hazard[r][c]:
                continue
            if classify_cell(solid, hazard, width, height, r, c) != ROLE_EDGE:
                continue
            gid = terrain[r][c]
            if not gid:
                continue
            region = _edge_region(solid, width, height, r, c)
            votes.setdefault(region, {}).setdefault(gid, 0)
            votes[region][gid] += 1

    return {
        region: max(gid_counts.items(), key=lambda kv: (kv[1], -kv[0]))[0]
        for region, gid_counts in votes.items()
    }


# --- coverage & validation --------------------------------------------------

def coverage(grid: Grid) -> float:
    total = sum(len(row) for row in grid)
    if total == 0:
        return 0.0
    painted = sum(1 for row in grid for cell in row if cell)
    return painted / total


def should_paint(index: int, include_17: bool) -> bool:
    """Screen-17 is the untouched reference unless explicitly included (§7)."""
    if index == REFERENCE_SCREEN:
        return include_17
    return True


def _entity_feet(entity: dict[str, Any]) -> tuple[float, float]:
    x, y = entity["pos"]
    return float(x), float(y)


def _has_support(colliders: list[dict[str, Any]], x: float, feet_y: float) -> bool:
    """True if some solid collider has an upward top edge at feet_y (+/-tol) over x."""
    for col in colliders:
        if col.get("type") not in SOLID_COLLIDER_TYPES:
            continue
        pts = col["points"]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        top = min(ys)
        if abs(top - feet_y) <= ENTITY_SUPPORT_TOL and min(xs) - 1e-9 <= x <= max(xs) + 1e-9:
            return True
    return False


def check_layout(screen: dict[str, Any], grid: Grid | None = None) -> list[str]:
    """Validate one screen against the paintable grammar (spec §4 items 5,7 + §5).

    Returns a list of human-readable violations (empty == clean). Reachability
    itself is the solver's job, not this tool's.
    """
    index = int(screen["screen"]["index"])
    if grid is None:
        grid = paint_grid(screen, index)

    violations: list[str] = []

    cov = coverage(grid)
    if cov < COVERAGE_MIN:
        violations.append(
            f"coverage {cov:.1%} below minimum {COVERAGE_MIN:.0%}"
        )
    elif cov > COVERAGE_MAX:
        violations.append(
            f"coverage {cov:.1%} above maximum {COVERAGE_MAX:.0%}"
        )

    # §4.5: every collider coordinate is an integer (grid-snapped).
    for col in screen.get("colliders", []):
        for (px, py) in col["points"]:
            if px != int(px) or py != int(py):
                violations.append(
                    f"collider {col.get('id')} has non-integer point ({px}, {py})"
                )
                break

    # §4.7: each entity keeps a supporting surface at its feet.
    colliders = screen.get("colliders", [])
    for ent in screen.get("entities", []):
        x, feet_y = _entity_feet(ent)
        if not _has_support(colliders, x, feet_y):
            violations.append(
                f"entity {ent.get('type')} at ({x}, {feet_y}) has no supporting surface"
            )

    return violations


# --- I/O + .tmj refresh -----------------------------------------------------

def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def refresh_tmj(index: int) -> None:
    """Rewrite screen-NN.tmj from the (freshly painted) .map.json."""
    import sys as _sys
    _sys.path.insert(0, str(Path(__file__).resolve().parent))
    from tiled_convert import screen_map_to_tiled  # noqa: E402

    screen = load_json(SCREENS_DIR / f"screen-{index:02d}.map.json")
    tmj = screen_map_to_tiled(screen)
    write_json(TILED_DIR / f"screen-{index:02d}.tmj", tmj)


def parse_screens_arg(value: str) -> list[int]:
    """Parse '12-16' / '0,3' / '0,12-15' into a sorted unique list of indices."""
    indices: list[int] = []
    for part in value.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            lo, hi = part.split("-", 1)
            indices.extend(range(int(lo), int(hi) + 1))
        else:
            indices.append(int(part))
    return sorted(set(indices))


# --- CLI --------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--screens", required=True,
                        help="screen indices, e.g. '12-16' or '0,3,12-15'")
    parser.add_argument("--preview", metavar="DIR",
                        help="also write screen-NN-preview.png into DIR")
    parser.add_argument("--check", action="store_true",
                        help="validate only; do not write any files")
    parser.add_argument("--include-17", action="store_true",
                        help="allow repainting the screen-17 reference (§7)")
    args = parser.parse_args()

    manifest = load_manifest()
    edge_mapping = None
    ref_path = SCREENS_DIR / f"screen-{REFERENCE_SCREEN:02d}.map.json"
    if ref_path.exists():
        edge_mapping = learn_edge_mapping(load_json(ref_path))

    exit_code = 0
    for index in parse_screens_arg(args.screens):
        if not should_paint(index, args.include_17):
            print(f"screen-{index:02d}: skipped (reference screen; use --include-17)")
            continue

        path = SCREENS_DIR / f"screen-{index:02d}.map.json"
        screen = load_json(path)
        biome = screen.get("biome", "courtyard")
        gids = load_biome_gids(biome, manifest)
        mapping = edge_mapping if biome == "courtyard" else None
        grid = paint_grid(screen, index, edge_mapping=mapping, biome_gids=gids)

        violations = check_layout(screen, grid)

        if args.check:
            status = "OK" if not violations else f"{len(violations)} violation(s)"
            print(f"screen-{index:02d}: {biome:<12} coverage={coverage(grid):.1%} {status}")
            for v in violations:
                print(f"    - {v}")
            if violations:
                exit_code = 1
            continue

        screen["tiles"]["terrain"] = grid
        write_json(path, screen)
        refresh_tmj(index)
        painted = sum(1 for row in grid for cell in row if cell)
        print(f"screen-{index:02d}: painted {painted} cells "
              f"({coverage(grid):.1%}) biome={biome}")
        for v in violations:
            print(f"    ! {v}")

        if args.preview:
            preview_dir = Path(args.preview)
            preview_dir.mkdir(parents=True, exist_ok=True)
            render_preview(screen, grid, preview_dir / f"screen-{index:02d}-preview.png")

    return exit_code


def render_preview(screen: dict[str, Any], grid: Grid, out_path: Path) -> None:
    """Composite the painted tiles straight from the atlas into a preview PNG."""
    from PIL import Image  # local import: only needed for --preview

    atlas_path = REPO_ROOT / "assets" / "generated" / "castle.png"
    atlas = Image.open(atlas_path).convert("RGBA")
    ts = ATLAS_TILE_SIZE
    height = len(grid)
    width = len(grid[0]) if height else 0
    canvas = Image.new("RGBA", (width * ts, height * ts), (24, 20, 28, 255))

    for r in range(height):
        for c in range(width):
            gid = grid[r][c]
            if not gid:
                continue
            local = gid - FIRSTGID
            ax = (local % ATLAS_COLUMNS) * ts
            ay = (local // ATLAS_COLUMNS) * ts
            tile = atlas.crop((ax, ay, ax + ts, ay + ts))
            canvas.paste(tile, (c * ts, r * ts), tile)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out_path)


if __name__ == "__main__":
    raise SystemExit(main())
