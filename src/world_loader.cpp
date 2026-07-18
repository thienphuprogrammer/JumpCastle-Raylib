#include "jumpcastle/world.hpp"

#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace jumpcastle {
namespace {

[[noreturn]] void parse_error(
    const std::string_view filename,
    const std::size_t line,
    const std::size_t column,
    const std::string& reason) {
    throw std::runtime_error(
        std::string{filename} + ':' + std::to_string(line) + ':' +
        std::to_string(column) + ": " + reason);
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
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return lines;
}

std::vector<std::string_view> words(const std::string_view line) {
    std::vector<std::string_view> result;
    std::size_t start = 0;
    while (start < line.size()) {
        while (start < line.size() && line[start] == ' ') {
            ++start;
        }
        if (start == line.size()) {
            break;
        }
        const std::size_t end = line.find(' ', start);
        result.push_back(line.substr(
            start,
            end == std::string_view::npos ? line.size() - start : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

int parse_integer(
    const std::string_view token,
    const std::string_view filename,
    const std::size_t line,
    const std::string& label) {
    int value{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
        parse_error(filename, line, 1, label + " must be an integer");
    }
    return value;
}

Biome parse_biome(
    const std::string_view token,
    const std::string_view filename,
    const std::size_t line) {
    if (token == "courtyard") {
        return Biome::courtyard;
    }
    if (token == "frosted_keep") {
        return Biome::frosted_keep;
    }
    if (token == "crown_spire") {
        return Biome::crown_spire;
    }
    parse_error(filename, line, 1, "unknown biome '" + std::string{token} + "'");
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Unable to open campaign file: " + path.string());
    }
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

}  // namespace

WorldMap parse_campaign(
    const std::string_view source,
    const std::string_view filename) {
    const auto lines = split_lines(source);
    int width{};
    int height{};
    int screen_height{};
    int spawn_x{};
    int spawn_y{};
    int goal_x{};
    int goal_y{};
    bool has_version{};
    bool has_tile_size{};
    bool has_size{};
    bool has_screen_height{};
    bool has_spawn{};
    bool has_goal{};
    std::vector<BiomeRange> biomes;
    std::size_t separator = lines.size();

    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (lines[index] == "---") {
            separator = index;
            break;
        }
        const auto fields = words(lines[index]);
        if (fields.empty()) {
            parse_error(filename, index + 1, 1, "empty metadata line");
        }
        if (fields[0] == "version" && fields.size() == 2 && !has_version) {
            has_version = true;
            if (parse_integer(fields[1], filename, index + 1, "version") != 2) {
                parse_error(filename, index + 1, 1, "unsupported campaign version");
            }
        } else if (fields[0] == "tile_size" && fields.size() == 2 && !has_tile_size) {
            has_tile_size = true;
            if (parse_integer(fields[1], filename, index + 1, "tile_size") != 16) {
                parse_error(filename, index + 1, 1, "tile_size must be 16");
            }
        } else if (fields[0] == "size" && fields.size() == 3 && !has_size) {
            has_size = true;
            width = parse_integer(fields[1], filename, index + 1, "width");
            height = parse_integer(fields[2], filename, index + 1, "height");
        } else if (fields[0] == "screen_height" && fields.size() == 2 &&
                   !has_screen_height) {
            has_screen_height = true;
            screen_height = parse_integer(
                fields[1], filename, index + 1, "screen_height");
        } else if (fields[0] == "spawn" && fields.size() == 3 && !has_spawn) {
            has_spawn = true;
            spawn_x = parse_integer(fields[1], filename, index + 1, "spawn x");
            spawn_y = parse_integer(fields[2], filename, index + 1, "spawn y");
        } else if (fields[0] == "goal" && fields.size() == 3 && !has_goal) {
            has_goal = true;
            goal_x = parse_integer(fields[1], filename, index + 1, "goal x");
            goal_y = parse_integer(fields[2], filename, index + 1, "goal y");
        } else if (fields[0] == "biome" && fields.size() == 4) {
            biomes.push_back({
                parse_integer(fields[1], filename, index + 1, "first biome screen"),
                parse_integer(fields[2], filename, index + 1, "last biome screen"),
                parse_biome(fields[3], filename, index + 1),
            });
        } else {
            parse_error(filename, index + 1, 1, "unknown, duplicate, or malformed metadata");
        }
    }

    if (!has_version || !has_tile_size || !has_size || !has_screen_height ||
        !has_spawn || !has_goal || biomes.empty() || separator == lines.size()) {
        parse_error(filename, separator + 1, 1, "missing required campaign metadata");
    }
    if (separator + 1 >= lines.size() || lines[separator + 1] != "[collision]") {
        parse_error(filename, separator + 2, 1, "expected [collision] section");
    }
    if (width <= 0 || height <= 0 || screen_height <= 0 ||
        height % screen_height != 0) {
        parse_error(filename, separator + 1, 1, "invalid world dimensions");
    }

    const std::size_t grid_start = separator + 2;
    if (lines.size() < grid_start + static_cast<std::size_t>(height)) {
        parse_error(filename, grid_start + 1, 1, "collision grid has too few rows");
    }
    std::vector<WorldTile> tiles;
    tiles.reserve(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) {
        const std::size_t line_index = grid_start + static_cast<std::size_t>(y);
        const std::string_view row = lines[line_index];
        if (row.size() != static_cast<std::size_t>(width)) {
            parse_error(filename, line_index + 1, 1, "collision row has incorrect width");
        }
        for (int x = 0; x < width; ++x) {
            const char token = row[static_cast<std::size_t>(x)];
            if (token == '.') {
                tiles.push_back(WorldTile::empty);
            } else if (token == '#') {
                tiles.push_back(WorldTile::solid);
            } else {
                parse_error(
                    filename,
                    line_index + 1,
                    static_cast<std::size_t>(x + 1),
                    "unknown collision token '" + std::string(1, token) + "'");
            }
        }
    }
    for (std::size_t index = grid_start + static_cast<std::size_t>(height);
         index < lines.size(); ++index) {
        if (!lines[index].empty()) {
            parse_error(filename, index + 1, 1, "unexpected content after collision grid");
        }
    }

    try {
        return WorldMap{
            width,
            height,
            screen_height,
            std::move(tiles),
            {static_cast<float>(spawn_x) + 0.5F, static_cast<float>(spawn_y) + 0.5F},
            {static_cast<float>(goal_x) + 0.5F, static_cast<float>(goal_y) + 0.5F},
            std::move(biomes),
        };
    } catch (const std::invalid_argument& error) {
        parse_error(filename, 1, 1, error.what());
    }
}

WorldMap WorldMap::load(const std::filesystem::path& path) {
    return parse_campaign(read_file(path), path.string());
}

}  // namespace jumpcastle
