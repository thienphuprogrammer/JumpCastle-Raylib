#!/usr/bin/env python3
"""Render a labeled palette cheat-sheet for the castle terrain atlas.

Authoring aid for hand-painting in Tiled: scales `assets/generated/castle.png`
up (nearest-neighbor) and overlays, on every named tile region, its short role
name (e.g. `top_left`, `ledge`, `hazard`) and its painting GID (Tiled tileid +
1) -- so the user can find a tile in Tiled's Tilesets panel without
cross-referencing `assets/generated/manifest.json` by hand.

Output: docs/tiled-guides/palette-cheatsheet.png

Usage:
    .venv/bin/python tools/tiled_cheatsheet.py
"""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = REPO_ROOT / "assets" / "generated" / "manifest.json"
OUTPUT_PATH = REPO_ROOT / "docs" / "tiled-guides" / "palette-cheatsheet.png"

SCALE = 6
TILE_SIZE = 16
ATLAS_COLUMNS = 24
CELL_PX = TILE_SIZE * SCALE  # 96
HEADER_HEIGHT = 64
CELL_MARGIN = 3
LABEL_LINE_GAP = 3
BASE_LABEL_FONT_SIZE = 12
MIN_LABEL_FONT_SIZE = 7
HEADER_FONT_SIZE = 30

BIOME_ORDER = ["courtyard", "frosted_keep", "crown_spire"]

CELL_OUTLINE = (255, 220, 0, 255)
LABEL_FILL = (255, 255, 255, 255)
LABEL_STROKE = (0, 0, 0, 255)
HEADER_FILL = (255, 255, 255, 255)
HEADER_STROKE = (0, 0, 0, 255)
HEADER_BG = (20, 20, 24, 255)


def tileid_from_xy(x: int, y: int) -> int:
    """Tiled tileid (0-based) for an atlas pixel position, per the plan's formula."""
    col = x // TILE_SIZE
    row = y // TILE_SIZE
    return row * ATLAS_COLUMNS + col


def group_regions_by_cell(regions: dict) -> dict[tuple[int, int], list[str]]:
    """Group region names that share the same (x, y) atlas cell, name sorted."""
    by_cell: dict[tuple[int, int], list[str]] = {}
    for name, region in regions.items():
        key = (region["x"], region["y"])
        by_cell.setdefault(key, []).append(name)
    for names in by_cell.values():
        names.sort()
    return by_cell


def build_report(manifest: dict) -> dict[str, list[tuple[str, int, int]]]:
    """biome -> [(region_name, tileid, gid), ...] sorted by tileid then name."""
    biomes = manifest["atlases"]["castle"]["biomes"]
    report: dict[str, list[tuple[str, int, int]]] = {}
    for biome in BIOME_ORDER:
        rows = []
        for name, region in biomes[biome]["regions"].items():
            tileid = tileid_from_xy(region["x"], region["y"])
            rows.append((name, tileid, tileid + 1))
        rows.sort(key=lambda row: (row[1], row[0]))
        report[biome] = rows
    return report


def print_report(report: dict[str, list[tuple[str, int, int]]]) -> None:
    print("biome -> region -> GID (Tiled tileid = GID - 1)")
    for biome, rows in report.items():
        print(f"[{biome}]")
        for name, tileid, gid in rows:
            print(f"  {name:<16} tileid={tileid:<4} GID={gid}")


def fit_font(draw: ImageDraw.ImageDraw, lines: list[str], max_width: int) -> ImageFont.FreeTypeFont:
    """Largest font (within [MIN_LABEL_FONT_SIZE, BASE_LABEL_FONT_SIZE]) whose
    widest line still fits max_width, so long role names never bleed into the
    neighbouring tile cell."""
    for size in range(BASE_LABEL_FONT_SIZE, MIN_LABEL_FONT_SIZE - 1, -1):
        font = ImageFont.load_default(size=size)
        widest = max(draw.textbbox((0, 0), line, font=font, stroke_width=2)[2] for line in lines)
        if widest <= max_width:
            return font
    return ImageFont.load_default(size=MIN_LABEL_FONT_SIZE)


def draw_cell(draw: ImageDraw.ImageDraw, origin_px: tuple[int, int], names: list[str], gid: int) -> None:
    x, y = origin_px
    draw.rectangle([x, y, x + CELL_PX - 1, y + CELL_PX - 1], outline=CELL_OUTLINE, width=2)

    lines = [*names, f"GID {gid}"]
    usable_width = CELL_PX - 2 * CELL_MARGIN
    font = fit_font(draw, lines, usable_width)
    line_height = font.size + LABEL_LINE_GAP

    text_y = y + CELL_MARGIN
    for line in lines:
        draw.text(
            (x + CELL_MARGIN, text_y),
            line,
            font=font,
            fill=LABEL_FILL,
            stroke_width=2,
            stroke_fill=LABEL_STROKE,
        )
        text_y += line_height


def render_cheatsheet(manifest: dict) -> Image.Image:
    atlas_info = manifest["atlases"]["castle"]
    atlas_path = REPO_ROOT / "assets" / "generated" / atlas_info["file"]
    atlas = Image.open(atlas_path).convert("RGBA")
    scaled = atlas.resize((atlas.width * SCALE, atlas.height * SCALE), Image.NEAREST)

    canvas = Image.new("RGBA", (scaled.width, scaled.height + HEADER_HEIGHT), HEADER_BG)
    canvas.paste(scaled, (0, HEADER_HEIGHT))
    draw = ImageDraw.Draw(canvas)
    header_font = ImageFont.load_default(size=HEADER_FONT_SIZE)

    biomes = atlas_info["biomes"]
    for biome in BIOME_ORDER:
        grid = biomes[biome]["terrain_grid"]
        col_x0 = grid["x"] * SCALE
        col_width = grid["columns"] * TILE_SIZE * SCALE

        bbox = draw.textbbox((0, 0), biome, font=header_font, stroke_width=2)
        text_w = bbox[2] - bbox[0]
        text_x = col_x0 + (col_width - text_w) // 2
        draw.text(
            (text_x, 16),
            biome,
            font=header_font,
            fill=HEADER_FILL,
            stroke_width=2,
            stroke_fill=HEADER_STROKE,
        )

        by_cell = group_regions_by_cell(biomes[biome]["regions"])
        for (rx, ry), names in by_cell.items():
            gid = tileid_from_xy(rx, ry) + 1
            origin_px = (rx * SCALE, ry * SCALE + HEADER_HEIGHT)
            draw_cell(draw, origin_px, names, gid)

    return canvas


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text())
    report = build_report(manifest)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    canvas = render_cheatsheet(manifest)
    canvas.save(OUTPUT_PATH)

    print(f"Wrote {OUTPUT_PATH.relative_to(REPO_ROOT)} ({canvas.width}x{canvas.height})")
    print()
    print_report(report)


if __name__ == "__main__":
    main()
