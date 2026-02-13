#ifndef ATOM_ALGORITHM_GRAPHICS_FLOOD_IMPL_HPP
#define ATOM_ALGORITHM_GRAPHICS_FLOOD_IMPL_HPP

// This file contains template implementations for FloodFill methods.
// It is automatically included by flood.hpp — do not include directly.

namespace atom::algorithm {

template <Grid GridType>
void FloodFill::validateInput(const GridType& grid, i32 start_x, i32 start_y) {
    if (grid.empty() || grid[0].empty()) {
        THROW_INVALID_ARGUMENT("Grid cannot be empty");
    }

    i32 rows = static_cast<i32>(grid.size());
    i32 cols = static_cast<i32>(grid[0].size());

    if (!isInBounds(start_x, start_y, rows, cols)) {
        THROW_INVALID_ARGUMENT("Starting coordinates out of bounds");
    }
}

template <Grid GridType>
void FloodFill::validateExtendedInput(
    const GridType& grid, i32 start_x, i32 start_y,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color,
    const FloodFillConfig& config) {
    // Basic validation
    validateInput(grid, start_x, start_y);
    validateGridSize(grid);

    // Check configuration validity
    if (!config.isValid()) {
        THROW_INVALID_ARGUMENT("Invalid flood fill configuration");
    }

    // Additional validations specific to grid type
    if constexpr (std::is_arithmetic_v<
                      typename GridType::value_type::value_type>) {
        // For numeric types, check if colors are within valid ranges
        if (target_color == fill_color) {
            THROW_INVALID_ARGUMENT(
                "Target color and fill color cannot be the same");
        }
    }
}

template <Grid GridType>
void FloodFill::validateGridSize(const GridType& grid) {
    // Check if grid dimensions are within reasonable limits
    const usize max_dimension =
        static_cast<usize>(atom::algorithm::I32::MAX) / 2;

    if (grid.size() > max_dimension) {
        THROW_INVALID_ARGUMENT("Grid row count exceeds maximum allowed size");
    }

    for (const auto& row : grid) {
        if (row.size() > max_dimension) {
            THROW_INVALID_ARGUMENT(
                "Grid column count exceeds maximum allowed size");
        }
    }

    // Check for uniform row sizes
    if (!grid.empty()) {
        const usize first_row_size = grid[0].size();
        for (usize i = 1; i < grid.size(); ++i) {
            if (grid[i].size() != first_row_size) {
                THROW_INVALID_ARGUMENT("Grid has non-uniform row sizes");
            }
        }
    }
}

template <Grid GridType>
usize FloodFill::fillBFS(GridType& grid, i32 start_x, i32 start_y,
                         typename GridType::value_type::value_type target_color,
                         typename GridType::value_type::value_type fill_color,
                         Connectivity conn) {
    spdlog::info("Starting BFS Flood Fill at position ({}, {})", start_x,
                 start_y);

    usize filled_cells = 0;  // Counter for filled cells

    try {
        validateInput(grid, start_x, start_y);

        if (grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] !=
                target_color ||
            target_color == fill_color) {
            spdlog::warn(
                "Start position does not match target color or target color is "
                "the same as fill color");
            return filled_cells;
        }

        i32 rows = static_cast<i32>(grid.size());
        i32 cols = static_cast<i32>(grid[0].size());
        const auto directions = getDirections(conn);  // Now returns vector
        std::queue<std::pair<i32, i32>> toVisitQueue;

        toVisitQueue.emplace(start_x, start_y);
        grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            fill_color;
        filled_cells++;  // Count filled cells

        while (!toVisitQueue.empty()) {
            auto [x, y] = toVisitQueue.front();
            toVisitQueue.pop();
            spdlog::debug("Filling position ({}, {})", x, y);

            // Now we can directly iterate over the vector
            for (const auto& [dx, dy] : directions) {
                i32 newX = x + dx;
                i32 newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] ==
                        target_color) {
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] =
                        fill_color;
                    filled_cells++;  // Count filled cells
                    toVisitQueue.emplace(newX, newY);
                    spdlog::debug("Adding position ({}, {}) to queue", newX,
                                  newY);
                }
            }
        }

        return filled_cells;
    } catch (const std::exception& e) {
        spdlog::error("Exception in fillBFS: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

template <Grid GridType>
usize FloodFill::fillDFS(GridType& grid, i32 start_x, i32 start_y,
                         typename GridType::value_type::value_type target_color,
                         typename GridType::value_type::value_type fill_color,
                         Connectivity conn) {
    spdlog::info("Starting DFS Flood Fill at position ({}, {})", start_x,
                 start_y);

    usize filled_cells = 0;  // Counter for filled cells

    try {
        validateInput(grid, start_x, start_y);

        if (grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] !=
                target_color ||
            target_color == fill_color) {
            spdlog::warn(
                "Start position does not match target color or target color is "
                "the same as fill color");
            return filled_cells;
        }

        i32 rows = static_cast<i32>(grid.size());
        i32 cols = static_cast<i32>(grid[0].size());
        auto directions = getDirections(conn);
        std::stack<std::pair<i32, i32>> toVisitStack;

        toVisitStack.emplace(start_x, start_y);
        grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            fill_color;
        filled_cells++;  // Count filled cells

        while (!toVisitStack.empty()) {
            auto [x, y] = toVisitStack.top();
            toVisitStack.pop();
            spdlog::debug("Filling position ({}, {})", x, y);

            for (auto [dx, dy] : directions) {
                i32 newX = x + dx;
                i32 newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] ==
                        target_color) {
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] =
                        fill_color;
                    filled_cells++;  // Count filled cells
                    toVisitStack.emplace(newX, newY);
                    spdlog::debug("Adding position ({}, {}) to stack", newX,
                                  newY);
                }
            }
        }

        return filled_cells;
    } catch (const std::exception& e) {
        spdlog::error("Exception in fillDFS: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

template <Grid GridType>
usize FloodFill::fillParallel(
    GridType& grid, i32 start_x, i32 start_y,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color,
    const FloodFillConfig& config) {
    spdlog::info(
        "Starting Parallel Flood Fill at position ({}, {}) with {} threads",
        start_x, start_y, config.numThreads);

    usize filled_cells = 0;  // Counter for filled cells

    try {
        // Enhanced validation with the extended input function
        validateExtendedInput(grid, start_x, start_y, target_color, fill_color,
                              config);

        if (grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] !=
                target_color ||
            target_color == fill_color) {
            spdlog::warn(
                "Start position does not match target color or target color is "
                "the same as fill color");
            return filled_cells;
        }

        i32 rows = static_cast<i32>(grid.size());
        i32 cols = static_cast<i32>(grid[0].size());
        auto directions = getDirections(config.connectivity);

        // First BFS phase to find initial seed points for parallel processing
        // We don't fill cells here, just identify starting points for worker
        // threads
        std::vector<std::pair<i32, i32>> seeds;
        std::queue<std::pair<i32, i32>> queue;
        std::vector<std::vector<bool>> visited(
            static_cast<usize>(rows),
            std::vector<bool>(static_cast<usize>(cols), false));

        queue.emplace(start_x, start_y);
        visited[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            true;
        seeds.emplace_back(start_x,
                           start_y);  // Add starting point as first seed

        // Find additional seed points for parallel processing
        while (!queue.empty() && seeds.size() < config.numThreads) {
            auto [x, y] = queue.front();
            queue.pop();

            // Explore neighbors to find more potential seeds
            for (auto [dx, dy] : directions) {
                i32 newX = x + dx;
                i32 newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] ==
                        target_color &&
                    !visited[static_cast<usize>(newX)]
                            [static_cast<usize>(newY)]) {
                    visited[static_cast<usize>(newX)]
                           [static_cast<usize>(newY)] = true;
                    queue.emplace(newX, newY);

                    // Add as seed if we need more seeds
                    if (seeds.size() < config.numThreads) {
                        seeds.emplace_back(newX, newY);
                    }
                }
            }
        }

        // Use mutex to protect concurrent access to the grid
        std::mutex gridMutex;
        std::atomic<bool> shouldTerminate{false};
        std::atomic<usize> threadFilledCells{0};

        // Worker function for each thread
        auto worker = [&](const std::pair<i32, i32>& seed) {
            std::queue<std::pair<i32, i32>> localQueue;
            usize localFilledCells = 0;

            // Fill the seed point first
            {
                std::lock_guard<std::mutex> lock(gridMutex);
                if (grid[static_cast<usize>(seed.first)]
                        [static_cast<usize>(seed.second)] == target_color) {
                    grid[static_cast<usize>(seed.first)]
                        [static_cast<usize>(seed.second)] = fill_color;
                    localFilledCells++;
                    localQueue.push(seed);
                }
            }

            while (!localQueue.empty() && !shouldTerminate) {
                auto [x, y] = localQueue.front();
                localQueue.pop();

                for (auto [dx, dy] : directions) {
                    i32 newX = x + dx;
                    i32 newY = y + dy;

                    if (isInBounds(newX, newY, rows, cols)) {
                        std::lock_guard<std::mutex> lock(gridMutex);
                        if (grid[static_cast<usize>(newX)]
                                [static_cast<usize>(newY)] == target_color) {
                            grid[static_cast<usize>(newX)]
                                [static_cast<usize>(newY)] = fill_color;
                            localFilledCells++;
                            localQueue.emplace(newX, newY);
                        }
                    }
                }
            }

            threadFilledCells += localFilledCells;
        };

        // Launch worker threads
        std::vector<std::jthread> threads;
        threads.reserve(seeds.size());

        for (const auto& seed : seeds) {
            threads.emplace_back(worker, seed);
        }

        // No need to join explicitly as std::jthread automatically joins on
        // destruction

        filled_cells += threadFilledCells.load();
        return filled_cells;

    } catch (const std::exception& e) {
        spdlog::error("Exception in fillParallel: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_FLOOD_IMPL_HPP
