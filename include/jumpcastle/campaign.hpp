#pragma once

#include "jumpcastle/player.hpp"

#include <cstddef>

namespace jumpcastle {

enum class CampaignEvent {
    none,
    checkpoint_activated,
    respawned,
    completed,
};

struct CampaignState {
    Vector2 respawn_position{};
    std::size_t checkpoint_room{};
    int deaths{};
    float elapsed_seconds{};
    bool complete{};
};

void activate_checkpoint(
    CampaignState& campaign,
    Vector2 position,
    std::size_t room) noexcept;
void reset_player(PlayerState& player, Vector2 position) noexcept;
void respawn_player(CampaignState& campaign, PlayerState& player) noexcept;
void restart_campaign(
    CampaignState& campaign,
    Vector2 spawn,
    std::size_t room) noexcept;

}  // namespace jumpcastle
