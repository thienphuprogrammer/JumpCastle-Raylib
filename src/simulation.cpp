#include "jumpcastle/simulation.hpp"

namespace jumpcastle {
namespace {

struct MarkerHit {
    bool found{};
    Vector2 position{};
};

MarkerHit marker_hit(
    const RoomSelection& selection,
    const Vector2 player_position,
    const Tile marker) noexcept {
    Vector2 local_position = player_position;
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

}  // namespace jumpcastle
