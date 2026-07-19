#!/usr/bin/env python3
"""Two-way converter between JumpCastle screen maps and Tiled (.tmj) maps.

JumpCastle stores each screen as ``screen-NN.map.json`` (see src/map_format.cpp):

    {
      "schema_version": 1,
      "screen": {"index": N, "width": W, "height": H},   # tile units
      "biome": "courtyard",
      "colliders": [{"id": 1, "type": "solid", "points": [[x, y], ...]}],
      "entities":  [{"type": "spawn", "pos": [x, y]}]
    }

Tiled stores maps as ``.tmj`` (JSON) in *pixels* with a top-left origin. This
module maps between the two so the polygon colliders can be authored in Tiled's
object layers:

    tile_coord = pixel / tile_size          (Tiled -> JumpCastle)
    pixel      = tile_coord * tile_size      (JumpCastle -> Tiled)

Colliders  <-> Tiled rectangle/polygon objects, class in {solid, oneway, hazard}
Entities   <-> Tiled point objects,             class in {spawn, checkpoint, goal}

The object class is read from either the "type" (Tiled 1.10+) or "class"
(Tiled 1.9) JSON key, and written under "type" for the widest compatibility.
Objects are classified by geometry + class string, not by layer name, so
renaming layers in Tiled does not break the round trip.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

TILE_SIZE = 16
COLLIDER_CLASSES = ("solid", "oneway", "hazard")
ENTITY_CLASSES = ("spawn", "checkpoint", "goal")
DEFAULT_BIOME = "courtyard"
SCREEN_INDEX_PATTERN = re.compile(r"screen-(\d+)")

# The runtime terrain atlas. Every generated .tmj references the shared external
# tileset (assets/levels/tiled/castle.tsx) so Tiled's Tilesets panel shows the
# biome bricks and the painted "terrain" tile layer resolves to real art. Path
# is relative to a .tmj under assets/levels/tiled/. Dimensions match
# assets/generated/castle.png.
TILESET_IMAGE = "../../generated/castle.png"
TILESET_IMAGE_WIDTH = 384
TILESET_IMAGE_HEIGHT = 128
TILESET_SOURCE = "castle.tsx"  # external tileset shared by every screen .tmj


# --------------------------------------------------------------------------
# Shared helpers
# --------------------------------------------------------------------------
def object_class(obj: dict[str, Any]) -> str:
    """Return a Tiled object's class, tolerating the type/class key rename."""
    value = obj.get("type", obj.get("class", ""))
    return value if isinstance(value, str) else ""


def _map_property(tmj: dict[str, Any], name: str) -> Any:
    for prop in tmj.get("properties", []) or []:
        if prop.get("name") == name:
            return prop.get("value")
    return None


# --------------------------------------------------------------------------
# Tiled -> JumpCastle
# --------------------------------------------------------------------------
def _object_tile_points(
    obj: dict[str, Any], tile_size: int
) -> list[list[float]] | None:
    """Absolute collider vertices in tile units, or None if not a collider shape.

    Handles Tiled rectangles (x, y, width, height) and polygons (origin plus
    a relative ``polygon`` vertex list). Points and polylines are not colliders.
    """
    origin_x = float(obj.get("x", 0.0))
    origin_y = float(obj.get("y", 0.0))

    if "polygon" in obj:
        vertices = [
            [(origin_x + p["x"]) / tile_size, (origin_y + p["y"]) / tile_size]
            for p in obj["polygon"]
        ]
        return vertices if len(vertices) >= 3 else None

    width = float(obj.get("width", 0.0))
    height = float(obj.get("height", 0.0))
    if width <= 0.0 or height <= 0.0:
        return None  # zero-area rectangle / a bare point
    x0, y0 = origin_x / tile_size, origin_y / tile_size
    x1 = (origin_x + width) / tile_size
    y1 = (origin_y + height) / tile_size
    return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]


