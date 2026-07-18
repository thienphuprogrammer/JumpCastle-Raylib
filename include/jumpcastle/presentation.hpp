#pragma once

namespace jumpcastle {

struct PresentationLayout {
    float scale{};
    float width{};
    float height{};
    float offset_x{};
    float offset_y{};
};

[[nodiscard]] int preferred_window_scale(
    int available_width,
    int available_height) noexcept;
[[nodiscard]] PresentationLayout fit_presentation(
    int window_width,
    int window_height) noexcept;

}  // namespace jumpcastle
