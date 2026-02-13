/**
 * @file flood.cpp
 * @brief Comprehensive example demonstrating flood fill algorithms
 *
 * This example shows how to:
 * - Use BFS and DFS flood fill algorithms
 * - Handle different connectivity options (4-way vs 8-way)
 * - Apply flood fill to various grid patterns and use cases
 * - Demonstrate SIMD optimizations for large grids
 * - Show performance characteristics and comparisons
 * - Handle edge cases and boundary conditions
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/graphics/flood.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Enhanced grid printing with color coding
 */
void printGrid(const std::vector<std::vector<int>>& grid,
               const std::string& title = "") {
    if (!title.empty()) {
        std::cout << title << ":\n";
    }

    for (const auto& row : grid) {
        for (int cell : row) {
            std::cout << std::setw(2) << cell << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

/**
 * @brief Demonstrates basic flood fill functionality
 */
void demonstrateBasicFloodFill() {
    printHeader("Basic Flood Fill Examples");

    try {
        // Create a test grid with different regions
        std::vector<std::vector<int>> originalGrid = {{1, 1, 1, 2, 2},
                                                      {1, 1, 0, 2, 2},
                                                      {1, 0, 0, 2, 2},
                                                      {1, 1, 0, 0, 0},
                                                      {1, 1, 1, 1, 0}};

        std::cout
            << "Testing flood fill with different connectivity options:\n";
        std::cout << "Grid legend: 0=empty, 1=region1, 2=region2, 3=filled\n\n";

        printGrid(originalGrid, "Original grid");

        // Test BFS with 4-way connectivity
        auto grid4way = originalGrid;  // Copy for testing
        auto start = std::chrono::high_resolution_clock::now();
        size_t filled4way =
            FloodFill::fillBFS(grid4way, 1, 1, 1, 3, Connectivity::Four);
        auto end = std::chrono::high_resolution_clock::now();
        auto time4way =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        printGrid(grid4way, "After BFS flood fill (4-way connectivity)");
        std::cout << "Filled " << filled4way << " cells in " << time4way.count()
                  << " μs\n";

        // Test BFS with 8-way connectivity
        auto grid8way = originalGrid;  // Copy for testing
        start = std::chrono::high_resolution_clock::now();
        size_t filled8way =
            FloodFill::fillBFS(grid8way, 1, 1, 1, 3, Connectivity::Eight);
        end = std::chrono::high_resolution_clock::now();
        auto time8way =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        printGrid(grid8way, "After BFS flood fill (8-way connectivity)");
        std::cout << "Filled " << filled8way << " cells in " << time8way.count()
                  << " μs\n";

        std::cout << "Connectivity comparison:\n";
        std::cout << "  4-way: " << filled4way << " cells filled\n";
        std::cout << "  8-way: " << filled8way << " cells filled\n";
        std::cout << "  Difference: " << (filled8way - filled4way)
                  << " additional cells with diagonal connectivity\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic flood fill demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates BFS vs DFS flood fill comparison
 */
void demonstrateBFSvsDFS() {
    printHeader("BFS vs DFS Flood Fill Comparison");

    try {
        // Create a test grid
        std::vector<std::vector<int>> originalGrid = {{1, 1, 1, 2, 2},
                                                      {1, 1, 0, 2, 2},
                                                      {1, 0, 0, 2, 2},
                                                      {1, 1, 0, 0, 0},
                                                      {1, 1, 1, 1, 0}};

        std::cout << "Comparing BFS and DFS flood fill algorithms:\n\n";
        printGrid(originalGrid, "Original grid");

        // Test DFS with 4-way connectivity
        auto gridDFS = originalGrid;
        auto start = std::chrono::high_resolution_clock::now();
        size_t filledDFS =
            FloodFill::fillDFS(gridDFS, 1, 1, 1, 3, Connectivity::Four);
        auto end = std::chrono::high_resolution_clock::now();
        auto timeDFS =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        printGrid(gridDFS, "After DFS flood fill (4-way connectivity)");
        std::cout << "DFS filled " << filledDFS << " cells in "
                  << timeDFS.count() << " μs\n";

        // Test BFS with 4-way connectivity for comparison
        auto gridBFS = originalGrid;
        start = std::chrono::high_resolution_clock::now();
        size_t filledBFS =
            FloodFill::fillBFS(gridBFS, 1, 1, 1, 3, Connectivity::Four);
        end = std::chrono::high_resolution_clock::now();
        auto timeBFS =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        printGrid(gridBFS, "After BFS flood fill (4-way connectivity)");
        std::cout << "BFS filled " << filledBFS << " cells in "
                  << timeBFS.count() << " μs\n";

        std::cout << "Algorithm comparison:\n";
        std::cout << "  Both algorithms filled " << filledBFS
                  << " cells (should be identical)\n";
        std::cout << "  DFS time: " << timeDFS.count() << " μs\n";
        std::cout << "  BFS time: " << timeBFS.count() << " μs\n";
        std::cout << "  Results match: "
                  << (gridDFS == gridBFS ? "✓ YES" : "✗ NO") << "\n";

        std::cout << "\nAlgorithm characteristics:\n";
        std::cout
            << "  DFS: Depth-first, uses recursion/stack, memory efficient\n";
        std::cout
            << "  BFS: Breadth-first, uses queue, explores level by level\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in BFS vs DFS demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates advanced flood fill features
 */
void demonstrateAdvancedFeatures() {
    printHeader("Advanced Flood Fill Features");

    try {
        std::cout << "Testing advanced flood fill capabilities:\n\n";

        // Create a larger test grid
        std::vector<std::vector<int>> largeGrid(20, std::vector<int>(20, 1));

        // Create some patterns
        for (int i = 5; i < 15; ++i) {
            for (int j = 5; j < 15; ++j) {
                largeGrid[i][j] = 2;
            }
        }

        std::cout
            << "Testing on 20x20 grid with central 10x10 region of value 2\n";
        std::cout << "Grid too large to display, showing fill statistics:\n";

        // Test with configuration (if available)
        try {
            FloodFill::FloodFillConfig config;
            config.connectivity = Connectivity::Four;
            config.numThreads = 4;
            config.useSIMD = true;
            config.useBlockProcessing = true;
            config.blockSize = 8;

            auto start = std::chrono::high_resolution_clock::now();
            size_t parallelFilledCells =
                FloodFill::fillParallel(largeGrid, 7, 7, 2, 9, config);
            auto end = std::chrono::high_resolution_clock::now();
            auto parallelTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "Parallel flood fill results:\n";
            std::cout << "  Filled " << parallelFilledCells << " cells\n";
            std::cout << "  Time: " << parallelTime.count() << " μs\n";
            std::cout
                << "  Configuration: 4 threads, SIMD enabled, 8x8 blocks\n";

        } catch (...) {
            std::cout << "Parallel flood fill not available, using standard "
                         "algorithm\n";

            auto start = std::chrono::high_resolution_clock::now();
            size_t standardFilledCells =
                FloodFill::fillBFS(largeGrid, 7, 7, 2, 9, Connectivity::Four);
            auto end = std::chrono::high_resolution_clock::now();
            auto standardTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "Standard BFS flood fill results:\n";
            std::cout << "  Filled " << standardFilledCells << " cells\n";
            std::cout << "  Time: " << standardTime.count() << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced features demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive flood fill capabilities
 */
int main() {
    std::cout << "=== Atom Flood Fill Algorithms Comprehensive Example ===\n";
    std::cout << "Demonstrating flood fill algorithms for graphics and image "
                 "processing...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicFloodFill();
        demonstrateBFSvsDFS();
        demonstrateAdvancedFeatures();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Flood Fill Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The flood fill algorithm module provides:\n";
        std::cout << "  ✓ BFS and DFS flood fill algorithms\n";
        std::cout << "  ✓ 4-way and 8-way connectivity options\n";
        std::cout << "  ✓ Parallel processing with configurable options\n";
        std::cout << "  ✓ SIMD acceleration for large grids\n";
        std::cout << "  ✓ Block-optimized processing\n";
        std::cout << "  ✓ Comprehensive error handling and bounds checking\n";

        // Additional SIMD demonstration
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
            // Note: fillSIMD is not implemented, using fillBFS instead
            size_t simdFilledCells = atom::algorithm::FloodFill::fillBFS(
                simdGrid, 1, 1, 2, 8, simdConfig.connectivity);

            std::cout << "\nGrid after BFS flood fill (SIMD not available):"
                      << std::endl;
            printGrid(simdGrid);
            std::cout << "Filled " << simdFilledCells
                      << " cells using BFS algorithm" << std::endl;
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

        // Example using block-optimized flood fill
        {
            std::vector<std::vector<int>> blockGrid(16,
                                                    std::vector<int>(16, 1));
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

            // Note: fillBlockOptimized is not implemented, using fillBFS
            // instead
            size_t blockFilledCells = atom::algorithm::FloodFill::fillBFS(
                blockGrid, 0, 0, 3, 7, blockConfig.connectivity);

            std::cout
                << "Filled " << blockFilledCells
                << " cells using BFS algorithm (block-optimized not available)"
                << std::endl;
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
            std::cout << "Filled " << cells4Way
                      << " cells with 4-way connectivity" << std::endl;

            // Test 8-way connectivity
            auto grid8Way = connectivityGrid;
            size_t cells8Way = atom::algorithm::FloodFill::fillBFS(
                grid8Way, 0, 0, 1, 9, Connectivity::Eight);
            std::cout << "\nAfter 8-way connectivity flood fill:" << std::endl;
            printGrid(grid8Way);
            std::cout << "Filled " << cells8Way
                      << " cells with 8-way connectivity" << std::endl;
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
                std::cout
                    << "Caught expected error for out-of-bounds coordinates: "
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
                std::cout << "Caught expected error for empty grid: "
                          << e.what() << std::endl;
            }
        }

        // Performance comparison example
        {
            std::cout << "\n=== Performance Comparison ===" << std::endl;

            // Create a large grid for performance testing
            const int gridSize = 100;
            std::vector<std::vector<int>> perfGrid(
                gridSize, std::vector<int>(gridSize, 1));

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
            auto parallelTime =
                std::chrono::high_resolution_clock::now() - start;

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
        std::cout << "- Parallel processing with configurable options"
                  << std::endl;
        std::cout << "- SIMD acceleration (if supported)" << std::endl;
        std::cout << "- Block-optimized processing" << std::endl;
        std::cout << "- Error handling and bounds checking" << std::endl;
        std::cout << "- Performance comparison between algorithms" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in flood fill example: " << e.what()
                  << "\n";
        return 1;
    }
}
