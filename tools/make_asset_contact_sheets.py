#!/usr/bin/env python3
"""Create coordinate-labelled previews for deliberate CC0 sprite selection."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def render_grid(source: Path, destination: Path, tile_size: int, scale: int) -> None:
    with Image.open(source) as opened:
        image = opened.convert("RGBA")

    columns = image.width // tile_size
    rows = image.height // tile_size
    if columns == 0 or rows == 0:
        raise ValueError(f"{source} is smaller than its {tile_size}px grid")

    font = ImageFont.load_default()
    label_height = 12
    cell_width = tile_size * scale
    cell_height = label_height + tile_size * scale
    sheet = Image.new(
        "RGBA",
        (columns * cell_width, rows * cell_height),
        (18, 20, 26, 255),
    )
    draw = ImageDraw.Draw(sheet)

    for row in range(rows):
        for column in range(columns):
            x = column * tile_size
            y = row * tile_size
            cell = image.crop((x, y, x + tile_size, y + tile_size)).resize(
                (tile_size * scale, tile_size * scale),
                Image.Resampling.NEAREST,
            )
            destination_x = column * cell_width
            destination_y = row * cell_height
            draw.text(
                (destination_x + 1, destination_y + 1),
                f"{x},{y}",
                fill=(245, 223, 92, 255),
                font=font,
            )
            sheet.alpha_composite(cell, (destination_x, destination_y + label_height))
            draw.rectangle(
                (
                    destination_x,
                    destination_y + label_height,
                    destination_x + cell_width - 1,
                    destination_y + cell_height - 1,
                ),
                outline=(70, 75, 88, 255),
            )

    destination.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(destination, format="PNG", optimize=False, compress_level=9)


def source_jobs(downloads: Path) -> list[tuple[Path, str, int]]:
    gloomy = downloads / "extracted/gloomy/Gloomy Knight Asset Pack/knight"
    kenney = downloads / "extracted/kenney-ui/Tilesheets/Small tiles/Thin outline"
    return [
        (downloads / "castle_tileset_part1.png", "castle-part1-grid32.png", 32),
        (downloads / "castle_tileset_part2.png", "castle-part2-grid32.png", 32),
        (downloads / "castle_tileset_part3.png", "castle-part3-grid32.png", 32),
        (gloomy / "knightAtlas.png", "gloomy-knight-atlas-grid16.png", 16),
        (gloomy / "knightIdle.png", "gloomy-knight-idle-grid16.png", 16),
        (gloomy / "knightWalk.png", "gloomy-knight-walk-grid16.png", 16),
        (gloomy / "knightJumpFall.png", "gloomy-knight-jump-grid16.png", 16),
        (kenney / "tilemap_packed.png", "kenney-ui-small-grid16.png", 16),
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--downloads", type=Path, required=True)
    parser.add_argument("--scale", type=int, default=3)
    arguments = parser.parse_args()
    if arguments.scale < 1:
        parser.error("--scale must be positive")

    output = arguments.downloads / "contact-sheets"
    for source, filename, tile_size in source_jobs(arguments.downloads):
        if not source.is_file():
            raise FileNotFoundError(f"source image is missing: {source}")
        destination = output / filename
        render_grid(source, destination, tile_size, arguments.scale)
        print(destination)


if __name__ == "__main__":
    main()
