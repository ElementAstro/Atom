#include "pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <spdlog/spdlog.h>

namespace atom::algorithm {

//=============================================================================
// Heuristic Function Implementations
//=============================================================================
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
    return static_cast<f32>(1.0f * std::max(dx, dy) +
                            0.414f * std::min(dx, dy));
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

//=============================================================================
// GridMap Implementation
//=============================================================================
GridMap::GridMap(i32 width, i32 height)
    : width_(width),
      height_(height),
      obstacles_(width * height, false),
      terrain_(width * height, TerrainType::Open) {}

GridMap::GridMap(std::span<const bool> obstacles, i32 width, i32 height)
    : width_(width),
      height_(height),
      obstacles_(obstacles.begin(), obstacles.end()),
      terrain_(width * height, TerrainType::Open) {
    for (usize i = 0; i < obstacles_.size(); ++i) {
        if (obstacles_[i]) {
            terrain_[i] = TerrainType::Obstacle;
        }
    }
}

GridMap::GridMap(std::span<const u8> obstacles, i32 width, i32 height)
    : width_(width),
      height_(height),
      obstacles_(width * height, false),
      terrain_(width * height, TerrainType::Open) {
    for (usize i = 0; i < obstacles_.size(); ++i) {
        if (obstacles[i] != 0) {
            obstacles_[i] = true;
            terrain_[i] = TerrainType::Obstacle;
        }
    }
}

std::vector<Point> GridMap::neighbors(const Point& p) const {
    std::vector<Point> result;
    result.reserve(8);

    static const std::array<std::pair<i32, i32>, 8> directions = {
        {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};

    for (const auto& [dx, dy] : directions) {
        Point neighbor{p.x + dx, p.y + dy};
        if (isValid(neighbor)) {
            if (dx != 0 && dy != 0) {
                Point n1{p.x + dx, p.y};
                Point n2{p.x, p.y + dy};
                if (isValid(n1) && isValid(n2)) {
                    result.push_back(neighbor);
                }
            } else {
                result.push_back(neighbor);
            }
        }
    }

    return result;
}

std::vector<Point> GridMap::naturalNeighbors(const Point& p) const {
    std::vector<Point> result;
    result.reserve(8);

    static const std::array<std::pair<i32, i32>, 8> directions = {
        {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};

    for (const auto& [dx, dy] : directions) {
        Point neighbor{p.x + dx, p.y + dy};

        if (isValid(neighbor)) {
            if (dx != 0 && dy != 0) {
                Point n1{p.x + dx, p.y};
                Point n2{p.x, p.y + dy};

                if (isValid(n1) && isValid(n2)) {
                    result.push_back(neighbor);
                }
            } else {
                result.push_back(neighbor);
            }
        }
    }

    return result;
}

f32 GridMap::cost(const Point& from, const Point& to) const {
    f32 baseCost;
    if (from.x != to.x && from.y != to.y) {
        baseCost = 1.414f;
    } else {
        baseCost = 1.0f;
    }

    return baseCost * getTerrainCost(getTerrain(to));
}

bool GridMap::isValid(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return false;
    }

    usize index = static_cast<usize>(p.y * width_ + p.x);
    return index < obstacles_.size() && !obstacles_[index] &&
           terrain_[index] != TerrainType::Obstacle;
}

void GridMap::setObstacle(const Point& p, bool isObstacle) {
    if (p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_) {
        usize index = p.y * width_ + p.x;
        obstacles_[index] = isObstacle;

        terrain_[index] =
            isObstacle ? TerrainType::Obstacle : TerrainType::Open;
    }
}

bool GridMap::hasObstacle(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return true;
    }

    usize index = static_cast<usize>(p.y * width_ + p.x);
    return index < obstacles_.size() && obstacles_[index];
}

void GridMap::setTerrain(const Point& p, TerrainType terrain) {
    if (p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_) {
        usize index = p.y * width_ + p.x;
        terrain_[index] = terrain;

        obstacles_[index] = (terrain == TerrainType::Obstacle);
    }
}

GridMap::TerrainType GridMap::getTerrain(const Point& p) const {
    if (p.x < 0 || p.x >= width_ || p.y < 0 || p.y >= height_) {
        return TerrainType::Obstacle;
    }

    usize index = static_cast<usize>(p.y * width_ + p.x);
    return index < terrain_.size() ? terrain_[index] : TerrainType::Obstacle;
}

f32 GridMap::getTerrainCost(TerrainType terrain) const {
    switch (terrain) {
        case TerrainType::Open:
            return 1.0f;
        case TerrainType::Difficult:
            return 1.5f;
        case TerrainType::VeryDifficult:
            return 2.0f;
        case TerrainType::Road:
            return 0.8f;
        case TerrainType::Water:
            return 3.0f;
        case TerrainType::Obstacle:
        default:
            return std::numeric_limits<f32>::infinity();
    }
}

std::vector<Point> GridMap::getNeighborsForJPS(
    const Point& p, Direction allowedDirections) const {
    std::vector<Point> result;
    result.reserve(8);

    static const std::array<std::pair<i32, i32>, 8> offsets = {
        {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};

    static const std::array<Direction, 8> dirs = {N, E, S, W, NE, SE, SW, NW};

    for (usize i = 0; i < offsets.size(); ++i) {
        if ((allowedDirections & dirs[i]) != dirs[i]) {
            continue;
        }

        const auto [dx, dy] = offsets[i];
        Point neighbor{p.x + dx, p.y + dy};

        if (isValid(neighbor)) {
            if (dx != 0 && dy != 0) {
                Point n1{p.x + dx, p.y};
                Point n2{p.x, p.y + dy};
                if (isValid(n1) && isValid(n2)) {
                    result.push_back(neighbor);
                }
            } else {
                result.push_back(neighbor);
            }
        }
    }

    return result;
}

bool GridMap::hasForced(const Point& p, Direction dir) const {
    if (!isValid(p)) {
        return false;
    }

    switch (dir) {
        case N:
            return (!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y + 1})) ||
                   (!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y + 1}));
        case E:
            return (!isValid({p.x, p.y - 1}) && isValid({p.x + 1, p.y - 1})) ||
                   (!isValid({p.x, p.y + 1}) && isValid({p.x + 1, p.y + 1}));
        case S:
            return (!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y - 1})) ||
                   (!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y - 1}));
        case W:
            return (!isValid({p.x, p.y - 1}) && isValid({p.x - 1, p.y - 1})) ||
                   (!isValid({p.x, p.y + 1}) && isValid({p.x - 1, p.y + 1}));
        case NE:
            return (dir == NE) &&
                   ((!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y + 1})) ||
                    (!isValid({p.x, p.y - 1}) && isValid({p.x + 1, p.y - 1})));
        case SE:
            return (dir == SE) &&
                   ((!isValid({p.x - 1, p.y}) && isValid({p.x - 1, p.y - 1})) ||
                    (!isValid({p.x, p.y + 1}) && isValid({p.x + 1, p.y + 1})));
        case SW:
            return (dir == SW) &&
                   ((!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y - 1})) ||
                    (!isValid({p.x, p.y + 1}) && isValid({p.x - 1, p.y + 1})));
        case NW:
            return (dir == NW) &&
                   ((!isValid({p.x + 1, p.y}) && isValid({p.x + 1, p.y + 1})) ||
                    (!isValid({p.x, p.y - 1}) && isValid({p.x - 1, p.y - 1})));
        default:
            return false;
    }
}

