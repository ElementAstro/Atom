#include "atom/algorithm/flood.hpp"

#include <chrono>
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
    size_t filledCells = atom::algorithm::FloodFill::fillBFS(
        grid, 1, 1, 1, 3, Connectivity::Four);
    std::cout << "\nGrid after BFS flood fill (4-way connectivity):"
              << std::endl;
    printGrid(grid);
    std::cout << "Filled " << filledCells << " cells" << std::endl;

    // Reset the grid to original state
    grid = {{1, 1, 1, 2, 2},
            {1, 1, 0, 2, 2},
            {1, 0, 0, 2, 2},
            {1, 1, 0, 0, 0},
            {1, 1, 1, 1, 0}};

    // Perform flood fill using BFS with 8-way connectivity
    filledCells = atom::algorithm::FloodFill::fillBFS(grid, 1, 1, 1, 3,
                                                      Connectivity::Eight);
    std::cout << "\nGrid after BFS flood fill (8-way connectivity):"
              << std::endl;
    printGrid(grid);
    std::cout << "Filled " << filledCells << " cells" << std::endl;

    // Reset the grid to original state
    grid = {{1, 1, 1, 2, 2},
            {1, 1, 0, 2, 2},
            {1, 0, 0, 2, 2},
            {1, 1, 0, 0, 0},
            {1, 1, 1, 1, 0}};

    // Perform flood fill using DFS with 4-way connectivity
    filledCells = atom::algorithm::FloodFill::fillDFS(grid, 1, 1, 1, 3,
                                                      Connectivity::Four);
    std::cout << "\nGrid after DFS flood fill (4-way connectivity):"
              << std::endl;
    printGrid(grid);
    std::cout << "Filled " << filledCells << " cells" << std::endl;

    // Reset the grid to original state
    grid = {{1, 1, 1, 2, 2},
            {1, 1, 0, 2, 2},
            {1, 0, 0, 2, 2},
            {1, 1, 0, 0, 0},
            {1, 1, 1, 1, 0}};

    // Perform flood fill using DFS with 8-way connectivity
    filledCells = atom::algorithm::FloodFill::fillDFS(grid, 1, 1, 1, 3,
                                                      Connectivity::Eight);
    std::cout << "\nGrid after DFS flood fill (8-way connectivity):"
              << std::endl;
    printGrid(grid);
    std::cout << "Filled " << filledCells << " cells" << std::endl;

    std::cout << "\n=== Advanced Flood Fill Examples ===" << std::endl;

    // Example using FloodFillConfig for parallel processing
    {
        std::vector<std::vector<int>> largeGrid(20, std::vector<int>(20, 1));
        // Create some patterns
        for (int i = 5; i < 15; ++i) {
            for (int j = 5; j < 15; ++j) {
                largeGrid[i][j] = 2;
            }
        }

        std::cout << "\nTesting parallel flood fill on larger grid..."
                  << std::endl;

        atom::algorithm::FloodFill::FloodFillConfig config;
        config.connectivity = Connectivity::Four;
        config.numThreads = 4;
        config.useSIMD = true;
        config.useBlockProcessing = true;
        config.blockSize = 8;

        size_t parallelFilledCells = atom::algorithm::FloodFill::fillParallel(
            largeGrid, 7, 7, 2, 9, config);

        std::cout << "Filled " << parallelFilledCells
                  << " cells using parallel algorithm" << std::endl;
    }

    // Example using SIMD-accelerated flood fill
    {
        std::vector<std::vector<int>> simdGrid = {{1, 1, 1, 1, 1},
                                                  {1, 2, 2, 2, 1},
                                                  {1, 2, 1, 2, 1},
                                                  {1, 2, 2, 2, 1},
                                                  {1, 1, 1, 1, 1}};

        std::cout << "\nOriginal SIMD test grid:" << std::endl;
        printGrid(simdGrid);

        atom::algorithm::FloodFill::FloodFillConfig simdConfig;
        simdConfig.connectivity = Connectivity::Eight;
        simdConfig.useSIMD = true;

        try {
            size_t simdFilledCells = atom::algorithm::FloodFill::fillSIMD(
                simdGrid, 1, 1, 2, 8, simdConfig);

            std::cout << "\nGrid after SIMD flood fill:" << std::endl;
            printGrid(simdGrid);
            std::cout << "Filled " << simdFilledCells
                      << " cells using SIMD algorithm" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "SIMD flood fill not supported: " << e.what()
                      << std::endl;
            // Fallback to regular BFS
            size_t fallbackFilledCells = atom::algorithm::FloodFill::fillBFS(
                simdGrid, 1, 1, 2, 8, Connectivity::Eight);
            std::cout << "\nUsed fallback BFS algorithm instead" << std::endl;
            printGrid(simdGrid);
            std::cout << "Filled " << fallbackFilledCells
                      << " cells using fallback BFS" << std::endl;
        }
    }

    // Example using block-optimized flood fill
    {
        std::vector<std::vector<int>> blockGrid(16, std::vector<int>(16, 1));
        // Create a checkerboard pattern
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                if ((i + j) % 2 == 0) {
                    blockGrid[i][j] = 3;
                }
            }
        }

        std::cout << "\nTesting block-optimized flood fill..." << std::endl;

        atom::algorithm::FloodFill::FloodFillConfig blockConfig;
        blockConfig.connectivity = Connectivity::Four;
        blockConfig.useBlockProcessing = true;
        blockConfig.blockSize = 4;

        size_t blockFilledCells =
            atom::algorithm::FloodFill::fillBlockOptimized(blockGrid, 0, 0, 3,
                                                           7, blockConfig);

        std::cout << "Filled " << blockFilledCells
                  << " cells using block-optimized algorithm" << std::endl;
    }

    // Example demonstrating different connectivity types
    {
        std::vector<std::vector<int>> connectivityGrid = {{1, 0, 0, 1, 1},
                                                          {0, 1, 0, 1, 0},
                                                          {0, 0, 1, 0, 0},
                                                          {1, 0, 0, 1, 0},
                                                          {1, 1, 0, 0, 1}};

        std::cout << "\nOriginal connectivity test grid:" << std::endl;
        printGrid(connectivityGrid);

        // Test 4-way connectivity
        auto grid4Way = connectivityGrid;
        size_t cells4Way = atom::algorithm::FloodFill::fillBFS(
            grid4Way, 0, 0, 1, 8, Connectivity::Four);
        std::cout << "\nAfter 4-way connectivity flood fill:" << std::endl;
        printGrid(grid4Way);
        std::cout << "Filled " << cells4Way << " cells with 4-way connectivity"
                  << std::endl;

        // Test 8-way connectivity
        auto grid8Way = connectivityGrid;
        size_t cells8Way = atom::algorithm::FloodFill::fillBFS(
            grid8Way, 0, 0, 1, 9, Connectivity::Eight);
        std::cout << "\nAfter 8-way connectivity flood fill:" << std::endl;
        printGrid(grid8Way);
        std::cout << "Filled " << cells8Way << " cells with 8-way connectivity"
                  << std::endl;
    }

    // Example demonstrating error handling
    {
        std::vector<std::vector<int>> errorGrid = {{1, 2, 3}};

        std::cout << "\nTesting error handling..." << std::endl;

        try {
            // This should work fine
            size_t validCells = atom::algorithm::FloodFill::fillBFS(
                errorGrid, 0, 0, 1, 5, Connectivity::Four);
            std::cout << "Valid operation completed successfully, filled "
                      << validCells << " cells" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Unexpected error: " << e.what() << std::endl;
        }

        try {
            // This should throw an exception (out of bounds)
            size_t invalidCells = atom::algorithm::FloodFill::fillBFS(
                errorGrid, 5, 5, 1, 5, Connectivity::Four);
            std::cout << "Unexpectedly succeeded, filled " << invalidCells
                      << " cells" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Caught expected error for out-of-bounds coordinates: "
                      << e.what() << std::endl;
        }

        try {
            // Test with empty grid
            std::vector<std::vector<int>> emptyGrid;
            size_t emptyCells = atom::algorithm::FloodFill::fillBFS(
                emptyGrid, 0, 0, 1, 5, Connectivity::Four);
            std::cout << "Unexpectedly succeeded on empty grid, filled "
                      << emptyCells << " cells" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Caught expected error for empty grid: " << e.what()
                      << std::endl;
        }
    }

    // Performance comparison example
    {
        std::cout << "\n=== Performance Comparison ===" << std::endl;

        // Create a large grid for performance testing
        const int gridSize = 100;
        std::vector<std::vector<int>> perfGrid(gridSize,
                                               std::vector<int>(gridSize, 1));

        // Fill half the grid with target color
        for (int i = 0; i < gridSize / 2; ++i) {
            for (int j = 0; j < gridSize; ++j) {
                perfGrid[i][j] = 2;
            }
        }

        auto testBFS = perfGrid;
        auto testDFS = perfGrid;
        auto testParallel = perfGrid;

        auto start = std::chrono::high_resolution_clock::now();
        size_t bfsResult = atom::algorithm::FloodFill::fillBFS(
            testBFS, 0, 0, 2, 7, Connectivity::Four);
        auto bfsTime = std::chrono::high_resolution_clock::now() - start;

        start = std::chrono::high_resolution_clock::now();
        size_t dfsResult = atom::algorithm::FloodFill::fillDFS(
            testDFS, 0, 0, 2, 7, Connectivity::Four);
        auto dfsTime = std::chrono::high_resolution_clock::now() - start;

        atom::algorithm::FloodFill::FloodFillConfig parallelConfig;
        parallelConfig.numThreads = 4;

        start = std::chrono::high_resolution_clock::now();
        size_t parallelResult = atom::algorithm::FloodFill::fillParallel(
            testParallel, 0, 0, 2, 7, parallelConfig);
        auto parallelTime = std::chrono::high_resolution_clock::now() - start;

        std::cout << "BFS filled " << bfsResult << " cells in "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         bfsTime)
                         .count()
                  << " microseconds" << std::endl;
        std::cout << "DFS filled " << dfsResult << " cells in "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         dfsTime)
                         .count()
                  << " microseconds" << std::endl;
        std::cout << "Parallel filled " << parallelResult << " cells in "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         parallelTime)
                         .count()
                  << " microseconds" << std::endl;

        // Verify all methods filled the same number of cells
        if (bfsResult == dfsResult && dfsResult == parallelResult) {
            std::cout << "All algorithms filled the same number of cells - "
                         "verification passed!"
                      << std::endl;
        } else {
            std::cout << "Warning: Different algorithms filled different "
                         "numbers of cells!"
                      << std::endl;
        }
    }

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Flood fill demonstration completed successfully!"
              << std::endl;
    std::cout << "Demonstrated features:" << std::endl;
    std::cout << "- BFS and DFS flood fill algorithms" << std::endl;
    std::cout << "- 4-way and 8-way connectivity" << std::endl;
    std::cout << "- Parallel processing with configurable options" << std::endl;
    std::cout << "- SIMD acceleration (if supported)" << std::endl;
    std::cout << "- Block-optimized processing" << std::endl;
    std::cout << "- Error handling and bounds checking" << std::endl;
    std::cout << "- Performance comparison between algorithms" << std::endl;

    return 0;
}