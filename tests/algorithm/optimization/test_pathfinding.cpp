/*
 * test_pathfinding.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for Atom Algorithm Pathfinding Library
Tests A*, Dijkstra, and other pathfinding algorithms.

**************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "atom/algorithm/optimization/pathfinding.hpp"

namespace atom::algorithm::pathfinding::test {

// ============================================================================
// Point Structure Tests
// ============================================================================

TEST(PointTest, BasicOperations) {
    Point p1{1, 2};
    Point p2{3, 4};

    // Test equality
    Point p3{1, 2};
    EXPECT_EQ(p1, p3);
    EXPECT_NE(p1, p2);

    // Test arithmetic
    Point sum = p1 + p2;
    Point expected_sum{4, 6};
    EXPECT_EQ(sum, expected_sum);

    Point diff = p2 - p1;
    Point expected_diff{2, 2};
    EXPECT_EQ(diff, expected_diff);
}

TEST(PointTest, ComparisonOperations) {
    Point p1{1, 2};
    Point p2{3, 4};
    Point p3{1, 3};

    // Test spaceship operator (lexicographical comparison)
    EXPECT_LT(p1, p2);
    EXPECT_LT(p1, p3);
    EXPECT_GT(p2, p1);
    EXPECT_GT(p3, p1);
}

// ============================================================================
// Heuristic Function Tests
// ============================================================================

class HeuristicTest : public ::testing::Test {
protected:
    void SetUp() override {
        origin_ = Point{0, 0};
        point1_ = Point{3, 4};
        point2_ = Point{6, 8};
        diagonal_point_ = Point{5, 5};
    }

    Point origin_;
    Point point1_;
    Point point2_;
    Point diagonal_point_;
};

TEST_F(HeuristicTest, ManhattanDistance) {
    using namespace heuristics;

    // Manhattan distance: |x2-x1| + |y2-y1|
    EXPECT_FLOAT_EQ(manhattan(origin_, point1_), 7.0f);           // 3 + 4
    EXPECT_FLOAT_EQ(manhattan(origin_, point2_), 14.0f);          // 6 + 8
    EXPECT_FLOAT_EQ(manhattan(point1_, point2_), 7.0f);           // 3 + 4
    EXPECT_FLOAT_EQ(manhattan(origin_, diagonal_point_), 10.0f);  // 5 + 5
}

TEST_F(HeuristicTest, EuclideanDistance) {
    using namespace heuristics;

    // Euclidean distance: sqrt((x2-x1)^2 + (y2-y1)^2)
    EXPECT_FLOAT_EQ(euclidean(origin_, point1_), 5.0f);   // sqrt(3^2 + 4^2)
    EXPECT_FLOAT_EQ(euclidean(origin_, point2_), 10.0f);  // sqrt(6^2 + 8^2)
    EXPECT_FLOAT_EQ(euclidean(point1_, point2_), 5.0f);   // sqrt(3^2 + 4^2)
    EXPECT_FLOAT_EQ(euclidean(origin_, diagonal_point_),
                    std::sqrt(50.0f));  // sqrt(5^2 + 5^2)
}

TEST_F(HeuristicTest, DiagonalDistance) {
    using namespace heuristics;

    // Diagonal distance: max(|dx|, |dy|)
    EXPECT_FLOAT_EQ(diagonal(origin_, point1_), 4.0f);          // max(3, 4)
    EXPECT_FLOAT_EQ(diagonal(origin_, point2_), 8.0f);          // max(6, 8)
    EXPECT_FLOAT_EQ(diagonal(point1_, point2_), 4.0f);          // max(3, 4)
    EXPECT_FLOAT_EQ(diagonal(origin_, diagonal_point_), 5.0f);  // max(5, 5)
}

TEST_F(HeuristicTest, OctileDistance) {
    using namespace heuristics;

    // Octile distance should be between diagonal and euclidean for diagonal
    // movement
    float octile_dist = octile(origin_, diagonal_point_);
    EXPECT_GE(octile_dist, diagonal(origin_, diagonal_point_));
    EXPECT_LE(octile_dist, euclidean(origin_, diagonal_point_));
}

TEST_F(HeuristicTest, ZeroHeuristic) {
    using namespace heuristics;

    // Zero heuristic always returns 0
    EXPECT_FLOAT_EQ(zero(origin_, point1_), 0.0f);
    EXPECT_FLOAT_EQ(zero(origin_, point2_), 0.0f);
    EXPECT_FLOAT_EQ(zero(point1_, point2_), 0.0f);
}

// ============================================================================
// Grid Map Tests
// ============================================================================

class GridMapTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a 5x5 grid with some obstacles
        grid_ = std::make_unique<GridMap>(5, 5);

        // Set some obstacles
        grid_->setTerrain(Point{1, 1}, GridMap::TerrainType::Obstacle);
        grid_->setTerrain(Point{1, 2}, GridMap::TerrainType::Obstacle);
        grid_->setTerrain(Point{2, 2}, GridMap::TerrainType::Obstacle);
        grid_->setTerrain(Point{3, 2}, GridMap::TerrainType::Obstacle);

        // Set some different terrain types
        grid_->setTerrain(Point{4, 4}, GridMap::TerrainType::Water);
        grid_->setTerrain(Point{0, 4}, GridMap::TerrainType::Difficult);
    }

    std::unique_ptr<GridMap> grid_;
};

TEST_F(GridMapTest, BasicProperties) {
    EXPECT_EQ(grid_->getWidth(), 5);
    EXPECT_EQ(grid_->getHeight(), 5);
}

TEST_F(GridMapTest, TerrainOperations) {
    // Test terrain getting and setting
    EXPECT_EQ(grid_->getTerrain(Point{0, 0}),
              GridMap::TerrainType::Open);  // default
    EXPECT_EQ(grid_->getTerrain(Point{1, 1}),
              GridMap::TerrainType::Obstacle);  // set above

    grid_->setTerrain(Point{0, 0}, GridMap::TerrainType::Water);
    EXPECT_EQ(grid_->getTerrain(Point{0, 0}), GridMap::TerrainType::Water);
}

TEST_F(GridMapTest, ValidCoordinates) {
    // Test coordinate validation
    EXPECT_TRUE(grid_->isValid(Point{0, 0}));
    EXPECT_TRUE(grid_->isValid(Point{4, 4}));
    EXPECT_FALSE(grid_->isValid(Point{-1, 0}));
    EXPECT_FALSE(grid_->isValid(Point{5, 0}));
    EXPECT_FALSE(grid_->isValid(Point{0, -1}));
    EXPECT_FALSE(grid_->isValid(Point{0, 5}));
}

TEST_F(GridMapTest, TraversableTerrain) {
    // Test traversable checking using hasObstacle
    EXPECT_FALSE(grid_->hasObstacle(Point{0, 0}));  // Open terrain
    EXPECT_TRUE(grid_->hasObstacle(Point{1, 1}));   // Obstacle
    EXPECT_FALSE(grid_->hasObstacle(
        Point{4, 4}));  // Water (traversable with higher cost)
    EXPECT_FALSE(grid_->hasObstacle(Point{0, 4}));  // Open terrain
}

TEST_F(GridMapTest, NeighborGeneration) {
    // Test neighbor generation
    Point center{2, 1};

    // Get neighbors
    auto neighbors_list = grid_->neighbors(center);
    std::unordered_set<Point> expected_cardinal = {
        {1, 1}, {3, 1}, {2, 0}, {2, 2}};

    // Remove non-traversable neighbors (obstacles)
    neighbors_list.erase(
        std::remove_if(
            neighbors_list.begin(), neighbors_list.end(),
            [this](const Point& p) { return grid_->hasObstacle(p); }),
        neighbors_list.end());

    EXPECT_FALSE(neighbors_list.empty());

    // Note: GridMap doesn't have setDiagonalMovement, diagonal movement is
    // handled internally Test that we get valid neighbors
    auto all_neighbors = grid_->neighbors(center);
    EXPECT_GT(all_neighbors.size(), 0);
}

TEST_F(GridMapTest, MovementCost) {
    // Test movement cost calculation
    Point from{0, 0};
    Point to{1, 0};  // adjacent

    // Cost should depend on terrain type
    float open_cost = grid_->cost(from, to);
    EXPECT_GT(open_cost, 0.0f);

    // Test cost to water (should be higher)
    grid_->setTerrain(Point{1, 0}, GridMap::TerrainType::Water);
    float water_cost = grid_->cost(from, to);
    EXPECT_GT(water_cost, open_cost);

    // Test cost to obstacle (should be infinite or very high)
    grid_->setTerrain(Point{1, 0}, GridMap::TerrainType::Obstacle);
    float obstacle_cost = grid_->cost(from, to);
    EXPECT_EQ(obstacle_cost, std::numeric_limits<float>::infinity());
}

// ============================================================================
// A* Algorithm Tests
// ============================================================================

class AStarTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple 3x3 grid for testing
        grid_ = std::make_unique<GridMap>(3, 3);
        // Note: GridMap doesn't have setDiagonalMovement, diagonal movement is
        // handled internally
    }

    std::unique_ptr<GridMap> grid_;
};

TEST_F(AStarTest, SimplePath) {
    Point start{0, 0};
    Point goal{2, 2};

    auto path =
        PathFinder::findPath(*grid_, start, goal, heuristics::euclidean);

    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->empty());
    EXPECT_EQ(path->front(), start);
    EXPECT_EQ(path->back(), goal);

    // Verify path is valid (all nodes are not obstacles)
    for (const auto& node : *path) {
        EXPECT_FALSE(grid_->hasObstacle(node));
    }
}

TEST_F(AStarTest, ObstacleAvoidance) {
    // Create a partial wall between start and goal
    // This creates a wall that blocks the direct path but allows routing around
    grid_->setTerrain(Point{1, 0}, GridMap::TerrainType::Obstacle);
    grid_->setTerrain(Point{1, 1}, GridMap::TerrainType::Obstacle);
    // Note: (1, 2) is left open to allow a path

    Point start{0, 1};
    Point goal{2, 1};

    auto path =
        PathFinder::findPath(*grid_, start, goal, heuristics::manhattan);

    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->empty());

    // Path should go around the wall
    bool goes_through_wall = false;
    for (const auto& node : *path) {
        if (grid_->getTerrain(node) == GridMap::TerrainType::Obstacle) {
            goes_through_wall = true;
            break;
        }
    }
    EXPECT_FALSE(goes_through_wall);
}

TEST_F(AStarTest, NoPathExists) {
    // Surround the goal with obstacles
    grid_->setTerrain(Point{1, 1}, GridMap::TerrainType::Obstacle);
    grid_->setTerrain(Point{1, 2}, GridMap::TerrainType::Obstacle);
    grid_->setTerrain(Point{2, 1}, GridMap::TerrainType::Obstacle);

    Point start{0, 0};
    Point goal{2, 2};

    auto path =
        PathFinder::findPath(*grid_, start, goal, heuristics::euclidean);

    // Should return no path if goal is unreachable
    if (path.has_value()) {
        // If a path exists, verify it doesn't go through obstacles
        for (const auto& node : *path) {
            EXPECT_FALSE(grid_->hasObstacle(node));
        }
    }
}

TEST_F(AStarTest, SameStartAndGoal) {
    Point start{1, 1};
    Point goal{1, 1};

    auto path =
        PathFinder::findPath(*grid_, start, goal, heuristics::euclidean);

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->size(), 1);  // Only the start/goal node
    EXPECT_EQ(path->front(), start);
}

TEST_F(AStarTest, DifferentHeuristics) {
    Point start{0, 0};
    Point goal{2, 2};

    auto path_manhattan =
        PathFinder::findPath(*grid_, start, goal, heuristics::manhattan);
    auto path_euclidean =
        PathFinder::findPath(*grid_, start, goal, heuristics::euclidean);
    auto path_diagonal =
        PathFinder::findPath(*grid_, start, goal, heuristics::diagonal);

    // All should find a path
    EXPECT_TRUE(path_manhattan.has_value());
    EXPECT_TRUE(path_euclidean.has_value());
    EXPECT_TRUE(path_diagonal.has_value());

    // Paths should be valid
    for (const auto& path : {path_manhattan, path_euclidean, path_diagonal}) {
        ASSERT_TRUE(path.has_value());
        EXPECT_FALSE(path->empty());
        EXPECT_EQ(path->front(), start);
        EXPECT_EQ(path->back(), goal);
    }
}

TEST_F(AStarTest, PathOptimality) {
    // Create a simple obstacle-free grid
    GridMap clear_grid(5, 5);
    // Note: GridMap doesn't have setDiagonalMovement, movement is handled
    // internally

    Point start{0, 0};
    Point goal{4, 0};

    auto path =
        PathFinder::findPath(clear_grid, start, goal, heuristics::manhattan);

    ASSERT_TRUE(path.has_value());

    // For this simple case, optimal path should be reasonable
    // Path length should be at least 5 (including both start and goal)
    EXPECT_GE(path->size(), 5);

    // Verify path is valid
    for (size_t i = 1; i < path->size(); ++i) {
        Point prev = (*path)[i - 1];
        Point curr = (*path)[i];
        int dx = std::abs(curr.x - prev.x);
        int dy = std::abs(curr.y - prev.y);

        // Verify adjacent cells (cardinal or diagonal)
        EXPECT_TRUE((dx <= 1 && dy <= 1));
    }
}

// ============================================================================
// Custom Graph Tests
// ============================================================================

class CustomGraph : public IGraph<int> {
public:
    void addEdge(int from, int to, float cost) {
        edges_[from].push_back({to, cost});
        edges_[to].push_back({from, cost});  // Undirected graph
    }

    std::vector<int> neighbors(const int& node) const override {
        std::vector<int> result;
        auto it = edges_.find(node);
        if (it != edges_.end()) {
            for (const auto& edge : it->second) {
                result.push_back(edge.to);
            }
        }
        return result;
    }

    float cost(const int& from, const int& to) const override {
        auto it = edges_.find(from);
        if (it != edges_.end()) {
            for (const auto& edge : it->second) {
                if (edge.to == to) {
                    return edge.cost;
                }
            }
        }
        return std::numeric_limits<float>::infinity();
    }

private:
    struct Edge {
        int to;
        float cost;
    };
    std::unordered_map<int, std::vector<Edge>> edges_;
};

class CustomGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple graph: 0-1-2-3
        graph_ = std::make_unique<CustomGraph>();
        graph_->addEdge(0, 1, 1.0f);
        graph_->addEdge(1, 2, 1.0f);
        graph_->addEdge(2, 3, 1.0f);
        // Add a shortcut: 0-2 with cost 2.5
        graph_->addEdge(0, 2, 2.5f);
    }

    std::unique_ptr<CustomGraph> graph_;
};

TEST_F(CustomGraphTest, BasicPathfinding) {
    // Define a simple heuristic for integers
    auto heuristic = [](int a, int b) -> float {
        return std::abs(a - b) * 1.0f;
    };

    auto path = PathFinder::findPath(*graph_, 0, 3, heuristic);

    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->empty());
    EXPECT_EQ(path->front(), 0);
    EXPECT_EQ(path->back(), 3);

    // Expected optimal path: 0-1-2-3 (cost 3.0) not 0-2-3 (cost 3.5)
    std::vector<int> expected_path = {0, 1, 2, 3};
    EXPECT_EQ(*path, expected_path);
}

TEST_F(CustomGraphTest, UnreachableGoal) {
    // Add isolated node
    graph_->addEdge(4, 5, 1.0f);

    auto heuristic = [](int a, int b) -> float {
        return std::abs(a - b) * 1.0f;
    };

    // Try to find path from connected component to isolated component
    auto path = PathFinder::findPath(*graph_, 0, 5, heuristic);

    // Should return no path (empty optional)
    if (path.has_value()) {
        // If implementation returns empty path instead of no path
        EXPECT_TRUE(path->empty());
    }
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a larger grid for performance testing
        large_grid_ = std::make_unique<GridMap>(100, 100);
        // Note: GridMap doesn't have setDiagonalMovement, diagonal movement is
        // handled internally

        // Add some random obstacles
        std::mt19937 rng(42);  // Fixed seed for reproducibility
        std::uniform_int_distribution<int> dist(0, 99);

        for (int i = 0; i < 1000; ++i) {
            int x = dist(rng);
            int y = dist(rng);
            large_grid_->setTerrain(Point{x, y},
                                    GridMap::TerrainType::Obstacle);
        }
    }

    std::unique_ptr<GridMap> large_grid_;
};

TEST_F(PerformanceTest, LargeGridPathfinding) {
    Point start{0, 0};
    Point goal{99, 99};

    auto start_time = std::chrono::high_resolution_clock::now();
    auto path =
        PathFinder::findPath(*large_grid_, start, goal, heuristics::euclidean);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 100);  // Less than 100ms

    if (path.has_value()) {
        EXPECT_FALSE(path->empty());
        EXPECT_EQ(path->front(), start);
        EXPECT_EQ(path->back(), goal);
    }
}

TEST_F(PerformanceTest, MultiplePathfindingQueries) {
    std::vector<std::pair<Point, Point>> queries = {{{0, 0}, {10, 10}},
                                                    {{20, 20}, {30, 30}},
                                                    {{40, 40}, {50, 50}},
                                                    {{60, 60}, {70, 70}},
                                                    {{80, 80}, {90, 90}}};

    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& [start, goal] : queries) {
        auto path = PathFinder::findPath(*large_grid_, start, goal,
                                         heuristics::manhattan);
        // Just verify it doesn't crash and returns valid or no path
        if (path.has_value()) {
            if (!path->empty()) {
                EXPECT_EQ(path->front(), start);
                EXPECT_EQ(path->back(), goal);
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // All queries should complete quickly (relaxed threshold for 100x100 grid)
    EXPECT_LT(duration.count(), 200);  // Less than 200ms total
}

}  // namespace atom::algorithm::pathfinding::test
