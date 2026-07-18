import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path("tools").resolve()))

from render_asset_smoke import alpha_coverage, render_asset_smoke  # noqa: E402

MANIFEST = Path("assets/generated/manifest.json")
CAMPAIGN = Path("assets/levels/campaign.level")
NAMES = ("courtyard.png", "frosted-keep.png", "crown-spire.png")


def test_three_biome_screens_are_nonempty_and_distinct(tmp_path):
    written = render_asset_smoke(MANIFEST, CAMPAIGN, tmp_path)
    assert [path.name for path in written] == list(NAMES)

    signatures = []
    for name in NAMES:
        image = Image.open(tmp_path / name).convert("RGBA")
        assert image.size == (448, 576)
        assert alpha_coverage(image) > 0.5
        assert len(image.getcolors(maxcolors=448 * 576)) > 8
        signatures.append(image.tobytes())

    # Every biome must render a visibly different scene from the others.
    assert len(set(signatures)) == len(NAMES)
