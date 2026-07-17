#include "jumpcastle/campaign.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace jumpcastle;

TEST_CASE("checkpoint changes deterministic respawn") {
    CampaignState campaign{.respawn_position = {8.5F, -3.5F}};
    PlayerState player{};

    activate_checkpoint(campaign, {3.5F, -18.5F}, 1);
    player.position = {9.0F, -30.0F};
    player.velocity = {4.0F, 7.0F};
    player.jump_hold_time = 0.5F;
    respawn_player(campaign, player);

    CHECK(player.position.x == 3.5F);
    CHECK(player.position.y == -18.5F);
    CHECK(player.velocity.x == 0.0F);
    CHECK(player.velocity.y == 0.0F);
    CHECK(player.jump_hold_time == 0.0F);
    CHECK(campaign.deaths == 1);
}

TEST_CASE("restart clears campaign progress") {
    CampaignState campaign{
        .respawn_position = {3.5F, -18.5F},
        .checkpoint_room = 1,
        .deaths = 7,
        .elapsed_seconds = 42.0F,
        .complete = true,
    };

    restart_campaign(campaign, {8.5F, -3.5F}, 0);

    CHECK(campaign.respawn_position.x == 8.5F);
    CHECK(campaign.checkpoint_room == 0);
    CHECK(campaign.deaths == 0);
    CHECK(campaign.elapsed_seconds == 0.0F);
    CHECK_FALSE(campaign.complete);
}