GridMap::Direction GridMap::getDirType(const Point& p,
                                       const Point& next) const {
    i32 dx = next.x - p.x;
    i32 dy = next.y - p.y;

    if (dx == 0 && dy == 1)
        return N;
    if (dx == 1 && dy == 0)
        return E;
    if (dx == 0 && dy == -1)
        return S;
    if (dx == -1 && dy == 0)
        return W;
    if (dx == 1 && dy == 1)
        return NE;
    if (dx == 1 && dy == -1)
        return SE;
    if (dx == -1 && dy == -1)
        return SW;
    if (dx == -1 && dy == 1)
        return NW;

    return NONE;
}

//=============================================================================
// PathFinder Implementation
//=============================================================================

std::optional<Point> PathFinder::jump(const GridMap& map, const Point& current,
                                      const Point& direction,
                                      const Point& goal) {
    Point next{current.x + direction.x, current.y + direction.y};

    if (!map.isValid(next)) {
        return std::nullopt;
    }

    if (next == goal) {
        return next;
    }

    GridMap::Direction dir = map.getDirType(current, next);

    if (map.hasForced(next, dir)) {
        return next;
    }

    if (direction.x != 0 && direction.y != 0) {
        if (jump(map, next, {direction.x, 0}, goal) ||
            jump(map, next, {0, direction.y}, goal)) {
            return next;
        }
    }

    return jump(map, next, direction, goal);
}

