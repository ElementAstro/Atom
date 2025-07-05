#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <optional>
#include <queue>
#include <ranges>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/rust_numeric.hpp"

namespace atom::algorithm {

//=============================================================================
// Point Structure
//=============================================================================
struct Point {
    i32 x;
    i32 y;

    // Using C++20 spaceship operator
    auto operator<=>(const Point&) const = default;
    bool operator==(const Point&) const = default;

    // Utility functions for point arithmetic
    Point operator+(const Point& other) const {
        return {x + other.x, y + other.y};
    }
    Point operator-(const Point& other) const {
        return {x - other.x, y - other.y};
    }
};

//=============================================================================
// Graph Interface & Concept
//=============================================================================
// Abstract graph interface
template <typename NodeType>
class IGraph {
public:
    using node_type = NodeType;

    virtual ~IGraph() = default;
    virtual std::vector<NodeType> neighbors(const NodeType& node) const = 0;
    virtual f32 cost(const NodeType& from, const NodeType& to) const = 0;
};

// Concept for a valid Graph type
template <typename G>
concept Graph = requires(G g, typename G::node_type n) {
    { g.neighbors(n) } -> std::ranges::range;
    { g.cost(n, n) } -> std::convertible_to<f32>;
};

//=============================================================================
// Heuristic Functions & Concept
//=============================================================================
namespace heuristics {

// Heuristic concept
template <typename F, typename Node>
concept Heuristic =
    std::invocable<F, Node, Node> &&
    std::convertible_to<std::invoke_result_t<F, Node, Node>, f32>;

// Heuristic functions
f32 manhattan(const Point& a, const Point& b);
f32 euclidean(const Point& a, const Point& b);
f32 diagonal(const Point& a, const Point& b);
f32 zero(const Point& a, const Point& b);
f32 octile(const Point& a, const Point& b);  // Optimized diagonal heuristic

}  // namespace heuristics

//=============================================================================
// Grid Map Implementation
//=============================================================================
class GridMap : public IGraph<Point> {
public:
    // Movement direction flags
    enum Direction : u8 {
        NONE = 0,
        N = 1,       // 0001
        E = 2,       // 0010
        S = 4,       // 0100
        W = 8,       // 1000
        NE = N | E,  // 0011
        SE = S | E,  // 0110
        SW = S | W,  // 1100
        NW = N | W   // 1001
    };

    // Terrain types with associated costs
    enum class TerrainType : u8 {
        Open = 0,           // Normal passage area
        Difficult = 1,      // Difficult terrain (like gravel, tall grass)
        VeryDifficult = 2,  // Very difficult terrain (like swamps)
        Road = 3,           // Roads (faster movement)
        Water = 4,          // Water (passable by some units)
        Obstacle = 5        // Obstacle (impassable)
    };

    /**
     * @brief Construct an empty grid map
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(i32 width, i32 height);

    /**
     * @brief Construct a grid map with obstacles
     * @param obstacles Array of obstacles (true = obstacle, false = free)
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(std::span<const bool> obstacles, i32 width, i32 height);

    /**
     * @brief Construct a grid map with obstacles from u8 values
     * @param obstacles Array of obstacles (non-zero = obstacle, 0 = free)
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(std::span<const u8> obstacles, i32 width, i32 height);

    // IGraph implementation
    std::vector<Point> neighbors(const Point& p) const override;
    f32 cost(const Point& from, const Point& to) const override;

    // Advanced neighborhood function with directional constraints for JPS
    std::vector<Point> getNeighborsForJPS(const Point& p,
                                          Direction allowedDirections) const;

    // Natural neighbors - returns only naturally accessible neighbors (no
    // diagonal movement if blocked)
    std::vector<Point> naturalNeighbors(const Point& p) const;

    // GridMap specific methods
    bool isValid(const Point& p) const;
    void setObstacle(const Point& p, bool isObstacle);
    bool hasObstacle(const Point& p) const;

    // Terrain functions
    void setTerrain(const Point& p, TerrainType terrain);
    TerrainType getTerrain(const Point& p) const;
    f32 getTerrainCost(TerrainType terrain) const;

    // Utility methods for JPS algorithm
    bool hasForced(const Point& p, Direction dir) const;
    Direction getDirType(const Point& p, const Point& next) const;

    // Accessors
    i32 getWidth() const { return width_; }
    i32 getHeight() const { return height_; }

    // Get position from index
    Point indexToPoint(i32 index) const {
        return {index % width_, index / width_};
    }

    // Get index from position
    i32 pointToIndex(const Point& p) const { return p.y * width_ + p.x; }

private:
    i32 width_;
    i32 height_;
    std::vector<bool>
        obstacles_;  // Can be replaced with terrain type matrix in the future
    std::vector<TerrainType> terrain_;  // Terrain types
};

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

// Hash function for Point
namespace std {
template <>
struct hash<atom::algorithm::Point> {
    size_t operator()(const atom::algorithm::Point& p) const {
        return hash<atom::algorithm::i32>()(p.x) ^
               (hash<atom::algorithm::i32>()(p.y) << 1);
    }
};
}  // namespace std
