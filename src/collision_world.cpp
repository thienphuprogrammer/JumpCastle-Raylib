#include "jumpcastle/collision_world.hpp"

#include "jumpcastle/convex.hpp"
#include "jumpcastle/game_config.hpp"
#include "jumpcastle/player.hpp"
#include "jumpcastle/shape_collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace jumpcastle {
namespace {

constexpr float kSubStep = 0.2F;      // max distance per collision sub-step (tiles)
constexpr float kGroundNormalY = -0.5F;  // normal.y below this counts as a floor (y-down)

Aabb player_box(const Vec2 center) noexcept {
    return {center - config::player_half_size, center + config::player_half_size};
}

// Cheap broad phase: disjoint AABBs cannot produce an exact shape contact, so
// this skips polygon, circle, and capsule narrow-phase dispatch for colliders
// far from the moving player (the solver's hot path).
bool aabb_overlap(const Aabb& a, const Aabb& b) noexcept {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y;
}

std::optional<Contact> contact_for(
    const Aabb box, const WorldCollider& collider) noexcept {
    std::optional<Contact> contact = std::visit([&](const auto& geometry) {
        using Geometry = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<Geometry, ConvexPolygon>) {
            return contact_aabb_polygon(box, geometry);
        } else if constexpr (std::is_same_v<Geometry, CircleGeometry>) {
            return contact_aabb_circle(box, geometry);
        } else {
            return contact_aabb_capsule(box, geometry);
        }
    }, collider.geometry);
    if (contact) {
        contact->collider_id = collider.id;
        contact->piece_index = collider.piece_index;
        contact->type = collider.type;
    }
    return contact;
}

bool is_polygon(const WorldCollider& collider) noexcept {
    return std::holds_alternative<ConvexPolygon>(collider.geometry);
}

bool is_blocking_contact(
    const WorldCollider& collider,
    const Contact& contact,
    const Vec2 step_start) noexcept {
    if (collider.type != ColliderType::oneway) {
        return collider.type == ColliderType::solid;
    }
    if (!is_polygon(collider)) {
        return false;
    }
    const float previous_bottom = step_start.y + config::player_half_size.y;
    return contact.normal.y < kGroundNormalY && previous_bottom <= collider.aabb.min.y;
}

bool prefer_ground_contact(const Contact& candidate, const Contact& current) noexcept {
    if (candidate.normal.y != current.normal.y) {
        return candidate.normal.y < current.normal.y;
    }
    if (candidate.depth != current.depth) {
        return candidate.depth > current.depth;
    }
    if (candidate.collider_id != current.collider_id) {
        return candidate.collider_id < current.collider_id;
    }
    return candidate.piece_index < current.piece_index;
}

bool better_ground_contact(const Contact& candidate, const Contact& current) noexcept {
    constexpr float epsilon = 1.0e-5F;
    if (candidate.normal.y < current.normal.y - epsilon) return true;
    if (candidate.normal.y > current.normal.y + epsilon) return false;
    if (candidate.depth > current.depth + epsilon) return true;
    if (candidate.depth < current.depth - epsilon) return false;
    return std::tie(candidate.collider_id, candidate.piece_index) <
           std::tie(current.collider_id, current.piece_index);
}

struct ScreenTargets {
    std::vector<WorldCollider>& colliders;
    std::vector<ConvexPolygon>& polygons;
};

ConvexPolygon world_polygon(
    std::vector<Vec2> points,
    const float offset,
    const ColliderType type) {
    for (Vec2& point : points) {
        point.y += offset;
    }
    const Aabb bounds = polygon_aabb(points);
    std::vector<Vec2> normals = outward_edge_normals(points);
    return {
        .points = std::move(points),
        .edge_normals = std::move(normals),
        .aabb = bounds,
        .type = type,
    };
}

void append_authored_geometry(
    const MapCollider& collider,
    const PolygonGeometry& geometry,
    const float offset,
    const ScreenTargets targets) {
    int piece_index = 0;
    for (std::vector<Vec2> piece : split_to_convex(geometry.points)) {
        ConvexPolygon polygon = world_polygon(
            std::move(piece), offset, collider.type);
        const Aabb bounds = polygon.aabb;
        targets.polygons.push_back(polygon);
        targets.colliders.push_back({
            .id = collider.id,
            .piece_index = piece_index++,
            .type = collider.type,
            .geometry = std::move(polygon),
            .aabb = bounds,
            .tag = collider.tag,
        });
    }
}

void append_authored_geometry(
    const MapCollider& collider,
    CircleGeometry circle,
    const float offset,
    const ScreenTargets targets) {
    circle.center.y += offset;
    const Aabb bounds = geometry_aabb(circle);
    targets.colliders.push_back({
        .id = collider.id,
        .piece_index = 0,
        .type = collider.type,
        .geometry = circle,
        .aabb = bounds,
        .tag = collider.tag,
    });
}

