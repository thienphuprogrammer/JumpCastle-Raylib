#include "jumpcastle/solver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <sstream>

namespace jumpcastle {
namespace {

struct Surface {
    std::size_t room;
    int row;
    int start_x;
    int end_x;
    float world_y;
};

struct Transition {
    std::size_t destination;
    SolverJump jump;
};

std::vector<Surface> extract_surfaces(
    const LevelRepository& level,
    const std::size_t room_filter) {
    std::vector<Surface> surfaces;
    const Room& room = level.room(room_filter);
    const float offset = -static_cast<float>((room_filter + 1) * config::tilemap_height);

    for (int y = 0; y < config::tilemap_height; ++y) {
        int x = 0;
        while (x < config::tilemap_width) {
            const bool top = room.tilemap.solid_at(x, y) &&
                !room.tilemap.solid_at(x, y - 1);
            if (!top) {
                ++x;
                continue;
            }
            const int start = x;
            while (x + 1 < config::tilemap_width &&
                   room.tilemap.solid_at(x + 1, y) &&
                   !room.tilemap.solid_at(x + 1, y - 1)) {
                ++x;
            }
            surfaces.push_back({room_filter, y, start, x, offset + static_cast<float>(y)});
            ++x;
        }
    }
    return surfaces;
}

std::optional<std::size_t> landing_surface(
    const std::vector<Surface>& surfaces,
    const PlayerState& player) {
    const float feet = player.position.y + config::player_half_size.y;
    for (std::size_t index = 0; index < surfaces.size(); ++index) {
        const auto& surface = surfaces[index];
        if (std::abs(feet - surface.world_y) > 0.12F) continue;
        const float left = static_cast<float>(surface.start_x) - config::player_half_size.x;
        const float right = static_cast<float>(surface.end_x + 1) + config::player_half_size.x;
        if (player.position.x >= left && player.position.x <= right) return index;
    }
    return std::nullopt;
}

PlayerInput input_for_direction(
    const JumpDirection direction,
    const bool jump_down,
    const bool jump_released) {
    return {
        .left = direction == JumpDirection::left,
        .right = direction == JumpDirection::right,
        .jump_down = jump_down,
        .jump_released = jump_released,
    };
}

std::optional<Transition> simulate_jump(
    const LevelRepository& level,
    const SolverConfig& config_value,
    const std::vector<Surface>& surfaces,
    const std::size_t source_index,
    const float launch_x,
    const JumpDirection direction,
    const int charge_frames) {
    const Surface& source = surfaces[source_index];
    PlayerState player{};
    player.position = {launch_x, source.world_y - config::player_half_size.y};
    player.on_ground = true;
    CampaignState campaign{
        .respawn_position = level.campaign_spawn(),
        .checkpoint_room = 0,
    };
    const float delta = 1.0F / static_cast<float>(config_value.simulation_hz);

    for (int frame = 0; frame < charge_frames; ++frame) {
        if (simulate_step(
                player,
                campaign,
                level,
                input_for_direction(direction, true, false),
                delta) == CampaignEvent::respawned) {
            return std::nullopt;
        }
    }

    if (simulate_step(
            player,
            campaign,
            level,
            input_for_direction(direction, false, true),
            delta) == CampaignEvent::respawned) {
        return std::nullopt;
    }

    for (int frame = 0; frame < config_value.maximum_air_frames; ++frame) {
        const CampaignEvent event = simulate_step(
            player, campaign, level, PlayerInput{}, delta);
        if (event == CampaignEvent::respawned) return std::nullopt;

        if (frame > 1 && player.on_ground) {
            const auto destination = landing_surface(surfaces, player);
            if (!destination || *destination == source_index) return std::nullopt;
            return Transition{
                *destination,
                {
                    {launch_x, source.world_y - config::player_half_size.y},
                    player.position,
                    direction,
                    charge_frames,
                    source.room,
                    surfaces[*destination].room,
                },
            };
        }
    }
    return std::nullopt;
}

bool tolerant_jump(
    const LevelRepository& level,
    const SolverConfig& config_value,
    const std::vector<Surface>& surfaces,
    const std::size_t source,
    const std::size_t destination,
    const SolverJump& jump) {
    const std::array<std::pair<float, int>, 5> variants{{
        {0.0F, 0}, {0.0F, -2}, {0.0F, 2}, {-0.1F, 0}, {0.1F, 0},
    }};
    int passed = 0;
    for (const auto [x_delta, frame_delta] : variants) {
        const auto result = simulate_jump(
            level,
            config_value,
            surfaces,
            source,
            jump.start.x + x_delta,
            jump.direction,
            std::max(1, jump.charge_frames + frame_delta));
        if (result && result->destination == destination) ++passed;
    }
    return passed >= 3;
}

SolverResult solve_surfaces(
    const LevelRepository& level,
    const SolverConfig& config_value,
    const std::size_t room_index) {
    const auto surfaces = extract_surfaces(level, room_index);
    if (surfaces.size() < 2) {
        return {.failure = "nearest surface unavailable: room has fewer than two surfaces"};
    }

    const auto lowest = std::max_element(
        surfaces.begin(), surfaces.end(),
        [](const Surface& a, const Surface& b) { return a.world_y < b.world_y; });
    const auto highest = std::min_element(
        surfaces.begin(), surfaces.end(),
        [](const Surface& a, const Surface& b) { return a.world_y < b.world_y; });
    const std::size_t start = static_cast<std::size_t>(lowest - surfaces.begin());
    const std::size_t goal = static_cast<std::size_t>(highest - surfaces.begin());

    std::vector<bool> visited(surfaces.size());
    std::vector<std::optional<std::size_t>> parent(surfaces.size());
    std::vector<SolverJump> parent_jump(surfaces.size());
    std::queue<std::size_t> pending;
    visited[start] = true;
    pending.push(start);

    while (!pending.empty() && !visited[goal]) {
        const std::size_t source_index = pending.front();
        pending.pop();
        const auto& source = surfaces[source_index];
        const float min_x = static_cast<float>(source.start_x) +
            config::player_half_size.x + 0.1F;
        const float max_x = static_cast<float>(source.end_x + 1) -
            config::player_half_size.x - 0.1F;

        for (float x = min_x; x <= max_x + 0.001F; x += config_value.launch_sample_spacing) {
            for (const JumpDirection direction : {
                     JumpDirection::left, JumpDirection::neutral, JumpDirection::right}) {
                for (const int charge : config_value.charge_frames) {
                    const float ratio = static_cast<float>(charge) / 47.0F;
                    if (ratio > 0.85F) continue;
                    const auto transition = simulate_jump(
                        level, config_value, surfaces, source_index, x, direction, charge);
                    if (!transition || visited[transition->destination]) continue;
                    visited[transition->destination] = true;
                    parent[transition->destination] = source_index;
                    parent_jump[transition->destination] = transition->jump;
                    pending.push(transition->destination);
                }
            }
        }
    }

    if (!visited[goal]) {
        return {.failure = "nearest surface reached but upper route is outside jump envelope"};
    }

    std::vector<std::size_t> path_surfaces;
    std::vector<SolverJump> jumps;
    for (std::size_t current = goal; current != start; current = *parent[current]) {
        path_surfaces.push_back(current);
        jumps.push_back(parent_jump[current]);
    }
    path_surfaces.push_back(start);
    std::reverse(path_surfaces.begin(), path_surfaces.end());
    std::reverse(jumps.begin(), jumps.end());

    bool tolerance = true;
    float maximum_ratio = 0.0F;
    for (std::size_t index = 0; index < jumps.size(); ++index) {
        maximum_ratio = std::max(
            maximum_ratio,
            static_cast<float>(jumps[index].charge_frames) / 47.0F);
        tolerance = tolerance && tolerant_jump(
            level,
            config_value,
            surfaces,
            path_surfaces[index],
            path_surfaces[index + 1],
            jumps[index]);
    }

    return {
        .reachable = tolerance,
        .tolerance_passed = tolerance,
        .maximum_charge_ratio = maximum_ratio,
        .jumps = std::move(jumps),
        .failure = tolerance ? std::string{} : "route failed tolerance replay",
    };
}

}  // namespace

ReachabilitySolver::ReachabilitySolver(
    const LevelRepository& level,
    const SolverConfig config_value)
    : level_{level}, config_{config_value} {}

SolverResult ReachabilitySolver::solve_room(const std::size_t room_index) const {
    if (room_index >= LevelRepository::room_count) {
        return {.failure = "room index is outside campaign"};
    }
    return solve_surfaces(level_, config_, room_index);
}

SolverResult ReachabilitySolver::solve_campaign() const {
    SolverResult campaign{.reachable = true, .tolerance_passed = true};
    for (std::size_t room = 0; room < LevelRepository::room_count; ++room) {
        SolverResult result = solve_room(room);
        if (!result.reachable) {
            result.failure = "room " + std::to_string(room + 1) + ": " + result.failure;
            return result;
        }
        campaign.maximum_charge_ratio = std::max(
            campaign.maximum_charge_ratio, result.maximum_charge_ratio);
        campaign.jumps.insert(
            campaign.jumps.end(), result.jumps.begin(), result.jumps.end());
    }
    return campaign;
}

std::string_view to_string(const JumpDirection direction) noexcept {
    switch (direction) {
    case JumpDirection::left: return "left";
    case JumpDirection::neutral: return "neutral";
    case JumpDirection::right: return "right";
    }
    return "unknown";
}

}  // namespace jumpcastle
