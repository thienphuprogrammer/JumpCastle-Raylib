"""Tests for the deterministic 17-style screen painter (tools/paint_screen.py).

The painter reads a screen's collision polygons and repaints its
``tiles.terrain`` GID grid in the chunky carved-stone style of the
hand-painted screen-17, following the visual grammar in
``docs/superpowers/specs/2026-07-20-level-redesign-17-style-design.md`` §5.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from paint_screen import (  # noqa: E402
    ACCENT_GIDS,
    COLUMN_GID,
    COVERAGE_MAX,
    COVERAGE_MIN,
    FILL_GID,
    ROLE_BRIDGE,
    ROLE_COLUMN,
    ROLE_EDGE,
    ROLE_FILL,
    ROLE_HAZARD,
    ROLE_SLOPE,
    check_layout,
    classify_cell,
    coverage,
    load_biome_gids,
    paint_grid,
    rasterize_colliders,
    should_paint,
    slope_cells,
)

WIDTH = 28
HEIGHT = 36


def _rect(x: int, y: int, w: int, h: int, kind: str = "solid") -> dict:
    """A grid-aligned rectangular collider spanning [x,x+w) x [y,y+h) tiles."""
    return {
        "id": 0,
        "type": kind,
        "points": [
            [float(x), float(y)],
            [float(x + w), float(y)],
            [float(x + w), float(y + h)],
            [float(x), float(y + h)],
        ],
    }


def _screen(colliders: list[dict], *, index: int = 12, biome: str = "courtyard",
            entities: list[dict] | None = None) -> dict:
    return {
        "schema_version": 2,
        "screen": {"index": index, "width": float(WIDTH), "height": float(HEIGHT)},
        "biome": biome,
        "tileset": "castle",
        "tiles": {"terrain": [[0] * WIDTH for _ in range(HEIGHT)]},
        "colliders": colliders,
        "entities": entities or [],
    }


# --- role classification (spec §5) ------------------------------------------

def test_interior_cell_classified_as_fill():
    # A 4x4 solid block: the (r=6,c=6) cell is fully enclosed on all 8 sides.
    solid, hazard = rasterize_colliders([_rect(5, 5, 4, 4)], WIDTH, HEIGHT)
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 6, 6) == ROLE_FILL


def test_top_of_block_classified_as_edge():
    solid, hazard = rasterize_colliders([_rect(5, 5, 4, 4)], WIDTH, HEIGHT)
    # top row of the block (row 5) has open air above -> boundary edge.
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 5, 6) == ROLE_EDGE


def test_one_wide_vertical_stack_classified_as_column():
    # A 1-wide, 4-tall solid stack: middle cell is open left & right.
    solid, hazard = rasterize_colliders([_rect(10, 4, 1, 4)], WIDTH, HEIGHT)
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 5, 10) == ROLE_COLUMN


def test_one_thick_horizontal_plank_classified_as_bridge():
    # A 4-wide, 1-tall plank: middle cell is open above & below.
    solid, hazard = rasterize_colliders([_rect(3, 20, 4, 1)], WIDTH, HEIGHT)
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 20, 4) == ROLE_BRIDGE


def test_hazard_cell_classified_as_hazard():
    solid, hazard = rasterize_colliders([_rect(8, 8, 2, 2, kind="hazard")],
                                        WIDTH, HEIGHT)
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 8, 8) == ROLE_HAZARD


# --- palette (spec §5, courtyard 17-palette) --------------------------------

def test_courtyard_fill_uses_screen17_stone_gid():
    grid = paint_grid(_screen([_rect(5, 5, 5, 5)]), index=12)
    assert grid[6][6] == FILL_GID  # interior fill -> local id 191 -> gid 192


def test_courtyard_column_uses_screen17_column_gid():
    grid = paint_grid(_screen([_rect(10, 4, 1, 5)]), index=12)
    assert grid[6][10] == COLUMN_GID  # local id 161 -> gid 162


# --- determinism (spec §5 "no RNG", acceptance §4) --------------------------

def test_paint_grid_is_deterministic():
    colliders = [_rect(4, 4, 6, 3), _rect(14, 10, 5, 5), _rect(20, 6, 1, 8)]
    a = paint_grid(_screen(colliders, index=13), index=13)
    b = paint_grid(_screen(colliders, index=13), index=13)
    assert a == b


def test_accents_are_deterministic_and_bounded():
    # A large solid mass leaves room for accents; they must be capped (<=12)
    # and identical across runs (fixed hash, no RNG).
    colliders = [_rect(2, 2, 20, 20)]
    grid = paint_grid(_screen(colliders, index=14), index=14)
    n_accents = sum(cell in ACCENT_GIDS for row in grid for cell in row)
    assert 0 < n_accents <= 12


# --- coverage bounds (spec §5) ----------------------------------------------

def test_coverage_reports_painted_fraction():
    grid = [[0] * 10 for _ in range(10)]
    for c in range(10):
        grid[0][c] = FILL_GID  # 10 of 100 cells painted
    assert coverage(grid) == pytest.approx(0.10)


def test_check_layout_flags_coverage_below_minimum():
    # A single 2x2 block on a 28x36 screen is ~0.4% painted, far under 35%.
    screen = _screen([_rect(5, 5, 2, 2)], index=12)
    violations = check_layout(screen)
    assert any("coverage" in v.lower() for v in violations)


# --- screen-17 skip behavior (spec §7) --------------------------------------

def test_screen_17_is_skipped_by_default():
    assert should_paint(17, include_17=False) is False


def test_screen_17_painted_only_when_explicitly_included():
    assert should_paint(17, include_17=True) is True


def test_non_reference_screens_always_painted():
    assert should_paint(12, include_17=False) is True


# --- entity support validation (spec §4 item 7) -----------------------------

def test_check_layout_flags_entity_without_supporting_surface():
    # spawn floating in mid-air: feet at y=20 but no solid top edge near it.
    screen = _screen(
        [_rect(2, 2, 24, 2)],  # a ceiling far above the entity's feet
        index=12,
        entities=[{"type": "spawn", "pos": [14.0, 20.0]}],
    )
    violations = check_layout(screen)
    assert any("spawn" in v.lower() and "support" in v.lower() for v in violations)


def test_check_layout_accepts_entity_standing_on_surface():
    # spawn feet at y=30 resting on the top edge of a solid block at row 30.
    screen = _screen(
        [_rect(10, 30, 6, 6)],
        index=12,
        entities=[{"type": "spawn", "pos": [12.5, 30.0]}],
    )
    violations = [v for v in check_layout(screen) if "support" in v.lower()]
    assert violations == []


# --- slopes (spec 2026-07-23-sloped-terrain-design.md, Option A) ------------

def _slope(x0: int, y0: int, x1: int, y1: int) -> dict:
    """Right-triangle slope collider rising from (x0,y0) to the top-right."""
    return {
        "id": 0, "type": "solid", "shape": "slope",
        "points": [[float(x0), float(y0)], [float(x1), float(y0)],
                   [float(x1), float(y1)]],
    }


def test_slope_cells_marks_triangle_interior():
    # Triangle (6,20)-(12,20)-(12,17): hypotenuse rises right→up.
    grid = slope_cells([_slope(6, 20, 12, 17)], WIDTH, HEIGHT)
    assert grid[19][7] is True    # under the incline
    assert grid[5][5] is False    # far outside


def test_top_slope_cell_classified_as_slope():
    cols = [_slope(6, 20, 12, 17)]
    solid, hazard = rasterize_colliders(cols, WIDTH, HEIGHT)
    sl = slope_cells(cols, WIDTH, HEIGHT)
    # (19,7) is a slope cell with open air above -> a walkable slope surface.
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 19, 7, slope=sl) == ROLE_SLOPE


def test_classify_without_slope_grid_is_unchanged():
    # Backward compatibility: omitting the slope arg keeps the old behavior.
    solid, hazard = rasterize_colliders([_rect(5, 5, 5, 5)], WIDTH, HEIGHT)
    assert classify_cell(solid, hazard, WIDTH, HEIGHT, 6, 6) == ROLE_FILL


def test_paint_grid_renders_slope_tile_when_biome_provides_one():
    cols = [_slope(6, 20, 12, 17)]
    gids = dict(load_biome_gids("courtyard"))
    gids["slope_ne"] = 999  # a distinct NE-rising diagonal tile
    grid = paint_grid(_screen(cols, index=12), index=12, biome_gids=gids)
    assert grid[19][7] == 999  # the walkable slope top uses the diagonal tile


def _octagon(cx: float, cy: float) -> dict:
    """Octagon inscribing a 2x2 disc centred at (cx,cy) tagged shape:'round'."""
    d = 0.5
    return {"id": 0, "type": "solid", "shape": "round", "points": [
        [cx - 1, cy - d], [cx - d, cy - 1], [cx + d, cy - 1], [cx + 1, cy - d],
        [cx + 1, cy + d], [cx + d, cy + 1], [cx - d, cy + 1], [cx - 1, cy + d]]}


def test_round_cells_tags_quadrants():
    from paint_screen import round_cells
    grid = round_cells([_octagon(10, 10)], WIDTH, HEIGHT)
    assert grid[9][9] == "round_tl"    # up-left of centre (10,10)
    assert grid[10][10] == "round_br"  # down-right of centre
    assert grid[5][5] is None          # outside


def test_paint_grid_renders_round_tile_by_quadrant():
    gids = dict(load_biome_gids("courtyard"))
    gids["round_tl"] = 777
    grid = paint_grid(_screen([_octagon(10, 10)], index=12), index=12, biome_gids=gids)
    assert grid[9][9] == 777  # top-left quadrant uses the round_tl tile
