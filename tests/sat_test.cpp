#include "jumpcastle/sat.hpp"

#include "jumpcastle/convex.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace jumpcastle;

namespace {
Mtv test_overlap(const Aabb& box, const std::vector<Vec2>& poly) {
    return aabb_vs_convex(box, poly, outward_edge_normals(poly));
}
}  // namespace

TEST_CASE("box resting slightly inside a floor is pushed up") {
    const std::vector<Vec2> floor{{0, 10}, {16, 10}, {16, 12}, {0, 12}};
    const Aabb box{{5.0F, 9.6F}, {5.6F, 10.1F}};  // overlaps floor top by 0.1 (y-down)
    const Mtv mtv = test_overlap(box, floor);
    REQUIRE(mtv.overlapping);
    REQUIRE(mtv.normal.y < 0.0F);  // push up (y-down => negative y is up)
    REQUIRE(mtv.depth == Approx(0.1F).margin(1e-4));
}

TEST_CASE("separated box reports no overlap") {
    const std::vector<Vec2> floor{{0, 10}, {16, 10}, {16, 12}, {0, 12}};
    const Aabb box{{5.0F, 8.0F}, {5.6F, 9.0F}};  // gap of 1.0 above the floor
    REQUIRE_FALSE(test_overlap(box, floor).overlapping);
}

TEST_CASE("box against a wall is pushed sideways") {
    const std::vector<Vec2> wall{{10, 0}, {12, 0}, {12, 15}, {10, 15}};
    const Aabb box{{9.7F, 5.0F}, {10.3F, 5.6F}};  // penetrates wall left face by 0.3
    const Mtv mtv = test_overlap(box, wall);
    REQUIRE(mtv.overlapping);
    REQUIRE(mtv.normal.x < 0.0F);  // pushed left, away from wall
    REQUIRE(mtv.depth == Approx(0.3F).margin(1e-4));
}