std::optional<std::vector<Point>> PathFinder::findJPSPath(const GridMap& map,
                                                          const Point& start,
                                                          const Point& goal) {
    if (!map.isValid(start) || !map.isValid(goal)) {
        spdlog::debug("Invalid start or goal position for pathfinding");
        return std::nullopt;
    }

    auto heuristic = heuristics::octile;

    using QueueItem = std::pair<f32, Point>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
        openSet;

    std::unordered_map<Point, Point> cameFrom;
    std::unordered_map<Point, f32> gScore;
    std::unordered_set<Point> closedSet;

    usize estimatedSize = std::sqrt(map.getWidth() * map.getHeight());
    cameFrom.reserve(estimatedSize);
    gScore.reserve(estimatedSize);
    closedSet.reserve(estimatedSize);

    gScore[start] = 0.0f;
    openSet.emplace(heuristic(start, goal), start);

    while (!openSet.empty()) {
        auto current = openSet.top().second;
        openSet.pop();

        if (closedSet.contains(current)) {
            continue;
        }

        if (current == goal) {
            std::vector<Point> path;
            path.reserve(estimatedSize);

            while (current != start) {
                path.push_back(current);
                current = cameFrom[current];
            }
            path.push_back(start);
            std::ranges::reverse(path);

            spdlog::debug("Path found with JPS algorithm, length: {}",
                          path.size());
            return std::make_optional(smoothPath(path, map));
        }

        closedSet.insert(current);

        for (const auto& neighbor : map.naturalNeighbors(current)) {
            Point direction{neighbor.x - current.x, neighbor.y - current.y};

            auto jumpPoint = jump(map, current, direction, goal);
            if (!jumpPoint) {
                continue;
            }

            if (closedSet.contains(*jumpPoint)) {
                continue;
            }

            f32 tentativeG = gScore[current];

            f32 dx = jumpPoint->x - current.x;
            f32 dy = jumpPoint->y - current.y;
            f32 dist = std::sqrt(dx * dx + dy * dy);

            tentativeG += dist * 1.0f;

            if (!gScore.contains(*jumpPoint) ||
                tentativeG < gScore[*jumpPoint]) {
                cameFrom[*jumpPoint] = current;
                gScore[*jumpPoint] = tentativeG;
                f32 fScore = tentativeG + heuristic(*jumpPoint, goal);
                openSet.emplace(fScore, *jumpPoint);
            }
        }
    }

    spdlog::debug("No path found with JPS algorithm");
    return std::nullopt;
}

std::optional<std::vector<Point>> PathFinder::findGridPath(
    const GridMap& map, const Point& start, const Point& goal,
    HeuristicType heuristicType, AlgorithmType algorithmType) {
    if (!map.isValid(start) || !map.isValid(goal)) {
        spdlog::debug("Invalid start or goal position for pathfinding");
        return std::nullopt;
    }

    switch (algorithmType) {
        case AlgorithmType::AStar: {
            spdlog::debug("Using A* algorithm for pathfinding");
            switch (heuristicType) {
                case HeuristicType::Manhattan:
                    return findPath(map, start, goal, heuristics::manhattan);
                case HeuristicType::Euclidean:
                    return findPath(map, start, goal, heuristics::euclidean);
                case HeuristicType::Diagonal:
                    return findPath(map, start, goal, heuristics::diagonal);
                case HeuristicType::Octile:
                    return findPath(map, start, goal, heuristics::octile);
                default:
                    return findPath(map, start, goal, heuristics::manhattan);
            }
        }
        case AlgorithmType::Dijkstra:
            spdlog::debug("Using Dijkstra algorithm for pathfinding");
            return findPath(map, start, goal, heuristics::zero);
        case AlgorithmType::BiDirectional: {
            spdlog::debug("Using bidirectional search for pathfinding");
            switch (heuristicType) {
                case HeuristicType::Manhattan:
                    return findBidirectionalPath(map, start, goal,
                                                 heuristics::manhattan);
                case HeuristicType::Euclidean:
                    return findBidirectionalPath(map, start, goal,
                                                 heuristics::euclidean);
                case HeuristicType::Diagonal:
                    return findBidirectionalPath(map, start, goal,
                                                 heuristics::diagonal);
                case HeuristicType::Octile:
                    return findBidirectionalPath(map, start, goal,
                                                 heuristics::octile);
                default:
                    return findBidirectionalPath(map, start, goal,
                                                 heuristics::manhattan);
            }
        }
        case AlgorithmType::JPS:
            spdlog::debug("Using Jump Point Search algorithm for pathfinding");
            return findJPSPath(map, start, goal);
        default:
            spdlog::debug(
                "Using default A* with octile heuristic for pathfinding");
            return findPath(map, start, goal, heuristics::octile);
    }
}

