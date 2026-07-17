#pragma once

#include "jumpcastle/tilemap.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jumpcastle {

enum class Biome {
    pixel_adventure,
    kenney,
    kings_and_pigs,
};

struct RoomMetadata {
    std::string name;
    Biome biome{Biome::pixel_adventure};
    int difficulty{};
};

struct Room {
    RoomMetadata metadata;
    Tilemap tilemap;
};

struct RoomSelection {
    std::size_t index;
    const Room* room;
    float vertical_offset;
};

class LevelRepository {
public:
    static constexpr std::size_t room_count = 12;

    explicit LevelRepository(std::array<Room, room_count> rooms);

    [[nodiscard]] static LevelRepository load(const std::filesystem::path& directory);
    [[nodiscard]] const Room& room(std::size_t index) const;
    [[nodiscard]] std::optional<RoomSelection> select(float world_y) const noexcept;
    [[nodiscard]] Vector2 campaign_spawn() const;

private:
    std::array<Room, room_count> rooms_;
};

[[nodiscard]] Room parse_room(std::string_view source, std::string_view filename);
[[nodiscard]] std::optional<std::size_t> room_index_for_world_y(float world_y) noexcept;
[[nodiscard]] std::vector<Vector2> marker_positions(
    const Room& room,
    Tile marker,
    float vertical_offset);

}  // namespace jumpcastle
