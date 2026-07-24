#include "jumpcastle/solver.hpp"

#include "jumpcastle/game_config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace jumpcastle {
namespace {

// A standable horizontal span in world coordinates. `y` is the world-y of the
// surface top (the player's feet rest here); `screen` is the bottom-up screen
// number (screen 0 at the base, rising toward the crown) so it matches the
// numbering the grid solver reported.
struct Surface {
    float y{};
    float start_x{};
    float end_x{};
    int screen{};
};

struct Transition {
    std::size_t destination{};
    SolverJump jump;
};

// Bottom-up screen number for a world-y. CampaignWorld indexes bands from the
// top (band 0 at y=0); invert so climbing (decreasing y) yields larger numbers.
[[nodiscard]] int screen_number(const CampaignWorld& world, const float world_y) {
    const int band = static_cast<int>(
        std::floor(world_y / static_cast<float>(world.screen_height)));
    const int clamped = std::clamp(band, 0, world.screen_count() - 1);
    return world.screen_count() - 1 - clamped;
}

void append_upward_edges(
    const CampaignWorld& world,
    const ConvexPolygon& polygon,
    std::vector<Surface>& surfaces) {
    const std::size_t count = polygon.points.size();
    for (std::size_t edge = 0; edge < count; ++edge) {
        if (polygon.edge_normals[edge].y > -0.9F) {
            continue;  // not an upward-facing (top) edge
        }
        const Vec2 a = polygon.points[edge];
        const Vec2 b = polygon.points[(edge + 1) % count];
        if (std::abs(a.x - b.x) < 0.01F) {
            continue;  // vertical/degenerate, not standable
        }
        const float y = (a.y + b.y) * 0.5F;
        surfaces.push_back({
            .y = y,
            .start_x = std::min(a.x, b.x),
            .end_x = std::max(a.x, b.x),
            .screen = screen_number(world, y),
        });
    }
}

[[nodiscard]] std::vector<Surface> merge_surfaces(std::vector<Surface> surfaces) {
    std::sort(surfaces.begin(), surfaces.end(), [](const Surface& l, const Surface& r) {
        if (std::abs(l.y - r.y) > 0.05F) {
            return l.y < r.y;
        }
        return l.start_x < r.start_x;
    });
    std::vector<Surface> merged;
    for (const Surface& surface : surfaces) {
        if (!merged.empty() && std::abs(merged.back().y - surface.y) < 0.05F &&
            surface.start_x <= merged.back().end_x + 0.01F) {
            merged.back().end_x = std::max(merged.back().end_x, surface.end_x);
        } else {
            merged.push_back(surface);
        }
    }
    return merged;
}

// Every upward-facing solid polygon edge becomes a walkable span; collinear
// abutting spans are merged so adjacent colliders read as one surface.
[[nodiscard]] std::vector<Surface> extract_surfaces(const CampaignWorld& world) {
    std::vector<Surface> surfaces;
    for (int index = 0; index < world.screen_count(); ++index) {
        const auto* colliders = world.collision.colliders_for_screen(index);
        if (colliders == nullptr) {
            continue;
        }
        for (const WorldCollider& collider : *colliders) {
            const auto* polygon = std::get_if<ConvexPolygon>(&collider.geometry);
            if (polygon != nullptr && collider.type == ColliderType::solid) {
                append_upward_edges(world, *polygon, surfaces);
            }
        }
    }
    return merge_surfaces(std::move(surfaces));
}

[[nodiscard]] std::optional<std::size_t> supporting_surface(
    const std::vector<Surface>& surfaces,
    const Vec2 position) {
    const float feet = position.y + config::player_half_size.y;
    for (std::size_t index = 0; index < surfaces.size(); ++index) {
        const Surface& surface = surfaces[index];
        if (std::abs(feet - surface.y) > 0.13F) {
            continue;
        }
        if (position.x > surface.start_x - config::player_half_size.x &&
            position.x < surface.end_x + config::player_half_size.x) {
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
    const CampaignWorld& world,
    const SolverConfig& solver_config,
    const std::vector<Surface>& surfaces,
    const std::size_t source_index,
    const std::size_t goal_index,
    const float launch_x,
    const JumpDirection direction,
    const int charge_ticks) {
    const Surface& source = surfaces[source_index];
    const Vec2 launch{launch_x, source.y - config::player_half_size.y};
    PlayerState player{
        .position = launch,
        .mode = PlayerMode::grounded,
        .on_ground = true,
        .facing_right = direction != JumpDirection::left,
    };
    CampaignState campaign{.spawn = world.spawn};

    const auto reached_goal = [&]() {
        return Transition{
            goal_index,
            {launch, player.position, direction, charge_ticks,
             source.screen, surfaces[goal_index].screen},
        };
    };

    for (int tick = 0; tick < charge_ticks; ++tick) {
        const CampaignEvent event = step_world(
            player, campaign, world, input_for(direction, true, false));
        if (event == CampaignEvent::fell_below_world ||
            event == CampaignEvent::respawned) {
            return std::nullopt;
        }
        if (event == CampaignEvent::completed) {
            return reached_goal();
        }
    }

    CampaignEvent event = step_world(
        player, campaign, world, input_for(direction, false, true));
    if (event == CampaignEvent::fell_below_world ||
        event == CampaignEvent::respawned) {
        return std::nullopt;
    }
    if (event == CampaignEvent::completed) {
        return reached_goal();
    }

    for (int tick = 0; tick < solver_config.maximum_air_ticks; ++tick) {
        event = step_world(player, campaign, world, {});
        if (event == CampaignEvent::fell_below_world ||
            event == CampaignEvent::respawned) {
            return std::nullopt;
        }
        if (event == CampaignEvent::completed) {
            return reached_goal();
        }
        if (tick > 1 && player.mode == PlayerMode::grounded) {
            const auto destination = supporting_surface(surfaces, player.position);
            if (!destination || *destination == source_index) {
                return std::nullopt;
            }
            // Reject corner-hangs where the player's centre sits past a platform
            // edge. The solver launches from an idealised rest position, so a
            // marginal edge catch it certifies will not reproduce when the game
            // walks to the launch and jumps — demand the centre land over the
            // surface so the route is replay-robust.
            const Surface& landing = surfaces[*destination];
            if (player.position.x < landing.start_x ||
                player.position.x > landing.end_x) {
                return std::nullopt;
            }
            return Transition{
                *destination,
                {launch, player.position, direction, charge_ticks,
                 source.screen, surfaces[*destination].screen},
            };
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool tolerant_jump(
    const CampaignWorld& world,
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
    const CampaignWorld& world,
    const SolverConfig config_value)
    : world_{world}, config_{config_value} {}

SolverResult ReachabilitySolver::solve_campaign() const {
    const std::vector<Surface> surfaces = extract_surfaces(world_);
    const auto start = supporting_surface(surfaces, world_.spawn);
    const auto goal = supporting_surface(surfaces, world_.goal);
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
        const float min_x = source.start_x + config::player_half_size.x + 0.05F;
        const float max_x = source.end_x - config::player_half_size.x - 0.05F;

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
                         surfaces[destination].y < surfaces[highest_surface].y)) {
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
                "; nearest surface y " + std::to_string(surfaces[highest_surface].y),
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