std::vector<Point> PathFinder::smoothPath(const std::vector<Point>& path,
                                          const GridMap& map) {
    if (path.size() <= 2) {
        return path;
    }

    std::vector<Point> result;
    result.reserve(path.size());
    result.push_back(path.front());

    usize currentIndex = 0;

    while (currentIndex < path.size() - 1) {
        usize lastVisible = currentIndex;

        for (usize i = path.size() - 1; i > currentIndex; --i) {
            bool canSee = true;

            i32 x1 = path[currentIndex].x;
            i32 y1 = path[currentIndex].y;
            i32 x2 = path[i].x;
            i32 y2 = path[i].y;

            const i32 dx = std::abs(x2 - x1);
            const i32 dy = std::abs(y2 - y1);
            const i32 sx = x1 < x2 ? 1 : -1;
            const i32 sy = y1 < y2 ? 1 : -1;
            i32 err = dx - dy;

            i32 x = x1;
            i32 y = y1;

            while (x != x2 || y != y2) {
                i32 e2 = 2 * err;
                if (e2 > -dy) {
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) {
                    err += dx;
                    y += sy;
                }

                if ((x == x1 && y == y1) || (x == x2 && y == y2)) {
                    continue;
                }

                if (!map.isValid({x, y})) {
                    canSee = false;
                    break;
                }
            }

            if (canSee) {
                lastVisible = i;
                break;
            }
        }

        if (lastVisible != currentIndex) {
            result.push_back(path[lastVisible]);
            currentIndex = lastVisible;
        } else {
            result.push_back(path[currentIndex + 1]);
            currentIndex++;
        }
    }

    spdlog::debug("Path smoothed: original size = {}, smoothed size = {}",
                  path.size(), result.size());
    return result;
}

// Helper function to determine if a sequence of three points forms a left turn
bool isLeftTurn(const Point& a, const Point& b, const Point& c) {
    return ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) > 0;
}

std::vector<Point> PathFinder::funnelAlgorithm(const std::vector<Point>& path,
                                               const GridMap& map) {
    if (path.size() <= 2) {
        return path;
    }

    std::vector<Point> result;
    result.reserve(path.size());

    Point apex = path[0];
    result.push_back(apex);

    Point left = path[1];
    Point right = path[1];

    usize i = 2;
    while (i < path.size()) {
        Point next = path[i];

        // Check if we can directly move from apex to next (line of sight check)
        bool directPathPossible = true;
        i32 x1 = apex.x, y1 = apex.y;
        i32 x2 = next.x, y2 = next.y;
        const i32 dx = std::abs(x2 - x1);
        const i32 dy = std::abs(y2 - y1);
        const i32 sx = x1 < x2 ? 1 : -1;
        const i32 sy = y1 < y2 ? 1 : -1;
        i32 err = dx - dy;

        i32 x = x1, y = y1;
        while (x != x2 || y != y2) {
            i32 e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }

            if (!map.isValid({x, y})) {
                directPathPossible = false;
                break;
            }
        }

        if (isLeftTurn(apex, left, next)) {
            if (isLeftTurn(right, apex, next)) {
                // Update left side of funnel
                left = next;
            } else {
                // Right vertex is part of shortest path
                if (directPathPossible) {
                    result.push_back(right);
                    apex = right;
                    left = apex;
                    right = next;
                    i = std::find(path.begin(), path.end(), apex) -
                        path.begin() + 1;
                    continue;
                }
            }
        } else {
            if (isLeftTurn(apex, right, next)) {
                // Update right side of funnel
                right = next;
            } else {
                // Left vertex is part of shortest path
                if (directPathPossible) {
                    result.push_back(left);
                    apex = left;
                    right = apex;
                    left = next;
                    i = std::find(path.begin(), path.end(), apex) -
                        path.begin() + 1;
                    continue;
                }
            }
        }

        i++;
    }

    result.push_back(path.back());
    spdlog::debug(
        "Funnel algorithm applied: original size = {}, optimized size = {}",
        path.size(), result.size());
    return result;
}

}  // namespace atom::algorithm