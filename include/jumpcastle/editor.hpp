#pragma once

#include "jumpcastle/map_format.hpp"
#include "jumpcastle/math.hpp"

#include <vector>

namespace jumpcastle {

// Headless in-game map editor state: builds polygon colliders and entities for
// a single screen. Pure logic (no raylib) so it can be unit-tested; the raylib
// input/render/save layer drives it. Coordinates are screen-local tile units.
class EditorState {
public:
    static constexpr float snap_step = 0.25F;

    EditorState() = default;
    EditorState(int screen_index, float width, float height) noexcept;

    // --- Polygon drafting ---
    void begin_polygon(ColliderType type);
    void add_vertex(Vec2 world_pos, bool snap);
    // Commits the draft if it has >= 3 points (concave shapes are split into
    // convex pieces at export). Returns whether a polygon was committed.
    bool close_polygon();
    void cancel_polygon();
    [[nodiscard]] bool is_drafting() const noexcept { return drafting_; }
    [[nodiscard]] const std::vector<Vec2>& draft_points() const noexcept {
        return draft_points_;
    }

    // --- Selection / editing of committed polygons ---
    // Selects the nearest committed vertex within radius; returns whether one
    // was found. On success move_selected_vertex() repositions it.
    bool select_vertex(Vec2 world_pos, float radius);
    void move_selected_vertex(Vec2 world_pos, bool snap);
    // Selects a committed polygon containing world_pos; returns whether found.
    bool select_polygon(Vec2 world_pos);
    void delete_selected_polygon();
    [[nodiscard]] int selected_polygon() const noexcept { return selected_polygon_; }
    [[nodiscard]] std::size_t polygon_count() const noexcept { return polygons_.size(); }

    // --- Entities ---
    void place_entity(EntityType type, Vec2 world_pos, bool snap);
    [[nodiscard]] std::size_t entity_count() const noexcept { return entities_.size(); }

    // Exports the authored screen (polygons split to convex + precomputed).
    [[nodiscard]] ScreenMap to_screen_map() const;

private:
    struct DraftPolygon {
        std::vector<Vec2> points;
        ColliderType type{ColliderType::solid};
    };

    int screen_index_{};
    float width_{16.0F};
    float height_{12.0F};

    std::vector<DraftPolygon> polygons_;
    std::vector<MapEntity> entities_;

    bool drafting_{};
    ColliderType draft_type_{ColliderType::solid};
    std::vector<Vec2> draft_points_;

    int selected_polygon_{-1};
    int selected_vertex_{-1};
};

}  // namespace jumpcastle
