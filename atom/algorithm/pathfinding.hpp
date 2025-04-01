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

namespace atom::algorithm {

//=============================================================================
// Point Structure
//=============================================================================
struct Point {
    int x;
    int y;

    // Using C++20 spaceship operator
    auto operator<=>(const Point&) const = default;
    bool operator==(const Point&) const = default;
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
    virtual float cost(const NodeType& from, const NodeType& to) const = 0;
};

// Concept for a valid Graph type
template <typename G>
concept Graph = requires(G g, typename G::node_type n) {
    { g.neighbors(n) } -> std::ranges::range;
    { g.cost(n, n) } -> std::convertible_to<float>;
};

//=============================================================================
// Heuristic Functions & Concept
//=============================================================================
namespace heuristics {

// Heuristic concept
template <typename F, typename Node>
concept Heuristic =
    std::invocable<F, Node, Node> &&
    std::convertible_to<std::invoke_result_t<F, Node, Node>, float>;

// Heuristic functions
float manhattan(const Point& a, const Point& b);
float euclidean(const Point& a, const Point& b);
float diagonal(const Point& a, const Point& b);
float zero(const Point& a, const Point& b);

}  // namespace heuristics

//=============================================================================
// Grid Map Implementation
//=============================================================================
class GridMap : public IGraph<Point> {
public:
    /**
     * @brief Construct an empty grid map
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(int width, int height);

    /**
     * @brief Construct a grid map with obstacles
     * @param obstacles Array of obstacles (true = obstacle, false = free)
     * @param width Width of the grid
     * @param height Height of the grid
     */
    GridMap(std::span<const bool> obstacles, int width, int height);

    // IGraph implementation
    std::vector<Point> neighbors(const Point& p) const override;
    float cost(const Point& from, const Point& to) const override;

    // GridMap specific methods
    bool isValid(const Point& p) const;
    void setObstacle(const Point& p, bool isObstacle);
    bool hasObstacle(const Point& p) const;

    // Accessors
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    int width_;
    int height_;
    std::vector<bool> obstacles_;
};

//=============================================================================
// Pathfinder Class
//=============================================================================
class PathFinder {
public:
    // Enum for selecting heuristic type
    enum class HeuristicType { Manhattan, Euclidean, Diagonal };

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
        using QueueItem = std::pair<float, Node>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSet;

        // Maps for tracking
        std::unordered_map<Node, Node> cameFrom;
        std::unordered_map<Node, float> gScore;
        std::unordered_set<Node> closedSet;

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

            // Add to closed set
            closedSet.insert(current);

            // Check if we reached the goal
            if (current == goal) {
                // Reconstruct path
                std::vector<Node> path;
                while (current != start) {
                    path.push_back(current);
                    current = cameFrom[current];
                }
                path.push_back(start);
                std::ranges::reverse(path);
                return std::make_optional(path);
            }

            // Process neighbors
            for (const auto& neighbor : graph.neighbors(current)) {
                // Skip if already processed
                if (closedSet.contains(neighbor))
                    continue;

                // Calculate tentative g-score
                float tentativeG =
                    gScore[current] + graph.cost(current, neighbor);

                // If better path found
                if (!gScore.contains(neighbor) ||
                    tentativeG < gScore[neighbor]) {
                    // Update tracking information
                    cameFrom[neighbor] = current;
                    gScore[neighbor] = tentativeG;
                    float fScore = tentativeG + heuristic(neighbor, goal);

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
     * @brief Convenient method to find path on a grid map
     * @param map The grid map
     * @param start Starting position
     * @param goal Goal position
     * @param heuristicType Type of heuristic to use
     * @return Optional path from start to goal (empty if no path exists)
     */
    static std::optional<std::vector<Point>> findGridPath(
        const GridMap& map, const Point& start, const Point& goal,
        HeuristicType heuristicType = HeuristicType::Manhattan);
};

}  // namespace atom::algorithm

// Hash function for Point
namespace std {
template <>
struct hash<atom::algorithm::Point> {
    size_t operator()(const atom::algorithm::Point& p) const {
        return hash<int>()(p.x) ^ (hash<int>()(p.y) << 1);
    }
};
}  // namespace std