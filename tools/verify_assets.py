#!/usr/bin/env python3
"""Validate the committed schema-v2 asset manifest without third-party packages."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


APPROVED_SOURCES = {
    "castle_tileset": "https://opengameart.org/content/pixel-art-castle-tileset",
    "gloomy_knight": "https://loveosstudio.itch.io/gloomy-knight-16x16",
    "kenney_ui": "https://kenney.nl/assets/ui-pack-pixel-adventure",
    "sunnyland_winter": "https://ansimuz.itch.io/sunnyland-forest",
    "gothicvania_swamp": "https://ansimuz.itch.io/gothicvania-swamp",
}
EXPECTED_ATLASES = {"castle", "knight", "ui"}
EXPECTED_BIOMES = {"courtyard", "frosted_keep", "crown_spire"}
EXPECTED_ANIMATIONS = {"idle", "run", "charge", "rise", "fall", "respawn"}
EXPECTED_UI = {"panel", "button", "button_pressed", "keycap"}
# Mirrors NAMED_SLOT_POSITIONS in tools/build_assets.py (the 3x3 nine-slice
# hard contract plus the enrichment slots) and the four semantic aliases the
# game resolves directly (spike/background alias hazard/isolated cells).
EXPECTED_TERRAIN_REGIONS = {
    "top_left", "top", "top_right",
    "left", "center", "right",
    "bottom_left", "bottom", "bottom_right",
    "inner_corner_tl", "inner_corner_tr", "inner_corner_bl", "inner_corner_br",
    "platform_left", "platform_mid", "platform_right",
    "ledge", "pillar", "isolated", "detail_1", "detail_2", "hazard",
    "spike", "checkpoint", "exit", "background",
    "slope_ne", "slope_nw", "slope_se", "slope_sw",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    require(data[:8] == b"\x89PNG\r\n\x1a\n", f"{path} is not a PNG")
    return struct.unpack(">II", data[16:24])


def check_rect(region: object, width: int, height: int, label: str) -> None:
    require(isinstance(region, dict), f"{label} must be an object")
    values = [region.get(key) for key in ("x", "y", "width", "height")]
    require(all(isinstance(value, int) for value in values), f"{label} is malformed")
    x, y, region_width, region_height = values
    require(x >= 0 and y >= 0, f"{label} has a negative origin")
    require(region_width > 0 and region_height > 0, f"{label} is empty")
    require(x + region_width <= width, f"{label} exceeds atlas width")
    require(y + region_height <= height, f"{label} exceeds atlas height")


def verify(manifest_path: Path) -> None:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest.get("schema_version") == 2, "unsupported manifest schema")
    require(manifest.get("license") == "CC0-1.0", "manifest license must be CC0-1.0")

    sources = manifest.get("sources")
    require(isinstance(sources, list), "sources must be an array")
    source_map = {source.get("id"): source for source in sources}
    require(set(source_map) == set(APPROVED_SOURCES), "source IDs are not approved")
    for source_id, url in APPROVED_SOURCES.items():
        source = source_map[source_id]
        require(source.get("url") == url, f"unexpected URL for {source_id}")
        require(source.get("license") == "CC0-1.0", f"unexpected license for {source_id}")
        files = source.get("files")
        require(isinstance(files, list) and files, f"missing source files for {source_id}")
        for file in files:
            require(isinstance(file.get("name"), str), f"missing filename for {source_id}")
            digest = file.get("sha256")
            require(isinstance(digest, str) and len(digest) == 64,
                    f"missing SHA-256 for {source_id}")

    atlases = manifest.get("atlases")
    require(isinstance(atlases, dict), "atlases must be an object")
    require(set(atlases) == EXPECTED_ATLASES, "atlas names do not match runtime contract")
    atlas_sizes: dict[str, tuple[int, int]] = {}
    for atlas_id, atlas in atlases.items():
        require(isinstance(atlas, dict), f"atlas {atlas_id} must be an object")
        filename = atlas.get("file")
        require(isinstance(filename, str) and Path(filename).name == filename,
                f"atlas {atlas_id} has unsafe filename")
        require(filename.endswith(".png"), f"atlas {atlas_id} is not PNG")
        path = manifest_path.parent / filename
        require(path.is_file(), f"atlas file is missing: {filename}")
        width, height = png_size(path)
        require((atlas.get("width"), atlas.get("height")) == (width, height),
                f"atlas dimensions do not match PNG: {filename}")
        require(hashlib.sha256(path.read_bytes()).hexdigest() == atlas.get("sha256"),
                f"atlas digest does not match: {filename}")
        atlas_sizes[atlas_id] = (width, height)

    castle = atlases["castle"]
    biomes = castle.get("biomes")
    require(isinstance(biomes, dict) and set(biomes) == EXPECTED_BIOMES,
            "castle biome records are incomplete")
    castle_width, castle_height = atlas_sizes["castle"]
    for biome_name, biome in biomes.items():
        grid = biome.get("terrain_grid")
        require(isinstance(grid, dict), f"{biome_name} has no terrain grid")
        require(grid.get("tile_size") == 16, f"{biome_name} tile size must be 16")
        require(grid.get("columns") == 8 and grid.get("rows") == 8,
                f"{biome_name} terrain grid changed unexpectedly")
        check_rect({
            "x": grid.get("x"),
            "y": grid.get("y"),
            "width": grid.get("columns", 0) * grid.get("tile_size", 0),
            "height": grid.get("rows", 0) * grid.get("tile_size", 0),
        }, castle_width, castle_height, f"{biome_name}.terrain_grid")
        regions = biome.get("regions")
        require(isinstance(regions, dict) and
                set(regions) == EXPECTED_TERRAIN_REGIONS,
                f"{biome_name} terrain regions are incomplete")
        for name, region in regions.items():
            check_rect(region, castle_width, castle_height, f"{biome_name}.{name}")

    ui = atlases["ui"]
    require(set(ui.get("regions", {})) == EXPECTED_UI, "UI regions are incomplete")
    for name, region in ui["regions"].items():
        check_rect(region, *atlas_sizes["ui"], f"ui.{name}")

    animations = manifest.get("animations")
    require(isinstance(animations, dict), "animations must be an object")
    require(set(animations) == EXPECTED_ANIMATIONS, "knight animation states are incomplete")
    for state, animation in animations.items():
        require(animation.get("atlas") == "knight", f"{state} uses the wrong atlas")
        require(isinstance(animation.get("fps"), int) and animation["fps"] > 0,
                f"{state} has invalid FPS")
        frames = animation.get("frames")
        require(isinstance(frames, list) and frames, f"{state} has no frames")
        for index, frame in enumerate(frames):
            check_rect(frame, *atlas_sizes["knight"], f"animation.{state}[{index}]")

    repository = manifest_path.parents[2]
    tracked = subprocess.run(
        ["git", "-C", str(repository), "ls-files", "*.zip"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    require(not tracked, "source ZIP archives must not be tracked by git")

    print(
        f"verified {len(atlases)} atlases, {len(animations)} knight animations, "
        f"and {len(sources)} approved CC0 sources"
    )


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_assets.py MANIFEST", file=sys.stderr)
        return 2
    try:
        verify(Path(sys.argv[1]).resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"asset verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
