#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <vector>
#include "atom/algorithm/pathfinding.hpp"

using namespace atom::algorithm;

class PathfindingTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    // Helper to create a simple 5x5 grid with some obstacles
    GridMap createSimpleGrid() {
        GridMap map(5, 5);
        // Add some obstacles
        map.setObstacle({1, 1}, true);
        map.setObstacle({1, 2}, true);
        map.setObstacle({2, 1}, true);
        return map;
    }

    // Helper to create a maze-like grid
    GridMap createMazeGrid() {
        GridMap map(10, 10);
        // Create a simple maze pattern
        for (int i = 1; i < 9; i += 2) {
            for (int j = 1; j < 9; j += 2) {
                map.setObstacle({i, j}, true);
            }
        }
        return map;
    }

    // Helper to verify path validity
    bool isValidPath(const std::vector<Point>& path, const GridMap& map,
                     const Point& start, const Point& goal,
                     bool allowLargeJumps = false) {
        if (path.empty())
            return false;
        if (path.front() != start || path.back() != goal)
            return false;

        for (const auto& point : path) {
            if (!map.isValid(point) || map.hasObstacle(point)) {
                return false;
            }
        }

        // Check path continuity (more lenient for smoothed paths)
        if (!allowLargeJumps) {
            for (size_t i = 1; i < path.size(); ++i) {
                auto diff = path[i] - path[i - 1];
                int distance = std::abs(diff.x) + std::abs(diff.y);
                if (distance > 2)
                    return false;  // Allow diagonal movement
            }
        }

        return true;
    }
};

// Test Point structure
TEST_F(PathfindingTest, PointOperations) {
    Point p1{3, 4};
    Point p2{1, 2};

    auto sum = p1 + p2;
    EXPECT_EQ(sum.x, 4);
    EXPECT_EQ(sum.y, 6);

    auto diff = p1 - p2;
    EXPECT_EQ(diff.x, 2);
    EXPECT_EQ(diff.y, 2);

    EXPECT_TRUE(p1 == p1);
    EXPECT_FALSE(p1 == p2);
}

// Test GridMap basic functionality
TEST_F(PathfindingTest, GridMapBasics) {
    GridMap map(10, 10);

    EXPECT_EQ(map.getWidth(), 10);
    EXPECT_EQ(map.getHeight(), 10);

    Point valid{5, 5};
    Point invalid{15, 15};

    EXPECT_TRUE(map.isValid(valid));
    EXPECT_FALSE(map.isValid(invalid));

    EXPECT_FALSE(map.hasObstacle(valid));
    map.setObstacle(valid, true);
    EXPECT_TRUE(map.hasObstacle(valid));
}

// Test GridMap neighbors
TEST_F(PathfindingTest, GridMapNeighbors) {
    GridMap map(5, 5);
    Point center{2, 2};

    auto neighbors = map.neighbors(center);
    EXPECT_EQ(neighbors.size(), 8);  // 8-directional movement

    // Test corner case
    Point corner{0, 0};
    auto cornerNeighbors = map.neighbors(corner);
    EXPECT_EQ(cornerNeighbors.size(), 3);  // Only 3 valid neighbors
}

// Test heuristic functions
TEST_F(PathfindingTest, HeuristicFunctions) {
    Point a{0, 0};
    Point b{3, 4};

    EXPECT_FLOAT_EQ(heuristics::manhattan(a, b), 7.0f);
    EXPECT_FLOAT_EQ(heuristics::euclidean(a, b), 5.0f);
    EXPECT_FLOAT_EQ(heuristics::zero(a, b), 0.0f);

    // Diagonal heuristic (Chebyshev) should be less than or equal to Euclidean
    // and Manhattan
    float diagonal = heuristics::diagonal(a, b);
    EXPECT_LE(diagonal, heuristics::euclidean(a, b));
    EXPECT_LE(diagonal, heuristics::manhattan(a, b));
}

// Test A* pathfinding
TEST_F(PathfindingTest, AStarBasicPath) {
    auto map = createSimpleGrid();
    Point start{0, 0};
    Point goal{4, 4};

    auto path = PathFinder::findPath(map, start, goal, heuristics::manhattan);

    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(isValidPath(*path, map, start, goal));
    EXPECT_GT(path->size(), 1);
}

