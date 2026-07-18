#pragma once

#include "jumpcastle/campaign.hpp"
#include "jumpcastle/campaign_world.hpp"

namespace jumpcastle {

[[nodiscard]] CampaignEvent step_world(
    PlayerState& player,
    CampaignState& campaign,
    const CampaignWorld& world,
    PlayerInput input) noexcept;

}  // namespace jumpcastle
