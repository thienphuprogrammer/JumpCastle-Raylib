#include "jumpcastle/editor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("add_vertex snaps to the quarter-tile grid when requested") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({1.13F, 2.36F}, true);
    REQUIRE(editor.draft_points().size() == 1);
    CHECK(editor.draft_points()[0].x == Approx(1.25F));
    CHECK(editor.draft_points()[0].y == Approx(2.25F));
}

TEST_CASE("drawing three vertices commits one polygon") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({2, 0}, false);
    editor.add_vertex({1, 2}, false);
    REQUIRE(editor.close_polygon());
    REQUIRE(editor.polygon_count() == 1);
    CHECK(editor.to_screen_map().polygons.size() == 1);
}

TEST_CASE("a draft with fewer than three points does not commit") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({2, 0}, false);
    CHECK_FALSE(editor.close_polygon());
    CHECK(editor.polygon_count() == 0);
}

TEST_CASE("a concave polygon exports as multiple convex pieces") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({4, 2}, false);
    editor.add_vertex({0, 4}, false);
    editor.add_vertex({1, 2}, false);  // reflex notch -> concave
    REQUIRE(editor.close_polygon());
    CHECK(editor.to_screen_map().polygons.size() >= 2);
}

TEST_CASE("selecting and moving a vertex repositions it") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({2, 0}, false);
    editor.add_vertex({1, 2}, false);
    editor.close_polygon();

    REQUIRE(editor.select_vertex({2.05F, 0.05F}, 0.3F));
    editor.move_selected_vertex({3.0F, 1.0F}, false);
    const ScreenMap map = editor.to_screen_map();
    bool moved = false;
    for (const ConvexPolygon& polygon : map.polygons) {
        for (const Vec2 point : polygon.points) {
            if (point.x == Approx(3.0F) && point.y == Approx(1.0F)) { moved = true; }
        }
    }
    CHECK(moved);
}

TEST_CASE("deleting a selected polygon removes it") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({4, 0}, false);
    editor.add_vertex({4, 4}, false);
    editor.add_vertex({0, 4}, false);
    editor.close_polygon();
    REQUIRE(editor.polygon_count() == 1);

    REQUIRE(editor.select_polygon({2.0F, 2.0F}));
    editor.delete_selected_polygon();
    CHECK(editor.polygon_count() == 0);
}

TEST_CASE("stamping a rectangle commits one four-corner polygon") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_rectangle(ColliderType::solid, {2.0F, 2.0F}, false);
    editor.update_rectangle({6.0F, 5.0F}, false);  // drag to opposite corner
    REQUIRE(editor.commit_rectangle());
    REQUIRE(editor.polygon_count() == 1);

    const ScreenMap map = editor.to_screen_map();
    bool has_top_left = false;
    bool has_bottom_right = false;
    for (const ConvexPolygon& polygon : map.polygons) {
        for (const Vec2 point : polygon.points) {
            if (point.x == Approx(2.0F) && point.y == Approx(2.0F)) { has_top_left = true; }
            if (point.x == Approx(6.0F) && point.y == Approx(5.0F)) { has_bottom_right = true; }
        }
    }
    CHECK(has_top_left);
    CHECK(has_bottom_right);
}

TEST_CASE("a rectangle drag corners commit regardless of drag direction") {
    EditorState editor{0, 16.0F, 12.0F};
    // Drag up-left (opposite corner has smaller coords) — must normalise.
    editor.begin_rectangle(ColliderType::solid, {8.0F, 8.0F}, false);
    editor.update_rectangle({4.0F, 3.0F}, false);
    REQUIRE(editor.commit_rectangle());
    CHECK(editor.polygon_count() == 1);
}

TEST_CASE("a zero-area rectangle click is not committed") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_rectangle(ColliderType::solid, {3.0F, 3.0F}, false);
    editor.update_rectangle({3.0F, 3.0F}, false);  // no drag
    CHECK_FALSE(editor.commit_rectangle());
    CHECK(editor.polygon_count() == 0);
    CHECK_FALSE(editor.is_drafting());
}

