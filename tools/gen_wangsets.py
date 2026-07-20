#!/usr/bin/env python3
"""Generate per-biome corner-type Wang sets and inject them into castle.tsx.

This is an authoring aid for hand-painting terrain in Tiled: it gives each
biome a "Terrain Brush" (a Tiled Wang set) so dragging tiles on the `terrain`
layer auto-places edges/corners instead of the user placing every tile by hand.

Reads `assets/generated/manifest.json` for the castle atlas's per-biome named
tile regions, computes each region's Tiled tileid, and emits a `<wangsets>`
block (one corner-type wangset per biome, 13 wangtiles each) using the fixed
corner-Wang mapping table below (see
docs/superpowers/plans/2026-07-20-tiled-handpaint-support-kit.md Task 1).

The block is inserted into `assets/levels/tiled/castle.tsx` via plain TEXT
replacement before `</tileset>` -- castle.tsx is intentionally NOT XML-parsed
here (it's a small, trusted, hand-verifiable format; a text insert avoids
adding a defusedxml dependency / stdlib-XML lints for this one-shot tool).

Idempotent: re-running replaces any existing `<wangsets>...</wangsets>` block
rather than duplicating it.

Regions not in the mapping table (platforms/decor/hazard/etc.) are not part
of the terrain fill and are skipped -- they stay manual tiles.

Usage:
    .venv/bin/python tools/gen_wangsets.py
"""

from __future__ import annotations

import json
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = REPO_ROOT / "assets" / "generated" / "manifest.json"
TSX_PATH = REPO_ROOT / "assets" / "levels" / "tiled" / "castle.tsx"

ATLAS_COLUMNS = 24
TILE_SIZE = 16

# Exact corner-Wang mapping table: wangid slots are
# (T, TR, R, BR, B, BL, L, TL). Only the four corner slots (TR=idx1, BR=idx3,
# BL=idx5, TL=idx7) are ever set to 1 (solid); the four edge slots
# (T, R, B, L) always stay 0. This is the plan's exact table -- do not modify
# without updating the plan doc.
WANGID_TABLE: dict[str, str] = {
    "center": "0,1,0,1,0,1,0,1",
    "top": "0,0,0,1,0,1,0,0",
    "bottom": "0,1,0,0,0,0,0,1",
    "left": "0,1,0,1,0,0,0,0",
    "right": "0,0,0,0,0,1,0,1",
    "top_left": "0,0,0,1,0,0,0,0",
    "top_right": "0,0,0,0,0,1,0,0",
    "bottom_left": "0,1,0,0,0,0,0,0",
    "bottom_right": "0,0,0,0,0,0,0,1",
    "inner_corner_tl": "0,1,0,1,0,1,0,0",
    "inner_corner_tr": "0,0,0,1,0,1,0,1",
    "inner_corner_bl": "0,1,0,1,0,0,0,1",
    "inner_corner_br": "0,1,0,0,0,1,0,1",
}

# Emission order for wangtiles within a wangset (matches the plan's table).
REGION_ORDER: list[str] = list(WANGID_TABLE.keys())

# One wangcolor ("solid") per biome; each biome gets its own wangset so the
# Tiled "Terrain Brush" only offers tiles from that biome's columns.
BIOME_COLORS: dict[str, str] = {
    "courtyard": "#b06a3a",
    "frosted_keep": "#8aa0c8",
    "crown_spire": "#6a8a4a",
}

# Preferred biome emission order; any biome present in the manifest but not
# listed here is appended afterwards (keeps the script correct even if the
# manifest gains a biome later).
BIOME_ORDER: list[str] = ["courtyard", "frosted_keep", "crown_spire"]

WANGSETS_BLOCK_RE = re.compile(r"[ \t]*<wangsets>.*?</wangsets>\n?", re.DOTALL)


def region_tileid(region: dict[str, int]) -> int:
    """Tiled tileid (0-based) for a manifest region, per the plan's formula:

    local = (region.y // 16) * 24 + (region.x // 16); tileid = local.
    """
    col = region["x"] // TILE_SIZE
    row = region["y"] // TILE_SIZE
    return row * ATLAS_COLUMNS + col


def build_wangset(biome: str, regions: dict[str, dict[str, int]]) -> tuple[str, list[tuple[str, int]]]:
    """Build one `<wangset>` XML block plus its region->tileid mapping."""
    color = BIOME_COLORS[biome]
    lines = [f'  <wangset name="{biome}_terrain" type="corner" tile="-1">']
    lines.append(f'   <wangcolor name="solid" color="{color}" tile="-1" probability="1"/>')

    mapping: list[tuple[str, int]] = []
    for region_name in REGION_ORDER:
        region = regions.get(region_name)
        if region is None:
            raise KeyError(f"biome {biome!r} is missing required region {region_name!r}")
        tileid = region_tileid(region)
        wangid = WANGID_TABLE[region_name]
        lines.append(f'   <wangtile tileid="{tileid}" wangid="{wangid}"/>')
        mapping.append((region_name, tileid))

    lines.append("  </wangset>")
    return "\n".join(lines), mapping


def build_wangsets_block(manifest: dict) -> tuple[str, dict[str, list[tuple[str, int]]]]:
    """Build the full `<wangsets>...</wangsets>` block for every biome."""
    biomes = manifest["atlases"]["castle"]["biomes"]
    order = [b for b in BIOME_ORDER if b in biomes]
    order += [b for b in biomes if b not in order]

    blocks: list[str] = []
    report: dict[str, list[tuple[str, int]]] = {}
    for biome in order:
        regions = biomes[biome]["regions"]
        block, mapping = build_wangset(biome, regions)
        blocks.append(block)
        report[biome] = mapping

    body = "\n".join(blocks)
    return f" <wangsets>\n{body}\n </wangsets>", report


def inject(tsx_text: str, wangsets_block: str) -> str:
    """Insert (or idempotently replace) the wangsets block in castle.tsx text."""
    existing = WANGSETS_BLOCK_RE.search(tsx_text)
    if existing:
        return tsx_text[: existing.start()] + wangsets_block + "\n" + tsx_text[existing.end() :]

    marker = "</tileset>"
    idx = tsx_text.rfind(marker)
    if idx == -1:
        raise ValueError(f"{TSX_PATH} has no </tileset> closing tag")
    return tsx_text[:idx] + wangsets_block + "\n" + tsx_text[idx:]


def print_report(report: dict[str, list[tuple[str, int]]]) -> None:
    print()
    print("biome -> region -> tileid (painting GID = tileid + 1)")
    for biome, mapping in report.items():
        print(f"[{biome}]")
        for region_name, tileid in mapping:
            print(f"  {region_name:<16} tileid={tileid:<4} GID={tileid + 1}")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text())
    wangsets_block, report = build_wangsets_block(manifest)

    tsx_text = TSX_PATH.read_text()
    new_text = inject(tsx_text, wangsets_block)
    TSX_PATH.write_text(new_text)

    print(f"Wrote {TSX_PATH.relative_to(REPO_ROOT)}")
    print_report(report)


if __name__ == "__main__":
    main()
