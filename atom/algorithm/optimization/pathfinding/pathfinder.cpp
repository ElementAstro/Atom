#include "pathfinder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <spdlog/spdlog.h>

namespace atom::algorithm {

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

            f32 dx = static_cast<f32>(jumpPoint->x - current.x);
            f32 dy = static_cast<f32>(jumpPoint->y - current.y);
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
static bool isLeftTurn(const Point& a, const Point& b, const Point& c) {
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