TEST_CASE("dragging a selected polygon translates every vertex") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({4, 0}, false);
    editor.add_vertex({4, 4}, false);
    editor.add_vertex({0, 4}, false);
    editor.close_polygon();

    REQUIRE(editor.select_polygon({2.0F, 2.0F}));
    editor.move_selected_polygon({3.0F, -1.0F});

    const ScreenMap map = editor.to_screen_map();
    bool found_shifted_corner = false;
    for (const ConvexPolygon& polygon : map.polygons) {
        for (const Vec2 point : polygon.points) {
            // The original (0,0) corner must now sit at (3,-1).
            if (point.x == Approx(3.0F) && point.y == Approx(-1.0F)) {
                found_shifted_corner = true;
            }
        }
    }
    CHECK(found_shifted_corner);
}

TEST_CASE("moving a polygon with no selection is a no-op") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({4, 0}, false);
    editor.add_vertex({4, 4}, false);
    editor.close_polygon();

    editor.move_selected_polygon({5.0F, 5.0F});  // nothing selected

    const ScreenMap map = editor.to_screen_map();
    bool untouched = false;
    for (const ConvexPolygon& polygon : map.polygons) {
        for (const Vec2 point : polygon.points) {
            if (point.x == Approx(0.0F) && point.y == Approx(0.0F)) {
                untouched = true;
            }
        }
    }
    CHECK(untouched);
}

TEST_CASE("load_screen makes an existing map visible and editable") {
    // Author a screen (one polygon + a spawn), export it, then load it back
    // into a fresh editor as if entering edit mode on that screen.
    EditorState authored{7, 20.0F, 15.0F};
    authored.begin_polygon(ColliderType::solid);
    authored.add_vertex({1, 1}, false);
    authored.add_vertex({5, 1}, false);
    authored.add_vertex({5, 5}, false);
    authored.add_vertex({1, 5}, false);
    authored.close_polygon();
    authored.place_entity(EntityType::spawn, {2, 4}, false);
    const ScreenMap source = authored.to_screen_map();

    EditorState loaded;
    loaded.load_screen(source);

    // The map is now populated (not a blank canvas) and re-exports losslessly.
    CHECK(loaded.polygon_count() == 1);
    CHECK(loaded.entity_count() == 1);
    const ScreenMap round_trip = loaded.to_screen_map();
    CHECK(round_trip.index == 7);
    CHECK(round_trip.width == Approx(20.0F));
    CHECK(round_trip.polygons.size() == source.polygons.size());
    REQUIRE(round_trip.entities.size() == 1);
    CHECK(round_trip.entities[0].type == EntityType::spawn);
}

TEST_CASE("load_screen replaces any prior editor state") {
    EditorState editor{0, 16.0F, 12.0F};
    editor.begin_polygon(ColliderType::solid);
    editor.add_vertex({0, 0}, false);
    editor.add_vertex({2, 0}, false);
    editor.add_vertex({2, 2}, false);
    editor.close_polygon();
    REQUIRE(editor.polygon_count() == 1);

    ScreenMap empty;
    empty.index = 3;
    empty.width = 16.0F;
    empty.height = 12.0F;
    editor.load_screen(empty);

    CHECK(editor.polygon_count() == 0);
    CHECK(editor.entity_count() == 0);
    CHECK(editor.to_screen_map().index == 3);
}

TEST_CASE("placing an entity records it in the exported screen") {
    EditorState editor{3, 16.0F, 12.0F};
    editor.place_entity(EntityType::spawn, {2.13F, 5.0F}, true);
    REQUIRE(editor.entity_count() == 1);
    const ScreenMap map = editor.to_screen_map();
    REQUIRE(map.index == 3);
    REQUIRE(map.entities.size() == 1);
    CHECK(map.entities[0].type == EntityType::spawn);
    CHECK(map.entities[0].pos.x == Approx(2.25F));
}
