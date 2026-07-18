#include "jumpcastle/simulation.hpp"

#include "jumpcastle/game_config.hpp"

namespace jumpcastle {
namespace {

bool overlaps_goal(const PlayerState& player, const Vec2 goal) noexcept {
    return player.position.x + config::player_half_size.x > goal.x - 0.5F &&
        player.position.x - config::player_half_size.x < goal.x + 0.5F &&
        player.position.y + config::player_half_size.y > goal.y - 0.5F &&
        player.position.y - config::player_half_size.y < goal.y + 0.5F;
}

}  // namespace

CampaignEvent step_world(
    PlayerState& player,
    CampaignState& campaign,
    const CampaignWorld& world,
    const PlayerInput input) noexcept {
    if (campaign.complete) {
        return CampaignEvent::completed;
    }

    const ResolveResult result =
        step_player(player, world.collision, input, config::fixed_delta);

    if (result.hit_hazard) {
        respawn_player(campaign, player);
        return CampaignEvent::respawned;
    }

    if (player.position.y - config::player_half_size.y >
        static_cast<float>(world.height)) {
        reset_player(player, campaign.spawn);
        ++campaign.falls;
        return CampaignEvent::fell_below_world;
    }

    if (overlaps_goal(player, world.goal)) {
        campaign.complete = true;
        return CampaignEvent::completed;
    }

    campaign.elapsed_seconds += static_cast<double>(config::fixed_delta);
    return CampaignEvent::none;
}

}  // namespace jumpcastle
