#!/usr/bin/env python3
"""Throwaway visual helper: label a source sheet's 16px grid so exact tile
coordinates can be read by eye (used while curating assets/source-selection.json).

Not part of the runtime pipeline; not covered by tests. Usage:

    .venv/bin/python tools/slice_preview.py --src assets/sources/downloads/foo.png \\
        --out /tmp/foo_preview.png --scale 3
    .venv/bin/python tools/slice_preview.py --src assets/sources/downloads/foo.png \\
        --x 0 --y 192 --w 128 --h 64 --out /tmp/foo_region.png --scale 4
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

TILE = 16
GRID_LINE_COLOR = (255, 0, 255, 200)
LABEL_COLOR = (255, 255, 0, 255)
LABEL_BG = (0, 0, 0, 160)


def slice_preview(
    src: Path, out: Path, scale: int, region: tuple[int, int, int, int] | None
) -> None:
    with Image.open(src) as source:
        image = source.convert("RGBA")
        if region is not None:
            x, y, width, height = region
            image = image.crop((x, y, x + width, y + height))
        else:
            x = y = 0

        scaled = image.resize(
            (image.width * scale, image.height * scale), Image.Resampling.NEAREST
        )
        canvas = scaled.convert("RGBA")
        draw = ImageDraw.Draw(canvas)
        step = TILE * scale

        for grid_x in range(0, canvas.width + 1, step):
            draw.line([(grid_x, 0), (grid_x, canvas.height)], fill=GRID_LINE_COLOR)
        for grid_y in range(0, canvas.height + 1, step):
            draw.line([(0, grid_y), (canvas.width, grid_y)], fill=GRID_LINE_COLOR)

        columns = (canvas.width + step - 1) // step
        rows = (canvas.height + step - 1) // step
        for row in range(rows):
            for col in range(columns):
                # Absolute source-sheet tile coordinates (account for the crop
                # offset) so labels can be copy-pasted straight into
                # source-selection.json as {"x": col*16, "y": row*16}.
                abs_x = x + col * TILE
                abs_y = y + row * TILE
                label = f"{abs_x},{abs_y}"
                text_x = col * step + 2
                text_y = row * step + 2
                bbox = draw.textbbox((text_x, text_y), label)
                draw.rectangle(bbox, fill=LABEL_BG)
                draw.text((text_x, text_y), label, fill=LABEL_COLOR)

        out.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(out, format="PNG")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--scale", type=int, default=3)
    parser.add_argument("--x", type=int, default=None)
    parser.add_argument("--y", type=int, default=None)
    parser.add_argument("--w", type=int, default=None)
    parser.add_argument("--h", type=int, default=None)
    args = parser.parse_args()

    region = None
    if any(value is not None for value in (args.x, args.y, args.w, args.h)):
        if None in (args.x, args.y, args.w, args.h):
            parser.error("--x/--y/--w/--h must be given together")
        region = (args.x, args.y, args.w, args.h)

    slice_preview(args.src, args.out, args.scale, region)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
