#pragma once

#include "jumpcastle/map_format.hpp"

namespace jumpcastle {

// Atlas dimensions (in tiles) used to bound tile GIDs. Zero (the default)
// means the atlas is not yet known -- e.g. validation running from
// `parse_screen_map`, before `AssetCatalog` resolves the real tileset -- so
// the GID-range check for each tile layer is skipped until the caller
// supplies real atlas dimensions.
struct MapValidationContext {
    int atlas_columns{};
    int atlas_rows{};
};

// Validates every structural invariant a screen map must uphold: finite and
// positive screen dimensions, unique collider ids, per-geometry invariants
// (minimum polygon point count, minimum curve radius, oneway restricted to
// polygon geometry), collider and entity bounds, tile layer dimensions
// matching the screen size, and -- when `context` supplies atlas
// dimensions -- every tile GID resolving within the atlas. Throws
// std::runtime_error naming the screen index and the offending
// object/layer.
void validate_screen_map(const ScreenMap& map, const MapValidationContext& context);

}  // namespace jumpcastle
