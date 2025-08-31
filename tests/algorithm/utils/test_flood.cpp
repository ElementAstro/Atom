#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "atom/algorithm/flood.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper functions to create and debug test grids
void printGrid(const std::vector<std::vector<int>>& grid) {
    std::cout << "Grid:" << std::endl;
    for (const auto& row : grid) {
        for (int val : row) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

std::vector<std::vector<int>> createGrid(int rows, int cols,
                                         int defaultValue = 0) {
    return std::vector<std::vector<int>>(rows,
                                         std::vector<int>(cols, defaultValue));
}

// Helper to count occurrences of a value in grid
int countOccurrences(const std::vector<std::vector<int>>& grid, int value) {
    int count = 0;
    for (const auto& row : grid) {
        count += std::count(row.begin(), row.end(), value);
    }
    return count;
}

// Create a grid with random values
std::vector<std::vector<int>> createRandomGrid(int rows, int cols,
                                               int maxValue = 5) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, maxValue);

    std::vector<std::vector<int>> grid(rows, std::vector<int>(cols));
    for (auto& row : grid) {
        for (auto& cell : row) {
            cell = dis(gen);
        }
    }
    return grid;
}

// Test fixture for FloodFill tests
class FloodFillTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Common test grids
    std::vector<std::vector<int>> createSimpleGrid() {
        return {{1, 1, 1, 1, 1},
                {1, 0, 0, 0, 1},
                {1, 0, 0, 0, 1},
                {1, 0, 0, 0, 1},
                {1, 1, 1, 1, 1}};
    }

    std::vector<std::vector<int>> createIslandGrid() {
        return {{0, 0, 0, 0, 0},
                {0, 1, 1, 1, 0},
                {0, 1, 1, 1, 0},
                {0, 1, 1, 1, 0},
                {0, 0, 0, 0, 0}};
    }

    std::vector<std::vector<int>> createMazeGrid() {
        return {{1, 0, 1, 1, 1},
                {1, 0, 0, 0, 1},
                {1, 1, 1, 0, 1},
                {1, 0, 0, 0, 1},
                {1, 1, 1, 1, 1}};
    }
};

// Basic BFS Fill Tests
TEST_F(FloodFillTest, BFSFillSimple) {
    auto grid = createSimpleGrid();
    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillBFS(grid, 2, 2, targetColor, fillColor);

    // Check that all inner cells (0s) are now filled with 2s
    for (int i = 1; i < 4; ++i) {
        for (int j = 1; j < 4; ++j) {
            EXPECT_EQ(grid[i][j], fillColor)
                << "at position (" << i << ", " << j << ")";
        }
    }

    // Border should remain unchanged
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(grid[0][i], 1);
        EXPECT_EQ(grid[4][i], 1);
        EXPECT_EQ(grid[i][0], 1);
        EXPECT_EQ(grid[i][4], 1);
    }
}

TEST_F(FloodFillTest, BFSFillIsland) {
    auto grid = createIslandGrid();
    int targetColor = 1;
    int fillColor = 2;

    FloodFill::fillBFS(grid, 2, 2, targetColor, fillColor);

    // Check that all island cells (1s) are now filled with 2s
    for (int i = 1; i < 4; ++i) {
        for (int j = 1; j < 4; ++j) {
            EXPECT_EQ(grid[i][j], fillColor)
                << "at position (" << i << ", " << j << ")";
        }
    }

    // Water should remain unchanged
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(grid[0][i], 0);
        EXPECT_EQ(grid[4][i], 0);
        EXPECT_EQ(grid[i][0], 0);
        EXPECT_EQ(grid[i][4], 0);
    }
}

TEST_F(FloodFillTest, BFSFillMaze) {
    auto grid = createMazeGrid();
    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillBFS(grid, 1, 2, targetColor, fillColor);

    // Check that all accessible path cells (0s) are filled
    EXPECT_EQ(grid[1][1], fillColor);
    EXPECT_EQ(grid[1][2], fillColor);
    EXPECT_EQ(grid[1][3], fillColor);
    EXPECT_EQ(grid[3][1], fillColor);
    EXPECT_EQ(grid[3][2], fillColor);
    EXPECT_EQ(grid[3][3], fillColor);

    // Walls should remain unchanged
    EXPECT_EQ(grid[0][0], 1);
    EXPECT_EQ(grid[2][0], 1);
    EXPECT_EQ(grid[2][1], 1);
    EXPECT_EQ(grid[2][2], 1);
}

