#include "jumpcastle/solver.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace jumpcastle {
namespace {

struct Surface {
    int row{};
    int start_x{};
    int end_x{};
    int screen{};
};

struct Transition {
    std::size_t destination{};
    SolverJump jump;
};

[[nodiscard]] std::vector<Surface> extract_surfaces(const WorldMap& world) {
    std::vector<Surface> surfaces;
    for (int y = 0; y < world.height(); ++y) {
        int x = 0;
        while (x < world.width()) {
            if (!world.solid_at(x, y) || world.solid_at(x, y - 1)) {
                ++x;
                continue;
            }

            const int start = x;
            while (x + 1 < world.width() && world.solid_at(x + 1, y) &&
                   !world.solid_at(x + 1, y - 1)) {
                ++x;
            }
            const bool boundary_cap = start == x &&
                (start == 0 || start == world.width() - 1);
            if (!boundary_cap) {
                surfaces.push_back({
                    .row = y,
                    .start_x = start,
                    .end_x = x,
                    .screen = world.screen_for_y(static_cast<float>(y) - 0.5F),
                });
            }
            ++x;
        }
    }
    return surfaces;
}

[[nodiscard]] std::optional<std::size_t> supporting_surface(
    const std::vector<Surface>& surfaces,
    const Vec2 position) {
    const float feet = position.y + config::player_half_size.y;
    for (std::size_t index = 0; index < surfaces.size(); ++index) {
        const Surface& surface = surfaces[index];
        if (std::abs(feet - static_cast<float>(surface.row)) > 0.13F) {
            continue;
        }
        if (position.x > static_cast<float>(surface.start_x) - config::player_half_size.x &&
            position.x < static_cast<float>(surface.end_x + 1) + config::player_half_size.x) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> marker_surface(
    const std::vector<Surface>& surfaces,
    const Vec2 marker) {
    for (std::size_t index = 0; index < surfaces.size(); ++index) {
        const Surface& surface = surfaces[index];
        if (std::abs(static_cast<float>(surface.row) - (marker.y + 0.5F)) < 0.01F &&
            marker.x >= static_cast<float>(surface.start_x) &&
            marker.x < static_cast<float>(surface.end_x + 1)) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] PlayerInput input_for(
    const JumpDirection direction,
    const bool jump_down,
    const bool jump_released) noexcept {
    return {
        .left = direction == JumpDirection::left,
        .right = direction == JumpDirection::right,
        .jump_down = jump_down,
        .jump_released = jump_released,
    };
}

[[nodiscard]] std::optional<Transition> simulate_jump(
    const WorldMap& world,
    const SolverConfig& solver_config,
    const std::vector<Surface>& surfaces,
    const std::size_t source_index,
    const std::size_t goal_index,
    const float launch_x,
    const JumpDirection direction,
    const int charge_ticks) {
    const Surface& source = surfaces[source_index];
    PlayerState player{
        .position = {launch_x, static_cast<float>(source.row) - config::player_half_size.y},
        .mode = PlayerMode::grounded,
        .on_ground = true,
        .facing_right = direction != JumpDirection::left,
    };
    CampaignState campaign{.spawn = world.spawn()};

    for (int tick = 0; tick < charge_ticks; ++tick) {
        const CampaignEvent event = step_world(
            player, campaign, world, input_for(direction, true, false));
        if (event == CampaignEvent::fell_below_world) {
            return std::nullopt;
        }
        if (event == CampaignEvent::completed) {
            return Transition{
                goal_index,
                {{launch_x, static_cast<float>(source.row) - config::player_half_size.y},
                 player.position,
                 direction,
                 charge_ticks,
                 source.screen,
                 surfaces[goal_index].screen},
            };
        }
    }

    CampaignEvent event = step_world(
        player, campaign, world, input_for(direction, false, true));
    if (event == CampaignEvent::fell_below_world) {
        return std::nullopt;
    }
    if (event == CampaignEvent::completed) {
        return Transition{
            goal_index,
            {{launch_x, static_cast<float>(source.row) - config::player_half_size.y},
             player.position,
             direction,
             charge_ticks,
             source.screen,
             surfaces[goal_index].screen},
        };
    }

    for (int tick = 0; tick < solver_config.maximum_air_ticks; ++tick) {
        event = step_world(player, campaign, world, {});
        if (event == CampaignEvent::fell_below_world) {
            return std::nullopt;
        }
        if (event == CampaignEvent::completed) {
            return Transition{
                goal_index,
                {{launch_x, static_cast<float>(source.row) - config::player_half_size.y},
                 player.position,
                 direction,
                 charge_ticks,
                 source.screen,
                 surfaces[goal_index].screen},
            };
        }
        if (tick > 1 && player.mode == PlayerMode::grounded) {
            const auto destination = supporting_surface(surfaces, player.position);
            if (!destination || *destination == source_index) {
                return std::nullopt;
            }
            return Transition{
                *destination,
                {{launch_x, static_cast<float>(source.row) - config::player_half_size.y},
                 player.position,
                 direction,
                 charge_ticks,
                 source.screen,
                 surfaces[*destination].screen},
            };
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool tolerant_jump(
    const WorldMap& world,
    const SolverConfig& solver_config,
    const std::vector<Surface>& surfaces,
    const std::size_t source,
    const std::size_t destination,
    const std::size_t goal,
    const SolverJump& jump) {
    constexpr std::array<std::pair<float, int>, 5> variants{{
        {0.0F, 0}, {0.0F, -2}, {0.0F, 2}, {-0.1F, 0}, {0.1F, 0},
    }};
    int passed = 0;
    bool negative_x_passed{};
    bool positive_x_passed{};
    for (std::size_t variant = 0; variant < variants.size(); ++variant) {
        const auto [x_delta, tick_delta] = variants[variant];
        const auto result = simulate_jump(
            world,
            solver_config,
            surfaces,
            source,
            goal,
            jump.start.x + x_delta,
            jump.direction,
            std::max(1, jump.charge_ticks + tick_delta));
        if (result && result->destination == destination) {
            ++passed;
            negative_x_passed = negative_x_passed || variant == 3;
            positive_x_passed = positive_x_passed || variant == 4;
        }
    }
    return passed >= 3 && negative_x_passed && positive_x_passed;
}

}  // namespace

ReachabilitySolver::ReachabilitySolver(
    const WorldMap& world,
    const SolverConfig config_value)
    : world_{world}, config_{config_value} {}

SolverResult ReachabilitySolver::solve_campaign() const {
    const std::vector<Surface> surfaces = extract_surfaces(world_);
    const auto start = marker_surface(surfaces, world_.spawn());
    const auto goal = marker_surface(surfaces, world_.goal());
    if (!start || !goal) {
        return {.failure = "spawn or goal has no stable supporting surface"};
    }

    std::vector<bool> visited(surfaces.size());
    std::vector<std::optional<std::size_t>> parent(surfaces.size());
    std::vector<SolverJump> parent_jump(surfaces.size());
    std::queue<std::size_t> pending;
    visited[*start] = true;
    pending.push(*start);
    int highest_screen = surfaces[*start].screen;
    std::size_t highest_surface = *start;
    const int maximum_charge_ticks = static_cast<int>(
        std::floor(config::maximum_charge_seconds / config::fixed_delta));

    while (!pending.empty() && !visited[*goal]) {
        const std::size_t source_index = pending.front();
        pending.pop();
        const Surface& source = surfaces[source_index];
        const float min_x = static_cast<float>(source.start_x) +
            config::player_half_size.x + 0.05F;
        const float max_x = static_cast<float>(source.end_x + 1) -
            config::player_half_size.x - 0.05F;

        for (float x = min_x; x <= max_x + 0.001F; x += config_.launch_sample_spacing) {
            for (const JumpDirection direction : {
                     JumpDirection::left, JumpDirection::right, JumpDirection::neutral}) {
                for (const int charge : config_.charge_ticks) {
                    if (static_cast<float>(charge) /
                            static_cast<float>(maximum_charge_ticks) >
                        config_.max_charge_ratio) {
                        continue;
                    }
                    const auto transition = simulate_jump(
                        world_, config_, surfaces, source_index, *goal, x, direction, charge);
                    if (!transition || visited[transition->destination]) {
                        continue;
                    }
                    if (!tolerant_jump(
                            world_, config_, surfaces, source_index,
                            transition->destination, *goal, transition->jump)) {
                        continue;
                    }

                    const std::size_t destination = transition->destination;
                    visited[destination] = true;
                    parent[destination] = source_index;
                    parent_jump[destination] = transition->jump;
                    pending.push(destination);
                    if (surfaces[destination].screen > highest_screen ||
                        (surfaces[destination].screen == highest_screen &&
                         surfaces[destination].row < surfaces[highest_surface].row)) {
                        highest_screen = surfaces[destination].screen;
                        highest_surface = destination;
                    }
                }
            }
        }
    }

    if (!visited[*goal]) {
        return {
            .highest_screen = highest_screen,
            .failure = "highest screen " + std::to_string(highest_screen + 1) +
                " of " + std::to_string(world_.screen_count()) +
                "; nearest surface row " + std::to_string(surfaces[highest_surface].row),
        };
    }

    std::vector<std::size_t> path_surfaces;
    std::vector<SolverJump> jumps;
    for (std::size_t current = *goal; current != *start; current = *parent[current]) {
        path_surfaces.push_back(current);
        jumps.push_back(parent_jump[current]);
    }
    path_surfaces.push_back(*start);
    std::reverse(path_surfaces.begin(), path_surfaces.end());
    std::reverse(jumps.begin(), jumps.end());

    bool tolerance = true;
    float maximum_ratio = 0.0F;
    for (std::size_t index = 0; index < jumps.size(); ++index) {
        maximum_ratio = std::max(
            maximum_ratio,
            static_cast<float>(jumps[index].charge_ticks) /
                static_cast<float>(maximum_charge_ticks));
        tolerance = tolerance && tolerant_jump(
            world_, config_, surfaces, path_surfaces[index],
            path_surfaces[index + 1], *goal, jumps[index]);
    }

    return {
        .reachable = tolerance,
        .tolerance_passed = tolerance,
        .maximum_charge_ratio = maximum_ratio,
        .highest_screen = surfaces[*goal].screen,
        .jumps = std::move(jumps),
        .failure = tolerance ? std::string{} : "route failed tolerance replay",
    };
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