void append_authored_geometry(
    const MapCollider& collider,
    CapsuleGeometry capsule,
    const float offset,
    const ScreenTargets targets) {
    capsule.a.y += offset;
    capsule.b.y += offset;
    const Aabb bounds = geometry_aabb(capsule);
    targets.colliders.push_back({
        .id = collider.id,
        .piece_index = 0,
        .type = collider.type,
        .geometry = capsule,
        .aabb = bounds,
        .tag = collider.tag,
    });
}

void append_authored_collider(
    const MapCollider& collider,
    const float offset,
    const ScreenTargets targets) {
    std::visit([&](const auto& geometry) {
        append_authored_geometry(collider, geometry, offset, targets);
    }, collider.geometry);
}

void import_legacy_polygons(
    const ScreenMap& screen,
    const float offset,
    const ScreenTargets targets) {
    int collider_id = 1;
    for (ConvexPolygon polygon : screen.polygons) {
        for (Vec2& point : polygon.points) {
            point.y += offset;
        }
        polygon.aabb.min.y += offset;
        polygon.aabb.max.y += offset;
        const Aabb bounds = polygon.aabb;
        const ColliderType type = polygon.type;
        targets.polygons.push_back(polygon);
        targets.colliders.push_back({
            .id = collider_id++,
            .piece_index = 0,
            .type = type,
            .geometry = std::move(polygon),
            .aabb = bounds,
            .tag = {},
        });
    }
}

void append_screen_geometry(
    const ScreenMap& screen,
    const float offset,
    const ScreenTargets targets) {
    for (const MapCollider& collider : screen.colliders) {
        append_authored_collider(collider, offset, targets);
    }
    if (screen.colliders.empty()) {
        import_legacy_polygons(screen, offset, targets);
    }
}

struct SnapshotContacts {
    bool hit_hazard{};
    std::optional<Contact> ground_contact{};
};

void consider_snapshot_contact(
    const WorldCollider& collider,
    const Contact& contact,
    const Vec2 step_start,
    SnapshotContacts& snapshot) noexcept {
    if (collider.type == ColliderType::hazard) {
        snapshot.hit_hazard = true;
        return;
    }
    if (!is_blocking_contact(collider, contact, step_start) ||
        contact.normal.y >= kGroundNormalY) {
        return;
    }
    if (!snapshot.ground_contact ||
        prefer_ground_contact(contact, *snapshot.ground_contact)) {
        snapshot.ground_contact = contact;
    }
}

SnapshotContacts collect_snapshot_contacts(
    const CollisionWorld& world,
    const int screen_index,
    const Aabb box,
    const Vec2 step_start) noexcept {
    SnapshotContacts snapshot;
    for (int k = screen_index - 1; k <= screen_index + 1; ++k) {
        const auto* colliders = world.colliders_for_screen(k);
        if (colliders == nullptr) { continue; }
        for (const WorldCollider& collider : *colliders) {
            if (!aabb_overlap(box, collider.aabb)) { continue; }
            const std::optional<Contact> contact = contact_for(box, collider);
            if (contact) {
                consider_snapshot_contact(collider, *contact, step_start, snapshot);
            }
        }
    }
    return snapshot;
}

void merge_snapshot(
    const SnapshotContacts& snapshot,
    ResolveResult& result) noexcept {
    result.hit_hazard = result.hit_hazard || snapshot.hit_hazard;
    if (!snapshot.ground_contact) { return; }
    result.on_ground = true;
    if (!result.ground_contact ||
        prefer_ground_contact(*snapshot.ground_contact, *result.ground_contact)) {
        result.ground_contact = snapshot.ground_contact;
    }
}

void apply_contact_velocity(
    const Contact& contact,
    PlayerState& player) noexcept {
    const float velocity_along_normal = dot(player.velocity, contact.normal);
    if (velocity_along_normal >= 0.0F) { return; }
    const bool is_wall =
        std::abs(contact.normal.x) > std::abs(contact.normal.y);
    float restitution = 0.0F;
    if (is_wall && player.mode == PlayerMode::airborne) {
        restitution =
            config::wall_bounce_restitution(-velocity_along_normal);
    }
    player.velocity = player.velocity -
        contact.normal * (velocity_along_normal * (1.0F + restitution));
}

void apply_collider_response(
    const WorldCollider& collider,
    const Vec2 step_start,
    PlayerState& player,
    ResolveResult& result) noexcept {
    const Aabb box = player_box(player.position);
    if (!aabb_overlap(box, collider.aabb)) { return; }
    const std::optional<Contact> contact = contact_for(box, collider);
    if (!contact) { return; }
    if (collider.type == ColliderType::hazard) {
        result.hit_hazard = true;
        return;
    }
    if (!is_blocking_contact(collider, *contact, step_start)) { return; }
    player.position = player.position + contact->normal * contact->depth;
    apply_contact_velocity(*contact, player);
}

