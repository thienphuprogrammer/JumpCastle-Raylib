#include "jumpcastle/surface_samples.hpp"

#include "jumpcastle/collider.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using Catch::Approx;
using namespace jumpcastle;

namespace {

void check_stable_identity(
    const std::vector<SurfaceSample>& samples, const WorldCollider& collider) {
    for (std::size_t index = 0; index < samples.size(); ++index) {
        CHECK(samples[index].collider_id == collider.id);
        CHECK(samples[index].piece_index == collider.piece_index);
        CHECK(samples[index].sample_index == static_cast<int>(index));
    }
}

void check_position_order(const std::vector<SurfaceSample>& samples) {
    for (std::size_t index = 1; index < samples.size(); ++index) {
        const SurfaceSample& previous = samples[index - 1];
        const SurfaceSample& current = samples[index];
        const bool ordered =
            previous.position.y < current.position.y ||
            (previous.position.y == current.position.y &&
             (previous.position.x < current.position.x ||
              (previous.position.x == current.position.x &&
               previous.sample_index < current.sample_index)));
        CHECK(ordered);
    }
}

Vec2 closest_point_on_segment(const Vec2 point, const Vec2 a, const Vec2 b) {
    const Vec2 segment = b - a;
    const float squared_length = dot(segment, segment);
    if (squared_length == 0.0F) {
        return a;
    }
    const float t = std::clamp(dot(point - a, segment) / squared_length, 0.0F, 1.0F);
    return a + segment * t;
}

}  // namespace

TEST_CASE("polygon samples only walkable edges at bounded spacing", "[surface_samples]") {
    const WorldCollider collider{
        .id = 5,
        .piece_index = 3,
        .type = ColliderType::solid,
        .geometry = ConvexPolygon{
            .points = {{0.0F, 4.0F}, {4.0F, 4.0F}, {4.0F, 8.0F}, {0.0F, 8.0F}},
            .edge_normals = {
                {0.0F, -1.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {-1.0F, 0.0F},
            },
            .aabb = {{0.0F, 4.0F}, {4.0F, 8.0F}},
        },
        .aabb = {{0.0F, 4.0F}, {4.0F, 8.0F}},
    };

    const auto samples = sample_walkable_surfaces(collider, 1.5F);

    REQUIRE(samples.size() == 4);
    CHECK(samples[0].position == Vec2{0.0F, 4.0F});
    CHECK(samples[1].position.x == Approx(4.0F / 3.0F));
    CHECK(samples[2].position.x == Approx(8.0F / 3.0F));
    CHECK(samples[3].position == Vec2{4.0F, 4.0F});
    for (const SurfaceSample& sample : samples) {
        CHECK(sample.position.y == 4.0F);
        CHECK(sample.normal == Vec2{0.0F, -1.0F});
    }
    check_stable_identity(samples, collider);
}

TEST_CASE("circle samples its complete normalized walkable upper arc", "[surface_samples]") {
    const WorldCollider collider{
        .id = 7,
        .piece_index = 2,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{{10.0F, 10.0F}, 3.0F},
        .aabb = {{7.0F, 7.0F}, {13.0F, 13.0F}},
    };
    const auto samples = sample_walkable_surfaces(collider, 0.5F);

    REQUIRE(samples.size() == 14);
    for (const SurfaceSample& sample : samples) {
        CHECK(sample.normal.y <= -0.5F);
        CHECK(length(sample.normal) == Approx(1.0F).margin(1.0e-5F));
        CHECK(length(sample.position - Vec2{10.0F, 10.0F}) ==
              Approx(3.0F).margin(1.0e-5F));
    }
    CHECK(samples.front().normal.x < 0.0F);
    CHECK(samples.back().normal.x > 0.0F);
    check_stable_identity(samples, collider);
}

TEST_CASE(
    "capsule samples local side and end arcs in canonical order", "[surface_samples]") {
    const WorldCollider collider{
        .id = 9,
        .piece_index = 4,
        .type = ColliderType::solid,
        .geometry = CapsuleGeometry{{4.0F, 8.0F}, {12.0F, 8.0F}, 1.5F},
        .aabb = {{2.5F, 6.5F}, {13.5F, 9.5F}},
    };
    const auto samples = sample_walkable_surfaces(collider, 0.25F);

    REQUIRE_FALSE(samples.empty());
    CHECK(samples == sample_walkable_surfaces(collider, 0.25F));
    bool has_left_arc = false;
    bool has_right_arc = false;
    bool has_top_side = false;
    for (const SurfaceSample& sample : samples) {
        const Vec2 closest =
            closest_point_on_segment(sample.position, {4.0F, 8.0F}, {12.0F, 8.0F});
        CHECK(length(sample.position - closest) == Approx(1.5F).margin(1.0e-5F));
        CHECK(length(sample.normal) == Approx(1.0F).margin(1.0e-5F));
        CHECK(sample.normal.y <= -0.5F);
        has_left_arc = has_left_arc || sample.normal.x < -0.1F;
        has_right_arc = has_right_arc || sample.normal.x > 0.1F;
        has_top_side =
            has_top_side || (std::abs(sample.normal.x) < 1.0e-5F &&
                             sample.normal.y < -0.999F &&
                             sample.position.x > 4.0F && sample.position.x < 12.0F);
    }
    CHECK(has_left_arc);
    CHECK(has_right_arc);
    CHECK(has_top_side);
    check_position_order(samples);
    check_stable_identity(samples, collider);
}

TEST_CASE("surface sampling rejects invalid spacing", "[surface_samples]") {
    const WorldCollider collider{
        .id = 11,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{{2.0F, 3.0F}, 1.0F},
        .aabb = {{1.0F, 2.0F}, {3.0F, 4.0F}},
    };

    CHECK(sample_walkable_surfaces(collider, 0.0F).empty());
    CHECK(sample_walkable_surfaces(collider, -0.25F).empty());
    CHECK(sample_walkable_surfaces(
              collider, std::numeric_limits<float>::infinity())
              .empty());
}
