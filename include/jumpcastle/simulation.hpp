#pragma once

#include "jumpcastle/campaign.hpp"
#include "jumpcastle/campaign_world.hpp"
#include "jumpcastle/level.hpp"

namespace jumpcastle {

[[nodiscard]] CampaignEvent simulate_step(
    PlayerState& player,
    CampaignState& campaign,
    const LevelRepository& level,
    PlayerInput input,
    float delta) noexcept;

[[nodiscard]] CampaignEvent step_world(
    PlayerState& player,
    CampaignState& campaign,
    const WorldMap& world,
    PlayerInput input) noexcept;

[[nodiscard]] CampaignEvent step_world(
    PlayerState& player,
    CampaignState& campaign,
    const CampaignWorld& world,
    PlayerInput input) noexcept;

}  // namespace jumpcastle
