#!/usr/bin/env python3
"""Render a per-screen collision underlay guide for hand-painting in Tiled.

Authoring aid: for every `assets/levels/screens/screen-NN.map.json`, draws a
dark image sized to the screen (tile grid at 16 px/tile, scaled up for
legibility) with every collider polygon filled semi-transparent + outlined by
type (solid=grey, oneway=blue, hazard=red) and every entity marked by a
colored dot (spawn=green, checkpoint=cyan, goal=gold). This is a
"paint-by-numbers" underlay: keep it open next to Tiled so painted terrain
tiles land on the real platforms instead of guessing from the raw geometry.

Output: docs/tiled-guides/screen-NN-collision.png for NN = 00..17.

Usage:
    .venv/bin/python tools/tiled_collision_guide.py
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont

REPO_ROOT = Path(__file__).resolve().parent.parent
SCREENS_DIR = REPO_ROOT / "assets" / "levels" / "screens"
OUTPUT_DIR = REPO_ROOT / "docs" / "tiled-guides"

TILE_SIZE = 16
SCALE = 2  # upscale factor for legibility; underlying grid stays width*16 x height*16
HEADER_HEIGHT = 56
TITLE_FONT_SIZE = 22
LEGEND_FONT_SIZE = 13
ENTITY_RADIUS_PX = 10

SCREEN_FILE_PATTERN = re.compile(r"^screen-(\d+)\.map\.json$")

BACKGROUND = (18, 18, 24, 255)
HEADER_BG = (8, 8, 12, 255)
TITLE_FILL = (255, 255, 255, 255)
TITLE_STROKE = (0, 0, 0, 255)

# type -> (fill RGBA with alpha, outline RGB)
COLLIDER_STYLE: dict[str, tuple[tuple[int, int, int, int], tuple[int, int, int, int]]] = {
    "solid": ((160, 160, 160, 120), (200, 200, 200, 255)),
    "oneway": ((70, 140, 255, 120), (120, 180, 255, 255)),
    "hazard": ((230, 60, 60, 130), (255, 110, 110, 255)),
}
UNKNOWN_COLLIDER_STYLE = ((255, 0, 255, 130), (255, 0, 255, 255))

# type -> RGBA dot color
ENTITY_STYLE: dict[str, tuple[int, int, int, int]] = {
    "spawn": (60, 220, 90, 255),
    "checkpoint": (60, 220, 220, 255),
    "goal": (230, 200, 60, 255),
}
UNKNOWN_ENTITY_STYLE = (255, 255, 255, 255)

LEGEND_TEXT = (
    "colliders: solid=grey  oneway=blue  hazard=red    "
    "entities: spawn=green  checkpoint=cyan  goal=gold"
)


def tile_to_px(point: list[float]) -> tuple[float, float]:
    x, y = point
    return x * TILE_SIZE * SCALE, y * TILE_SIZE * SCALE


def draw_colliders(overlay_draw: ImageDraw.ImageDraw, colliders: list[dict[str, Any]]) -> set[str]:
    unknown_types: set[str] = set()
    for collider in colliders:
        collider_type = collider.get("type", "")
        fill, outline = COLLIDER_STYLE.get(collider_type, UNKNOWN_COLLIDER_STYLE)
        if collider_type not in COLLIDER_STYLE:
            unknown_types.add(collider_type or "<missing>")
        points = [tile_to_px(p) for p in collider.get("points", [])]
        if len(points) < 3:
            continue
        overlay_draw.polygon(points, fill=fill, outline=outline, width=2)
    return unknown_types


def draw_entities(canvas_draw: ImageDraw.ImageDraw, entities: list[dict[str, Any]]) -> set[str]:
    unknown_types: set[str] = set()
    for entity in entities:
        entity_type = entity.get("type", "")
        color = ENTITY_STYLE.get(entity_type, UNKNOWN_ENTITY_STYLE)
        if entity_type not in ENTITY_STYLE:
            unknown_types.add(entity_type or "<missing>")
        px, py = tile_to_px(entity.get("pos", [0, 0]))
        canvas_draw.ellipse(
            [px - ENTITY_RADIUS_PX, py - ENTITY_RADIUS_PX, px + ENTITY_RADIUS_PX, py + ENTITY_RADIUS_PX],
            fill=color,
            outline=(0, 0, 0, 255),
            width=2,
        )
    return unknown_types


def render_screen_guide(map_data: dict[str, Any], index_str: str) -> Image.Image:
    screen = map_data["screen"]
    width_px = int(screen["width"]) * TILE_SIZE * SCALE
    height_px = int(screen["height"]) * TILE_SIZE * SCALE
    biome = map_data.get("biome", "?")

    canvas = Image.new("RGBA", (width_px, height_px + HEADER_HEIGHT), HEADER_BG)
    grid = Image.new("RGBA", (width_px, height_px), BACKGROUND)
    canvas.paste(grid, (0, HEADER_HEIGHT))

    overlay = Image.new("RGBA", (width_px, height_px), (0, 0, 0, 0))
    overlay_draw = ImageDraw.Draw(overlay)
    unknown_collider_types = draw_colliders(overlay_draw, map_data.get("colliders", []))

    grid_region = canvas.crop((0, HEADER_HEIGHT, width_px, HEADER_HEIGHT + height_px))
    grid_region = Image.alpha_composite(grid_region, overlay)
    canvas.paste(grid_region, (0, HEADER_HEIGHT))

    canvas_draw = ImageDraw.Draw(canvas)
    # Entities are drawn last, in header-relative canvas space, so offset y.
    shifted_entities = [
        {**e, "pos": [e.get("pos", [0, 0])[0], e.get("pos", [0, 0])[1] + HEADER_HEIGHT / (TILE_SIZE * SCALE)]}
        for e in map_data.get("entities", [])
    ]
    unknown_entity_types = draw_entities(canvas_draw, shifted_entities)

    title_font = ImageFont.load_default(size=TITLE_FONT_SIZE)
    legend_font = ImageFont.load_default(size=LEGEND_FONT_SIZE)
    canvas_draw.text(
        (10, 4),
        f"Screen {index_str} - {biome}",
        font=title_font,
        fill=TITLE_FILL,
        stroke_width=2,
        stroke_fill=TITLE_STROKE,
    )
    canvas_draw.text(
        (10, HEADER_HEIGHT - 18),
        LEGEND_TEXT,
        font=legend_font,
        fill=TITLE_FILL,
        stroke_width=1,
        stroke_fill=TITLE_STROKE,
    )

    if unknown_collider_types:
        print(f"  [screen-{index_str}] WARNING unknown collider type(s): {sorted(unknown_collider_types)}")
    if unknown_entity_types:
        print(f"  [screen-{index_str}] WARNING unknown entity type(s): {sorted(unknown_entity_types)}")

    return canvas


def iter_screen_files() -> list[tuple[str, Path]]:
    found: list[tuple[str, Path]] = []
    for path in sorted(SCREENS_DIR.glob("screen-*.map.json")):
        match = SCREEN_FILE_PATTERN.match(path.name)
        if match:
            found.append((match.group(1), path))
    found.sort(key=lambda item: item[0])
    return found


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    screen_files = iter_screen_files()

    written: list[Path] = []
    for index_str, path in screen_files:
        map_data = json.loads(path.read_text())
        canvas = render_screen_guide(map_data, index_str)
        out_path = OUTPUT_DIR / f"screen-{index_str}-collision.png"
        canvas.save(out_path)
        written.append(out_path)
        print(f"Wrote {out_path.relative_to(REPO_ROOT)} ({canvas.width}x{canvas.height})")

    print()
    print(f"Total guides written: {len(written)} (expected 18 for screens 00..17)")
    if len(written) != 18:
        print("WARNING: expected exactly 18 screen files -- check assets/levels/screens/")


if __name__ == "__main__":
    main()