// Basic DFS Fill Tests
TEST_F(FloodFillTest, DFSFillSimple) {
    auto grid = createSimpleGrid();
    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillDFS(grid, 2, 2, targetColor, fillColor);

    // Check that all inner cells (0s) are now filled with 2s
    for (int i = 1; i < 4; ++i) {
        for (int j = 1; j < 4; ++j) {
            EXPECT_EQ(grid[i][j], fillColor)
                << "at position (" << i << ", " << j << ")";
        }
    }

    // Border should remain unchanged
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(grid[0][i], 1);
        EXPECT_EQ(grid[4][i], 1);
        EXPECT_EQ(grid[i][0], 1);
        EXPECT_EQ(grid[i][4], 1);
    }
}

TEST_F(FloodFillTest, DFSFillIsland) {
    auto grid = createIslandGrid();
    int targetColor = 1;
    int fillColor = 2;

    FloodFill::fillDFS(grid, 2, 2, targetColor, fillColor);

    // Check that all island cells (1s) are now filled with 2s
    for (int i = 1; i < 4; ++i) {
        for (int j = 1; j < 4; ++j) {
            EXPECT_EQ(grid[i][j], fillColor)
                << "at position (" << i << ", " << j << ")";
        }
    }

    // Water should remain unchanged
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(grid[0][i], 0);
        EXPECT_EQ(grid[4][i], 0);
        EXPECT_EQ(grid[i][0], 0);
        EXPECT_EQ(grid[i][4], 0);
    }
}

TEST_F(FloodFillTest, DFSFillMaze) {
    auto grid = createMazeGrid();
    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillDFS(grid, 1, 2, targetColor, fillColor);

    // Check that all accessible path cells (0s) are filled
    EXPECT_EQ(grid[1][1], fillColor);
    EXPECT_EQ(grid[1][2], fillColor);
    EXPECT_EQ(grid[1][3], fillColor);
    EXPECT_EQ(grid[3][1], fillColor);
    EXPECT_EQ(grid[3][2], fillColor);
    EXPECT_EQ(grid[3][3], fillColor);

    // Walls should remain unchanged
    EXPECT_EQ(grid[0][0], 1);
    EXPECT_EQ(grid[2][0], 1);
    EXPECT_EQ(grid[2][1], 1);
    EXPECT_EQ(grid[2][2], 1);
}

// Connectivity Tests
TEST_F(FloodFillTest, BFSFillFourConnectivity) {
    // Create a grid where diagonal connectivity matters
    std::vector<std::vector<int>> grid = {{1, 1, 1, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 0, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 1, 1, 1}};

    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillBFS(grid, 2, 2, targetColor, fillColor, Connectivity::Four);

    // Only center cell should be filled with 4-way connectivity
    EXPECT_EQ(grid[2][2], fillColor);

    // Diagonally adjacent cells should remain unchanged
    EXPECT_EQ(grid[1][1], 0);
    EXPECT_EQ(grid[1][3], 0);
    EXPECT_EQ(grid[3][1], 0);
    EXPECT_EQ(grid[3][3], 0);
}

TEST_F(FloodFillTest, BFSFillEightConnectivity) {
    // Create a grid where diagonal connectivity matters
    std::vector<std::vector<int>> grid = {{1, 1, 1, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 0, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 1, 1, 1}};

    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillBFS(grid, 2, 2, targetColor, fillColor, Connectivity::Eight);

    // Center and diagonally adjacent cells should be filled with 8-way
    // connectivity
    EXPECT_EQ(grid[2][2], fillColor);
    EXPECT_EQ(grid[1][1], fillColor);
    EXPECT_EQ(grid[1][3], fillColor);
    EXPECT_EQ(grid[3][1], fillColor);
    EXPECT_EQ(grid[3][3], fillColor);
}

TEST_F(FloodFillTest, DFSFillFourConnectivity) {
    // Create a grid where diagonal connectivity matters
    std::vector<std::vector<int>> grid = {{1, 1, 1, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 0, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 1, 1, 1}};

    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillDFS(grid, 2, 2, targetColor, fillColor, Connectivity::Four);

    // Only center cell should be filled with 4-way connectivity
    EXPECT_EQ(grid[2][2], fillColor);

    // Diagonally adjacent cells should remain unchanged
    EXPECT_EQ(grid[1][1], 0);
    EXPECT_EQ(grid[1][3], 0);
    EXPECT_EQ(grid[3][1], 0);
    EXPECT_EQ(grid[3][3], 0);
}

