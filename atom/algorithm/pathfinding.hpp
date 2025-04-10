#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
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
float octile(const Point& a, const Point& b);  // 优化的对角线启发式

}  // namespace heuristics

//=============================================================================
// Grid Map Implementation
//=============================================================================
class GridMap : public IGraph<Point> {
public:
    // Movement direction flags
    enum Direction : uint8_t {
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
    enum class TerrainType : uint8_t {
        Open = 0,           // 正常通行区域
        Difficult = 1,      // 困难地形 (如砂砾，高草)
        VeryDifficult = 2,  // 非常困难的地形 (如沼泽)
        Road = 3,           // 道路 (移动更快)
        Water = 4,          // 水域 (某些单位可通过)
        Obstacle = 5        // 障碍物 (不可通行)
    };

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
    float getTerrainCost(TerrainType terrain) const;

    // Utility methods for JPS algorithm
    bool hasForced(const Point& p, Direction dir) const;
    Direction getDirType(const Point& p, const Point& next) const;

    // Accessors
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Get position from index
    Point indexToPoint(int index) const {
        return {index % width_, index / width_};
    }

    // Get index from position
    int pointToIndex(const Point& p) const { return p.y * width_ + p.x; }

private:
    int width_;
    int height_;
    std::vector<bool> obstacles_;       // 将来可以替换为地形类型矩阵
    std::vector<TerrainType> terrain_;  // 地形类型
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
        using QueueItem = std::pair<float, Node>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSet;

        // Maps for tracking (预分配以提高性能)
        std::unordered_map<Node, Node> cameFrom;
        std::unordered_map<Node, float> gScore;
        std::unordered_set<Node> closedSet;

        // Reserve space to reduce allocations
        const size_t estimatedSize = std::sqrt(1000);  // 估计节点数量
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
                path.reserve(estimatedSize);  // 预分配空间
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

        // 从起点和终点同时开始搜索
        std::unordered_map<Node, Node> cameFromStart;
        std::unordered_map<Node, float> gScoreStart;
        std::unordered_set<Node> closedSetStart;

        std::unordered_map<Node, Node> cameFromGoal;
        std::unordered_map<Node, float> gScoreGoal;
        std::unordered_set<Node> closedSetGoal;

        // Priority queues
        using QueueItem = std::pair<float, Node>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSetStart;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>>
            openSetGoal;

        // 预分配空间以提高性能
        const size_t estimatedSize = 1000;
        cameFromStart.reserve(estimatedSize);
        gScoreStart.reserve(estimatedSize);
        closedSetStart.reserve(estimatedSize);
        cameFromGoal.reserve(estimatedSize);
        gScoreGoal.reserve(estimatedSize);
        closedSetGoal.reserve(estimatedSize);

        // 初始化
        gScoreStart[start] = 0.0f;
        openSetStart.emplace(heuristic(start, goal), start);

        gScoreGoal[goal] = 0.0f;
        openSetGoal.emplace(heuristic(goal, start), goal);

        // 用于存储最佳交汇点
        std::optional<Node> meetingPoint;
        float bestTotalCost = std::numeric_limits<float>::infinity();

        // 两个方向轮流搜索
        while (!openSetStart.empty() && !openSetGoal.empty()) {
            // 从起点方向搜索一步
            if (!processOneStep(graph, openSetStart, closedSetStart,
                                cameFromStart, gScoreStart, goal, heuristic,
                                closedSetGoal, meetingPoint, bestTotalCost)) {
                break;  // 找到路径或无路径可达
            }

            // 从终点方向搜索一步
            if (!processOneStep(
                    graph, openSetGoal, closedSetGoal, cameFromGoal, gScoreGoal,
                    start,
                    [&](const Node& a, const Node& b) {
                        return heuristic(b, a);
                    },
                    closedSetStart, meetingPoint, bestTotalCost)) {
                break;  // 找到路径或无路径可达
            }
        }

        // 如果找到交汇点，重建路径
        if (meetingPoint) {
            std::vector<Node> pathFromStart;
            Node current = *meetingPoint;

            // 构建从起点到交汇点的路径
            while (current != start) {
                pathFromStart.push_back(current);
                current = cameFromStart[current];
            }
            pathFromStart.push_back(start);
            std::ranges::reverse(pathFromStart);

            // 构建从交汇点到终点的路径
            std::vector<Node> pathToGoal;
            current = *meetingPoint;
            while (current != goal) {
                current = cameFromGoal[current];
                pathToGoal.push_back(current);
            }

            // 合并路径
            pathFromStart.insert(pathFromStart.end(), pathToGoal.begin(),
                                 pathToGoal.end());
            return std::make_optional(pathFromStart);
        }

        // 没找到路径
        return std::nullopt;
    }

    /**
     * @brief Process one step of bidirectional search
     */
    template <Graph G, heuristics::Heuristic<typename G::node_type> H>
    static bool processOneStep(
        const G& graph,
        std::priority_queue<
            std::pair<float, typename G::node_type>,
            std::vector<std::pair<float, typename G::node_type>>,
            std::greater<>>& openSet,
        std::unordered_set<typename G::node_type>& closedSet,
        std::unordered_map<typename G::node_type, typename G::node_type>&
            cameFrom,
        std::unordered_map<typename G::node_type, float>& gScore,
        const typename G::node_type& target, H&& heuristic,
        const std::unordered_set<typename G::node_type>& oppositeClosedSet,
        std::optional<typename G::node_type>& meetingPoint,
        float& bestTotalCost) {
        if (openSet.empty())
            return false;

        auto current = openSet.top().second;
        openSet.pop();

        // 跳过已处理节点
        if (closedSet.contains(current))
            return true;

        closedSet.insert(current);

        // 检查是否与另一个方向的搜索相遇
        if (oppositeClosedSet.contains(current)) {
            float totalCost = gScore[current];
            if (totalCost < bestTotalCost) {
                bestTotalCost = totalCost;
                meetingPoint = current;
            }
        }

        // 处理邻居节点
        for (const auto& neighbor : graph.neighbors(current)) {
            if (closedSet.contains(neighbor))
                continue;

            float tentativeG = gScore[current] + graph.cost(current, neighbor);

            if (!gScore.contains(neighbor) || tentativeG < gScore[neighbor]) {
                cameFrom[neighbor] = current;
                gScore[neighbor] = tentativeG;
                float fScore = tentativeG + heuristic(neighbor, target);
                openSet.emplace(fScore, neighbor);

                // 检查这个邻居是否与另一个方向的搜索相遇
                if (oppositeClosedSet.contains(neighbor)) {
                    float totalCost = tentativeG;
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
        return hash<int>()(p.x) ^ (hash<int>()(p.y) << 1);
    }
};
}  // namespace std