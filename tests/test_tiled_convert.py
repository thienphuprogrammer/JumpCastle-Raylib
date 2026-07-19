"""Tests for the JumpCastle <-> Tiled screen-map converter."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from tiled_convert import (  # noqa: E402
    screen_map_to_tiled,
    tiled_to_screen_map,
)

TILE = 16


def _floor_screen() -> dict:
    return {
        "schema_version": 1,
        "screen": {"index": 3, "width": 28.0, "height": 36.0},
        "biome": "frosted_keep",
        "colliders": [
            {"id": 1, "type": "solid", "points": [[0, 35], [28, 35], [28, 36], [0, 36]]},
            {"id": 2, "type": "hazard", "points": [[10, 20], [12, 20], [11, 22]]},
        ],
        "entities": [
            {"type": "spawn", "pos": [3, 34]},
            {"type": "goal", "pos": [24, 1]},
        ],
    }


def test_screen_to_tiled_scales_tile_units_to_pixels():
    tmj = screen_map_to_tiled(_floor_screen())

    assert tmj["width"] == 28 and tmj["height"] == 36
    assert tmj["tilewidth"] == TILE and tmj["tileheight"] == TILE

    collision = next(l for l in tmj["layers"] if l["name"] == "collision")
    floor = collision["objects"][0]
    assert floor["type"] == "solid"
    # First vertex (0, 35) tiles -> (0, 560) px.
    assert floor["polygon"][0] == {"x": 0, "y": 35 * TILE}
    assert floor["polygon"][1] == {"x": 28 * TILE, "y": 35 * TILE}

    entities = next(l for l in tmj["layers"] if l["name"] == "entities")
    spawn = entities["objects"][0]
    assert spawn["point"] is True
    assert spawn["type"] == "spawn"
    assert spawn["x"] == 3 * TILE and spawn["y"] == 34 * TILE


def test_round_trip_preserves_geometry_and_metadata():
    original = _floor_screen()
    tmj = screen_map_to_tiled(original)
    restored = tiled_to_screen_map(tmj, index=original["screen"]["index"])

    assert restored["screen"] == original["screen"]
    assert restored["biome"] == original["biome"]
    assert len(restored["colliders"]) == len(original["colliders"])
    for got, want in zip(restored["colliders"], original["colliders"]):
        assert got["type"] == want["type"]
        # tile -> px (*16) -> tile (/16) is exact for tile and quarter-tile coords
        assert got["points"] == want["points"]
    assert restored["entities"] == original["entities"]


def test_biome_and_index_recovered_from_map_properties():
    tmj = screen_map_to_tiled(_floor_screen())
    # Drop the explicit biome argument: it must come from the map property.
    restored = tiled_to_screen_map(tmj, index=99)
    assert restored["biome"] == "frosted_keep"
    assert restored["screen"]["index"] == 99  # explicit index wins


def test_tiled_rectangle_becomes_four_corner_collider():
    tmj = {
        "width": 28,
        "height": 36,
        "layers": [
            {
                "type": "objectgroup",
                "name": "collision",
                "objects": [
                    {"id": 1, "type": "solid", "x": 32, "y": 48, "width": 64, "height": 16}
                ],
            }
        ],
    }
    screen = tiled_to_screen_map(tmj, index=1)
    assert len(screen["colliders"]) == 1
    pts = screen["colliders"][0]["points"]
    # (32,48)px -> (2,3) tiles ; +64x16 px -> (+4,+1) tiles.
    assert pts == [[2, 3], [6, 3], [6, 4], [2, 4]]


def test_polygon_origin_offset_is_applied():
    tmj = {
        "width": 28,
        "height": 36,
        "layers": [
            {
                "type": "objectgroup",
                "name": "collision",
                "objects": [
                    {
                        "id": 1,
                        "type": "oneway",
                        "x": 160,  # origin 10 tiles right
                        "y": 0,
                        "polygon": [{"x": 0, "y": 0}, {"x": 32, "y": 0}, {"x": 16, "y": 32}],
                    }
                ],
            }
        ],
    }
    screen = tiled_to_screen_map(tmj, index=1)
    assert screen["colliders"][0]["type"] == "oneway"
    assert screen["colliders"][0]["points"] == [[10, 0], [12, 0], [11, 2]]


def test_class_key_is_accepted_as_alias_for_type():
    tmj = {
        "width": 28,
        "height": 36,
        "layers": [
            {
                "type": "objectgroup",
                "name": "entities",
                "objects": [
                    {"id": 1, "class": "spawn", "x": 48, "y": 560, "point": True}
                ],
            }
        ],
    }
    screen = tiled_to_screen_map(tmj, index=1)
    assert screen["entities"] == [{"type": "spawn", "pos": [3, 35]}]


def test_castle_tsx_matches_the_atlas():
    tsx = (
        Path(__file__).resolve().parents[1]
        / "assets" / "levels" / "tiled" / "castle.tsx"
    ).read_text(encoding="utf-8")
    assert 'name="castle"' in tsx
    assert 'columns="21"' in tsx
    assert 'tilecount="126"' in tsx
    assert 'tilewidth="16"' in tsx and 'tileheight="16"' in tsx
    assert 'source="../../generated/castle.png"' in tsx
    assert 'width="336"' in tsx and 'height="96"' in tsx


def test_untyped_and_polyline_objects_are_ignored():
    tmj = {
        "width": 28,
        "height": 36,
        "layers": [
            {
                "type": "objectgroup",
                "name": "misc",
                "objects": [
                    {"id": 1, "type": "", "x": 0, "y": 0, "width": 16, "height": 16},
                    {"id": 2, "type": "decoration", "x": 0, "y": 0, "point": True},
                    {"id": 3, "type": "solid", "x": 0, "y": 0,
                     "polyline": [{"x": 0, "y": 0}, {"x": 16, "y": 0}]},
                ],
            }
        ],
    }
    screen = tiled_to_screen_map(tmj, index=1)
    assert screen["colliders"] == []  # untyped rect, and polyline (not a shape)
    assert screen["entities"] == []  # 'decoration' is not an entity class
