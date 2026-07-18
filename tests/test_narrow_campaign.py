from tools.narrow_campaign import CHAMBERS, render_campaign


def test_tower_has_eighteen_distinct_authored_chambers():
    assert len(CHAMBERS) == 18
    assert len({chamber.name for chamber in CHAMBERS}) == 18
    assert len({chamber.solids for chamber in CHAMBERS}) == 18


def test_rendered_campaign_has_approved_dimensions():
    text = render_campaign(CHAMBERS)
    collision = (
        text.split("[collision]\n", 1)[1].split("[decoration]\n", 1)[0].splitlines()
    )
    decoration = text.split("[decoration]\n", 1)[1].splitlines()

    assert len(collision) == 648
    assert len(decoration) == 648
    assert {len(row) for row in collision} == {28}
    assert {len(row) for row in decoration} == {28}
    assert "size 28 648" in text
    assert "screen_height 36" in text


def test_courtyard_covers_six_training_mechanics():
    assert {room.mechanic for room in CHAMBERS[:6]} == {
        "training_ascent",
        "long_gap",
        "wall_rebound",
        "low_ceiling",
        "central_tower",
        "gatehouse_exam",
    }
    assert all(8 <= room.route_jump_count <= 12 for room in CHAMBERS[:6])


def test_frosted_keep_covers_midgame_mechanics():
    assert {room.mechanic for room in CHAMBERS[6:12]} == {
        "split_shaft",
        "window_steps",
        "crossing_chamber",
        "reversal_climb",
        "narrow_gallery",
        "bell_tower_exam",
    }


def test_crown_spire_covers_endgame_mechanics():
    assert {room.mechanic for room in CHAMBERS[12:]} == {
        "broken_bridge",
        "crown_chamber",
        "vertical_chimney",
        "overhang_reversal",
        "fall_funnel",
        "throne_leap",
    }