TEST_F(FloodFillTest, DFSFillEightConnectivity) {
    // Create a grid where diagonal connectivity matters
    std::vector<std::vector<int>> grid = {{1, 1, 1, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 0, 1, 1},
                                          {1, 0, 1, 0, 1},
                                          {1, 1, 1, 1, 1}};

    int targetColor = 0;
    int fillColor = 2;

    FloodFill::fillDFS(grid, 2, 2, targetColor, fillColor, Connectivity::Eight);

    // Center and diagonally adjacent cells should be filled with 8-way
    // connectivity
    EXPECT_EQ(grid[2][2], fillColor);
    EXPECT_EQ(grid[1][1], fillColor);
    EXPECT_EQ(grid[1][3], fillColor);
    EXPECT_EQ(grid[3][1], fillColor);
    EXPECT_EQ(grid[3][3], fillColor);
}

// Edge cases
TEST_F(FloodFillTest, EmptyGrid) {
    std::vector<std::vector<int>> emptyGrid;

    // Should throw an exception
    EXPECT_THROW(FloodFill::fillBFS(emptyGrid, 0, 0, 0, 1),
                 std::invalid_argument);

    EXPECT_THROW(FloodFill::fillDFS(emptyGrid, 0, 0, 0, 1),
                 std::invalid_argument);
}

TEST_F(FloodFillTest, OutOfBoundsCoordinates) {
    auto grid = createSimpleGrid();

    // Out of bounds starting points should throw
    EXPECT_THROW(FloodFill::fillBFS(grid, -1, 0, 0, 1), std::invalid_argument);

    EXPECT_THROW(FloodFill::fillDFS(grid, 0, -1, 0, 1), std::invalid_argument);

    EXPECT_THROW(FloodFill::fillBFS(grid, 5, 0, 0, 1), std::invalid_argument);

    EXPECT_THROW(FloodFill::fillDFS(grid, 0, 5, 0, 1), std::invalid_argument);
}

TEST_F(FloodFillTest, SameTargetAndFillColor) {
    auto grid = createSimpleGrid();
    auto originalGrid = grid;  // Make a copy for comparison

    // When target and fill colors are the same, no change should occur
    FloodFill::fillBFS(grid, 2, 2, 0, 0);

    // Grid should remain unchanged
    for (size_t i = 0; i < grid.size(); ++i) {
        for (size_t j = 0; j < grid[0].size(); ++j) {
            EXPECT_EQ(grid[i][j], originalGrid[i][j])
                << "at position (" << i << ", " << j << ")";
        }
    }
}

TEST_F(FloodFillTest, StartPositionDoesNotMatchTarget) {
    auto grid = createSimpleGrid();
    auto originalGrid = grid;  // Make a copy for comparison

    // Starting position has color 1, but target color is 0
    FloodFill::fillBFS(grid, 0, 0, 0, 2);

    // Grid should remain unchanged
    for (size_t i = 0; i < grid.size(); ++i) {
        for (size_t j = 0; j < grid[0].size(); ++j) {
            EXPECT_EQ(grid[i][j], originalGrid[i][j])
                << "at position (" << i << ", " << j << ")";
        }
    }
}

TEST_F(FloodFillTest, SingleCellGrid) {
    std::vector<std::vector<int>> singleCellGrid = {{5}};

    FloodFill::fillBFS(singleCellGrid, 0, 0, 5, 10);

    EXPECT_EQ(singleCellGrid[0][0], 10);
}

TEST_F(FloodFillTest, EntireGridSameColor) {
    auto grid = createGrid(5, 5, 1);
    int targetColor = 1;
    int fillColor = 2;

    FloodFill::fillBFS(grid, 2, 2, targetColor, fillColor);

    // The entire grid should now be filled with the fill color
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            EXPECT_EQ(grid[i][j], fillColor)
                << "at position (" << i << ", " << j << ")";
        }
    }
}

// Parallel fill tests
TEST_F(FloodFillTest, ParallelFillSimple) {
    auto grid1 = createSimpleGrid();
    auto grid2 = createSimpleGrid();
    int targetColor = 0;
    int fillColor = 2;

    // Fill one grid using BFS and the other with parallel fill
    FloodFill::fillBFS(grid1, 2, 2, targetColor, fillColor);
    FloodFill::fillParallel(grid2, 2, 2, targetColor, fillColor, FloodFill::FloodFillConfig{});

    // Results should be identical
    for (size_t i = 0; i < grid1.size(); ++i) {
        for (size_t j = 0; j < grid1[0].size(); ++j) {
            EXPECT_EQ(grid1[i][j], grid2[i][j])
                << "at position (" << i << ", " << j << ")";
        }
    }
}

