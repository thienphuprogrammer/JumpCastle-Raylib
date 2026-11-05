#include "jumpcastle/map_validation.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>

namespace jumpcastle {
namespace {

// Matches the flip-bit layout in tile_view.hpp: the top two bits are
// horizontal/vertical flip flags, the low 29 bits are the tile id.
constexpr std::uint32_t kGidIdMask = 0x1FFFFFFFu;

std::string screen_prefix(const ScreenMap& map) {
    return "screen " + std::to_string(map.index) + ": ";
}

void validate_dimensions(const ScreenMap& map) {
    if (!std::isfinite(map.width) || !std::isfinite(map.height) ||
        map.width <= 0.0F || map.height <= 0.0F) {
        throw std::runtime_error(screen_prefix(map) + "dimensions must be positive");
    }
}

void validate_unique_collider_ids(const ScreenMap& map) {
    std::unordered_set<int> seen;
    for (const MapCollider& collider : map.colliders) {
        if (!seen.insert(collider.id).second) {
            throw std::runtime_error(
                screen_prefix(map) + "duplicate collider id " + std::to_string(collider.id));
        }
    }
}

// Per-geometry-family invariants plus the collider's overall in-bounds check.
// Named per the task brief so each error can name the screen and collider.
void validate_geometry(
    const MapCollider& collider, const ScreenMap& map, const std::string& label) {
    std::visit(
        [&](const auto& geometry) {
            using T = std::decay_t<decltype(geometry)>;
            if constexpr (std::is_same_v<T, PolygonGeometry>) {
                if (geometry.points.size() < 3) {
                    throw std::runtime_error(label + " must have at least three points");
                }
                for (std::size_t i = 0; i < geometry.points.size(); ++i) {
                    const Vec2 point = geometry.points[i];
                    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                        throw std::runtime_error(
                            label + ".points[" + std::to_string(i) +
                            "] must contain finite coordinates");
                    }
                }
            } else if constexpr (std::is_same_v<T, CircleGeometry>) {
                if (!std::isfinite(geometry.center.x) || !std::isfinite(geometry.center.y)) {
                    throw std::runtime_error(label + ".center must contain finite coordinates");
                }
                if (collider.type == ColliderType::oneway) {
                    throw std::runtime_error(label + " curved geometry cannot be oneway");
                }
                if (!std::isfinite(geometry.radius) || geometry.radius < 0.5F) {
                    throw std::runtime_error(label + " radius must be at least 0.5");
                }
            } else {
                static_assert(std::is_same_v<T, CapsuleGeometry>);
                if (!std::isfinite(geometry.a.x) || !std::isfinite(geometry.a.y) ||
                    !std::isfinite(geometry.b.x) || !std::isfinite(geometry.b.y)) {
                    throw std::runtime_error(label + " must contain finite coordinates");
                }
                if (geometry.a == geometry.b) {
                    throw std::runtime_error(label + " capsule endpoints must differ");
                }
                if (collider.type == ColliderType::oneway) {
                    throw std::runtime_error(label + " curved geometry cannot be oneway");
                }
                if (!std::isfinite(geometry.radius) || geometry.radius < 0.5F) {
                    throw std::runtime_error(label + " radius must be at least 0.5");
                }
            }
        },
        collider.geometry);

    const Aabb bounds = std::visit(
        [](const auto& geometry) { return geometry_aabb(geometry); }, collider.geometry);
    if (bounds.min.x < 0.0F || bounds.min.y < 0.0F ||
        bounds.max.x > map.width || bounds.max.y > map.height) {
        throw std::runtime_error(label + " is outside screen bounds");
    }
}

// Validates one entity's coordinates. Named per the task brief so each error
// can name the screen and the entity's index.
void validate_entity(const MapEntity& entity, const ScreenMap& map, const std::size_t index) {
    const std::string label = screen_prefix(map) + "entities[" + std::to_string(index) + "]";
    if (!std::isfinite(entity.pos.x) || !std::isfinite(entity.pos.y)) {
        throw std::runtime_error(label + " must contain finite coordinates");
    }
    if (entity.pos.x < 0.0F || entity.pos.y < 0.0F ||
        entity.pos.x > map.width || entity.pos.y > map.height) {
        throw std::runtime_error(label + " is outside screen bounds");
    }
}

// Validates one named tile layer: declared dimensions match the screen size
// and are internally consistent, and (once the atlas is known) every GID's
// masked tile id resolves within the atlas. Named per the task brief so each
// error can name the screen and layer.
void validate_layer(
    const TileLayer& layer,
    const char* name,
    const ScreenMap& map,
    const MapValidationContext& context) {
    if (layer.gids.empty()) {
        return;  // Layer not authored for this screen.
    }

    const std::string label = screen_prefix(map) + name + " layer";
    if (layer.columns <= 0 || layer.rows <= 0) {
        throw std::runtime_error(label + " must have positive dimensions");
    }
    if (layer.columns != static_cast<int>(map.width) ||
        layer.rows != static_cast<int>(map.height)) {
        throw std::runtime_error(label + " dimensions must match screen size");
    }
    const std::size_t expected_cells =
        static_cast<std::size_t>(layer.columns) * static_cast<std::size_t>(layer.rows);
    if (layer.gids.size() != expected_cells) {
        throw std::runtime_error(label + " tile count must match its declared dimensions");
    }

    if (context.atlas_columns <= 0 || context.atlas_rows <= 0) {
        return;  // Atlas dimensions unknown yet: skip the atlas-dependent check.
    }
    const long long tile_count =
        static_cast<long long>(context.atlas_columns) * static_cast<long long>(context.atlas_rows);
    for (const std::uint32_t gid : layer.gids) {
        const std::uint32_t masked = gid & kGidIdMask;
        if (masked == 0u) {
            continue;  // Empty cell.
        }
        if (static_cast<long long>(masked) > tile_count) {
            throw std::runtime_error(
                screen_prefix(map) + name + " GID " + std::to_string(masked) +
                " exceeds atlas tile count " + std::to_string(tile_count));
        }
    }
}

}  // namespace

void validate_screen_map(const ScreenMap& map, const MapValidationContext& context) {
    validate_dimensions(map);
    validate_unique_collider_ids(map);

    for (const MapCollider& collider : map.colliders) {
        validate_geometry(
            collider, map, screen_prefix(map) + "collider id " + std::to_string(collider.id));
    }

    for (std::size_t i = 0; i < map.entities.size(); ++i) {
        validate_entity(map.entities[i], map, i);
    }

    validate_layer(map.tiles.background, "background", map, context);
    validate_layer(map.tiles.terrain, "terrain", map, context);
    validate_layer(map.tiles.decor, "decor", map, context);
    validate_layer(map.tiles.foreground, "foreground", map, context);
}

}  // namespace jumpcastle
