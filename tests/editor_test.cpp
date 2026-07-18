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
