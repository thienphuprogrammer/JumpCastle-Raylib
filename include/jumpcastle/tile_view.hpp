#pragma once

#include <cstdint>

namespace jumpcastle {

// A resolved atlas cell for one painted tile: the top-left source pixel plus
// whether it is mirrored. Pure data so the GID bit-math is unit-testable
// without raylib.
struct TileCell {
    int src_x{};
    int src_y{};
    bool flip_h{};
    bool flip_v{};
};

// Resolve a Tiled GID to its atlas cell. `columns` is the atlas width in tiles,
// `tile_size` its tile pixel size. firstgid is 1 (single tileset), so the tile
// index is (gid without flip bits) - 1. The caller must skip empty cells
// (masked id 0); this assumes id >= 1.
[[nodiscard]] inline TileCell tile_source_cell(
    const std::uint32_t gid, const int columns, const int tile_size) noexcept {
    constexpr std::uint32_t flip_horizontal = 0x80000000u;
    constexpr std::uint32_t flip_vertical = 0x40000000u;
    constexpr std::uint32_t id_mask = 0x1FFFFFFFu;

    const bool flip_h = (gid & flip_horizontal) != 0u;
    const bool flip_v = (gid & flip_vertical) != 0u;
    const std::uint32_t local = (gid & id_mask) - 1u;  // 0-based; caller ensures id >= 1
    const int cols = columns > 0 ? columns : 1;
    const int col = static_cast<int>(local % static_cast<std::uint32_t>(cols));
    const int row = static_cast<int>(local / static_cast<std::uint32_t>(cols));
    return {col * tile_size, row * tile_size, flip_h, flip_v};
}

}  // namespace jumpcastle
