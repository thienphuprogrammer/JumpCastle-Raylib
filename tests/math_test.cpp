#include "jumpcastle/math.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using jumpcastle::Vec2;

TEST_CASE("core vector arithmetic is deterministic") {
    const Vec2 value = Vec2{2.0F, -4.0F} + Vec2{1.0F, 3.0F};

    CHECK(value.x == Approx(3.0F));
    CHECK(value.y == Approx(-1.0F));
    CHECK(jumpcastle::length(Vec2{3.0F, 4.0F}) == Approx(5.0F));
}
