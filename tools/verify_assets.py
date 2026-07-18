#!/usr/bin/env python3
"""Validate the committed generated asset manifest without third-party packages."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


APPROVED_SOURCES = {
    "pixel_adventure": "https://pixelfrog-assets.itch.io/pixel-adventure-1",
    "kenney": "https://kenney.nl/assets/pixel-platformer",
    "kings_and_pigs": "https://pixelfrog-assets.itch.io/kings-and-pigs",
    "gothicvania_hero": "https://ansimuz.itch.io/gothicvania-swamp",
}
EXPECTED_ATLASES = {"pixel_adventure", "kenney", "kings_and_pigs", "player"}
EXPECTED_ANIMATIONS = {"idle", "run", "charge", "rise", "fall", "respawn"}


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
    require(manifest.get("schema_version") == 1, "unsupported manifest schema")
    require(manifest.get("license") == "CC0-1.0", "manifest license must be CC0-1.0")

    sources = manifest.get("sources")
    require(isinstance(sources, list), "sources must be an array")
    source_map = {source.get("id"): source for source in sources}
    require(set(source_map) == set(APPROVED_SOURCES), "source IDs are not approved")
    for source_id, url in APPROVED_SOURCES.items():
        source = source_map[source_id]
        require(source.get("url") == url, f"unexpected URL for {source_id}")
        require(source.get("license") == "CC0-1.0", f"unexpected license for {source_id}")
        require(str(source.get("archive", "")).lower().endswith(".zip"),
                f"missing archive metadata for {source_id}")

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
        for region_id, region in atlas.get("regions", {}).items():
            check_rect(region, width, height, f"{atlas_id}.{region_id}")

        if atlas_id != "player":
            grid = atlas.get("terrain_grid")
            require(isinstance(grid, dict), f"atlas {atlas_id} has no terrain grid")
            require(grid.get("tile_size") == 16, f"atlas {atlas_id} tile size must be 16")
            require(grid.get("columns") == 7 and grid.get("rows") == 5,
                    f"atlas {atlas_id} terrain grid changed unexpectedly")
            check_rect({
                "x": grid.get("x"),
                "y": grid.get("y"),
                "width": grid.get("columns", 0) * grid.get("tile_size", 0),
                "height": grid.get("rows", 0) * grid.get("tile_size", 0),
            }, width, height, f"{atlas_id}.terrain_grid")

    animations = manifest.get("animations")
    require(isinstance(animations, dict), "animations must be an object")
    require(set(animations) == EXPECTED_ANIMATIONS, "King animation states are incomplete")
    for state, animation in animations.items():
        require(animation.get("atlas") == "player", f"{state} uses the wrong atlas")
        require(isinstance(animation.get("fps"), int) and animation["fps"] > 0,
                f"{state} has invalid FPS")
        frames = animation.get("frames")
        require(isinstance(frames, list) and frames, f"{state} has no frames")
        width, height = atlas_sizes["player"]
        for index, frame in enumerate(frames):
            check_rect(frame, width, height, f"animation.{state}[{index}]")

    repository = manifest_path.parents[2]
    tracked = subprocess.run(
        ["git", "-C", str(repository), "ls-files", "*.zip"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    require(not tracked, "source ZIP archives must not be tracked by git")

    print(
        f"verified {len(atlases)} atlases, {len(animations)} King animations, "
        f"and {len(sources)} CC0 sources")


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
