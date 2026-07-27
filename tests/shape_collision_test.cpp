#include "jumpcastle/shape_collision.hpp"

#include "jumpcastle/convex.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using namespace jumpcastle;

TEST_CASE("AABB overlapping circle top returns upward contact", "[shape_collision]") {
    const Aabb box{{4.5F, 2.5F}, {5.5F, 3.5F}};
    const CircleGeometry circle{{5.0F, 5.0F}, 2.0F};

    const auto contact = contact_aabb_circle(box, circle);

    REQUIRE(contact);
    CHECK(contact->point.x == Approx(5.0F).margin(1e-5));
    CHECK(contact->point.y == Approx(3.0F).margin(1e-5));
    CHECK(contact->normal.x == Approx(0.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(0.5F).margin(1e-5));
}

TEST_CASE("AABB inside circle uses deterministic nearest-face normal", "[shape_collision]") {
    const Aabb box{{4.5F, 4.5F}, {5.5F, 5.5F}};
    const CircleGeometry circle{{5.0F, 5.0F}, 3.0F};

    const auto contact = contact_aabb_circle(box, circle);

    REQUIRE(contact);
    CHECK(std::isfinite(contact->normal.x));
    CHECK(std::isfinite(contact->normal.y));
    CHECK(contact->normal.x == Approx(0.0F));
    CHECK(contact->normal.y == Approx(-1.0F));
    CHECK(length(contact->normal) == Approx(1.0F));
    CHECK(contact->depth == Approx(3.5F).margin(1e-5));
}

TEST_CASE("off-center circle containment chooses the minimum full exit", "[shape_collision]") {
    const Aabb box{{4.0F, 4.0F}, {7.0F, 7.0F}};
    const CircleGeometry circle{{5.0F, 4.5F}, 1.5F};

    const auto contact = contact_aabb_circle(box, circle);

    REQUIRE(contact);
    CHECK(contact->point.x == Approx(5.0F).margin(1e-5));
    CHECK(contact->point.y == Approx(6.0F).margin(1e-5));
    CHECK(contact->normal.x == Approx(0.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(2.0F).margin(1e-5));
}

TEST_CASE("separated AABB and circle return no contact", "[shape_collision]") {
    const Aabb box{{0.0F, 0.0F}, {1.0F, 1.0F}};
    const CircleGeometry circle{{3.0F, 3.0F}, 1.0F};

    CHECK_FALSE(contact_aabb_circle(box, circle));
}

TEST_CASE("AABB contacts horizontal capsule end arc", "[shape_collision]") {
    const Aabb box{{8.7F, 3.6F}, {9.4F, 4.4F}};
    const CapsuleGeometry capsule{{3.0F, 5.0F}, {8.0F, 5.0F}, 1.5F};

    const auto contact = contact_aabb_capsule(box, capsule);

    REQUIRE(contact);
    CHECK(contact->normal.x == Approx(0.7592566F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-0.6507914F).margin(1e-5));
    CHECK(length(contact->normal) == Approx(1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(0.5780455F).margin(1e-5));
}

TEST_CASE("AABB contacts capsule side through an edge feature", "[shape_collision]") {
    const Aabb box{{4.0F, 3.0F}, {6.0F, 4.0F}};
    const CapsuleGeometry capsule{{2.0F, 5.0F}, {8.0F, 5.0F}, 1.5F};

    const auto contact = contact_aabb_capsule(box, capsule);

    REQUIRE(contact);
    CHECK(contact->point.x == Approx(6.0F).margin(1e-5));
    CHECK(contact->point.y == Approx(3.5F).margin(1e-5));
    CHECK(contact->normal.x == Approx(0.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(0.5F).margin(1e-5));
}

TEST_CASE("capsule spine crossing AABB uses minimum full exit", "[shape_collision]") {
    const Aabb box{{4.0F, 4.0F}, {6.0F, 6.0F}};
    const CapsuleGeometry capsule{{3.0F, 5.0F}, {7.0F, 5.0F}, 0.75F};

    const auto contact = contact_aabb_capsule(box, capsule);

    REQUIRE(contact);
    CHECK(contact->point.x == Approx(5.0F).margin(1e-5));
    CHECK(contact->point.y == Approx(4.25F).margin(1e-5));
    CHECK(contact->normal.x == Approx(0.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(1.75F).margin(1e-5));
}

TEST_CASE("diagonal capsule spine intersection uses its side axis", "[shape_collision]") {
    const Aabb box{{-1.0F, -1.0F}, {1.0F, 1.0F}};
    const CapsuleGeometry capsule{{-2.0F, -2.0F}, {2.0F, 2.0F}, 0.25F};

    const auto contact = contact_aabb_capsule(box, capsule);

    REQUIRE(contact);
    CHECK(contact->point.x == Approx(0.1767767F).margin(1e-5));
    CHECK(contact->point.y == Approx(-0.1767767F).margin(1e-5));
    CHECK(contact->normal.x == Approx(0.7071068F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-0.7071068F).margin(1e-5));
    CHECK(contact->depth == Approx(1.6642135F).margin(1e-5));
}

TEST_CASE("degenerate capsule returns the same radial contact as a circle", "[shape_collision]") {
    const Aabb box{{5.5F, 4.5F}, {6.5F, 5.5F}};
    const CapsuleGeometry capsule{{5.0F, 5.0F}, {5.0F, 5.0F}, 1.0F};
    const CircleGeometry circle{{5.0F, 5.0F}, 1.0F};

    const auto capsule_contact = contact_aabb_capsule(box, capsule);
    const auto circle_contact = contact_aabb_circle(box, circle);

    REQUIRE(capsule_contact);
    REQUIRE(circle_contact);
    CHECK(capsule_contact->point.x == Approx(circle_contact->point.x).margin(1e-5));
    CHECK(capsule_contact->point.y == Approx(circle_contact->point.y).margin(1e-5));
    CHECK(capsule_contact->normal.x == Approx(circle_contact->normal.x).margin(1e-5));
    CHECK(capsule_contact->normal.y == Approx(circle_contact->normal.y).margin(1e-5));
    CHECK(capsule_contact->depth == Approx(circle_contact->depth).margin(1e-5));
}

TEST_CASE("degenerate capsule inside AABB keeps circle fallback semantics", "[shape_collision]") {
    const Aabb box{{4.0F, 4.0F}, {7.0F, 6.0F}};
    const CapsuleGeometry capsule{{5.0F, 5.0F}, {5.0F, 5.0F}, 1.5F};
    const CircleGeometry circle{{5.0F, 5.0F}, 1.5F};

    const auto capsule_contact = contact_aabb_capsule(box, capsule);
    const auto circle_contact = contact_aabb_circle(box, circle);

    REQUIRE(capsule_contact);
    REQUIRE(circle_contact);
    CHECK(capsule_contact->point.x == Approx(circle_contact->point.x).margin(1e-5));
    CHECK(capsule_contact->point.y == Approx(circle_contact->point.y).margin(1e-5));
    CHECK(capsule_contact->normal.x == Approx(circle_contact->normal.x).margin(1e-5));
    CHECK(capsule_contact->normal.y == Approx(circle_contact->normal.y).margin(1e-5));
    CHECK(capsule_contact->depth == Approx(circle_contact->depth).margin(1e-5));
    CHECK(capsule_contact->depth == Approx(2.5F).margin(1e-5));
}

TEST_CASE("separated AABB and capsule return no contact", "[shape_collision]") {
    const Aabb box{{12.0F, 1.0F}, {13.0F, 2.0F}};
    const CapsuleGeometry capsule{{2.0F, 8.0F}, {6.0F, 8.0F}, 1.0F};

    CHECK_FALSE(contact_aabb_capsule(box, capsule));
}

TEST_CASE("polygon contact wraps the existing SAT result", "[shape_collision]") {
    const ConvexPolygon polygon{
        .points = {{0.0F, 10.0F}, {16.0F, 10.0F}, {16.0F, 12.0F}, {0.0F, 12.0F}},
        .edge_normals =
            outward_edge_normals(
                {{0.0F, 10.0F}, {16.0F, 10.0F}, {16.0F, 12.0F}, {0.0F, 12.0F}}),
    };
    const Aabb box{{5.0F, 9.6F}, {5.6F, 10.1F}};

    const auto contact = contact_aabb_polygon(box, polygon);

    REQUIRE(contact);
    CHECK(contact->normal.y == Approx(-1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(0.1F).margin(1e-4));
}

TEST_CASE("polygon containment depth fully separates the AABB", "[shape_collision]") {
    const std::vector<Vec2> points{
        {-10.0F, -10.0F},
        {10.0F, -10.0F},
        {10.0F, 10.0F},
        {-10.0F, 10.0F},
    };
    const ConvexPolygon polygon{
        .points = points,
        .edge_normals = outward_edge_normals(points),
    };
    const Aabb box{{-1.0F, -1.0F}, {1.0F, 1.0F}};

    const auto contact = contact_aabb_polygon(box, polygon);

    REQUIRE(contact);
    CHECK(contact->normal.x == Approx(0.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(-1.0F).margin(1e-5));
    CHECK(contact->depth == Approx(11.0F).margin(1e-5));

    const Vec2 correction = contact->normal * contact->depth;
    const Aabb resolved{box.min + correction, box.max + correction};
    CHECK_FALSE(contact_aabb_polygon(resolved, polygon));
}

TEST_CASE("polygon vertex contact reports the actual support feature", "[shape_collision]") {
    const std::vector<Vec2> points{
        {0.0F, 0.0F},
        {1.0F, 10.0F},
        {0.0F, 20.0F},
    };
    const ConvexPolygon polygon{
        .points = points,
        .edge_normals = outward_edge_normals(points),
    };
    const Aabb box{{0.9F, 8.0F}, {1.5F, 15.0F}};

    const auto contact = contact_aabb_polygon(box, polygon);

    REQUIRE(contact);
    CHECK(contact->normal.x == Approx(1.0F).margin(1e-5));
    CHECK(contact->normal.y == Approx(0.0F).margin(1e-5));
    CHECK(contact->point.x == Approx(1.0F).margin(1e-5));
    CHECK(contact->point.y == Approx(10.0F).margin(1e-5));
}

TEST_CASE("polygon edge contact lies on the resolved AABB manifold", "[shape_collision]") {
    const std::vector<Vec2> points{
        {0.0F, 0.0F},
        {10.0F, 0.0F},
        {0.0F, 10.0F},
    };
    const ConvexPolygon polygon{
        .points = points,
        .edge_normals = outward_edge_normals(points),
    };
    const Aabb box{{5.0F, 4.5F}, {7.0F, 5.5F}};

    const auto contact = contact_aabb_polygon(box, polygon);

    REQUIRE(contact);
    CHECK(contact->normal.x == Approx(0.7071068F).margin(1e-5));
    CHECK(contact->normal.y == Approx(0.7071068F).margin(1e-5));
    CHECK(contact->depth == Approx(0.3535534F).margin(1e-5));
    CHECK(contact->point.x == Approx(5.25F).margin(1e-5));
    CHECK(contact->point.y == Approx(4.75F).margin(1e-5));
    CHECK(contact->point.x + contact->point.y == Approx(10.0F).margin(1e-5));

    const Vec2 correction = contact->normal * contact->depth;
    const Aabb resolved{box.min + correction, box.max + correction};
    CHECK(contact->point.x == Approx(resolved.min.x).margin(1e-5));
    CHECK(contact->point.y == Approx(resolved.min.y).margin(1e-5));
}
