#pragma once

#include "jumpcastle/campaign.hpp"
#include "jumpcastle/level.hpp"

namespace jumpcastle {

[[nodiscard]] CampaignEvent simulate_step(
    PlayerState& player,
    CampaignState& campaign,
    const LevelRepository& level,
    PlayerInput input,
    float delta) noexcept;

}  // namespace jumpcastle
