#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "graph.hpp"
#include "grid_map.hpp"
#include "heuristics.hpp"
#include "point.hpp"

namespace atom::algorithm {

//=============================================================================
// Pathfinder Class
//=============================================================================
class PathFinder {
public:
    // Enum for selecting heuristic type
    enum class HeuristicType { Manhattan, Euclidean, Diagonal, Octile };

    // Enum for selecting algorithm type
    enum class AlgorithmType { AStar, Dijkstra, BiDirectional, JPS };

    /**
     * @brief Find a path using A* algorithm
     * @param graph The graph to search in
     * @param start Starting node
     * @param goal Goal node
     * @param heuristic Heuristic function
     * @return Optional path from start to goal (empty if no path exists)
     */
    template <Graph G, heuristics::Heuristic<typename G::node_type> H>
    static std::optional<std::vector<typename G::node_type>> findPath(
        const G& graph, const typename G::node_type& start,
        const typename G::node_type& goal, H&& heuristic) {
        using Node = typename G::node_type;

        // Priority queue for open set
        using QueueItem = std::pair<f32, Node>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSet;

        // Maps for tracking (pre-allocate to improve performance)
        std::unordered_map<Node, Node> cameFrom;
        std::unordered_map<Node, f32> gScore;
        std::unordered_set<Node> closedSet;

        // Reserve space to reduce allocations
        const usize estimatedSize = std::sqrt(1000);  // Estimate node count
        cameFrom.reserve(estimatedSize);
        gScore.reserve(estimatedSize);
        closedSet.reserve(estimatedSize);

        // Initialize
        gScore[start] = 0.0f;
        openSet.emplace(heuristic(start, goal), start);

        while (!openSet.empty()) {
            // Get node with lowest f-score
            auto current = openSet.top().second;
            openSet.pop();

            // Skip if already processed
            if (closedSet.contains(current))
                continue;

            // Check if we reached the goal
            if (current == goal) {
                // Reconstruct path
                std::vector<Node> path;
                path.reserve(estimatedSize);  // Pre-allocate space
                while (current != start) {
                    path.push_back(current);
                    current = cameFrom[current];
                }
                path.push_back(start);
                std::ranges::reverse(path);
                return std::make_optional(path);
            }

            // Add to closed set
            closedSet.insert(current);

            // Process neighbors
            for (const auto& neighbor : graph.neighbors(current)) {
                // Skip if already processed
                if (closedSet.contains(neighbor))
                    continue;

                // Calculate tentative g-score
                f32 tentativeG =
                    gScore[current] + graph.cost(current, neighbor);

                // If better path found
                if (!gScore.contains(neighbor) ||
                    tentativeG < gScore[neighbor]) {
                    // Update tracking information
                    cameFrom[neighbor] = current;
                    gScore[neighbor] = tentativeG;
                    f32 fScore = tentativeG + heuristic(neighbor, goal);

                    // Add to open set
                    openSet.emplace(fScore, neighbor);
                }
            }
        }

        // No path found
        return std::nullopt;
    }

    /**
     * @brief Find a path using Dijkstra's algorithm
     * @param graph The graph to search in
     * @param start Starting node
     * @param goal Goal node
     * @return Optional path from start to goal (empty if no path exists)
     */
    template <Graph G>
    static std::optional<std::vector<typename G::node_type>> findPath(
        const G& graph, const typename G::node_type& start,
        const typename G::node_type& goal) {
        // Use A* with zero heuristic (Dijkstra)
        return findPath(graph, start, goal, heuristics::zero);
    }

    /**
     * @brief Find a path using bidirectional search
     * @param graph The graph to search in
     * @param start Starting node
     * @param goal Goal node
     * @param heuristic Heuristic function
     * @return Optional path from start to goal (empty if no path exists)
     */
    template <Graph G, heuristics::Heuristic<typename G::node_type> H>
    static std::optional<std::vector<typename G::node_type>>
    findBidirectionalPath(const G& graph, const typename G::node_type& start,
                          const typename G::node_type& goal, H&& heuristic) {
        using Node = typename G::node_type;

        // Search from both start and goal simultaneously
        std::unordered_map<Node, Node> cameFromStart;
        std::unordered_map<Node, f32> gScoreStart;
        std::unordered_set<Node> closedSetStart;

        std::unordered_map<Node, Node> cameFromGoal;
        std::unordered_map<Node, f32> gScoreGoal;
        std::unordered_set<Node> closedSetGoal;

        // Priority queues
        using QueueItem = std::pair<f32, Node>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSetStart;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSetGoal;

        // Pre-allocate space to improve performance
        const usize estimatedSize = 1000;
        cameFromStart.reserve(estimatedSize);
        gScoreStart.reserve(estimatedSize);
        closedSetStart.reserve(estimatedSize);
        cameFromGoal.reserve(estimatedSize);
        gScoreGoal.reserve(estimatedSize);
        closedSetGoal.reserve(estimatedSize);

        // Initialize
        gScoreStart[start] = 0.0f;
        openSetStart.emplace(heuristic(start, goal), start);

        gScoreGoal[goal] = 0.0f;
        openSetGoal.emplace(heuristic(goal, start), goal);

        // For storing best meeting point
        std::optional<Node> meetingPoint;
        f32 bestTotalCost = std::numeric_limits<f32>::infinity();

        // Alternate searching from both directions
        while (!openSetStart.empty() && !openSetGoal.empty()) {
            // Search one step from start direction
            if (!processOneStep(graph, openSetStart, closedSetStart,
                                cameFromStart, gScoreStart, goal, heuristic,
                                closedSetGoal, meetingPoint, bestTotalCost)) {
                break;  // Found path or no path exists
            }

            // Search one step from goal direction
            if (!processOneStep(
                    graph, openSetGoal, closedSetGoal, cameFromGoal, gScoreGoal,
                    start,
                    [&](const Node& a, const Node& b) {
                        return heuristic(b, a);
                    },
                    closedSetStart, meetingPoint, bestTotalCost)) {
                break;  // Found path or no path exists
            }
        }

        // If meeting point found, reconstruct path
        if (meetingPoint) {
            std::vector<Node> pathFromStart;
            Node current = *meetingPoint;

            // Build path from start to meeting point
            while (current != start) {
                pathFromStart.push_back(current);
                current = cameFromStart[current];
            }
            pathFromStart.push_back(start);
            std::ranges::reverse(pathFromStart);

            // Build path from meeting point to goal
            std::vector<Node> pathToGoal;
            current = *meetingPoint;
            while (current != goal) {
                current = cameFromGoal[current];
                pathToGoal.push_back(current);
            }

            // Combine paths
            pathFromStart.insert(pathFromStart.end(), pathToGoal.begin(),
                                 pathToGoal.end());
            return std::make_optional(pathFromStart);
        }

        // No path found
        return std::nullopt;
    }