TEST_F(FloodFillTest, ParallelFillLargeGrid) {
    // Create a larger grid to better test parallel fill
    int size = 50;
    auto grid = createGrid(size, size, 0);

    // Create a border
    for (int i = 0; i < size; ++i) {
        grid[0][i] = grid[size - 1][i] = grid[i][0] = grid[i][size - 1] = 1;
    }

    int targetColor = 0;
    int fillColor = 2;

    // Fill using parallel method with different thread counts
    FloodFill::fillParallel(
        grid, size / 2, size / 2, targetColor, fillColor,
        FloodFill::FloodFillConfig{Connectivity::Four, 4});

    // Verify inner area is filled
    for (int i = 1; i < size - 1; ++i) {
        for (int j = 1; j < size - 1; ++j) {
            EXPECT_EQ(grid[i][j], fillColor)
                << "at position (" << i << ", " << j << ")";
        }
    }

    // Verify border is unchanged
    for (int i = 0; i < size; ++i) {
        EXPECT_EQ(grid[0][i], 1) << "at top border position " << i;
        EXPECT_EQ(grid[size - 1][i], 1) << "at bottom border position " << i;
        EXPECT_EQ(grid[i][0], 1) << "at left border position " << i;
        EXPECT_EQ(grid[i][size - 1], 1) << "at right border position " << i;
    }
}

TEST_F(FloodFillTest, ParallelFillWithDifferentThreadCounts) {
    // Create a sizable grid
    int size = 30;
    auto grid = createGrid(size, size, 0);

    // Create a border
    for (int i = 0; i < size; ++i) {
        grid[0][i] = grid[size - 1][i] = grid[i][0] = grid[i][size - 1] = 1;
    }

    int targetColor = 0;
    int fillColor = 2;

    // Test with 1 thread (should be similar to sequential)
    auto grid1 = grid;
    FloodFill::fillParallel(
        grid1, size / 2, size / 2, targetColor, fillColor,
        FloodFill::FloodFillConfig{Connectivity::Four, 1});

    // Test with 2 threads
    auto grid2 = grid;
    FloodFill::fillParallel(
        grid2, size / 2, size / 2, targetColor, fillColor,
        FloodFill::FloodFillConfig{Connectivity::Four, 2});

    // Test with 4 threads
    auto grid4 = grid;
    FloodFill::fillParallel(
        grid4, size / 2, size / 2, targetColor, fillColor,
        FloodFill::FloodFillConfig{Connectivity::Four, 4});

    // Test with default threads
    auto gridDefault = grid;
    FloodFill::fillParallel(
        gridDefault, size / 2, size / 2, targetColor, fillColor,
        FloodFill::FloodFillConfig{});

    // All results should be identical
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            EXPECT_EQ(grid1[i][j], grid2[i][j])
                << "Difference between 1 and 2 threads at (" << i << "," << j
                << ")";
            EXPECT_EQ(grid1[i][j], grid4[i][j])
                << "Difference between 1 and 4 threads at (" << i << "," << j
                << ")";
            EXPECT_EQ(grid1[i][j], gridDefault[i][j])
                << "Difference between 1 and default threads at (" << i << ","
                << j << ")";
        }
    }
}

