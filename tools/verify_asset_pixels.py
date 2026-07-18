#!/usr/bin/env python3
"""Verify that required runtime regions contain visible pixels."""

from __future__ import annotations

import json
from pathlib import Path
import sys

from PIL import Image


def alpha_coverage(image: Image.Image) -> float:
    alpha = image.convert("RGBA").getchannel("A")
    visible = sum(value > 0 for value in alpha.get_flattened_data())
    return visible / (image.width * image.height)


def require_coverage(image: Image.Image, minimum: float, label: str) -> None:
    coverage = alpha_coverage(image)
    if coverage < minimum:
        raise ValueError(
            f"{label} alpha coverage {coverage:.1%} is below {minimum:.1%}"
        )


def crop_region(image: Image.Image, region: dict[str, int]) -> Image.Image:
    x = region["x"]
    y = region["y"]
    return image.crop((x, y, x + region["width"], y + region["height"]))


def verify(manifest_path: Path) -> None:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    directory = manifest_path.parent
    atlases = manifest["atlases"]
    castle = Image.open(directory / atlases["castle"]["file"]).convert("RGBA")
    knight = Image.open(directory / atlases["knight"]["file"]).convert("RGBA")
    ui = Image.open(directory / atlases["ui"]["file"]).convert("RGBA")
    try:
        for biome_name, biome in atlases["castle"]["biomes"].items():
            grid = biome["terrain_grid"]
            for row in range(grid["rows"]):
                for column in range(grid["columns"]):
                    region = {
                        "x": grid["x"] + column * grid["tile_size"],
                        "y": grid["y"] + row * grid["tile_size"],
                        "width": grid["tile_size"],
                        "height": grid["tile_size"],
                    }
                    require_coverage(
                        crop_region(castle, region),
                        0.05,
                        f"{biome_name}.terrain[{column},{row}]",
                    )
            for name, region in biome["regions"].items():
                minimum = 0.01 if name == "background" else 0.03
                require_coverage(
                    crop_region(castle, region), minimum, f"{biome_name}.{name}"
                )

        for state, animation in manifest["animations"].items():
            for index, region in enumerate(animation["frames"]):
                require_coverage(
                    crop_region(knight, region), 0.08, f"{state}[{index}]"
                )

        for name, region in atlases["ui"]["regions"].items():
            require_coverage(crop_region(ui, region), 0.08, f"ui.{name}")
    finally:
        castle.close()
        knight.close()
        ui.close()

    print("verified visible pixels in castle, knight, and UI atlases")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_asset_pixels.py MANIFEST", file=sys.stderr)
        return 2
    try:
        verify(Path(sys.argv[1]).resolve())
    except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"asset pixel verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
