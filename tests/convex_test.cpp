#include "jumpcastle/convex.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("square is convex with unit outward normals") {
    const std::vector<Vec2> square{{0, 0}, {2, 0}, {2, 2}, {0, 2}};
    REQUIRE(is_convex(square));
    const auto normals = outward_edge_normals(square);
    REQUIRE(normals.size() == 4);
    for (const Vec2 n : normals) {
        REQUIRE(length(n) == Approx(1.0F));
    }
}

TEST_CASE("polygon_aabb bounds all points") {
    const Aabb box = polygon_aabb({{1, 1}, {4, 2}, {2, 5}});
    REQUIRE(box.min == Vec2{1, 1});
    REQUIRE(box.max == Vec2{4, 5});
}

TEST_CASE("concave polygon is detected and split into convex pieces") {
    const std::vector<Vec2> dart{{0, 0}, {4, 2}, {0, 4}, {1, 2}};
    REQUIRE_FALSE(is_convex(dart));
    const auto pieces = split_to_convex(dart);
    REQUIRE(pieces.size() >= 2);
    for (const auto& piece : pieces) {
        REQUIRE(is_convex(piece));
    }
}

TEST_CASE("already-convex polygon is not split") {
    const std::vector<Vec2> tri{{0, 0}, {2, 0}, {1, 2}};
    REQUIRE(split_to_convex(tri).size() == 1);
}