// Test A* with no path available
TEST_F(PathfindingTest, AStarNoPath) {
    GridMap map(5, 5);
    // Create a wall that blocks the path
    for (int i = 0; i < 5; ++i) {
        map.setObstacle({2, i}, true);
    }

    Point start{0, 0};
    Point goal{4, 4};

    auto path = PathFinder::findPath(map, start, goal, heuristics::manhattan);
    EXPECT_FALSE(path.has_value());
}

// Test Dijkstra pathfinding
TEST_F(PathfindingTest, DijkstraPath) {
    auto map = createSimpleGrid();
    Point start{0, 0};
    Point goal{4, 4};

    auto path = PathFinder::findPath(map, start, goal);

    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(isValidPath(*path, map, start, goal));
}

// Test bidirectional search
TEST_F(PathfindingTest, BidirectionalSearch) {
    auto map = createSimpleGrid();
    Point start{0, 0};
    Point goal{4, 4};

    auto path = PathFinder::findBidirectionalPath(map, start, goal,
                                                  heuristics::manhattan);

    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(isValidPath(*path, map, start, goal));
}

// Test grid path convenience function
TEST_F(PathfindingTest, GridPathConvenience) {
    auto map = createSimpleGrid();
    Point start{0, 0};
    Point goal{4, 4};

    // Test different heuristics
    auto pathManhattan = PathFinder::findGridPath(
        map, start, goal, PathFinder::HeuristicType::Manhattan);
    auto pathEuclidean = PathFinder::findGridPath(
        map, start, goal, PathFinder::HeuristicType::Euclidean);

    ASSERT_TRUE(pathManhattan.has_value());
    ASSERT_TRUE(pathEuclidean.has_value());

    EXPECT_TRUE(isValidPath(*pathManhattan, map, start, goal));
    EXPECT_TRUE(isValidPath(*pathEuclidean, map, start, goal));
}

// Test terrain types
TEST_F(PathfindingTest, TerrainTypes) {
    GridMap map(5, 5);
    Point p{2, 2};

    EXPECT_EQ(map.getTerrain(p), GridMap::TerrainType::Open);

    map.setTerrain(p, GridMap::TerrainType::Difficult);
    EXPECT_EQ(map.getTerrain(p), GridMap::TerrainType::Difficult);

    // Test terrain costs
    EXPECT_GT(map.getTerrainCost(GridMap::TerrainType::Difficult),
              map.getTerrainCost(GridMap::TerrainType::Open));
}

// Test path smoothing
TEST_F(PathfindingTest, PathSmoothing) {
    auto map = createSimpleGrid();
    Point start{0, 0};
    Point goal{4, 4};

    auto path = PathFinder::findGridPath(map, start, goal);
    ASSERT_TRUE(path.has_value());

    auto smoothedPath = PathFinder::smoothPath(*path, map);

    // Smoothed path should be valid and potentially shorter
    EXPECT_TRUE(isValidPath(smoothedPath, map, start, goal,
                            true));  // Allow large jumps for smoothed paths
    EXPECT_LE(smoothedPath.size(), path->size());
}

// Performance test
TEST_F(PathfindingTest, PerformanceTest) {
    GridMap largeMap(100, 100);

    // Add some random obstacles
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);

    for (int i = 0; i < 500; ++i) {
        Point obstacle{dis(gen), dis(gen)};
        largeMap.setObstacle(obstacle, true);
    }

    Point start{0, 0};
    Point goal{99, 99};

    auto startTime = std::chrono::high_resolution_clock::now();
    auto path = PathFinder::findGridPath(largeMap, start, goal);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    spdlog::info("Pathfinding on 100x100 grid took {} ms", duration.count());

    if (path.has_value()) {
        EXPECT_TRUE(isValidPath(*path, largeMap, start, goal));
    }
}

// Test edge cases
TEST_F(PathfindingTest, EdgeCases) {
    GridMap map(3, 3);

    // Same start and goal
    Point same{1, 1};
    auto path = PathFinder::findGridPath(map, same, same);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->size(), 1);
    EXPECT_EQ((*path)[0], same);

    // Adjacent points
    Point start{0, 0};
    Point adjacent{0, 1};
    auto adjacentPath = PathFinder::findGridPath(map, start, adjacent);
    ASSERT_TRUE(adjacentPath.has_value());
    EXPECT_EQ(adjacentPath->size(), 2);
}
