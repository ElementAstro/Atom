#include <gtest/gtest.h>

#include "atom/algorithm/flood.hpp"

class FloodFillTest : public ::testing::Test {
protected:
    std::vector<std::vector<int>> grid;
    void SetUp() override {
        grid = {
            {1, 1, 1, 1, 1},
            {1, 2, 2, 2, 1},
            {1, 2, 3, 2, 1},
            {1, 2, 2, 2, 1},
            {1, 1, 1, 1, 1}
        };
    }
};

// Test fillBFS with 4-way connectivity
TEST_F(FloodFillTest, FillBFS4Way) {
    FloodFill::fillBFS(grid, 2, 2, 3, 9, Connectivity::Four);
    EXPECT_EQ(grid[2][2], 9);
    EXPECT_EQ(grid[1][2], 2);
    EXPECT_EQ(grid[3][2], 2);
}

// Test fillBFS with 8-way connectivity
TEST_F(FloodFillTest, FillBFS8Way) {
    FloodFill::fillBFS(grid, 2, 2, 3, 9, Connectivity::Eight);
    EXPECT_EQ(grid[2][2], 9);
    EXPECT_EQ(grid[1][2], 2);
    EXPECT_EQ(grid[3][2], 2);
}

// Test fillDFS with 4-way connectivity
TEST_F(FloodFillTest, FillDFS4Way) {
    FloodFill::fillDFS(grid, 2, 2, 3, 9, Connectivity::Four);
    EXPECT_EQ(grid[2][2], 9);
    EXPECT_EQ(grid[1][2], 2);
    EXPECT_EQ(grid[3][2], 2);
}

// Test fillDFS with 8-way connectivity
TEST_F(FloodFillTest, FillDFS8Way) {
    FloodFill::fillDFS(grid, 2, 2, 3, 9, Connectivity::Eight);
    EXPECT_EQ(grid[2][2], 9);
    EXPECT_EQ(grid[1][2], 2);
    EXPECT_EQ(grid[3][2], 2);
}

// Test edge case: starting point already filled
TEST_F(FloodFillTest, FillBFSAlreadyFilled) {
    FloodFill::fillBFS(grid, 2, 2, 9, 9, Connectivity::Four);
    EXPECT_EQ(grid[2][2], 3);
}

// Test edge case: target color not present
TEST_F(FloodFillTest, FillBFSTargetColorNotPresent) {
    FloodFill::fillBFS(grid, 0, 0, 4, 9, Connectivity::Four);
    EXPECT_EQ(grid[0][0], 1);
}

// Test edge case: fill entire grid
TEST_F(FloodFillTest, FillBFSFillEntireGrid) {
    FloodFill::fillBFS(grid, 0, 0, 1, 9, Connectivity::Four);
    for (const auto& row : grid) {
        for (const auto& cell : row) {
            EXPECT_EQ(cell, 9);
        }
    }
}