#include "pathfinding.hpp"

#include <algorithm>

namespace atom::algorithm {

//=============================================================================
// Heuristic Function Implementations
//=============================================================================
namespace heuristics {

float manhattan(const Point& a, const Point& b) {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

float euclidean(const Point& a, const Point& b) {
    return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2));
}

float diagonal(const Point& a, const Point& b) {
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    return static_cast<float>(1.0f * std::max(dx, dy) +
                              0.414f * std::min(dx, dy));
}

float zero(const Point& a, const Point& b) {
    (void)a;
    (void)b;
    return 0.0f;
}

}  // namespace heuristics

//=============================================================================
// GridMap Implementation
//=============================================================================
GridMap::GridMap(int width, int height)
    : width_(width), height_(height), obstacles_(width * height, false) {}

GridMap::GridMap(std::span<const bool> obstacles, int width, int height)
    : width_(width),
      height_(height),
      obstacles_(obstacles.begin(), obstacles.end()) {}

std::vector<Point> GridMap::neighbors(const Point& p) const {
    std::vector<Point> result;
    result.reserve(8);  // Up to 8 neighbors in a grid

    // Possible movement directions (including diagonals)
    static const std::array<std::pair<int, int>, 8> directions = {{
        {0, 1},
        {1, 0},
        {0, -1},
        {-1, 0},  // Cardinal directions
        {1, 1},
        {1, -1},
        {-1, 1},
        {-1, -1}  // Diagonal directions
    }};

    for (const auto& [dx, dy] : directions) {
        Point neighbor{p.x + dx, p.y + dy};
        if (isValid(neighbor)) {
            result.push_back(neighbor);
        }
    }

    return result;
}

float GridMap::cost(const Point& from, const Point& to) const {
    // Diagonal movement costs more
    if (from.x != to.x && from.y != to.y) {
        return 1.414f;  // sqrt(2)
    }
    return 1.0f;
}

bool GridMap::isValid(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return false;
    }

    size_t index = static_cast<size_t>(p.y * width_ + p.x);
    return index < obstacles_.size() && !obstacles_[index];
}

void GridMap::setObstacle(const Point& p, bool isObstacle) {
    if (p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_) {
        obstacles_[p.y * width_ + p.x] = isObstacle;
    }
}

bool GridMap::hasObstacle(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return true;  // Out of bounds is considered an obstacle
    }

    size_t index = static_cast<size_t>(p.y * width_ + p.x);
    return index < obstacles_.size() && obstacles_[index];
}

//=============================================================================
// PathFinder Implementation
//=============================================================================
std::optional<std::vector<Point>> PathFinder::findGridPath(
    const GridMap& map, const Point& start, const Point& goal,
    HeuristicType heuristicType) {
    // Check if start and goal are valid
    if (!map.isValid(start) || !map.isValid(goal)) {
        return std::nullopt;
    }

    // Select heuristic based on type
    switch (heuristicType) {
        case HeuristicType::Manhattan:
            return findPath(map, start, goal, heuristics::manhattan);
        case HeuristicType::Euclidean:
            return findPath(map, start, goal, heuristics::euclidean);
        case HeuristicType::Diagonal:
            return findPath(map, start, goal, heuristics::diagonal);
        default:
            return findPath(map, start, goal, heuristics::manhattan);
    }
}

}  // namespace atom::algorithm