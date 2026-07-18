#!/usr/bin/env python3
"""Build deterministic runtime atlases from the three approved CC0 archives."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import zipfile

from PIL import Image


TILE = 16
TERRAIN_COLUMNS = 7
TERRAIN_ROWS = 5
ATLAS_SIZE = (TERRAIN_COLUMNS * TILE, (TERRAIN_ROWS + 1) * TILE)
PLAYER_CELL = 48

SOURCES = (
    {
        "id": "pixel_adventure",
        "creator": "Pixel Frog",
        "url": "https://pixelfrog-assets.itch.io/pixel-adventure-1",
        "archive": "Pixel Adventure 1.zip",
        "license": "CC0-1.0",
    },
    {
        "id": "kenney",
        "creator": "Kenney",
        "url": "https://kenney.nl/assets/pixel-platformer",
        "archive": "kenney_pixel-platformer.zip",
        "license": "CC0-1.0",
    },
    {
        "id": "kings_and_pigs",
        "creator": "Pixel Frog",
        "url": "https://pixelfrog-assets.itch.io/kings-and-pigs",
        "archive": "Kings and Pigs.zip",
        "license": "CC0-1.0",
    },
)


def member_bytes(archive: zipfile.ZipFile, expected: str) -> bytes:
    wanted = PurePosixPath(expected).as_posix().casefold()
    matches = [
        name for name in archive.namelist()
        if PurePosixPath(name).as_posix().casefold() == wanted
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected one archive member {expected!r}, found {len(matches)}")
    return archive.read(matches[0])


def open_rgba(archive: zipfile.ZipFile, expected: str) -> Image.Image:
    return Image.open(io.BytesIO(member_bytes(archive, expected))).convert("RGBA")


def fitted(image: Image.Image, size: int = TILE) -> Image.Image:
    image = image.convert("RGBA")
    image.thumbnail((size, size), Image.Resampling.NEAREST)
    result = Image.new("RGBA", (size, size))
    result.alpha_composite(
        image,
        ((size - image.width) // 2, size - image.height),
    )
    return result


def sheet_cells(image: Image.Image, source_tile: int, count: int) -> list[Image.Image]:
    columns = image.width // source_tile
    rows = image.height // source_tile
    if columns * rows < count:
        raise RuntimeError(
            f"sheet {image.width}x{image.height} has fewer than {count} cells")
    cells: list[Image.Image] = []
    for index in range(count):
        x = (index % columns) * source_tile
        y = (index // columns) * source_tile
        cell = image.crop((x, y, x + source_tile, y + source_tile))
        cells.append(cell.resize((TILE, TILE), Image.Resampling.NEAREST))
    return cells


def nonempty_sheet_cells(
    image: Image.Image,
    source_tile: int,
    count: int,
) -> list[Image.Image]:
    columns = image.width // source_tile
    rows = image.height // source_tile
    cells: list[Image.Image] = []
    for index in range(columns * rows):
        x = (index % columns) * source_tile
        y = (index // columns) * source_tile
        cell = image.crop((x, y, x + source_tile, y + source_tile))
        if cell.getbbox() is not None:
            cells.append(cell.resize((TILE, TILE), Image.Resampling.NEAREST))
        if len(cells) == count:
            return cells
    raise RuntimeError(f"sheet has fewer than {count} non-empty cells")


def repeated_sheet_cell(
    image: Image.Image,
    source_tile: int,
    index: int,
    count: int,
) -> list[Image.Image]:
    columns = image.width // source_tile
    rows = image.height // source_tile
    if index < 0 or index >= columns * rows:
        raise RuntimeError(f"terrain cell {index} is outside its source sheet")
    x = (index % columns) * source_tile
    y = (index // columns) * source_tile
    cell = image.crop((x, y, x + source_tile, y + source_tile))
    if cell.getbbox() is None:
        raise RuntimeError(f"terrain cell {index} is empty")
    normalized = cell.resize((TILE, TILE), Image.Resampling.NEAREST)
    return [normalized.copy() for _ in range(count)]


def first_frame(image: Image.Image, frame_width: int, frame_height: int) -> Image.Image:
    if image.width < frame_width or image.height < frame_height:
        raise RuntimeError("animation sheet is smaller than its declared frame")
    return image.crop((0, 0, frame_width, frame_height))


def rect(x: int, y: int, width: int = TILE, height: int = TILE) -> dict[str, int]:
    return {"x": x, "y": y, "width": width, "height": height}


def save_png(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG", optimize=False, compress_level=9)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atlas_record(path: Path, regions: dict[str, object]) -> dict[str, object]:
    with Image.open(path) as image:
        width, height = image.size
    return {
        "file": path.name,
        "sha256": digest(path),
        "width": width,
        "height": height,
        "terrain_grid": {
            "x": 0,
            "y": 0,
            "tile_size": TILE,
            "columns": TERRAIN_COLUMNS,
            "rows": TERRAIN_ROWS,
        },
        "regions": regions,
    }


def build_biome_atlas(
    terrain: list[Image.Image],
    specials: list[Image.Image],
    path: Path,
) -> dict[str, object]:
    if len(terrain) != TERRAIN_COLUMNS * TERRAIN_ROWS or len(specials) != 4:
        raise RuntimeError("biome atlas inputs do not match the stable layout")
    atlas = Image.new("RGBA", ATLAS_SIZE)
    for index, tile in enumerate(terrain):
        atlas.alpha_composite(
            tile,
            ((index % TERRAIN_COLUMNS) * TILE, (index // TERRAIN_COLUMNS) * TILE),
        )
    names = ("spike", "checkpoint", "exit", "background")
    regions: dict[str, object] = {}
    for index, (name, image) in enumerate(zip(names, specials, strict=True)):
        x = index * TILE
        y = TERRAIN_ROWS * TILE
        atlas.alpha_composite(fitted(image), (x, y))
        regions[name] = rect(x, y)
    save_png(atlas, path)
    return atlas_record(path, regions)


def pixel_adventure_atlas(archive: zipfile.ZipFile, output: Path) -> dict[str, object]:
    terrain_sheet = open_rgba(archive, "Free/Terrain/Terrain (16x16).png")
    terrain = repeated_sheet_cell(
        terrain_sheet, 16, 29, TERRAIN_COLUMNS * TERRAIN_ROWS)
    specials = [
        open_rgba(archive, "Free/Traps/Spikes/Idle.png"),
        first_frame(open_rgba(
            archive,
            "Free/Items/Checkpoints/Checkpoint/Checkpoint (Flag Idle)(64x64).png",
        ), 64, 64),
        open_rgba(archive, "Free/Items/Checkpoints/End/End (Idle).png"),
        open_rgba(archive, "Free/Background/Blue.png"),
    ]
    return build_biome_atlas(terrain, specials, output / "pixel-adventure.png")


def kenney_atlas(archive: zipfile.ZipFile, output: Path) -> dict[str, object]:
    base = open_rgba(archive, "Tiles/tile_0005.png").resize(
        (TILE, TILE), Image.Resampling.NEAREST)
    terrain = [base.copy() for _ in range(TERRAIN_COLUMNS * TERRAIN_ROWS)]
    specials = [
        open_rgba(archive, "Tiles/tile_0035.png"),
        open_rgba(archive, "Tiles/Characters/tile_0000.png"),
        open_rgba(archive, "Tiles/Characters/tile_0011.png"),
        open_rgba(archive, "Tiles/Backgrounds/tile_0008.png"),
    ]
    return build_biome_atlas(terrain, specials, output / "kenney.png")


def kings_atlas(archive: zipfile.ZipFile, output: Path) -> dict[str, object]:
    terrain_sheet = open_rgba(archive, "Sprites/14-TileSets/Terrain (32x32).png")
    decoration_sheet = open_rgba(
        archive, "Sprites/14-TileSets/Decorations (32x32).png")
    terrain = repeated_sheet_cell(
        terrain_sheet, 32, 162, TERRAIN_COLUMNS * TERRAIN_ROWS)
    decorations = nonempty_sheet_cells(decoration_sheet, 32, 4)
    return build_biome_atlas(
        terrain,
        [decorations[0], decorations[1], decorations[2], decorations[3]],
        output / "kings-and-pigs.png",
    )


KING_FRAME = (78, 58)
STABLE_STATES = frozenset({"idle", "run", "charge", "rise", "fall"})


def union_box(
    frames: list[Image.Image],
) -> tuple[int, int, int, int] | None:
    """Return the smallest box covering the opaque content of every frame."""
    box: tuple[int, int, int, int] | None = None
    for frame in frames:
        bounds = frame.getbbox()
        if bounds is None:
            continue
        box = bounds if box is None else (
            min(box[0], bounds[0]),
            min(box[1], bounds[1]),
            max(box[2], bounds[2]),
            max(box[3], bounds[3]),
        )
    return box


def register_king_frames(
    frames: list[Image.Image],
    box: tuple[int, int, int, int],
) -> list[Image.Image]:
    """Crop every frame to a shared content box, then scale and place it
    bottom-centre inside a PLAYER_CELL square so all King states line up on one
    anchor: horizontally centred, feet on the cell floor. This keeps the King a
    constant size, prevents a horizontal jump when the sprite is flipped, and
    lets the renderer align the sprite's feet to the player's feet."""
    left, top, right, bottom = box
    crop_width, crop_height = right - left, bottom - top
    scale = min(PLAYER_CELL / crop_width, PLAYER_CELL / crop_height)
    scaled_width = max(1, round(crop_width * scale))
    scaled_height = max(1, round(crop_height * scale))
    placed: list[Image.Image] = []
    for frame in frames:
        cropped = frame.crop(box).resize(
            (scaled_width, scaled_height), Image.Resampling.NEAREST)
        cell = Image.new("RGBA", (PLAYER_CELL, PLAYER_CELL))
        cell.alpha_composite(
            cropped,
            ((PLAYER_CELL - scaled_width) // 2, PLAYER_CELL - scaled_height),
        )
        placed.append(cell)
    return placed


def king_player_atlas(
    archive: zipfile.ZipFile,
    output: Path,
) -> tuple[dict[str, object], dict[str, object]]:
    states = (
        ("idle", "Idle (78x58).png"),
        ("run", "Run (78x58).png"),
        ("charge", "Ground (78x58).png"),
        ("rise", "Jump (78x58).png"),
        ("fall", "Fall (78x58).png"),
        ("respawn", "Dead (78x58).png"),
    )
    frame_width, frame_height = KING_FRAME
    raw: list[tuple[str, list[Image.Image]]] = []
    for state, filename in states:
        sheet = open_rgba(archive, f"Sprites/01-King Human/{filename}")
        if sheet.height != frame_height or sheet.width % frame_width != 0:
            raise RuntimeError(f"unexpected King animation dimensions for {filename}")
        frames = [
            sheet.crop((x, 0, x + frame_width, frame_height))
            for x in range(0, sheet.width, frame_width)
        ]
        raw.append((state, frames))

    state_boxes = {state: union_box(frames) for state, frames in raw}
    if any(box is None for box in state_boxes.values()):
        raise RuntimeError("a King animation has no opaque frames")
    stable = [state_boxes[state] for state in STABLE_STATES]
    horizontal_centre = (
        min(box[0] for box in stable) + max(box[2] for box in stable)) / 2.0
    every = list(state_boxes.values())
    content_left = min(box[0] for box in every)
    content_right = max(box[2] for box in every)
    half_width = max(
        horizontal_centre - content_left, content_right - horizontal_centre)
    # Put the standing states' feet on the cell floor so the renderer can align
    # the sprite's feet to the player's feet; the death slump may extend a hair
    # lower and is clipped rather than lifting every pose off the ground.
    standing_floor = max(state_boxes[state][3] for state in STABLE_STATES)
    shared_box = (
        max(0, round(horizontal_centre - half_width)),
        min(box[1] for box in every),
        min(frame_width, round(horizontal_centre + half_width)),
        standing_floor,
    )

    sheets = [
        (state, register_king_frames(frames, shared_box))
        for state, frames in raw
    ]

    width = max(len(frames) for _, frames in sheets) * PLAYER_CELL
    height = len(sheets) * PLAYER_CELL
    atlas = Image.new("RGBA", (width, height))
    animations: dict[str, object] = {}
    for row, (state, frames) in enumerate(sheets):
        frame_rects = []
        for column, frame in enumerate(frames):
            x = column * PLAYER_CELL
            y = row * PLAYER_CELL
            atlas.alpha_composite(frame, (x, y))
            frame_rects.append(rect(x, y, PLAYER_CELL, PLAYER_CELL))
        animations[state] = {
            "atlas": "player",
            "fps": 10,
            "frames": frame_rects,
        }

    path = output / "player.png"
    save_png(atlas, path)
    record = {
        "file": path.name,
        "sha256": digest(path),
        "width": width,
        "height": height,
        "regions": {},
    }
    return record, animations


def build(downloads: Path, output: Path, manifest_path: Path) -> None:
    archives = {
        source["id"]: zipfile.ZipFile(downloads / source["archive"])
        for source in SOURCES
    }
    try:
        atlases: dict[str, object] = {
            "pixel_adventure": pixel_adventure_atlas(
                archives["pixel_adventure"], output),
            "kenney": kenney_atlas(archives["kenney"], output),
            "kings_and_pigs": kings_atlas(
                archives["kings_and_pigs"], output),
        }
        player, animations = king_player_atlas(
            archives["kings_and_pigs"], output)
        atlases["player"] = player
    finally:
        for archive in archives.values():
            archive.close()

    manifest = {
        "schema_version": 1,
        "license": "CC0-1.0",
        "sources": list(SOURCES),
        "atlases": atlases,
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
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    arguments = parser.parse_args()
    build(arguments.downloads, arguments.output, arguments.manifest)


if __name__ == "__main__":
    main()
