#include "jumpcastle/editor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace jumpcastle {
namespace {

Vec2 snap_to_grid(const Vec2 point) noexcept {
    return {
        std::round(point.x / EditorState::snap_step) * EditorState::snap_step,
        std::round(point.y / EditorState::snap_step) * EditorState::snap_step,
    };
}

bool point_in_polygon(const Vec2 point, const std::vector<Vec2>& polygon) noexcept {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[j];
        if (((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

EditorState::EditorState(
    const int screen_index, const float width, const float height) noexcept
    : screen_index_{screen_index}, width_{width}, height_{height} {}

void EditorState::begin_polygon(const ColliderType type) {
    drafting_ = true;
    draft_type_ = type;
    draft_points_.clear();
}

void EditorState::add_vertex(const Vec2 world_pos, const bool snap) {
    if (!drafting_) { return; }
    draft_points_.push_back(snap ? snap_to_grid(world_pos) : world_pos);
}

bool EditorState::close_polygon() {
    if (!drafting_ || draft_points_.size() < 3) {
        return false;
    }
    polygons_.push_back({
        .points = draft_points_,
        .type = draft_type_,
        .id = std::nullopt,
        .tag = {},
    });
    drafting_ = false;
    draft_points_.clear();
    return true;
}

void EditorState::cancel_polygon() {
    drafting_ = false;
    draft_points_.clear();
}

void EditorState::begin_rectangle(
    const ColliderType type, const Vec2 origin, const bool snap) {
    drafting_ = true;
    draft_type_ = type;
    rect_origin_ = snap ? snap_to_grid(origin) : origin;
    draft_points_ = {rect_origin_};
}

void EditorState::update_rectangle(const Vec2 corner, const bool snap) {
    if (!drafting_) { return; }
    const Vec2 far_corner = snap ? snap_to_grid(corner) : corner;
    const float x0 = std::min(rect_origin_.x, far_corner.x);
    const float x1 = std::max(rect_origin_.x, far_corner.x);
    const float y0 = std::min(rect_origin_.y, far_corner.y);
    const float y1 = std::max(rect_origin_.y, far_corner.y);
    draft_points_ = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

bool EditorState::commit_rectangle() {
    if (!drafting_ || draft_points_.size() != 4) {
        cancel_polygon();
        return false;
    }
    const Vec2 low = draft_points_[0];
    const Vec2 high = draft_points_[2];
    // Reject a rectangle thinner than one snap step in either axis: a click
    // without a real drag should not stamp a degenerate sliver.
    if (high.x - low.x < snap_step || high.y - low.y < snap_step) {
        cancel_polygon();
        return false;
    }
    polygons_.push_back({
        .points = draft_points_,
        .type = draft_type_,
        .id = std::nullopt,
        .tag = {},
    });
    drafting_ = false;
    draft_points_.clear();
    return true;
}

bool EditorState::select_vertex(const Vec2 world_pos, const float radius) {
    selected_polygon_ = -1;
    selected_vertex_ = -1;
    float best = radius * radius;
    for (std::size_t p = 0; p < polygons_.size(); ++p) {
        const std::vector<Vec2>& points = polygons_[p].points;
        for (std::size_t v = 0; v < points.size(); ++v) {
            const Vec2 delta = points[v] - world_pos;
            const float distance_sq = delta.x * delta.x + delta.y * delta.y;
            if (distance_sq <= best) {
                best = distance_sq;
                selected_polygon_ = static_cast<int>(p);
                selected_vertex_ = static_cast<int>(v);
            }
        }
    }
    return selected_vertex_ >= 0;
}

void EditorState::move_selected_vertex(const Vec2 world_pos, const bool snap) {
    if (selected_polygon_ < 0 || selected_vertex_ < 0) { return; }
    polygons_[static_cast<std::size_t>(selected_polygon_)]
        .points[static_cast<std::size_t>(selected_vertex_)] =
        snap ? snap_to_grid(world_pos) : world_pos;
}

bool EditorState::select_polygon(const Vec2 world_pos) {
    selected_vertex_ = -1;
    selected_polygon_ = -1;
    for (std::size_t p = 0; p < polygons_.size(); ++p) {
        if (point_in_polygon(world_pos, polygons_[p].points)) {
            selected_polygon_ = static_cast<int>(p);
            return true;
        }
    }
    return false;
}

void EditorState::delete_selected_polygon() {
    if (selected_polygon_ < 0 ||
        selected_polygon_ >= static_cast<int>(polygons_.size())) {
        return;
    }
    polygons_.erase(polygons_.begin() + selected_polygon_);
    selected_polygon_ = -1;
    selected_vertex_ = -1;
}

void EditorState::move_selected_polygon(const Vec2 delta) {
    if (selected_polygon_ < 0 ||
        selected_polygon_ >= static_cast<int>(polygons_.size())) {
        return;
    }
    for (Vec2& point :
         polygons_[static_cast<std::size_t>(selected_polygon_)].points) {
        point = point + delta;
    }
}

void EditorState::place_entity(
    const EntityType type, const Vec2 world_pos, const bool snap) {
    entities_.push_back({type, snap ? snap_to_grid(world_pos) : world_pos});
}

void EditorState::load_screen(const ScreenMap& screen) {
    screen_index_ = screen.index;
    width_ = screen.width;
    height_ = screen.height;
    polygons_.clear();
    passthrough_colliders_.clear();
    next_collider_id_ = 1;
    for (const MapCollider& collider : screen.colliders) {
        next_collider_id_ = std::max(next_collider_id_, collider.id + 1);
        if (const auto* polygon = std::get_if<PolygonGeometry>(&collider.geometry)) {
            polygons_.push_back({
                .points = polygon->points,
                .type = collider.type,
                .id = collider.id,
                .tag = collider.tag,
            });
        } else {
            passthrough_colliders_.push_back(collider);
        }
    }
    entities_ = screen.entities;
    drafting_ = false;
    draft_points_.clear();
    selected_polygon_ = -1;
    selected_vertex_ = -1;
}

ScreenMap EditorState::to_screen_map() const {
    ScreenMap map;
    map.index = screen_index_;
    map.width = width_;
    map.height = height_;
    map.biome = "courtyard";
    int next_id = next_collider_id_;
    for (const MapCollider& collider : passthrough_colliders_) {
        map.colliders.push_back(collider);
    }
    for (const DraftPolygon& polygon : polygons_) {
        const int id = polygon.id ? *polygon.id : next_id++;
        map.colliders.push_back({
            .id = id,
            .type = polygon.type,
            .geometry = PolygonGeometry{polygon.points},
            .tag = polygon.tag,
        });
    }
    map.entities = entities_;
    return map;
}

}  // namespace jumpcastle
