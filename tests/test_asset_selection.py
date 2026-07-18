import json
from pathlib import Path


SELECTION_PATH = Path("assets/source-selection.json")
RECTANGLE_KEYS = {"file", "x", "y", "width", "height"}
FORBIDDEN_AUTOPICK_KEYS = {"first_nonempty", "scan", "auto_pick"}


def walk_keys(value):
    if isinstance(value, dict):
        for key, child in value.items():
            yield key
            yield from walk_keys(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk_keys(child)


def assert_rectangle(region):
    assert set(region) == RECTANGLE_KEYS
    assert region["width"] > 0
    assert region["height"] > 0


def test_selection_has_explicit_rectangles_for_every_runtime_name():
    data = json.loads(SELECTION_PATH.read_text(encoding="utf-8"))

    assert data["schema_version"] == 1
    assert set(data["biomes"]) == {
        "courtyard",
        "frosted_keep",
        "crown_spire",
    }
    assert set(data["player"]) == {
        "idle",
        "walk",
        "charge",
        "rise",
        "fall",
        "reset",
    }

    for group in [*data["biomes"].values(), data["props"], data["ui"]]:
        for region in group.values():
            assert_rectangle(region)

    for animation in data["player"].values():
        assert set(animation) == {"fps", "frames"}
        assert animation["fps"] > 0
        assert animation["frames"]
        for frame in animation["frames"]:
            assert RECTANGLE_KEYS <= set(frame)
            assert set(frame) <= RECTANGLE_KEYS | {"derive"}

    assert FORBIDDEN_AUTOPICK_KEYS.isdisjoint(walk_keys(data))