def tiled_to_screen_map(
    tmj: dict[str, Any],
    *,
    index: int,
    biome: str | None = None,
    tile_size: int = TILE_SIZE,
) -> dict[str, Any]:
    """Convert a parsed Tiled map into a JumpCastle screen-map dict."""
    resolved_biome = biome or _map_property(tmj, "biome") or DEFAULT_BIOME
    width = int(tmj.get("width", 0))
    height = int(tmj.get("height", 0))
    colliders: list[dict[str, Any]] = []
    entities: list[dict[str, Any]] = []
    terrain_grid: list[list[int]] | None = None

    for layer in tmj.get("layers", []):
        layer_type = layer.get("type")
        if layer_type == "tilelayer" and layer.get("name") == "terrain":
            data = layer.get("data", []) or []
            if width > 0 and any(int(gid) != 0 for gid in data):
                terrain_grid = [
                    [int(gid) for gid in data[row * width:(row + 1) * width]]
                    for row in range(height)
                ]
            continue
        if layer_type != "objectgroup":
            continue
        for obj in layer.get("objects", []):
            cls = object_class(obj)
            if obj.get("point") and cls in ENTITY_CLASSES:
                entities.append(
                    {
                        "type": cls,
                        "pos": [
                            float(obj.get("x", 0.0)) / tile_size,
                            float(obj.get("y", 0.0)) / tile_size,
                        ],
                    }
                )
                continue
            if cls not in COLLIDER_CLASSES:
                continue
            points = _object_tile_points(obj, tile_size)
            if points is None:
                continue
            colliders.append(
                {"id": len(colliders) + 1, "type": cls, "points": points}
            )

    result: dict[str, Any] = {
        "schema_version": 2 if terrain_grid is not None else 1,
        "screen": {"index": index, "width": float(width), "height": float(height)},
        "biome": resolved_biome,
    }
    if terrain_grid is not None:
        result["tileset"] = {
            "name": "castle",
            "columns": TILESET_IMAGE_WIDTH // tile_size,
            "tile_size": tile_size,
        }
        result["tiles"] = {"terrain": terrain_grid}
    result["colliders"] = colliders
    result["entities"] = entities
    return result


# --------------------------------------------------------------------------
# JumpCastle -> Tiled
# --------------------------------------------------------------------------
def _objectgroup(
    layer_id: int, name: str, objects: list[dict[str, Any]]
) -> dict[str, Any]:
    return {
        "id": layer_id,
        "name": name,
        "type": "objectgroup",
        "visible": True,
        "opacity": 1,
        "draworder": "topdown",
        "x": 0,
        "y": 0,
        "objects": objects,
    }


def _tilelayer(
    layer_id: int, name: str, width: int, height: int, grid: list[list[int]]
) -> dict[str, Any]:
    """A Tiled tile layer with flat row-major GID data (all zeros if grid empty)."""
    if grid:
        data = [int(gid) for row in grid for gid in row]
    else:
        data = [0] * (width * height)
    return {
        "id": layer_id,
        "name": name,
        "type": "tilelayer",
        "width": width,
        "height": height,
        "visible": True,
        "opacity": 1,
        "x": 0,
        "y": 0,
        "data": data,
    }