// Performance tests
TEST_F(FloodFillTest, PerformanceComparisonLargeGrid) {
    // Create a large grid for performance testing
    int size = 500;
    auto grid = createGrid(size, size, 0);

    // Create a border
    for (int i = 0; i < size; ++i) {
        grid[0][i] = grid[size - 1][i] = grid[i][0] = grid[i][size - 1] = 1;
    }

    int targetColor = 0;
    int fillColor = 2;

    // BFS timing
    auto gridBFS = grid;
    auto startBFS = std::chrono::high_resolution_clock::now();
    FloodFill::fillBFS(gridBFS, size / 2, size / 2, targetColor, fillColor);
    auto endBFS = std::chrono::high_resolution_clock::now();
    auto durationBFS =
        std::chrono::duration_cast<std::chrono::milliseconds>(endBFS - startBFS)
            .count();

    // DFS timing
    auto gridDFS = grid;
    auto startDFS = std::chrono::high_resolution_clock::now();
    FloodFill::fillDFS(gridDFS, size / 2, size / 2, targetColor, fillColor);
    auto endDFS = std::chrono::high_resolution_clock::now();
    auto durationDFS =
        std::chrono::duration_cast<std::chrono::milliseconds>(endDFS - startDFS)
            .count();

    // Parallel timing
    auto gridPar = grid;
    auto startPar = std::chrono::high_resolution_clock::now();
    FloodFill::fillParallel(gridPar, size / 2, size / 2, targetColor,
                            fillColor, FloodFill::FloodFillConfig{});
    auto endPar = std::chrono::high_resolution_clock::now();
    auto durationPar =
        std::chrono::duration_cast<std::chrono::milliseconds>(endPar - startPar)
            .count();

    std::cout << "Performance comparison for " << size << "x" << size
              << " grid:" << std::endl;
    std::cout << "BFS: " << durationBFS << "ms" << std::endl;
    std::cout << "DFS: " << durationDFS << "ms" << std::endl;
    std::cout << "Parallel: " << durationPar << "ms" << std::endl;

    // Verify all methods produce the same result
    int filledCountBFS = countOccurrences(gridBFS, fillColor);
    int filledCountDFS = countOccurrences(gridDFS, fillColor);
    int filledCountPar = countOccurrences(gridPar, fillColor);

    EXPECT_EQ(filledCountBFS, filledCountDFS);
    EXPECT_EQ(filledCountBFS, filledCountPar);
    EXPECT_GT(filledCountBFS, 0);  // Sanity check that filling happened
}

// Custom grid type tests
TEST_F(FloodFillTest, CustomGridTypes) {
    // Create a custom grid type with a different underlying type
    std::vector<std::vector<double>> doubleGrid = {{1.0, 1.0, 1.0, 1.0, 1.0},
                                                   {1.0, 0.0, 0.0, 0.0, 1.0},
                                                   {1.0, 0.0, 0.0, 0.0, 1.0},
                                                   {1.0, 0.0, 0.0, 0.0, 1.0},
                                                   {1.0, 1.0, 1.0, 1.0, 1.0}};

    // Using template version directly
    FloodFill::fillBFS<std::vector<std::vector<double>>>(
        doubleGrid, 2, 2, 0.0, 2.0, Connectivity::Four);

    // Check that all inner cells (0.0s) are now filled with 2.0s
    for (int i = 1; i < 4; ++i) {
        for (int j = 1; j < 4; ++j) {
            EXPECT_DOUBLE_EQ(doubleGrid[i][j], 2.0)
                << "at position (" << i << ", " << j << ")";
        }
    }
}

// Integration tests
TEST_F(FloodFillTest, IntegrationTestComplexGrid) {
    // Create a more complex maze-like grid
    std::vector<std::vector<int>> maze = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 0, 0, 0, 1, 0, 0, 0, 0, 1},
        {1, 0, 1, 0, 1, 0, 1, 1, 0, 1}, {1, 0, 1, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 1, 1, 1, 1, 0, 1, 0, 1}, {1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1}, {1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

    int targetColor = 0;
    int fillColor = 2;

    // Make copies for each fill method
    auto mazeBFS = maze;
    auto mazeDFS = maze;
    auto mazeParallel = maze;

    // Apply different fill methods
    FloodFill::fillBFS(mazeBFS, 1, 1, targetColor, fillColor);
    FloodFill::fillDFS(mazeDFS, 1, 1, targetColor, fillColor);
    FloodFill::fillParallel(mazeParallel, 1, 1, targetColor, fillColor, FloodFill::FloodFillConfig{});

    // All methods should produce the same result
    for (size_t i = 0; i < maze.size(); ++i) {
        for (size_t j = 0; j < maze[0].size(); ++j) {
            EXPECT_EQ(mazeBFS[i][j], mazeDFS[i][j])
                << "BFS and DFS differ at (" << i << "," << j << ")";
            EXPECT_EQ(mazeBFS[i][j], mazeParallel[i][j])
                << "BFS and Parallel differ at (" << i << "," << j << ")";
        }
    }

    // Check some specific cells that should be filled
    EXPECT_EQ(mazeBFS[1][1], fillColor);
    EXPECT_EQ(mazeBFS[1][2], fillColor);
    EXPECT_EQ(mazeBFS[1][3], fillColor);

    // Check some cells that should not be filled (walls)
    EXPECT_EQ(mazeBFS[0][0], 1);
    EXPECT_EQ(mazeBFS[2][2], 1);
    EXPECT_EQ(mazeBFS[4][4], 1);
}
