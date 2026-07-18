#!/usr/bin/env python3
"""Build the hand-authored narrow tower campaign collision grid."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

WIDTH = 28
BAND_HEIGHT = 36
SCREEN_COUNT = 18
HEIGHT = BAND_HEIGHT * SCREEN_COUNT

ROUTE_Y = (35, 32, 29, 26, 23, 20, 17, 14, 11, 8, 5, 2)
ROUTE_X = (
    (1, 7, 1, 7, 1, 7, 1, 7, 13, 7, 13, 19),
    (13, 19, 13, 19, 13, 7, 1, 7, 1, 7, 13, 7),
    (1, 7, 13, 7, 1, 7, 13, 19, 13, 19, 13, 19),
    (13, 19, 13, 7, 1, 7, 1, 7, 1, 7, 1, 7),
    (1, 7, 1, 7, 13, 19, 13, 19, 13, 7, 1, 7),
    (13, 19, 13, 7, 1, 7, 13, 7, 1, 7, 13, 19),
    (13, 7, 1, 7, 13, 19, 13, 7, 1, 7, 13, 19),
    (13, 7, 13, 19, 13, 19, 13, 19, 13, 19, 13, 7),
    (13, 7, 13, 19, 13, 7, 1, 7, 1, 7, 1, 7),
    (13, 7, 13, 7, 13, 7, 1, 7, 13, 19, 13, 7),
    (13, 7, 13, 19, 13, 7, 1, 7, 13, 19, 13, 19),
    (13, 19, 13, 19, 13, 19, 13, 7, 13, 19, 13, 7),
    (1, 7, 1, 7, 13, 19, 13, 19, 13, 7, 13, 7),
    (1, 7, 13, 19, 13, 7, 13, 7, 13, 19, 13, 19),
    (13, 7, 13, 19, 13, 7, 1, 7, 1, 7, 13, 7),
    (13, 19, 13, 7, 1, 7, 1, 7, 13, 7, 13, 19),
    (13, 19, 13, 19, 13, 7, 13, 19, 13, 7, 1, 7),
    (1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 13, 19),
)

@dataclass(frozen=True, order=True)
class SolidRect:
    x: int
    y: int
    width: int
    height: int = 1


@dataclass(frozen=True)
class Chamber:
    name: str
    mechanic: str
    route: tuple[SolidRect, ...]
    obstacles: tuple[SolidRect, ...] = ()

    @property
    def solids(self) -> tuple[SolidRect, ...]:
        return self.route + self.obstacles

    @property
    def route_jump_count(self) -> int:
        return len(self.route) - 1


COURTYARD = (
    ("Training Ascent", "training_ascent", (SolidRect(20, 34, 7),)),
    (
        "Long-Gap Court",
        "long_gap",
        (SolidRect(22, 25, 5), SolidRect(0, 16, 4)),
    ),
    ("Rebound Alley", "wall_rebound", (SolidRect(25, 10, 2, 16),)),
    ("Low-Ceiling Hall", "low_ceiling", (SolidRect(12, 22, 6, 2),)),
    ("Central Tower", "central_tower", (SolidRect(12, 24, 4, 8),)),
    (
        "Gatehouse Exam",
        "gatehouse_exam",
        (
            SolidRect(0, 9, 1, 12),
            SolidRect(25, 18, 2, 12),
            SolidRect(6, 31, 6, 2),
        ),
    ),
)

FROSTED_KEEP = (
    (
        "Split Shaft",
        "split_shaft",
        (SolidRect(0, 18, 1, 10), SolidRect(27, 8, 1, 10)),
    ),
    (
        "Window Steps",
        "window_steps",
        (SolidRect(22, 30, 5), SolidRect(0, 18, 5)),
    ),
    (
        "Crossing Chamber",
        "crossing_chamber",
        (SolidRect(19, 34, 5), SolidRect(19, 28, 5)),
    ),
    (
        "Reversal Climb",
        "reversal_climb",
        (SolidRect(0, 21, 1, 12), SolidRect(27, 7, 1, 12)),
    ),
    (
        "Narrow Gallery",
        "narrow_gallery",
        (SolidRect(22, 21, 5, 2), SolidRect(0, 12, 6, 2)),
    ),
    (
        "Bell-Tower Exam",
        "bell_tower_exam",
        (
            SolidRect(0, 23, 1, 10),
            SolidRect(27, 8, 1, 10),
            SolidRect(1, 31, 7, 2),
        ),
    ),
)

CROWN_SPIRE = (
    (
        "Broken Bridge",
        "broken_bridge",
        (SolidRect(20, 34, 7), SolidRect(0, 28, 5)),
    ),
    (
        "Crown Chamber",
        "crown_chamber",
        (SolidRect(2, 12, 4, 10), SolidRect(22, 12, 4, 10)),
    ),
    (
        "Vertical Chimney",
        "vertical_chimney",
        (SolidRect(0, 6, 1, 24), SolidRect(27, 6, 1, 24)),
    ),
    (
        "Overhang Reversal",
        "overhang_reversal",
        (SolidRect(21, 24, 6, 2), SolidRect(0, 12, 6, 2)),
    ),
    (
        "Fall Funnel",
        "fall_funnel",
        (
            SolidRect(0, 30, 5),
            SolidRect(23, 24, 5),
            SolidRect(0, 18, 5),
            SolidRect(23, 12, 5),
        ),
    ),
    (
        "Throne Leap",
        "throne_leap",
        (
            SolidRect(0, 34, 5),
            SolidRect(22, 28, 6),
            SolidRect(0, 16, 5),
            SolidRect(22, 10, 6),
        ),
    ),
)


def route_width(screen: int) -> int:
    """Landing-platform width per biome. Narrower = more precise landings.

    Reachability of these widths depends on the solver's charge/launch sampling
    density (see SolverConfig); the spawn floor and goal ledge are special-cased
    in _base_chamber and are not governed by this policy.
    """
    if screen <= len(COURTYARD):  # Courtyard 1-6
        return 4
    if screen <= len(COURTYARD) + len(FROSTED_KEEP):  # Frosted Keep 7-12
        return 5
    return 5  # Crown Spire 13-18


def _base_chamber(screen: int, route_x: tuple[int, ...]) -> Chamber:
    width = route_width(screen)
    route = tuple(
        SolidRect(
            0 if screen == 1 and y == BAND_HEIGHT - 1 else x,
            y,
            WIDTH if screen == 1 and y == BAND_HEIGHT - 1 else (
                6 if screen == SCREEN_COUNT and y == ROUTE_Y[-1] else width
            ),
        )
        for x, y in zip(route_x, ROUTE_Y, strict=True)
    )
    if screen <= len(COURTYARD):
        name, mechanic, obstacles = COURTYARD[screen - 1]
    elif screen <= len(COURTYARD) + len(FROSTED_KEEP):
        name, mechanic, obstacles = FROSTED_KEEP[screen - len(COURTYARD) - 1]
    else:
        name, mechanic, obstacles = CROWN_SPIRE[
            screen - len(COURTYARD) - len(FROSTED_KEEP) - 1
        ]
    return Chamber(name=name, mechanic=mechanic, route=route, obstacles=obstacles)


CHAMBERS = tuple(
    _base_chamber(screen, route_x)
    for screen, route_x in enumerate(ROUTE_X, start=1)
)


def _validate(chambers: tuple[Chamber, ...]) -> None:
    if len(chambers) != SCREEN_COUNT:
        raise ValueError(f"expected {SCREEN_COUNT} chambers, got {len(chambers)}")
    for screen, chamber in enumerate(chambers, start=1):
        occupied: set[tuple[int, int]] = set()
        for rect in chamber.solids:
            if (
                rect.x < 0
                or rect.y < 0
                or rect.width <= 0
                or rect.height <= 0
                or rect.x + rect.width > WIDTH
                or rect.y + rect.height > BAND_HEIGHT
            ):
                raise ValueError(f"screen {screen} has out-of-bounds solid {rect}")
            for y in range(rect.y, rect.y + rect.height):
                for x in range(rect.x, rect.x + rect.width):
                    if (x, y) in occupied:
                        raise ValueError(
                            f"screen {screen} has overlapping solid at ({x}, {y})"
                        )
                    occupied.add((x, y))


def render_campaign(chambers: tuple[Chamber, ...]) -> str:
    _validate(chambers)
    grid = [["." for _ in range(WIDTH)] for _ in range(HEIGHT)]
    for screen, chamber in enumerate(chambers, start=1):
        band_top = (SCREEN_COUNT - screen) * BAND_HEIGHT
        for rect in chamber.solids:
            for local_y in range(rect.y, rect.y + rect.height):
                for x in range(rect.x, rect.x + rect.width):
                    grid[band_top + local_y][x] = "#"

    metadata = (
        "version 2\n"
        "tile_size 16\n"
        "size 28 648\n"
        "screen_height 36\n"
        "spawn 3 646\n"
        "goal 24 1\n"
        "biome 1 6 courtyard\n"
        "biome 7 12 frosted_keep\n"
        "biome 13 18 crown_spire\n"
        "---\n"
        "[collision]\n"
    )
    return metadata + "\n".join("".join(row) for row in grid) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output.write_text(render_campaign(CHAMBERS), encoding="utf-8")


if __name__ == "__main__":
    main()