def screen_map_to_tiled(
    screen: dict[str, Any], *, tile_size: int = TILE_SIZE
) -> dict[str, Any]:
    """Convert a JumpCastle screen-map dict into a parsed Tiled map dict.

    Colliders become polygon objects (x/y origin 0, absolute px vertices);
    entities become point objects. Screen index and biome are stored as map
    custom properties so a later Tiled->JumpCastle pass can recover them.
    """
    next_object_id = 1
    collider_objects: list[dict[str, Any]] = []
    for collider in screen.get("colliders", []):
        polygon = [
            {"x": px * tile_size, "y": py * tile_size}
            for px, py in collider["points"]
        ]
        collider_objects.append(
            {
                "id": next_object_id,
                "name": "",
                "type": collider["type"],
                "x": 0,
                "y": 0,
                "width": 0,
                "height": 0,
                "rotation": 0,
                "visible": True,
                "polygon": polygon,
            }
        )
        next_object_id += 1

    entity_objects: list[dict[str, Any]] = []
    for entity in screen.get("entities", []):
        entity_objects.append(
            {
                "id": next_object_id,
                "name": "",
                "type": entity["type"],
                "x": entity["pos"][0] * tile_size,
                "y": entity["pos"][1] * tile_size,
                "width": 0,
                "height": 0,
                "point": True,
                "rotation": 0,
                "visible": True,
            }
        )
        next_object_id += 1

    screen_meta = screen.get("screen", {})
    width = int(screen_meta.get("width", 0))
    height = int(screen_meta.get("height", 0))
    tiles = screen.get("tiles") or {}
    terrain_grid = tiles.get("terrain", []) if isinstance(tiles, dict) else []

    return {
        "type": "map",
        "version": "1.10",
        "tiledversion": "1.10.2",
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "infinite": False,
        "width": width,
        "height": height,
        "tilewidth": tile_size,
        "tileheight": tile_size,
        "nextlayerid": 4,
        "nextobjectid": next_object_id,
        "tilesets": [{"firstgid": 1, "source": TILESET_SOURCE}],
        "properties": [
            {"name": "index", "type": "int", "value": int(screen_meta.get("index", 0))},
            {"name": "biome", "type": "string", "value": screen.get("biome", DEFAULT_BIOME)},
        ],
        "layers": [
            _tilelayer(1, "terrain", width, height, terrain_grid),
            _objectgroup(2, "collision", collider_objects),
            _objectgroup(3, "entities", entity_objects),
        ],
    }


# --------------------------------------------------------------------------
# CLI: batch-convert a directory of screens
# --------------------------------------------------------------------------
def _screen_index_from_name(path: Path, tmj: dict[str, Any]) -> int:
    prop = _map_property(tmj, "index")
    if isinstance(prop, int):
        return prop
    match = SCREEN_INDEX_PATTERN.search(path.stem)
    if not match:
        raise ValueError(f"cannot determine screen index from {path.name}")
    return int(match.group(1))


def _write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def convert_to_tiled(input_dir: Path, output_dir: Path, tile_size: int) -> int:
    count = 0
    for map_path in sorted(input_dir.glob("screen-*.map.json")):
        screen = json.loads(map_path.read_text(encoding="utf-8"))
        tmj = screen_map_to_tiled(screen, tile_size=tile_size)
        out = output_dir / f"{map_path.stem.replace('.map', '')}.tmj"
        _write_json(out, tmj)
        count += 1
    return count


def convert_from_tiled(input_dir: Path, output_dir: Path, tile_size: int) -> int:
    count = 0
    for tmj_path in sorted(input_dir.glob("screen-*.tmj")):
        tmj = json.loads(tmj_path.read_text(encoding="utf-8"))
        index = _screen_index_from_name(tmj_path, tmj)
        screen = tiled_to_screen_map(tmj, index=index, tile_size=tile_size)
        out = output_dir / f"screen-{index:02d}.map.json"
        _write_json(out, screen)
        count += 1
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "direction",
        choices=("to-tiled", "from-tiled"),
        help="to-tiled: .map.json -> .tmj ; from-tiled: .tmj -> .map.json",
    )
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--tile-size", type=int, default=TILE_SIZE)
    args = parser.parse_args()

    if args.direction == "to-tiled":
        written = convert_to_tiled(args.input_dir, args.output_dir, args.tile_size)
    else:
        written = convert_from_tiled(args.input_dir, args.output_dir, args.tile_size)
    print(f"{args.direction}: wrote {written} file(s) to {args.output_dir}")


if __name__ == "__main__":
    main()
