#include "jumpcastle/presentation.hpp"

#include "jumpcastle/game_config.hpp"

#include <algorithm>
#include <cmath>

namespace jumpcastle {

int preferred_window_scale(
    const int available_width,
    const int available_height) noexcept {
    const int width_scale = available_width / config::view_width;
    const int height_scale = available_height / config::view_height;
    return std::clamp(std::min(width_scale, height_scale), 1, 2);
}

PresentationLayout fit_presentation(
    const int window_width,
    const int window_height) noexcept {
    const float fitted_scale = std::min(
        static_cast<float>(window_width) / static_cast<float>(config::view_width),
        static_cast<float>(window_height) / static_cast<float>(config::view_height));
    const float scale = fitted_scale >= 2.0F
        ? std::floor(fitted_scale)
        : std::max(fitted_scale, 0.0F);
    const float width = scale * static_cast<float>(config::view_width);
    const float height = scale * static_cast<float>(config::view_height);
    return {
        .scale = scale,
        .width = width,
        .height = height,
        .offset_x = (static_cast<float>(window_width) - width) * 0.5F,
        .offset_y = (static_cast<float>(window_height) - height) * 0.5F,
    };
}

}  // namespace jumpcastle