    /**
     * @brief Process one step of bidirectional search
     */
    template <Graph G, heuristics::Heuristic<typename G::node_type> H>
    static bool processOneStep(
        const G& graph,
        std::priority_queue<std::pair<f32, typename G::node_type>,
                            std::vector<std::pair<f32, typename G::node_type>>,
                            std::greater<>>& openSet,
        std::unordered_set<typename G::node_type>& closedSet,
        std::unordered_map<typename G::node_type, typename G::node_type>&
            cameFrom,
        std::unordered_map<typename G::node_type, f32>& gScore,
        const typename G::node_type& target, H&& heuristic,
        const std::unordered_set<typename G::node_type>& oppositeClosedSet,
        std::optional<typename G::node_type>& meetingPoint,
        f32& bestTotalCost) {
        if (openSet.empty())
            return false;

        auto current = openSet.top().second;
        openSet.pop();

        // Skip already processed nodes
        if (closedSet.contains(current))
            return true;

        closedSet.insert(current);

        // Check if we've met the opposite direction search
        if (oppositeClosedSet.contains(current)) {
            f32 totalCost = gScore[current];
            if (totalCost < bestTotalCost) {
                bestTotalCost = totalCost;
                meetingPoint = current;
            }
        }

        // Process neighbors
        for (const auto& neighbor : graph.neighbors(current)) {
            if (closedSet.contains(neighbor))
                continue;

            f32 tentativeG = gScore[current] + graph.cost(current, neighbor);

            if (!gScore.contains(neighbor) || tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current;
                gScore[neighbor] = tentativeG;
                f32 fScore = tentativeG + heuristic(neighbor, target);
                openSet.emplace(fScore, neighbor);

                // Check if this neighbor meets the opposite search
                if (oppositeClosedSet.contains(neighbor)) {
                    f32 totalCost = tentativeG;
                    if (totalCost < bestTotalCost) {
                        bestTotalCost = totalCost;
                        meetingPoint = neighbor;
                    }
                }
            }
        }

        return true;
    }

    /**
     * @brief Find path using Jump Point Search algorithm (JPS)
     * @param map The grid map
     * @param start Starting position
     * @param goal Goal position
     * @return Optional path from start to goal (empty if no path exists)
     */
    static std::optional<std::vector<Point>> findJPSPath(const GridMap& map,
                                                         const Point& start,
                                                         const Point& goal);

    /**
     * @brief Helper function for JPS to identify jump points
     * @param map The grid map
     * @param current Current position
     * @param direction Direction of travel
     * @param goal Goal position
     * @return Jump point or nullopt if none found
     */
    static std::optional<Point> jump(const GridMap& map, const Point& current,
                                     const Point& direction, const Point& goal);

    /**
     * @brief Convenient method to find path on a grid map
     * @param map The grid map
     * @param start Starting position
     * @param goal Goal position
     * @param heuristicType Type of heuristic to use
     * @param algorithmType Type of algorithm to use
     * @return Optional path from start to goal (empty if no path exists)
     */
    static std::optional<std::vector<Point>> findGridPath(
        const GridMap& map, const Point& start, const Point& goal,
        HeuristicType heuristicType = HeuristicType::Manhattan,
        AlgorithmType algorithmType = AlgorithmType::AStar);

    /**
     * @brief Post-process a path to optimize it
     * @param path The path to optimize
     * @param map The grid map for validity checking
     * @return Optimized path
     */
    static std::vector<Point> smoothPath(const std::vector<Point>& path,
                                         const GridMap& map);

    /**
     * @brief Create a funnel algorithm path from a corridor
     * @param path The path containing waypoints
     * @param map The grid map
     * @return Optimized path with the funnel algorithm
     */
    static std::vector<Point> funnelAlgorithm(const std::vector<Point>& path,
                                              const GridMap& map);
};

}  // namespace atom::algorithm
