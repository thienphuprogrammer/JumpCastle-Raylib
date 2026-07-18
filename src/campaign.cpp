#include "jumpcastle/campaign.hpp"

namespace jumpcastle {

void activate_checkpoint(
    CampaignState& campaign,
    const Vec2 position,
    const std::size_t room) noexcept {
    campaign.respawn_position = position;
    campaign.checkpoint_room = room;
}

void reset_player(PlayerState& player, const Vec2 position) noexcept {
    player = PlayerState{};
    player.position = position;
}

void respawn_player(CampaignState& campaign, PlayerState& player) noexcept {
    reset_player(player, campaign.respawn_position);
    ++campaign.falls;
    ++campaign.deaths;
}

void restart_campaign(
    CampaignState& campaign,
    const Vec2 spawn,
    const std::size_t room) noexcept {
    campaign = CampaignState{};
    campaign.spawn = spawn;
    campaign.respawn_position = spawn;
    campaign.checkpoint_room = room;
}

}  // namespace jumpcastle
