#include "jumpcastle/convex.hpp"

#include <algorithm>
#include <cstddef>

namespace jumpcastle {
namespace {

float cross(const Vec2 a, const Vec2 b) noexcept { return a.x * b.y - a.y * b.x; }

}  // namespace

Vec2 polygon_centroid(const std::vector<Vec2>& points) {
    Vec2 sum{};
    for (const Vec2 p : points) { sum = sum + p; }
    const float n = static_cast<float>(points.size());
    return n == 0.0F ? Vec2{} : sum * (1.0F / n);
}

std::vector<Vec2> outward_edge_normals(const std::vector<Vec2>& points) {
    const Vec2 center = polygon_centroid(points);
    std::vector<Vec2> normals;
    normals.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec2 a = points[i];
        const Vec2 b = points[(i + 1) % points.size()];
        const Vec2 edge = b - a;
        Vec2 normal = normalized({edge.y, -edge.x});
        const Vec2 mid = (a + b) * 0.5F;
        if (dot(normal, mid - center) < 0.0F) { normal = normal * -1.0F; }
        normals.push_back(normal);
    }
    return normals;
}

bool is_convex(const std::vector<Vec2>& points) {
    if (points.size() < 3) { return false; }
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec2 a = points[i];
        const Vec2 b = points[(i + 1) % points.size()];
        const Vec2 c = points[(i + 2) % points.size()];
        const float turn = cross(b - a, c - b);
        if (turn > 0.0F) { positive = true; }
        if (turn < 0.0F) { negative = true; }
        if (positive && negative) { return false; }
    }
    return true;
}

Aabb polygon_aabb(const std::vector<Vec2>& points) {
    Aabb box{points.front(), points.front()};
    for (const Vec2 p : points) {
        box.min.x = std::min(box.min.x, p.x);
        box.min.y = std::min(box.min.y, p.y);
        box.max.x = std::max(box.max.x, p.x);
        box.max.y = std::max(box.max.y, p.y);
    }
    return box;
}

std::vector<std::vector<Vec2>> split_to_convex(const std::vector<Vec2>& points) {
    if (is_convex(points)) { return {points}; }

    std::vector<Vec2> poly = points;
    float area = 0.0F;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        area += cross(poly[i], poly[(i + 1) % poly.size()]);
    }
    if (area < 0.0F) { std::reverse(poly.begin(), poly.end()); }

    const auto in_triangle = [](Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
        const float d1 = cross(b - a, p - a);
        const float d2 = cross(c - b, p - b);
        const float d3 = cross(a - c, p - c);
        const bool neg = (d1 < 0.0F) || (d2 < 0.0F) || (d3 < 0.0F);
        const bool pos = (d1 > 0.0F) || (d2 > 0.0F) || (d3 > 0.0F);
        return !(neg && pos);
    };

    std::vector<std::vector<Vec2>> triangles;
    std::vector<std::size_t> idx(poly.size());
    for (std::size_t i = 0; i < poly.size(); ++i) { idx[i] = i; }

    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        for (std::size_t i = 0; i < idx.size(); ++i) {
            const Vec2 a = poly[idx[(i + idx.size() - 1) % idx.size()]];
            const Vec2 b = poly[idx[i]];
            const Vec2 c = poly[idx[(i + 1) % idx.size()]];
            if (cross(b - a, c - b) <= 0.0F) { continue; }  // reflex, not an ear
            bool contains = false;
            for (const std::size_t j : idx) {
                const Vec2 p = poly[j];
                if (p == a || p == b || p == c) { continue; }
                if (in_triangle(p, a, b, c)) { contains = true; break; }
            }
            if (contains) { continue; }
            triangles.push_back({a, b, c});
            idx.erase(idx.begin() + static_cast<long>(i));
            clipped = true;
            break;
        }
        if (!clipped) { break; }
    }
    if (idx.size() == 3) {
        triangles.push_back({poly[idx[0]], poly[idx[1]], poly[idx[2]]});
    }
    return triangles;
}

}  // namespace jumpcastle
