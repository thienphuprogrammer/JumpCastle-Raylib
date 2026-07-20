#!/usr/bin/env python3
"""Auto-generate diverse terrain tile layers for JumpCastle screen .tmj files.

For each requested screen, this reads the screen's collision polygons and
biome from ``assets/levels/screens/screen-NN.map.json``, rasterizes solid /
hazard cells onto the screen's tile grid, autotiles every solid cell to a
named atlas region (edges, outer/inner corners, ledges, isolated blocks,
interior fill with light variety, hazard override), and writes the resulting
flat GID array into the *existing* ``terrain`` tile layer of
``assets/levels/tiled/screen-NN.tmj`` -- every other key in the .tmj
(collision/entities object layers, tileset reference, map properties) is
left untouched.

Tiles are purely visual: the generated grid follows the screen's *existing*
collision geometry exactly and never changes gameplay collision.

Usage:
    python3 tools/gen_terrain.py --screens 00-16
    python3 tools/gen_terrain.py --screens 0,3,12-15
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
SOLID_COLLIDER_TYPES = ("solid", "oneway")
HAZARD_COLLIDER_TYPE = "hazard"

# Named atlas regions every biome must expose for the autotiler (see
# assets/generated/manifest.json -> atlases.castle.biomes.<biome>.regions).
REQUIRED_REGIONS = (
    "top_left", "top", "top_right",
    "left", "center", "right",
    "bottom_left", "bottom", "bottom_right",
    "inner_corner_tl", "inner_corner_tr", "inner_corner_bl", "inner_corner_br",
    "ledge", "isolated", "detail_1", "detail_2", "hazard",
)

BoolGrid = list[list[bool]]


def region_gid(region: dict[str, Any]) -> int:
    """Tiled GID (1-based) for an atlas region, from its top-left pixel."""
    col = region["x"] // ATLAS_TILE_SIZE
    row = region["y"] // ATLAS_TILE_SIZE
    return row * ATLAS_COLUMNS + col + 1


def load_biome_gids(manifest: dict[str, Any], biome: str) -> dict[str, int]:
    """Resolve every required named region to a GID for one biome."""
    biomes = manifest["atlases"]["castle"]["biomes"]
    if biome not in biomes:
        raise KeyError(f"biome {biome!r} not found in manifest (have {sorted(biomes)})")
    regions = biomes[biome]["regions"]
    missing = [name for name in REQUIRED_REGIONS if name not in regions]
    if missing:
        raise KeyError(f"biome {biome!r} manifest missing regions: {missing}")
    return {name: region_gid(regions[name]) for name in REQUIRED_REGIONS}


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
    """Rasterize collider polygons onto a H-row x W-col cell grid.

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


def _is_solid(solid: BoolGrid, width: int, height: int, r: int, c: int) -> bool:
    """Out-of-bounds counts as not-solid (open), per the autotiling spec."""
    if r < 0 or r >= height or c < 0 or c >= width:
        return False
    return solid[r][c]


def autotile_gid(
    solid: BoolGrid,
    hazard: BoolGrid,
    width: int,
    height: int,
    r: int,
    c: int,
    gids: dict[str, int],
) -> int:
    """Pick a GID for one solid cell from its 4 orthogonal + 4 diagonal neighbors."""
    if hazard[r][c]:
        return gids["hazard"]

    open_up = not _is_solid(solid, width, height, r - 1, c)
    open_down = not _is_solid(solid, width, height, r + 1, c)
    open_left = not _is_solid(solid, width, height, r, c - 1)
    open_right = not _is_solid(solid, width, height, r, c + 1)

    if open_up and open_down:
        if open_left and open_right:
            return gids["isolated"]
        return gids["ledge"]
    if open_up and open_left:
        return gids["top_left"]
    if open_up and open_right:
        return gids["top_right"]
    if open_down and open_left:
        return gids["bottom_left"]
    if open_down and open_right:
        return gids["bottom_right"]
    if open_up:
        return gids["top"]
    if open_down:
        return gids["bottom"]
    if open_left:
        return gids["left"]
    if open_right:
        return gids["right"]

    # Interior cell: fully enclosed orthogonally. Check diagonals for a
    # concave (inner) corner before falling back to plain interior fill.
    if not _is_solid(solid, width, height, r - 1, c - 1):
        return gids["inner_corner_tl"]
    if not _is_solid(solid, width, height, r - 1, c + 1):
        return gids["inner_corner_tr"]
    if not _is_solid(solid, width, height, r + 1, c - 1):
        return gids["inner_corner_bl"]
    if not _is_solid(solid, width, height, r + 1, c + 1):
        return gids["inner_corner_br"]

    variety_key = r * 7 + c * 13
    if variety_key % 11 == 0:
        return gids["detail_1"]
    if variety_key % 17 == 0:
        return gids["detail_2"]
    return gids["center"]


def generate_terrain_grid(screen: dict[str, Any], gids: dict[str, int]) -> list[int]:
    """Build the flat row-major GID array (0 = empty) for one screen."""
    width = int(screen["screen"]["width"])
    height = int(screen["screen"]["height"])
    colliders = screen.get("colliders", [])
    solid, hazard = rasterize_colliders(colliders, width, height)

    data = [0] * (width * height)
    for r in range(height):
        for c in range(width):
            if not solid[r][c]:
                continue
            data[r * width + c] = autotile_gid(solid, hazard, width, height, r, c, gids)
    return data


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def apply_terrain_to_tmj(tmj: dict[str, Any], data: list[int]) -> None:
    for layer in tmj.get("layers", []):
        if layer.get("type") == "tilelayer" and layer.get("name") == "terrain":
            layer["data"] = data
            return
    raise KeyError("no 'terrain' tilelayer found in .tmj")


def generate_screen(index: int, manifest: dict[str, Any]) -> dict[str, Any]:
    """Generate + write terrain for one screen; returns stats for reporting."""
    screen_path = SCREENS_DIR / f"screen-{index:02d}.map.json"
    tmj_path = TILED_DIR / f"screen-{index:02d}.tmj"
    screen = load_json(screen_path)
    biome = screen.get("biome", "courtyard")
    gids = load_biome_gids(manifest, biome)

    data = generate_terrain_grid(screen, gids)
    tmj = load_json(tmj_path)
    apply_terrain_to_tmj(tmj, data)
    write_json(tmj_path, tmj)

    return {
        "index": index,
        "biome": biome,
        "n_colliders": len(screen.get("colliders", [])),
        "n_solid_tiles": sum(1 for gid in data if gid),
        "total_tiles": len(data),
    }


def parse_screens_arg(value: str) -> list[int]:
    """Parse '0-16' / '0,1,2' / '0,3,12-15' into a sorted list of indices."""
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--screens", required=True,
        help="Screen indices, e.g. '0-16' or '0,1,2,12-15'",
    )
    args = parser.parse_args()
    manifest = load_json(MANIFEST_PATH)

    for index in parse_screens_arg(args.screens):
        stats = generate_screen(index, manifest)
        warn = "  <<< SPARSE/EMPTY COLLISION" if stats["n_solid_tiles"] == 0 else ""
        print(
            f"screen-{index:02d}: biome={stats['biome']:<13} "
            f"colliders={stats['n_colliders']:>2} "
            f"solid_tiles={stats['n_solid_tiles']:>4}/{stats['total_tiles']}"
            f"{warn}"
        )


if __name__ == "__main__":
    main()
