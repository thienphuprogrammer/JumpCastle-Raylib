#include "jumpcastle/simulation.hpp"

#include "test_level_factory.hpp"

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
