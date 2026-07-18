#include "jumpcastle/simulation.hpp"
#include "jumpcastle/fixed_step.hpp"

#include "jumpcastle/convex.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("step_world over a polygon campaign world lands, dies, and completes") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 16;
    screen.height = 15;
    const std::vector<Vec2> floor{{0, 14}, {16, 14}, {16, 15}, {0, 15}};
    screen.polygons.push_back(
        {floor, outward_edge_normals(floor), polygon_aabb(floor), ColliderType::solid});
    const std::vector<Vec2> spike{{7, 13}, {9, 13}, {8, 12.5F}};
    screen.polygons.push_back(
        {spike, outward_edge_normals(spike), polygon_aabb(spike), ColliderType::hazard});
    screen.entities.push_back({EntityType::spawn, {2.0F, 13.0F}});
    screen.entities.push_back({EntityType::goal, {13.0F, 13.0F}});
    const CampaignWorld world = CampaignWorld::from_screens({screen}, 15);

    REQUIRE(world.spawn == Vec2{2.0F, 13.0F});
    REQUIRE(world.goal == Vec2{13.0F, 13.0F});
    REQUIRE(world.height == 15);

    SECTION("falling onto the floor away from the spike stays alive") {
        PlayerState player;
        player.position = {2.0F, 8.0F};
        player.mode = PlayerMode::airborne;
        CampaignState campaign;
        campaign.spawn = world.spawn;
        CampaignEvent last = CampaignEvent::none;
        for (int tick = 0; tick < 600; ++tick) {
            last = step_world(player, campaign, world, PlayerInput{});
            if (last == CampaignEvent::respawned) { break; }
        }
        REQUIRE(last != CampaignEvent::respawned);
        REQUIRE(player.on_ground);
    }

    SECTION("landing on the spike respawns") {
        PlayerState player;
        player.position = {8.0F, 11.5F};
        player.mode = PlayerMode::airborne;
        CampaignState campaign;
        campaign.spawn = world.spawn;
        CampaignEvent last = CampaignEvent::none;
        for (int tick = 0; tick < 200; ++tick) {
            last = step_world(player, campaign, world, PlayerInput{});
            if (last == CampaignEvent::respawned) { break; }
        }
        REQUIRE(last == CampaignEvent::respawned);
    }
}

TEST_CASE("frame chunking produces identical fixed steps") {
    FixedStepClock one;
    FixedStepClock two;

    CHECK(one.consume(1.0F / 30.0F) == 4);
    CHECK(two.consume(1.0F / 60.0F) == 2);
    CHECK(two.consume(1.0F / 60.0F) == 2);
    CHECK(one.remainder() == Catch::Approx(two.remainder()));
}

TEST_CASE("polygon campaign world respawns below the world and completes at goal") {
    ScreenMap screen;
    screen.index = 0;
    screen.width = 16;
    screen.height = 15;
    const std::vector<Vec2> floor{{0, 14}, {16, 14}, {16, 15}, {0, 15}};
    screen.polygons.push_back(
        {floor, outward_edge_normals(floor), polygon_aabb(floor), ColliderType::solid});
    screen.entities.push_back({EntityType::spawn, {2.0F, 13.0F}});
    screen.entities.push_back({EntityType::goal, {8.0F, 13.0F}});
    const CampaignWorld world = CampaignWorld::from_screens({screen}, 15);

    SECTION("falling below the world resets to spawn") {
        CampaignState campaign{.spawn = world.spawn};
        PlayerState player{.position = {2.0F, 40.0F}, .mode = PlayerMode::airborne};
        CHECK(step_world(player, campaign, world, {}) == CampaignEvent::fell_below_world);
        CHECK(player.position == world.spawn);
        CHECK(campaign.falls == 1);
    }

    SECTION("overlapping the goal completes the campaign") {
        CampaignState campaign{.spawn = world.spawn};
        PlayerState player{
            .position = world.goal, .mode = PlayerMode::grounded, .on_ground = true};
        CHECK(step_world(player, campaign, world, {}) == CampaignEvent::completed);
        CHECK(campaign.complete);
    }
}
