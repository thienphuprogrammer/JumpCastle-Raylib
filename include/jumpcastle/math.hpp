#pragma once

#include <cmath>

namespace jumpcastle {

struct Vec2 {
    float x{};
    float y{};

    friend constexpr bool operator==(Vec2, Vec2) = default;
};

[[nodiscard]] constexpr Vec2 operator+(const Vec2 left, const Vec2 right) noexcept {
    return {left.x + right.x, left.y + right.y};
}

[[nodiscard]] constexpr Vec2 operator-(const Vec2 left, const Vec2 right) noexcept {
    return {left.x - right.x, left.y - right.y};
}

[[nodiscard]] constexpr Vec2 operator*(const Vec2 value, const float scale) noexcept {
    return {value.x * scale, value.y * scale};
}

[[nodiscard]] inline float length(const Vec2 value) noexcept {
    return std::hypot(value.x, value.y);
}

[[nodiscard]] inline Vec2 normalized(const Vec2 value) noexcept {
    const float magnitude = length(value);
    return magnitude == 0.0F ? Vec2{} : value * (1.0F / magnitude);
}

}  // namespace jumpcastle
