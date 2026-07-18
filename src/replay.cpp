#include "jumpcastle/replay.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace jumpcastle {
namespace {

using Json = nlohmann::json;

[[nodiscard]] JumpDirection parse_direction(const std::string& value) {
    if (value == "left") return JumpDirection::left;
    if (value == "neutral") return JumpDirection::neutral;
    if (value == "right") return JumpDirection::right;
    throw std::runtime_error("unknown jump direction '" + value + "'");
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

[[nodiscard]] std::string position_text(const Vec2 value) {
    return "(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ")";
}

[[nodiscard]] ReplayResult replay_failure(
    const std::size_t executed,
    const std::string& reason) {
    return {.executed_jumps = executed, .failure = reason};
}

}  // namespace

SolverTrace make_trace(const SolverResult& result) {
    SolverTrace trace;
    trace.jumps.reserve(result.jumps.size());
    for (const SolverJump& jump : result.jumps) {
        trace.jumps.push_back({
            .launch_position = jump.start,
            .direction = jump.direction,
            .charge_ticks = jump.charge_ticks,
            .expected_landing = jump.landing,
            .expected_screen = jump.landing_screen,
        });
    }
    return trace;
}

std::string serialize_trace(const SolverTrace& trace) {
    Json root{
        {"schema_version", trace.schema_version},
        {"fixed_delta", trace.fixed_delta},
        {"jumps", Json::array()},
    };
    for (const TraceJump& jump : trace.jumps) {
        root["jumps"].push_back({
            {"launch", {jump.launch_position.x, jump.launch_position.y}},
            {"direction", to_string(jump.direction)},
            {"charge_ticks", jump.charge_ticks},
            {"expected_landing", {jump.expected_landing.x, jump.expected_landing.y}},
            {"expected_screen", jump.expected_screen},
        });
    }
    return root.dump(2) + '\n';
}

SolverTrace parse_trace(
    const std::string_view source,
    const std::string_view filename) {
    try {
        const Json root = Json::parse(source);
        SolverTrace trace;
        trace.schema_version = root.at("schema_version").get<int>();
        trace.fixed_delta = root.at("fixed_delta").get<float>();
        if (trace.schema_version != 1) {
            throw std::runtime_error("unsupported trace schema");
        }
        if (std::abs(trace.fixed_delta - config::fixed_delta) > 0.0000001F) {
            throw std::runtime_error("trace fixed_delta does not match production physics");
        }
        for (const Json& encoded : root.at("jumps")) {
            const auto& launch = encoded.at("launch");
            const auto& landing = encoded.at("expected_landing");
            if (!launch.is_array() || launch.size() != 2 ||
                !landing.is_array() || landing.size() != 2) {
                throw std::runtime_error("trace positions must contain two numbers");
            }
            trace.jumps.push_back({
                .launch_position = {launch.at(0).get<float>(), launch.at(1).get<float>()},
                .direction = parse_direction(encoded.at("direction").get<std::string>()),
                .charge_ticks = encoded.at("charge_ticks").get<int>(),
                .expected_landing = {
                    landing.at(0).get<float>(), landing.at(1).get<float>()},
                .expected_screen = encoded.at("expected_screen").get<int>(),
            });
            if (trace.jumps.back().charge_ticks <= 0) {
                throw std::runtime_error("charge_ticks must be positive");
            }
        }
        return trace;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string{filename} + ": " + error.what());
    }
}

void write_trace(const std::filesystem::path& path, const SolverTrace& trace) {
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("Unable to write solver trace: " + path.string());
    }
    output << serialize_trace(trace);
    if (!output) {
        throw std::runtime_error("Unable to finish solver trace: " + path.string());
    }
}

SolverTrace read_trace(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Unable to open solver trace: " + path.string());
    }
    std::ostringstream source;
    source << input.rdbuf();
    return parse_trace(source.str(), path.string());
}

