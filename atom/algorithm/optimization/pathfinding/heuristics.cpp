#include "heuristics.hpp"

#include <algorithm>
#include <cmath>

namespace atom::algorithm {

namespace heuristics {

f32 manhattan(const Point& a, const Point& b) {
    return static_cast<f32>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

f32 euclidean(const Point& a, const Point& b) {
    return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2));
}

f32 diagonal(const Point& a, const Point& b) {
    i32 dx = std::abs(a.x - b.x);
    i32 dy = std::abs(a.y - b.y);
    // Diagonal distance is Chebyshev distance: max(|dx|, |dy|)
    return static_cast<f32>(std::max(dx, dy));
}

f32 octile(const Point& a, const Point& b) {
    constexpr f32 D = 1.0f;
    constexpr f32 D2 = 1.414f;

    i32 dx = std::abs(a.x - b.x);
    i32 dy = std::abs(a.y - b.y);

    return D * (dx + dy) + (D2 - 2 * D) * std::min(dx, dy);
}

f32 zero(const Point& a, const Point& b) {
    (void)a;
    (void)b;
    return 0.0f;
}

}  // namespace heuristics

}  // namespace atom::algorithm
