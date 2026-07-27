#include "jumpcastle/collision_world.hpp"

#include "jumpcastle/convex.hpp"
#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player.hpp"
#include "jumpcastle/shape_collision.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <utility>

using Catch::Approx;
using namespace jumpcastle;

namespace {

MapCollider make_polygon(
    const int id, std::vector<Vec2> points, const ColliderType type) {
    return {
        .id = id,
        .type = type,
        .geometry = PolygonGeometry{std::move(points)},
    };
}

ScreenMap floor_screen() {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.colliders.push_back(
        make_polygon(1, {{0, 14}, {16, 14}, {16, 15}, {0, 15}}, ColliderType::solid));
    return s;
}

// A tall solid column spanning x=[10,11], y=[8,14]; its left face has outward
// normal (-1, 0), so a player approaching from the left strikes a vertical wall.
ScreenMap wall_screen() {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.colliders.push_back(
        make_polygon(1, {{10, 8}, {11, 8}, {11, 14}, {10, 14}}, ColliderType::solid));
    return s;
}

// A solid ceiling slab spanning x=[4,12], y=[8,9]; its bottom face has outward
// normal (0, 1), so a player rising into it bonks a horizontal surface.
ScreenMap ceiling_screen() {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.colliders.push_back(
        make_polygon(1, {{4, 8}, {12, 8}, {12, 9}, {4, 9}}, ColliderType::solid));
    return s;
}

MapCollider circle_collider(
    const int id, const Vec2 center, const float radius) {
    return {
        .id = id,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{center, radius},
    };
}

ScreenMap collider_screen(
    const int index, std::vector<MapCollider> colliders) {
    ScreenMap screen;
    screen.index = index;
    screen.width = 28;
    screen.height = 15;
    screen.colliders = std::move(colliders);
    return screen;
}

}  // namespace

TEST_CASE("falling player lands on the floor and is grounded", "[collision_world]") {
    const CollisionWorld world = CollisionWorld::from_screens({floor_screen()}, 15);
    PlayerState player;
    const Vec2 prev{8.0F, 13.0F};
    player.position = {8.0F, 13.9F};
    player.velocity = {0.0F, 10.0F};
    const ResolveResult result = world.resolve(prev, player);
    REQUIRE(result.on_ground);
    REQUIRE(player.velocity.y == Approx(0.0F).margin(1e-3));
    REQUIRE(player.position.y + config::player_half_size.y == Approx(14.0F).margin(1e-2));
}

TEST_CASE("hazard overlap is reported without positional resolve", "[collision_world]") {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.colliders.push_back(
        make_polygon(1, {{7, 13}, {9, 13}, {8, 12}}, ColliderType::hazard));
    const CollisionWorld world = CollisionWorld::from_screens({s}, 15);
    PlayerState player;
    player.position = {8.0F, 12.6F};
    const ResolveResult result = world.resolve({8.0F, 12.0F}, player);
    REQUIRE(result.hit_hazard);
}

