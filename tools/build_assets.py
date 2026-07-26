#!/usr/bin/env python3
"""Compile approved CC0 source regions into deterministic runtime atlases."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from PIL import Image


TILE = 16
# Per-biome terrain region: 8 columns x 8 rows (P0 atlas enrichment). The
# top-left 3x3 (cols 0-2, rows 0-2) is a hard contract with the game: it is
# read as the 9-slice, and `terrain_fill_region` derives the solid "center"
# fill from cell (1, 1). Everything else in the 8x8 grid is this pipeline's
# choice to place as it sees fit.
TERRAIN_COLUMNS = 8
TERRAIN_ROWS = 8
BIOME_ORDER = ("courtyard", "frosted_keep", "crown_spire")
ANIMATION_NAMES = {
    "idle": "idle",
    "walk": "run",
    "charge": "charge",
    "rise": "rise",
    "fall": "fall",
    "reset": "respawn",
}
SOURCE_RECORDS = (
    {
        "id": "castle_tileset",
        "creator": "rubberduck",
        "url": "https://opengameart.org/content/pixel-art-castle-tileset",
        "license": "CC0-1.0",
        "files": [
            {"name": "castle_tileset_part1.png", "sha256": "cb25ec05aa353a50f87798548ea737228f9e72989143f7cb3f1bcecc7478bba6"},
            {"name": "castle_tileset_part2.png", "sha256": "11da497a383fc4807078e95575ca91b284ad7931bbaefaa56669bf4d917c5fd9"},
            {"name": "castle_tileset_part3.png", "sha256": "24354419ec6eb06f8b3675ac7a6049ff4c1018d1d92e7fb98d26a90c7702c5f8"},
        ],
    },
    {
        "id": "gloomy_knight",
        "creator": "loveOS by @cookiielove_",
        "url": "https://loveosstudio.itch.io/gloomy-knight-16x16",
        "license": "CC0-1.0",
        "files": [
            {"name": "Gloomy Knight.zip", "sha256": "ebb0d53789e7cb70c0a94dde307515a902fffc57027ec37ce7d3bf82b640a5c5"}
        ],
    },
    {
        "id": "kenney_ui",
        "creator": "Kenney",
        "url": "https://kenney.nl/assets/ui-pack-pixel-adventure",
        "license": "CC0-1.0",
        "files": [
            {"name": "kenney_ui-pack-pixel-adventure.zip", "sha256": "0b0ed4802ebcfff5e44e370f394baa1d751862a5a4a7612ac4ce84e85faa0627"}
        ],
    },
    {
        "id": "sunnyland_winter",
        "creator": "ansimuz",
        "url": "https://ansimuz.itch.io/sunnyland-forest",
        "license": "CC0-1.0",
        "files": [
            {"name": "sunnyland winter forest files.zip", "sha256": "63adce10f83d31d5ed05ddf0cb94ba8936ea2782c386a8b9f9707783164a0616"},
            {"name": "winter_tileset.png", "sha256": "81b54e37164e409497ae2dbe6bf8d9d4facdf7364237f13efac84eaec03de83a"},
        ],
    },
    {
        "id": "gothicvania_swamp",
        "creator": "ansimuz",
        "url": "https://ansimuz.itch.io/gothicvania-swamp",
        "license": "CC0-1.0",
        "files": [
            {"name": "Gothicvania Swamp files.zip", "sha256": "962daea4efab6252a9847ec23ace8834eb1e8d1ae9ff6216c4a3f3591200fd56"},
            {"name": "gothic_tileset.png", "sha256": "b5ea388a414ce2731e2424888ca70b4915330275b92819881feacf3bb864b187"},
        ],
    },
)


def rect(x: int, y: int, width: int = TILE, height: int = TILE) -> dict[str, int]:
    return {"x": x, "y": y, "width": width, "height": height}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save_png(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG", optimize=False, compress_level=9)


def atlas_record(path: Path) -> dict[str, Any]:
    with Image.open(path) as image:
        width, height = image.size
    return {
        "file": path.name,
        "sha256": digest(path),
        "width": width,
        "height": height,
    }


class SourceImages:
    def __init__(self, downloads: Path):
        self.downloads = downloads.resolve()
        self.images: dict[Path, Image.Image] = {}

    def crop(self, region: dict[str, Any]) -> Image.Image:
        relative = Path(region["file"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"unsafe source path: {relative}")
        path = (self.downloads / relative).resolve()
        if self.downloads not in path.parents:
            raise ValueError(f"source escapes downloads root: {relative}")
        if path not in self.images:
            if not path.is_file():
                raise FileNotFoundError(f"source image is missing: {path}")
            self.images[path] = Image.open(path).convert("RGBA")
        image = self.images[path]
        x = region["x"]
        y = region["y"]
        width = region["width"]
        height = region["height"]
        if min(x, y) < 0 or width <= 0 or height <= 0:
            raise ValueError(f"invalid source rectangle in {relative}")
        if x + width > image.width or y + height > image.height:
            raise ValueError(f"source rectangle exceeds {relative}")
        cropped = image.crop((x, y, x + width, y + height))
        if cropped.getbbox() is None:
            raise ValueError(f"source rectangle is transparent in {relative}")
        return cropped

    def close(self) -> None:
        for image in self.images.values():
            image.close()


def fitted_tile(image: Image.Image) -> Image.Image:
    return image.resize((TILE, TILE), Image.Resampling.NEAREST)


# Diagonal terrain: a slope tile is the biome's solid "center" stone masked to a
# right triangle so the exposed edge reads as a 45-degree incline. Four
# orientations cover walkable ramps (rise NE/NW) and overhang ceilings (SE/SW).
# Derived deterministically from already-verified atlas pixels -- no new source.
SLOPE_ORIENTATIONS = ("slope_ne", "slope_nw", "slope_se", "slope_sw")


def _slope_keep(orientation: str, px: int, py: int) -> bool:
    """True if pixel (px,py) is solid for this slope orientation (TILE-1 = 15)."""
    m = TILE - 1
    if orientation == "slope_ne":   # ramp rising to the right; solid lower-right
        return px + py >= m
    if orientation == "slope_nw":   # ramp rising to the left;  solid lower-left
        return py >= px
    if orientation == "slope_se":   # ceiling sloping down-right; solid upper-left
        return px + py <= m
    if orientation == "slope_sw":   # ceiling sloping down-left;  solid upper-right
        return py <= px
    raise ValueError(f"unknown slope orientation: {orientation}")


def slope_tile(center: Image.Image, orientation: str) -> Image.Image:
    """A TILE x TILE diagonal cut of the solid `center` stone tile."""
    stone = fitted_tile(center)
    out = Image.new("RGBA", (TILE, TILE))
    src = stone.load()
    dst = out.load()
    for py in range(TILE):
        for px in range(TILE):
            if _slope_keep(orientation, px, py):
                dst[px, py] = src[px, py]
    return out


# Slope tiles live in a free grid row (row 3) of every biome's 8x8 block; rows
# 0-2 are the 9-slice + named-slot + checkpoint/exit contract, rows 3-7 are
# filler this pipeline may repurpose.
SLOPE_SLOT_POSITIONS: dict[str, tuple[int, int]] = {
    "slope_ne": (3, 0), "slope_nw": (3, 1),
    "slope_se": (3, 2), "slope_sw": (3, 3),
}


def derived_frame(image: Image.Image, derive: str | None) -> Image.Image:
    normalized = fitted_tile(image)
    if derive is None:
        return normalized
    if derive != "charge_squash":
        raise ValueError(f"unsupported derived frame: {derive}")
    squashed = normalized.resize((TILE, 13), Image.Resampling.NEAREST)
    result = Image.new("RGBA", (TILE, TILE))
    result.alpha_composite(squashed, (0, TILE - squashed.height))
    return result


# The 9-slice occupies the hard-contract top-left 3x3 of every biome's 8x8
# terrain region. Everything else is enrichment: inner corners, platforms, a
# ledge, a pillar, an isolated accent, two decor details, and a hazard tile,
# each recorded in the manifest as its own named region so the Tiled painting
# palette (and any future tooling) can address them individually.
CORE_9SLICE_POSITIONS: dict[str, tuple[int, int]] = {
    "top_left": (0, 0), "top": (0, 1), "top_right": (0, 2),
    "left": (1, 0), "center": (1, 1), "right": (1, 2),
    "bottom_left": (2, 0), "bottom": (2, 1), "bottom_right": (2, 2),
}
EXTRA_SLOT_POSITIONS: dict[str, tuple[int, int]] = {
    "inner_corner_tl": (0, 3), "inner_corner_tr": (0, 4),
    "platform_left": (0, 5), "platform_mid": (0, 6), "platform_right": (0, 7),
    "inner_corner_bl": (1, 3), "inner_corner_br": (1, 4),
    "ledge": (1, 5), "pillar": (1, 6), "isolated": (1, 7),
    "detail_1": (2, 3), "detail_2": (2, 4), "hazard": (2, 5),
}
NAMED_SLOT_POSITIONS: dict[str, tuple[int, int]] = {
    **CORE_9SLICE_POSITIONS,
    **EXTRA_SLOT_POSITIONS,
}
# checkpoint/exit are painted from the shared `props` icons (not a per-biome
# terrain source region), so they reserve two grid cells rather than taking a
# name out of `selection["biomes"][biome]`.
RESERVED_SPECIAL_POSITIONS: dict[str, tuple[int, int]] = {
    "checkpoint": (2, 6),
    "exit": (2, 7),
}
# Cells not claimed by a named slot or a reserved special are filled by
# cycling this list so every atlas cell is painted (no dead/transparent holes
# in the Tiled tile picker). Order is arbitrary but must stay deterministic.
# Deliberately excludes top_left/top/top_right: those are the exposed-edge
# row of the 9-slice and, in every biome's source art, carry a partial-alpha
# silhouette (grass/snow fringe against open air). Repeating them mid-grid
# would scatter that fringe across cells meant to read as solid floor.
FILLER_CYCLE = (
    "left", "center", "right",
    "bottom_left", "bottom", "bottom_right",
    "isolated",
)


def terrain_layout() -> tuple[tuple[str | None, ...], ...]:
    """Deterministic 8x8 per-biome name grid.

    Rows/cols 0-2 are the 9-slice (a hard contract with the game). The
    remaining named slots (inner corners, platforms, ledge, pillar, isolated,
    decor details, hazard) sit at fixed positions alongside it. The two cells
    reserved for checkpoint/exit are left as `None` (painted separately from
    `props`, not from a named terrain region). Every other cell repeats a
    useful auto-tile name so the whole grid is painted.
    """
    grid: list[list[str | None]] = [
        [None] * TERRAIN_COLUMNS for _ in range(TERRAIN_ROWS)
    ]
    for name, (row, column) in NAMED_SLOT_POSITIONS.items():
        grid[row][column] = name
    filler_index = 0
    for row in range(TERRAIN_ROWS):
        for column in range(TERRAIN_COLUMNS):
            if grid[row][column] is not None:
                continue
            if (row, column) in RESERVED_SPECIAL_POSITIONS.values():
                continue
            grid[row][column] = FILLER_CYCLE[filler_index % len(FILLER_CYCLE)]
            filler_index += 1
    return tuple(tuple(row) for row in grid)


def build_castle(
    selection: dict[str, Any], sources: SourceImages, output: Path
) -> dict[str, Any]:
    biome_width = TERRAIN_COLUMNS * TILE
    biome_height = TERRAIN_ROWS * TILE
    atlas = Image.new("RGBA", (len(BIOME_ORDER) * biome_width, biome_height))
    biome_records: dict[str, Any] = {}
    props = selection["props"]
    layout = terrain_layout()

    for biome_index, biome_name in enumerate(BIOME_ORDER):
        origin_x = biome_index * biome_width
        regions = selection["biomes"][biome_name]
        for row, names in enumerate(layout):
            for column, name in enumerate(names):
                if name is None:
                    continue  # reserved for checkpoint/exit below
                atlas.alpha_composite(
                    fitted_tile(sources.crop(regions[name])),
                    (origin_x + column * TILE, row * TILE),
                )

        named_regions: dict[str, dict[str, int]] = {
            name: rect(origin_x + column * TILE, row * TILE)
            for name, (row, column) in NAMED_SLOT_POSITIONS.items()
        }

        special_regions: dict[str, dict[str, int]] = {}
        for name, source_region in (
            ("checkpoint", props["banner"]),
            ("exit", props["crown"]),
        ):
            row, column = RESERVED_SPECIAL_POSITIONS[name]
            x = origin_x + column * TILE
            y = row * TILE
            atlas.alpha_composite(fitted_tile(sources.crop(source_region)), (x, y))
            special_regions[name] = rect(x, y)
        # `spike` is now the biome's own curated hazard tile (previously a
        # torch-flame placeholder shared by all three biomes); `background`
        # reuses the isolated accent. Both alias an already-painted cell.
        special_regions["spike"] = named_regions["hazard"]
        special_regions["background"] = named_regions["isolated"]

        # Diagonal slope tiles, derived from this biome's solid center stone.
        slope_regions: dict[str, dict[str, int]] = {}
        center_stone = fitted_tile(sources.crop(regions["center"]))
        for slope_name, (row, column) in SLOPE_SLOT_POSITIONS.items():
            x = origin_x + column * TILE
            y = row * TILE
            atlas.alpha_composite(slope_tile(center_stone, slope_name), (x, y))
            slope_regions[slope_name] = rect(x, y)

        biome_records[biome_name] = {
            "terrain_grid": {
                "x": origin_x,
                "y": 0,
                "tile_size": TILE,
                "columns": TERRAIN_COLUMNS,
                "rows": TERRAIN_ROWS,
            },
            "regions": {**named_regions, **special_regions, **slope_regions},
        }

    path = output / "castle.png"
    save_png(atlas, path)
    record = atlas_record(path)
    record["biomes"] = biome_records
    return record


def build_knight(
    selection: dict[str, Any], sources: SourceImages, output: Path
) -> tuple[dict[str, Any], dict[str, Any]]:
    max_frames = max(len(animation["frames"]) for animation in selection["player"].values())
    atlas = Image.new("RGBA", (max_frames * TILE, len(ANIMATION_NAMES) * TILE))
    animations: dict[str, Any] = {}

    for row, (selection_name, runtime_name) in enumerate(ANIMATION_NAMES.items()):
        source_animation = selection["player"][selection_name]
        frames = []
        for column, source_region in enumerate(source_animation["frames"]):
            frame = derived_frame(
                sources.crop(source_region), source_region.get("derive")
            )
            x = column * TILE
            y = row * TILE
            atlas.alpha_composite(frame, (x, y))
            frames.append(rect(x, y))
        animations[runtime_name] = {
            "atlas": "knight",
            "fps": source_animation["fps"],
            "frames": frames,
        }

    path = output / "knight.png"
    save_png(atlas, path)
    return atlas_record(path), animations


def build_ui(
    selection: dict[str, Any], sources: SourceImages, output: Path
) -> dict[str, Any]:
    names = tuple(selection["ui"])
    atlas = Image.new("RGBA", (len(names) * TILE, TILE))
    regions: dict[str, Any] = {}
    for column, name in enumerate(names):
        x = column * TILE
        atlas.alpha_composite(fitted_tile(sources.crop(selection["ui"][name])), (x, 0))
        regions[name] = rect(x, 0)

    path = output / "ui.png"
    save_png(atlas, path)
    record = atlas_record(path)
    record["regions"] = regions
    return record


def build(downloads: Path, selection_path: Path, output: Path, manifest_path: Path) -> None:
    selection = json.loads(selection_path.read_text(encoding="utf-8"))
    if selection.get("schema_version") != 1:
        raise ValueError("unsupported source-selection schema")

    sources = SourceImages(downloads)
    try:
        castle = build_castle(selection, sources, output)
        knight, animations = build_knight(selection, sources, output)
        ui = build_ui(selection, sources, output)
    finally:
        sources.close()

    manifest = {
        "schema_version": 2,
        "license": "CC0-1.0",
        "sources": list(SOURCE_RECORDS),
        "atlases": {"castle": castle, "knight": knight, "ui": ui},
        "animations": animations,
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--downloads", type=Path, required=True)
    parser.add_argument("--selection", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    arguments = parser.parse_args()
    build(
        arguments.downloads,
        arguments.selection,
        arguments.output,
        arguments.manifest,
    )


if __name__ == "__main__":
    main()
