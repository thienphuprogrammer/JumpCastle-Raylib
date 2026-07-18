#include "jumpcastle/simulation.hpp"

namespace jumpcastle {
namespace {

bool overlaps_goal(const PlayerState& player, const Vec2 goal) noexcept {
    return player.position.x + config::player_half_size.x > goal.x - 0.5F &&
        player.position.x - config::player_half_size.x < goal.x + 0.5F &&
        player.position.y + config::player_half_size.y > goal.y - 0.5F &&
        player.position.y - config::player_half_size.y < goal.y + 0.5F;
}

struct MarkerHit {
    bool found{};
    Vec2 position{};
};

MarkerHit marker_hit(
    const RoomSelection& selection,
    const Vec2 player_position,
    const Tile marker) noexcept {
    Vec2 local_position = player_position;
    local_position.y -= selection.vertical_offset;
    const TileRange range = overlapped_tiles(local_position, config::player_half_size);

    for (int y = range.start_y; y <= range.end_y; ++y) {
        for (int x = range.start_x; x <= range.end_x; ++x) {
            if (selection.room->tilemap.tile_at(x, y) == marker) {
                return {
                    true,
                    {
                        static_cast<float>(x) + 0.5F,
                        selection.vertical_offset + static_cast<float>(y) + 0.5F,
                    },
                };
            }
        }
    }
    return {};
}

}  // namespace

CampaignEvent simulate_step(
    PlayerState& player,
    CampaignState& campaign,
    const LevelRepository& level,
    const PlayerInput input,
    const float delta) noexcept {
    const auto selected = level.select(player.position.y);
    if (!selected) {
        respawn_player(campaign, player);
        return CampaignEvent::respawned;
    }

    update_player(
        player,
        selected->room->tilemap,
        selected->vertical_offset,
        input,
        delta);
    resolve_tilemap_collision(
        selected->room->tilemap,
        selected->vertical_offset,
        player.position,
        player.velocity,
        config::player_half_size);

    if (marker_hit(*selected, player.position, Tile::spike).found) {
        respawn_player(campaign, player);
        return CampaignEvent::respawned;
    }

    const MarkerHit checkpoint = marker_hit(
        *selected, player.position, Tile::checkpoint);
    if (checkpoint.found &&
        (campaign.checkpoint_room != selected->index ||
         campaign.respawn_position.x != checkpoint.position.x ||
         campaign.respawn_position.y != checkpoint.position.y)) {
        activate_checkpoint(
            campaign, checkpoint.position, selected->index);
        return CampaignEvent::checkpoint_activated;
    }

    if (marker_hit(*selected, player.position, Tile::exit).found) {
        campaign.complete = true;
        return CampaignEvent::completed;
    }

    if (!campaign.complete) {
        campaign.elapsed_seconds += delta;
    }
    return CampaignEvent::none;
}

CampaignEvent step_world(
    PlayerState& player,
    CampaignState& campaign,
    const WorldMap& world,
    const PlayerInput input) noexcept {
    if (campaign.complete) {
        return CampaignEvent::completed;
    }

    step_player(player, world, input, config::fixed_delta);

    if (player.position.y - config::player_half_size.y >
        static_cast<float>(world.height())) {
        reset_player(player, campaign.spawn);
        ++campaign.falls;
        return CampaignEvent::fell_below_world;
    }

    if (overlaps_goal(player, world.goal())) {
        campaign.complete = true;
        return CampaignEvent::completed;
    }

    campaign.elapsed_seconds += static_cast<double>(config::fixed_delta);
    return CampaignEvent::none;
}

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