TEST_CASE(
    "airborne player rebounds off a vertical wall like Jump King",
    "[collision_world]") {
    const CollisionWorld world = CollisionWorld::from_screens({wall_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::airborne;
    player.position = {10.1F, 12.2F};
    player.velocity = {5.0F, 3.0F};
    static_cast<void>(world.resolve({9.4F, 12.0F}, player));

    // Horizontal velocity reverses and keeps 0.8x its magnitude; the downward
    // fall (y) is untouched, so the player loses horizontal control mid-air.
    REQUIRE(player.velocity.x ==
            Approx(-5.0F * config::wall_bounce_restitution(5.0F)).margin(1e-3));
    REQUIRE(player.velocity.y == Approx(3.0F).margin(1e-3));
    REQUIRE(player.position.x + config::player_half_size.x == Approx(10.0F).margin(1e-2));
}

TEST_CASE("landing on a floor does not bounce the player back up", "[collision_world]") {
    const CollisionWorld world = CollisionWorld::from_screens({floor_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::airborne;
    player.position = {8.0F, 13.9F};
    player.velocity = {0.0F, 10.0F};
    const ResolveResult result = world.resolve({8.0F, 13.0F}, player);

    REQUIRE(result.on_ground);
    REQUIRE(player.velocity.y == Approx(0.0F).margin(1e-3));  // absorbed, not reversed
}

TEST_CASE(
    "bonking a ceiling absorbs upward velocity without a wall rebound",
    "[collision_world]") {
    const CollisionWorld world = CollisionWorld::from_screens({ceiling_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::airborne;
    player.position = {8.0F, 9.3F};
    player.velocity = {2.0F, -8.0F};
    static_cast<void>(world.resolve({8.0F, 9.9F}, player));

    // Upward velocity is killed (slide), horizontal is carried through unchanged
    // -- a ceiling is not a wall, so no horizontal reversal.
    REQUIRE(player.velocity.y == Approx(0.0F).margin(1e-3));
    REQUIRE(player.velocity.x == Approx(2.0F).margin(1e-3));
}

TEST_CASE(
    "grounded player does not bounce off a wall it walks into",
    "[collision_world]") {
    const CollisionWorld world = CollisionWorld::from_screens({wall_screen()}, 15);
    PlayerState player;
    player.mode = PlayerMode::grounded;
    player.position = {10.1F, 12.2F};
    player.velocity = {5.0F, 0.0F};
    static_cast<void>(world.resolve({9.4F, 12.2F}, player));

    // Grounded contact slides to a stop instead of rebounding.
    REQUIRE(player.velocity.x == Approx(0.0F).margin(1e-3));
}

TEST_CASE(
    "one-way platform blocks a fall from above but not a rise from below",
    "[collision_world]") {
    ScreenMap s;
    s.index = 0;
    s.width = 16;
    s.height = 15;
    s.colliders.push_back(make_polygon(
        1,
        {{4, 10}, {12, 10}, {12, 10.4F}, {4, 10.4F}},
        ColliderType::oneway));
    const CollisionWorld world = CollisionWorld::from_screens({s}, 15);

    SECTION("falling from above lands") {
        PlayerState player;
        player.position = {8.0F, 9.7F};
        player.velocity = {0.0F, 8.0F};
        const ResolveResult result = world.resolve({8.0F, 9.2F}, player);
        REQUIRE(result.on_ground);
    }

    SECTION("rising from below passes through") {
        PlayerState player;
        player.position = {8.0F, 10.2F};
        player.velocity = {0.0F, -8.0F};
        const ResolveResult result = world.resolve({8.0F, 10.8F}, player);
        REQUIRE_FALSE(result.on_ground);
    }
}

TEST_CASE(
    "CollisionWorld offsets circle and capsule into screen world space",
    "[collision_world]") {
    ScreenMap screen;
    screen.index = 2;
    screen.width = 28;
    screen.height = 36;
    screen.colliders = {
        {.id = 4,
         .type = ColliderType::solid,
         .geometry = CircleGeometry{{8.0F, 5.0F}, 2.0F},
         .tag = "orb"},
        {.id = 5,
         .type = ColliderType::solid,
         .geometry = CapsuleGeometry{{3.0F, 8.0F}, {12.0F, 8.0F}, 1.0F},
         .tag = "bridge"},
    };

    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    const auto* colliders = world.colliders_for_screen(2);
    REQUIRE(colliders != nullptr);
    REQUIRE(colliders->size() == 2);
    CHECK(std::get<CircleGeometry>((*colliders)[0].geometry).center.y == Approx(77.0F));
    CHECK(std::get<CapsuleGeometry>((*colliders)[1].geometry).a.y == Approx(80.0F));
    CHECK((*colliders)[0].aabb.min.y == Approx(75.0F));
    CHECK((*colliders)[1].tag == "bridge");
}

TEST_CASE(
    "CollisionWorld decomposes authored concave polygons at runtime with stable pieces",
    "[collision_world]") {
    ScreenMap screen;
    screen.index = 1;
    screen.width = 16;
    screen.height = 15;
    screen.colliders.push_back({
        .id = 42,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{{
            {1.0F, 1.0F},
            {5.0F, 1.0F},
            {5.0F, 5.0F},
            {3.0F, 3.0F},
            {1.0F, 5.0F},
        }},
        .tag = "concave",
    });

    const CollisionWorld world = CollisionWorld::from_screens({screen}, 15);
    const auto* colliders = world.colliders_for_screen(1);
    REQUIRE(colliders != nullptr);
    REQUIRE(colliders->size() == 3);
    for (std::size_t index = 0; index < colliders->size(); ++index) {
        CHECK((*colliders)[index].id == 42);
        CHECK((*colliders)[index].piece_index == static_cast<int>(index));
        CHECK((*colliders)[index].tag == "concave");
        CHECK(std::holds_alternative<ConvexPolygon>((*colliders)[index].geometry));
    }

    const auto* polygons = world.polygons_for_screen(1);
    REQUIRE(polygons != nullptr);
    CHECK(polygons->size() == colliders->size());
}

TEST_CASE("falling player lands on circle with upward contact", "[collision_world]") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders.push_back({
        .id = 9,
        .type = ColliderType::solid,
        .geometry = CircleGeometry{{14.0F, 20.0F}, 4.0F},
        .tag = "round_platform",
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    PlayerState player{.position = {14.0F, 16.2F}, .velocity = {0.0F, 4.0F}};

    const ResolveResult result = world.resolve({14.0F, 15.0F}, player);

    CHECK(result.on_ground);
    REQUIRE(result.ground_contact);
    CHECK(result.ground_contact->collider_id == 9);
    CHECK(result.ground_contact->piece_index == 0);
    CHECK(result.ground_contact->normal.y <= -0.5F);
}

TEST_CASE("ground contact selection is deterministic", "[collision_world]") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders = {
        {.id = 7,
         .type = ColliderType::solid,
         .geometry = CircleGeometry{{14.0F, 20.0F}, 4.0F}},
        {.id = 3,
         .type = ColliderType::solid,
         .geometry = CircleGeometry{{14.0F, 20.0F}, 4.0F}},
    };
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);
    PlayerState player{.position = {14.0F, 16.2F}, .velocity = {0.0F, 4.0F}};

    const ResolveResult result = world.resolve({14.0F, 15.0F}, player);

    REQUIRE(result.ground_contact);
    CHECK(result.ground_contact->collider_id == 3);
}

TEST_CASE(
    "ground contact selection uses one snapshot across neighboring screens",
    "[collision_world]") {
    const MapCollider weak = circle_collider(20, {14.0F, 15.9F}, 0.5F);
    const MapCollider strong = circle_collider(10, {14.0F, 0.8F}, 0.5F);
    const CollisionWorld weak_first = CollisionWorld::from_screens(
        {collider_screen(1, {strong}), collider_screen(0, {weak})}, 15);

    const MapCollider strong_lower = circle_collider(10, {14.0F, 15.8F}, 0.5F);
    const MapCollider weak_upper = circle_collider(20, {14.0F, 0.9F}, 0.5F);
    const CollisionWorld strong_first = CollisionWorld::from_screens(
        {collider_screen(0, {strong_lower}), collider_screen(1, {weak_upper})}, 15);

    PlayerState weak_first_player{
        .position = {14.0F, 15.2F},
        .velocity = {0.0F, 4.0F},
    };
    PlayerState strong_first_player = weak_first_player;
    const ResolveResult weak_first_result =
        weak_first.resolve(weak_first_player.position, weak_first_player);
    const ResolveResult strong_first_result =
        strong_first.resolve(strong_first_player.position, strong_first_player);

    REQUIRE(weak_first_result.ground_contact);
    REQUIRE(strong_first_result.ground_contact);
    CHECK(weak_first_result.ground_contact->collider_id == 10);
    CHECK(strong_first_result.ground_contact->collider_id == 10);
    CHECK(weak_first_result.ground_contact->depth == Approx(0.3F).margin(1e-4F));
    CHECK(strong_first_result.ground_contact->depth == Approx(0.3F).margin(1e-4F));
    CHECK(weak_first_player.position.y ==
          Approx(strong_first_player.position.y).margin(1e-4F));
}

TEST_CASE(
    "ground contact ranking prioritizes normal then depth then identity",
    "[collision_world]") {
    SECTION("a more upward normal wins over greater depth and lower id") {
        ScreenMap screen = collider_screen(1, {
            circle_collider(1, {14.6F, 0.9F}, 0.7F),
            circle_collider(99, {14.0F, 1.0F}, 0.5F),
        });
        const CollisionWorld world = CollisionWorld::from_screens({screen}, 15);
        PlayerState player{.position = {14.0F, 15.2F}};

        const ResolveResult result = world.resolve(player.position, player);

        REQUIRE(result.ground_contact);
        CHECK(result.ground_contact->collider_id == 99);
        CHECK(result.ground_contact->normal.y == Approx(-1.0F));
        CHECK(result.ground_contact->depth == Approx(0.1F).margin(1e-4F));
    }

    SECTION("greater depth wins over lower id regardless of authored order") {
        ScreenMap screen = collider_screen(1, {
            circle_collider(99, {14.0F, 0.8F}, 0.5F),
            circle_collider(1, {14.0F, 0.9F}, 0.5F),
        });
        std::reverse(screen.colliders.begin(), screen.colliders.end());
        const CollisionWorld world = CollisionWorld::from_screens({screen}, 15);
        PlayerState player{.position = {14.0F, 15.2F}};

        const ResolveResult result = world.resolve(player.position, player);

        REQUIRE(result.ground_contact);
        CHECK(result.ground_contact->collider_id == 99);
        CHECK(result.ground_contact->normal.y == Approx(-1.0F));
        CHECK(result.ground_contact->depth == Approx(0.3F).margin(1e-4F));
    }
}

TEST_CASE(
    "equal contacts from one concave collider prefer the lower piece index",
    "[collision_world]") {
    ScreenMap screen = collider_screen(0, {{
        .id = 42,
        .type = ColliderType::solid,
        .geometry = PolygonGeometry{{
            {0.0F, 0.0F}, {2.0F, 0.0F}, {4.0F, 0.0F},
            {4.0F, 4.0F}, {3.0F, 4.0F}, {3.0F, 2.0F},
            {1.0F, 2.0F}, {1.0F, 4.0F}, {0.0F, 4.0F},
        }},
    }});
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 15);
    const auto* colliders = world.colliders_for_screen(0);
    REQUIRE(colliders != nullptr);

    const Aabb box{
        Vec2{2.0F, -0.3F} - config::player_half_size,
        Vec2{2.0F, -0.3F} + config::player_half_size,
    };
    std::vector<int> tied_piece_indices;
    for (const WorldCollider& collider : *colliders) {
        const auto* polygon = std::get_if<ConvexPolygon>(&collider.geometry);
        REQUIRE(polygon != nullptr);
        const auto contact = contact_aabb_polygon(box, *polygon);
        if (contact && contact->normal.y == Approx(-1.0F) &&
            contact->depth == Approx(0.1F).margin(1e-4F)) {
            tied_piece_indices.push_back(collider.piece_index);
        }
    }
    REQUIRE(tied_piece_indices.size() >= 2);

    PlayerState player{.position = {2.0F, -0.3F}};
    const ResolveResult result = world.resolve(player.position, player);

    REQUIRE(result.ground_contact);
    CHECK(result.ground_contact->collider_id == 42);
    CHECK(result.ground_contact->piece_index ==
          *std::min_element(tied_piece_indices.begin(), tied_piece_indices.end()));
}

TEST_CASE("blocking overlap dispatches to exact capsule geometry", "[collision_world]") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders.push_back({
        .id = 5,
        .type = ColliderType::solid,
        .geometry = CapsuleGeometry{{5.0F, 10.0F}, {15.0F, 10.0F}, 1.0F},
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);

    CHECK(world.overlaps_blocking({{9.8F, 8.8F}, {10.2F, 9.2F}}));
    CHECK_FALSE(world.overlaps_blocking({{9.8F, 7.8F}, {10.2F, 8.2F}}));
}

TEST_CASE("one-way behavior remains polygon-only", "[collision_world]") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 28;
    screen.height = 36;
    screen.colliders.push_back({
        .id = 6,
        .type = ColliderType::oneway,
        .geometry = CircleGeometry{{10.0F, 10.0F}, 2.0F},
    });
    const CollisionWorld world = CollisionWorld::from_screens({screen}, 36);

    CHECK_FALSE(world.overlaps_blocking({{9.5F, 8.0F}, {10.5F, 9.0F}}));
}

TEST_CASE("wall_bounce_restitution scales with impact speed and caps at 1.0") {
    using config::wall_bounce_restitution;

    // Clamps to the minimum at/below the low anchor.
    REQUIRE(wall_bounce_restitution(0.0F) == Approx(config::wall_bounce_min));
    REQUIRE(wall_bounce_restitution(config::wall_bounce_impact_lo) ==
            Approx(config::wall_bounce_min));

    // Clamps to the maximum at/above the high anchor, never exceeding 1.0.
    REQUIRE(wall_bounce_restitution(config::wall_bounce_impact_hi) ==
            Approx(config::wall_bounce_max));
    REQUIRE(wall_bounce_restitution(100.0F) == Approx(config::wall_bounce_max));
    REQUIRE(wall_bounce_restitution(100.0F) <= 1.0F);

    // Monotonic ramp strictly between the anchors.
    const float mid = wall_bounce_restitution(
        0.5F * (config::wall_bounce_impact_lo + config::wall_bounce_impact_hi));
    REQUIRE(mid > config::wall_bounce_min);
    REQUIRE(mid < config::wall_bounce_max);
}
