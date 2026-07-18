#!/usr/bin/env python3
"""Compose representative per-biome screens from the generated atlases.

Dependency-independent proof (Pillow only, no GPU/raylib) that the manifest
regions and campaign collision produce a distinct, non-empty scene for each
biome. Writes courtyard.png, frosted-keep.png and crown-spire.png at the exact
logical resolution declared by the runtime campaign.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

TILE = 16

# One representative screen (1-indexed from the bottom) per biome band.
BIOME_SCREENS = {
    "courtyard": (1, "courtyard.png"),
    "frosted_keep": (7, "frosted-keep.png"),
    "crown_spire": (13, "crown-spire.png"),
}


def alpha_coverage(image: Image.Image) -> float:
    pixels = image.convert("RGBA").get_flattened_data()
    opaque = sum(1 for pixel in pixels if pixel[3] > 0)
    return opaque / (image.width * image.height)


def _region(atlas: Image.Image, region: dict) -> Image.Image:
    return atlas.crop(
        (region["x"], region["y"],
         region["x"] + region["width"], region["y"] + region["height"]))


def _load_collision(campaign_path: Path) -> tuple[list[str], int, int, int]:
    lines = campaign_path.read_text(encoding="utf-8").splitlines()
    width = height = screen_height = 0
    for line in lines:
        fields = line.split()
        if len(fields) == 3 and fields[0] == "size":
            width, height = int(fields[1]), int(fields[2])
        elif len(fields) == 2 and fields[0] == "screen_height":
            screen_height = int(fields[1])
    grid_start = lines.index("[collision]") + 1
    grid = lines[grid_start:grid_start + height]
    return grid, width, screen_height, height


def _terrain_tile(castle: Image.Image, grid: dict, col: int, row: int) -> Image.Image:
    x = grid["x"] + col * grid["tile_size"]
    y = grid["y"] + row * grid["tile_size"]
    return castle.crop((x, y, x + grid["tile_size"], y + grid["tile_size"]))


def render_asset_smoke(manifest_path: Path, campaign_path: Path, output: Path) -> list[Path]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    base = manifest_path.parent
    castle = Image.open(base / manifest["atlases"]["castle"]["file"]).convert("RGBA")
    knight = Image.open(base / manifest["atlases"]["knight"]["file"]).convert("RGBA")
    idle = manifest["animations"]["idle"]["frames"][0]
    hero = _region(knight, idle)

    grid_rows, screen_width, screen_height, total_height = _load_collision(campaign_path)
    view_width = screen_width * TILE
    view_height = screen_height * TILE
    output.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []

    for biome, (screen_number, filename) in BIOME_SCREENS.items():
        biome_data = manifest["atlases"]["castle"]["biomes"][biome]
        grid = biome_data["terrain_grid"]
        background = _region(castle, biome_data["regions"]["background"])
        top_tile = _terrain_tile(castle, grid, 3, 0)
        fill_tile = _terrain_tile(castle, grid, 3, 2)

        scene = Image.new("RGBA", (view_width, view_height), (0, 0, 0, 255))
        for ty in range(screen_height):
            for tx in range(screen_width):
                scene.alpha_composite(background, (tx * TILE, ty * TILE))

        first_row = total_height - screen_number * screen_height
        for local_y in range(screen_height):
            row = grid_rows[first_row + local_y]
            for tx in range(min(screen_width, len(row))):
                if row[tx] != "#":
                    continue
                above_empty = local_y == 0 or grid_rows[first_row + local_y - 1][tx] != "#"
                tile = top_tile if above_empty else fill_tile
                scene.alpha_composite(tile, (tx * TILE, local_y * TILE))

        scene.alpha_composite(hero, (view_width // 2 - TILE // 2, view_height // 2))

        destination = output / filename
        scene.convert("RGB").save(destination)
        written.append(destination)

    return written


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    for path in render_asset_smoke(arguments.manifest, arguments.campaign, arguments.output):
        print(path)


if __name__ == "__main__":
    main()
