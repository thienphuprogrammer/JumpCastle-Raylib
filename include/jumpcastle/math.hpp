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

[[nodiscard]] constexpr float dot(const Vec2 a, const Vec2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}

struct Aabb {
    Vec2 min{};
    Vec2 max{};
};

[[nodiscard]] constexpr Vec2 aabb_center(const Aabb box) noexcept {
    return {(box.min.x + box.max.x) * 0.5F, (box.min.y + box.max.y) * 0.5F};
}

[[nodiscard]] constexpr Vec2 aabb_half(const Aabb box) noexcept {
    return {(box.max.x - box.min.x) * 0.5F, (box.max.y - box.min.y) * 0.5F};
}

}  // namespace jumpcastle
