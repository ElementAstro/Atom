#include "atom/algorithm/flood.hpp"

#include <iostream>
#include <vector>

void printGrid(const std::vector<std::vector<int>>& grid) {
    for (const auto& row : grid) {
        for (int cell : row) {
            std::cout << cell << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
    // Example grid
    std::vector<std::vector<int>> grid = {{1, 1, 1, 2, 2},
                                          {1, 1, 0, 2, 2},
                                          {1, 0, 0, 2, 2},
                                          {1, 1, 0, 0, 0},
                                          {1, 1, 1, 1, 0}};

    std::cout << "Original grid:" << std::endl;
    printGrid(grid);

    // Perform flood fill using BFS with 4-way connectivity
    FloodFill::fillBFS(grid, 1, 1, 1, 3, Connectivity::Four);
    std::cout << "\nGrid after BFS flood fill (4-way connectivity):"
              << std::endl;
    printGrid(grid);

    // Reset the grid to original state
    grid = {{1, 1, 1, 2, 2},
            {1, 1, 0, 2, 2},
            {1, 0, 0, 2, 2},
            {1, 1, 0, 0, 0},
            {1, 1, 1, 1, 0}};

    // Perform flood fill using BFS with 8-way connectivity
    FloodFill::fillBFS(grid, 1, 1, 1, 3, Connectivity::Eight);
    std::cout << "\nGrid after BFS flood fill (8-way connectivity):"
              << std::endl;
    printGrid(grid);

    // Reset the grid to original state
    grid = {{1, 1, 1, 2, 2},
            {1, 1, 0, 2, 2},
            {1, 0, 0, 2, 2},
            {1, 1, 0, 0, 0},
            {1, 1, 1, 1, 0}};

    // Perform flood fill using DFS with 4-way connectivity
    FloodFill::fillDFS(grid, 1, 1, 1, 3, Connectivity::Four);
    std::cout << "\nGrid after DFS flood fill (4-way connectivity):"
              << std::endl;
    printGrid(grid);

    // Reset the grid to original state
    grid = {{1, 1, 1, 2, 2},
            {1, 1, 0, 2, 2},
            {1, 0, 0, 2, 2},
            {1, 1, 0, 0, 0},
            {1, 1, 1, 1, 0}};

    // Perform flood fill using DFS with 8-way connectivity
    FloodFill::fillDFS(grid, 1, 1, 1, 3, Connectivity::Eight);
    std::cout << "\nGrid after DFS flood fill (8-way connectivity):"
              << std::endl;
    printGrid(grid);

    return 0;
}