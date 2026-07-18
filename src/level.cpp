#include "jumpcastle/level.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace jumpcastle {
namespace {

[[noreturn]] void parse_error(
    const std::string_view filename,
    const std::size_t line,
    const std::string& reason) {
    throw std::runtime_error(
        std::string{filename} + ':' + std::to_string(line) + ": " + reason);
}

std::vector<std::string_view> split_lines(const std::string_view source) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < source.size()) {
        const std::size_t end = source.find('\n', start);
        std::string_view line = source.substr(
            start,
            end == std::string_view::npos ? source.size() - start : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return lines;
}

Biome parse_biome(
    const std::string_view value,
    const std::string_view filename,
    const std::size_t line) {
    if (value == "pixel_adventure") return Biome::pixel_adventure;
    if (value == "kenney") return Biome::kenney;
    if (value == "kings_and_pigs") return Biome::kings_and_pigs;
    parse_error(filename, line, "unknown biome '" + std::string{value} + "'");
}

Tile parse_tile(
    const char value,
    const std::string_view filename,
    const std::size_t line) {
    switch (value) {
    case '.': return Tile::empty;
    case '#': return Tile::solid;
    case '^': return Tile::spike;
    case 'S': return Tile::spawn;
    case 'C': return Tile::checkpoint;
    case 'E': return Tile::exit;
    default:
        parse_error(filename, line, "unknown token '" + std::string(1, value) + "'");
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("Unable to open level file: " + path.string());
    }
    std::ostringstream content;
    content << stream.rdbuf();
    return content.str();
}

void validate_campaign(const std::array<Room, LevelRepository::room_count>& rooms) {
    std::size_t spawn_count = 0;
    std::size_t exit_count = 0;

    for (std::size_t room_index = 0; room_index < rooms.size(); ++room_index) {
        const Biome expected = room_index < 4
            ? Biome::pixel_adventure
            : room_index < 8 ? Biome::kenney : Biome::kings_and_pigs;
        if (rooms[room_index].metadata.biome != expected) {
            throw std::runtime_error(
                "Room " + std::to_string(room_index + 1) + " has invalid biome order");
        }

        std::size_t checkpoint_count = 0;
        for (const auto& row : rooms[room_index].tilemap.grid()) {
            for (const Tile tile : row) {
                spawn_count += tile == Tile::spawn ? 1U : 0U;
                exit_count += tile == Tile::exit ? 1U : 0U;
                checkpoint_count += tile == Tile::checkpoint ? 1U : 0U;
            }
        }

        const bool checkpoint_room = room_index == 0 || room_index == 4 || room_index == 8;
        if ((checkpoint_room && checkpoint_count != 1) ||
            (!checkpoint_room && checkpoint_count != 0)) {
            throw std::runtime_error(
                "Room " + std::to_string(room_index + 1) +
                " has invalid checkpoint count");
        }
    }

    if (spawn_count != 1 || exit_count != 1) {
        throw std::runtime_error("Campaign requires exactly one spawn and one exit");
    }
}

}  // namespace

Room parse_room(const std::string_view source, const std::string_view filename) {
    const auto lines = split_lines(source);
    RoomMetadata metadata;
    bool has_name = false;
    bool has_biome = false;
    bool has_difficulty = false;
    std::size_t separator = lines.size();

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto line = lines[index];
        if (line == "---") {
            separator = index;
            break;
        }
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) {
            parse_error(filename, index + 1, "expected key=value metadata");
        }
        const auto key = line.substr(0, equals);
        const auto value = line.substr(equals + 1);
        if (key == "name" && !has_name) {
            metadata.name = value;
            has_name = !value.empty();
        } else if (key == "biome" && !has_biome) {
            metadata.biome = parse_biome(value, filename, index + 1);
            has_biome = true;
        } else if (key == "difficulty" && !has_difficulty) {
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), metadata.difficulty);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                metadata.difficulty < 1 || metadata.difficulty > 4) {
                parse_error(filename, index + 1, "difficulty must be an integer from 1 to 4");
            }
            has_difficulty = true;
        } else {
            parse_error(filename, index + 1, "unknown or duplicate metadata key '" +
                std::string{key} + "'");
        }
    }

    if (!has_name || !has_biome || !has_difficulty || separator == lines.size()) {
        parse_error(filename, separator + 1, "missing required metadata or separator");
    }
    if (lines.size() < separator + 1 + config::tilemap_height) {
        parse_error(filename, separator + 2, "room must contain exactly 12 tile rows");
    }

    Tilemap::Grid grid{};
    for (int y = 0; y < config::tilemap_height; ++y) {
        const std::size_t line_index = separator + 1 + static_cast<std::size_t>(y);
        const auto row = lines[line_index];
        if (row.size() != config::tilemap_width) {
            parse_error(filename, line_index + 1, "tile row must contain exactly 16 cells");
        }
        for (int x = 0; x < config::tilemap_width; ++x) {
            grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                parse_tile(row[static_cast<std::size_t>(x)], filename, line_index + 1);
        }
    }

    const std::size_t expected_lines = separator + 1 + config::tilemap_height;
    for (std::size_t index = expected_lines; index < lines.size(); ++index) {
        if (!lines[index].empty()) {
            parse_error(filename, index + 1, "unexpected content after tile rows");
        }
    }

    return {std::move(metadata), Tilemap{grid}};
}

std::optional<std::size_t> room_index_for_world_y(const float world_y) noexcept {
    const int index = static_cast<int>(
        std::floor(-world_y / static_cast<float>(config::tilemap_height)));
    if (index < 0 || index >= static_cast<int>(LevelRepository::room_count)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(index);
}

LevelRepository::LevelRepository(std::array<Room, room_count> rooms)
    : rooms_{std::move(rooms)} {
    validate_campaign(rooms_);
}

LevelRepository LevelRepository::load(const std::filesystem::path& directory) {
    std::array<Room, room_count> rooms;
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        const auto filename = "room-" +
            (index + 1 < 10 ? std::string{"0"} : std::string{}) +
            std::to_string(index + 1) + ".level";
        const auto path = directory / filename;
        rooms[index] = parse_room(read_file(path), path.string());
    }
    return LevelRepository{std::move(rooms)};
}

const Room& LevelRepository::room(const std::size_t index) const {
    return rooms_.at(index);
}

std::optional<RoomSelection> LevelRepository::select(const float world_y) const noexcept {
    const auto index = room_index_for_world_y(world_y);
    if (!index) return std::nullopt;
    return RoomSelection{
        *index,
        &rooms_[*index],
        -static_cast<float>((*index + 1) * config::tilemap_height),
    };
}

Vec2 LevelRepository::campaign_spawn() const {
    const auto positions = marker_positions(rooms_.front(), Tile::spawn,
        -static_cast<float>(config::tilemap_height));
    if (positions.size() != 1) {
        throw std::runtime_error("Campaign spawn is missing or duplicated");
    }
    return positions.front();
}

std::vector<Vec2> marker_positions(
    const Room& room,
    const Tile marker,
    const float vertical_offset) {
    std::vector<Vec2> positions;
    for (int y = 0; y < config::tilemap_height; ++y) {
        for (int x = 0; x < config::tilemap_width; ++x) {
            if (room.tilemap.tile_at(x, y) == marker) {
                positions.push_back({
                    static_cast<float>(x) + 0.5F,
                    vertical_offset + static_cast<float>(y) + 0.5F,
                });
            }
        }
    }
    return positions;
}

}  // namespace jumpcastle
