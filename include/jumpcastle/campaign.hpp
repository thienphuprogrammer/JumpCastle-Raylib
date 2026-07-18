#pragma once

#include "jumpcastle/player.hpp"

#include <cstddef>

namespace jumpcastle {

enum class CampaignEvent {
    none,
    fell_below_world,
    checkpoint_activated,
    respawned,
    completed,
};

struct CampaignState {
    Vec2 spawn{};
    int falls{};

    // Legacy room-campaign compatibility. Removed with the room runtime.
    Vec2 respawn_position{};
    std::size_t checkpoint_room{};
    int deaths{};
    double elapsed_seconds{};
    bool complete{};
};

void activate_checkpoint(
    CampaignState& campaign,
    Vec2 position,
    std::size_t room) noexcept;
void reset_player(PlayerState& player, Vec2 position) noexcept;
void respawn_player(CampaignState& campaign, PlayerState& player) noexcept;
void restart_campaign(
    CampaignState& campaign,
    Vec2 spawn,
    std::size_t room) noexcept;

}  // namespace jumpcastle
