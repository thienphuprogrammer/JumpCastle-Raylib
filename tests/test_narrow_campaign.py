from tools.narrow_campaign import CHAMBERS, render_campaign


def test_tower_has_eighteen_distinct_authored_chambers():
    assert len(CHAMBERS) == 18
    assert len({chamber.name for chamber in CHAMBERS}) == 18
    assert len({chamber.solids for chamber in CHAMBERS}) == 18


def test_rendered_campaign_has_approved_dimensions():
    text = render_campaign(CHAMBERS)
    rows = text.split("[collision]\n", 1)[1].splitlines()

    assert len(rows) == 648
    assert {len(row) for row in rows} == {28}
    assert "size 28 648" in text
    assert "screen_height 36" in text
