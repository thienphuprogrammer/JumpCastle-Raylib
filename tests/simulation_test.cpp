#include "jumpcastle/simulation.hpp"

#include "test_level_factory.hpp"
#include "test_world_factory.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("spike overlap respawns through production step") {
    const LevelRepository level = test::make_level(Tile::spike);
    CampaignState campaign{.respawn_position = level.campaign_spawn()};
    PlayerState player{};
    player.position = {6.5F, -3.5F};

    const CampaignEvent event = simulate_step(
        player, campaign, level, PlayerInput{}, 1.0F / 60.0F);

    CHECK(event == CampaignEvent::respawned);
    CHECK(campaign.deaths == 1);
    CHECK(player.position.x == campaign.respawn_position.x);
    CHECK(player.position.y == campaign.respawn_position.y);
}

TEST_CASE("checkpoint overlap activates campaign progress") {
    const LevelRepository level = test::make_level();
    CampaignState campaign{.respawn_position = level.campaign_spawn()};
    PlayerState player{};
    player.position = {8.5F, -2.5F};

    const CampaignEvent event = simulate_step(
        player, campaign, level, PlayerInput{}, 1.0F / 60.0F);

    CHECK(event == CampaignEvent::checkpoint_activated);
    CHECK(campaign.checkpoint_room == 0);
}

TEST_CASE("only falling below the world resets progress") {
    const WorldMap world = test::three_screen_world();
    CampaignState campaign{.spawn = world.spawn()};
    PlayerState player{
        .position = {2.0F, 30.5F},
        .mode = PlayerMode::airborne,
    };

    CHECK(step_world(player, campaign, world, {}) == CampaignEvent::fell_below_world);
    CHECK(player.position == world.spawn());
    CHECK(campaign.falls == 1);
}

TEST_CASE("crossing an internal camera boundary remains a physical fall") {
    const WorldMap world = test::three_screen_world();
    CampaignState campaign{.spawn = world.spawn()};
    PlayerState player{
        .position = {5.5F, 20.25F},
        .velocity = {0.0F, 1.0F},
        .mode = PlayerMode::airborne,
    };

    CHECK(step_world(player, campaign, world, {}) == CampaignEvent::none);
    CHECK(campaign.falls == 0);
    CHECK(player.position.y > 20.25F);
}

TEST_CASE("overlapping the world goal completes the campaign") {
    const WorldMap world = test::three_screen_world();
    CampaignState campaign{.spawn = world.spawn()};
    PlayerState player{
        .position = world.goal(),
        .mode = PlayerMode::grounded,
        .on_ground = true,
    };

    CHECK(step_world(player, campaign, world, {}) == CampaignEvent::completed);
    CHECK(campaign.complete);
}
