from PIL import Image
import pytest

from tools.verify_asset_pixels import alpha_coverage, require_coverage


def test_required_region_rejects_three_pixel_strip():
    image = Image.new("RGBA", (16, 16))
    for y in range(3):
        for x in range(16):
            image.putpixel((x, y), (255, 255, 255, 255))

    assert alpha_coverage(image) == pytest.approx(3 / 16)
    with pytest.raises(ValueError, match="alpha coverage"):
        require_coverage(image, 0.20, "checkpoint")


def test_required_region_accepts_opaque_sprite():
    image = Image.new("RGBA", (16, 16), (255, 255, 255, 255))

    require_coverage(image, 0.20, "solid")