void apply_ordered_responses(
    const CollisionWorld& world,
    const int screen_index,
    const Vec2 step_start,
    PlayerState& player,
    ResolveResult& result) noexcept {
    for (int k = screen_index - 1; k <= screen_index + 1; ++k) {
        const auto* colliders = world.colliders_for_screen(k);
        if (colliders == nullptr) { continue; }
        for (const WorldCollider& collider : *colliders) {
            apply_collider_response(collider, step_start, player, result);
        }
    }
}

int screen_for_position(
    const Vec2 position, const int screen_height) noexcept {
    return static_cast<int>(
        std::floor(position.y / static_cast<float>(screen_height)));
}

void resolve_substep(
    const CollisionWorld& world,
    const int screen_height,
    const Vec2 step,
    PlayerState& player,
    ResolveResult& result) noexcept {
    const Vec2 step_start = player.position;
    player.position = player.position + step;
    const int screen_index = screen_for_position(player.position, screen_height);
    const SnapshotContacts snapshot = collect_snapshot_contacts(
        world, screen_index, player_box(player.position), step_start);
    merge_snapshot(snapshot, result);
    apply_ordered_responses(world, screen_index, step_start, player, result);
}

}  // namespace

CollisionWorld CollisionWorld::from_screens(
    const std::vector<ScreenMap>& screens,
    const int screen_height) {
    CollisionWorld world;
    world.screen_height_ = screen_height > 0 ? screen_height : 1;

    int max_index = 0;
    for (const ScreenMap& screen : screens) {
        max_index = std::max(max_index, screen.index);
    }
    world.by_screen_.resize(static_cast<std::size_t>(max_index) + 1);
    world.polygon_adapter_by_screen_.resize(static_cast<std::size_t>(max_index) + 1);

    for (const ScreenMap& screen : screens) {
        const float offset = static_cast<float>(screen.index * world.screen_height_);
        const std::size_t index = static_cast<std::size_t>(screen.index);
        append_screen_geometry(screen, offset, {
            world.by_screen_[index],
            world.polygon_adapter_by_screen_[index],
        });
    }
    return world;
}

const std::vector<WorldCollider>* CollisionWorld::colliders_for_screen(
    const int screen_index) const noexcept {
    if (screen_index < 0 || screen_index >= static_cast<int>(by_screen_.size())) {
        return nullptr;
    }
    return &by_screen_[static_cast<std::size_t>(screen_index)];
}

bool CollisionWorld::overlaps_blocking(const Aabb& box) const noexcept {
    const Vec2 center = aabb_center(box);
    const int screen_index =
        static_cast<int>(std::floor(center.y / static_cast<float>(screen_height_)));
    for (int k = screen_index - 1; k <= screen_index + 1; ++k) {
        const std::vector<WorldCollider>* colliders = colliders_for_screen(k);
        if (colliders == nullptr) { continue; }
        for (const WorldCollider& collider : *colliders) {
            if (collider.type == ColliderType::hazard) { continue; }
            if (collider.type == ColliderType::oneway && !is_polygon(collider)) {
                continue;
            }
            if (!aabb_overlap(box, collider.aabb)) { continue; }
            if (contact_for(box, collider)) {
                return true;
            }
        }
    }
    return false;
}

std::optional<Contact> CollisionWorld::support_contact(const Aabb box) const noexcept {
    const Vec2 center = aabb_center(box);
    const int screen_index =
        static_cast<int>(std::floor(center.y / static_cast<float>(screen_height_)));
    std::optional<Contact> best;
    for (int k = screen_index - 1; k <= screen_index + 1; ++k) {
        const std::vector<WorldCollider>* colliders = colliders_for_screen(k);
        if (colliders == nullptr) { continue; }
        for (const WorldCollider& collider : *colliders) {
            if (collider.type == ColliderType::hazard) { continue; }
            if (!aabb_overlap(box, collider.aabb)) { continue; }
            const std::optional<Contact> contact = contact_for(box, collider);
            if (!contact || contact->normal.y > kGroundNormalY) { continue; }
            if (!best || better_ground_contact(*contact, *best)) {
                best = contact;
            }
        }
    }
    return best;
}

ResolveResult CollisionWorld::resolve(const Vec2 previous_position, PlayerState& player) const {
    const Vec2 target = player.position;
    player.position = previous_position;
    ResolveResult result{};
    const Vec2 delta = target - previous_position;
    const int steps = std::max(1, static_cast<int>(std::ceil(length(delta) / kSubStep)));
    const Vec2 step = delta * (1.0F / static_cast<float>(steps));
    for (int s = 0; s < steps; ++s) {
        resolve_substep(*this, screen_height_, step, player, result);
    }
    return result;
}

}  // namespace jumpcastle