ReplayResult verify_trace(const CampaignWorld& world, const SolverTrace& trace) {
    PlayerState player{.position = world.spawn};
    CampaignState campaign{.spawn = world.spawn};

    for (int tick = 0; tick < 120 && player.mode != PlayerMode::grounded; ++tick) {
        if (step_world(player, campaign, world, {}) == CampaignEvent::fell_below_world) {
            return replay_failure(0, "spawn fell below the world");
        }
    }
    if (player.mode != PlayerMode::grounded) {
        return replay_failure(0, "spawn did not settle on a platform");
    }

    std::size_t executed{};
    for (std::size_t index = 0; index < trace.jumps.size(); ++index) {
        const TraceJump& jump = trace.jumps[index];
        if (std::abs(player.position.y - jump.launch_position.y) > 0.15F) {
            return replay_failure(
                executed,
                "jump " + std::to_string(index + 1) + " launch row mismatch: expected " +
                    position_text(jump.launch_position) + ", actual " +
                    position_text(player.position));
        }

        for (int tick = 0;
             tick < 3000 && std::abs(player.position.x - jump.launch_position.x) > 0.02F;
             ++tick) {
            const bool move_right = player.position.x < jump.launch_position.x;
            const CampaignEvent event = step_world(
                player,
                campaign,
                world,
                {.left = !move_right, .right = move_right});
            if (event != CampaignEvent::none || player.mode != PlayerMode::grounded) {
                return replay_failure(
                    executed,
                    "jump " + std::to_string(index + 1) +
                        " could not walk to its launch position");
            }
        }
        if (std::abs(player.position.x - jump.launch_position.x) > 0.03F) {
            return replay_failure(
                executed,
                "jump " + std::to_string(index + 1) + " launch x was not reached");
        }

        CampaignEvent event = CampaignEvent::none;
        for (int tick = 0; tick < jump.charge_ticks; ++tick) {
            event = step_world(
                player, campaign, world, input_for(jump.direction, true, false));
            if (event != CampaignEvent::none) break;
        }
        if (event == CampaignEvent::none) {
            event = step_world(
                player, campaign, world, input_for(jump.direction, false, true));
        }
        for (int tick = 0;
             tick < 480 && event == CampaignEvent::none &&
                 player.mode != PlayerMode::grounded;
             ++tick) {
            event = step_world(player, campaign, world, {});
        }
        if (event == CampaignEvent::fell_below_world ||
            (event == CampaignEvent::none && player.mode != PlayerMode::grounded)) {
            return replay_failure(
                executed,
                "jump " + std::to_string(index + 1) + " did not reach a landing");
        }

        // Polygon SAT lands the walked-in replay a little off the solver's
        // idealised launch (the two diverge most at platform corners), so the
        // landing tolerance is wider than the grid path used. The launch-row
        // check above stays tight, and the y bound still pins the target row.
        if (std::abs(player.position.x - jump.expected_landing.x) > 0.40F ||
            std::abs(player.position.y - jump.expected_landing.y) > 0.20F) {
            return replay_failure(
                executed,
                "jump " + std::to_string(index + 1) + " expected screen " +
                    std::to_string(jump.expected_screen + 1) + " at " +
                    position_text(jump.expected_landing) + ", actual " +
                    position_text(player.position));
        }
        ++executed;
    }

    for (int tick = 0; tick < 3000 && !campaign.complete; ++tick) {
        if (std::abs(player.position.y - world.goal.y) > 0.20F) {
            break;
        }
        const bool move_right = player.position.x < world.goal.x;
        const CampaignEvent event = step_world(
            player,
            campaign,
            world,
            {.left = !move_right, .right = move_right});
        if (event == CampaignEvent::fell_below_world) {
            break;
        }
    }
    if (!campaign.complete) {
        return replay_failure(executed, "trace ended before the goal");
    }
    return {.completed = true, .executed_jumps = executed};
}

}  // namespace jumpcastle
