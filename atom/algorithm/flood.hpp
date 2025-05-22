#ifndef ATOM_ALGORITHM_FLOOD_GPP
#define ATOM_ALGORITHM_FLOOD_GPP

#include <atomic>
#include <concepts>
#include <limits>
#include <mutex>
#include <queue>
#include <ranges>
#include <span>
#include <stack>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

/**
 * @enum Connectivity
 * @brief Enum to specify the type of connectivity for flood fill.
 */
enum class Connectivity {
    Four,  ///< 4-way connectivity (up, down, left, right)
    Eight  ///< 8-way connectivity (up, down, left, right, and diagonals)
};

// Static assertion to ensure enum values are as expected
static_assert(static_cast<int>(Connectivity::Four) == 0 &&
                  static_cast<int>(Connectivity::Eight) == 1,
              "Connectivity enum values must be 0 and 1");

/**
 * @concept Grid
 * @brief Concept that defines requirements for a type to be used as a grid.
 */
template <typename T>
concept Grid = requires(T t, size_t i) {
    { t[i] } -> std::ranges::random_access_range;
    { t[i][i] } -> std::convertible_to<typename T::value_type::value_type>;
    requires std::is_default_constructible_v<T>;
};

/**
 * @concept SIMDCompatibleGrid
 * @brief Concept that defines requirements for a type to be used with SIMD
 * operations.
 */
template <typename T>
concept SIMDCompatibleGrid =
    Grid<T> && (std::same_as<typename T::value_type::value_type, int> ||
                std::same_as<typename T::value_type::value_type, float> ||
                std::same_as<typename T::value_type::value_type, double> ||
                std::same_as<typename T::value_type::value_type, uint8_t> ||
                std::same_as<typename T::value_type::value_type, uint32_t>);

/**
 * @concept ContiguousGrid
 * @brief Concept that defines requirements for a grid with contiguous memory
 * layout.
 */
template <typename T>
concept ContiguousGrid = Grid<T> && requires(T t) {
    { t.data() } -> std::convertible_to<typename T::value_type*>;
    requires std::contiguous_iterator<typename T::iterator>;
};

/**
 * @concept SpanCompatibleGrid
 * @brief Concept for grids that can work with std::span for efficient views.
 */
template <typename T>
concept SpanCompatibleGrid = Grid<T> && requires(T t) {
    { std::span<typename T::value_type>(t) };
};

namespace atom::algorithm {

/**
 * @class FloodFill
 * @brief A class that provides static methods for performing flood fill
 * operations using various algorithms and optimizations.
 */
class FloodFill {
public:
    /**
     * @brief Configuration struct for flood fill operations
     */
    struct FloodFillConfig {
        Connectivity connectivity = Connectivity::Four;
        unsigned int numThreads = std::thread::hardware_concurrency();
        bool useSIMD = true;
        bool useBlockProcessing = true;
        unsigned int blockSize = 32;  // Size of cache-friendly blocks
        float loadBalancingFactor =
            1.5f;  // Work distribution factor for parallel processing

        // Validation method for configuration
        [[nodiscard]] constexpr bool isValid() const noexcept {
            return numThreads > 0 && blockSize > 0 && blockSize <= 256 &&
                   loadBalancingFactor > 0.0f;
        }
    };

    /**
     * @brief Perform flood fill using Breadth-First Search (BFS).
     *
     * @tparam GridType The type of grid to perform flood fill on
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param conn The type of connectivity to use (default is 4-way
     * connectivity).
     * @return Number of cells filled
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     * @throws std::runtime_error If operation fails during execution.
     */
    template <Grid GridType>
    [[nodiscard]] static size_t fillBFS(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        Connectivity conn = Connectivity::Four);

    /**
     * @brief Perform flood fill using Depth-First Search (DFS).
     *
     * @tparam GridType The type of grid to perform flood fill on
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param conn The type of connectivity to use (default is 4-way
     * connectivity).
     * @return Number of cells filled
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     * @throws std::runtime_error If operation fails during execution.
     */
    template <Grid GridType>
    [[nodiscard]] static size_t fillDFS(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        Connectivity conn = Connectivity::Four);

    /**
     * @brief Perform parallel flood fill using multiple threads.
     *
     * @tparam GridType The type of grid to perform flood fill on
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param config Configuration options for the flood fill operation.
     * @return Number of cells filled
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     * @throws std::runtime_error If operation fails during execution.
     */
    template <Grid GridType>
    [[nodiscard]] static size_t fillParallel(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        const FloodFillConfig& config);

    /**
     * @brief Perform SIMD-accelerated flood fill for suitable grid types.
     *
     * @tparam GridType The type of grid to perform flood fill on
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param config Configuration options for the flood fill operation.
     * @return Number of cells filled
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     * @throws std::runtime_error If operation fails during execution.
     * @throws std::logic_error If SIMD operations are not supported for this
     * grid type.
     */
    template <SIMDCompatibleGrid GridType>
    [[nodiscard]] static size_t fillSIMD(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        const FloodFillConfig& config);

    /**
     * @brief Asynchronous flood fill generator using C++20 coroutines.
     *        Returns a generator that yields each filled position.
     *
     * @tparam GridType The type of grid to perform flood fill on
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param conn The type of connectivity to use.
     * @return A generator yielding pairs of coordinates
     */
    template <Grid GridType>
    static auto fillAsync(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        Connectivity conn = Connectivity::Four);

    /**
     * @brief Cache-optimized flood fill using block-based processing
     *
     * @tparam GridType The type of grid to perform flood fill on
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param config Configuration options for the flood fill operation.
     * @return Number of cells filled
     */
    template <Grid GridType>
    [[nodiscard]] static size_t fillBlockOptimized(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        const FloodFillConfig& config);

    /**
     * @brief Specialized BFS flood fill method for
     * std::vector<std::vector<int>>
     * @return Number of cells filled
     */
    [[nodiscard]] static size_t fillBFS(std::vector<std::vector<int>>& grid,
                                        int start_x, int start_y,
                                        int target_color, int fill_color,
                                        Connectivity conn = Connectivity::Four);

    /**
     * @brief Specialized DFS flood fill method for
     * std::vector<std::vector<int>>
     * @return Number of cells filled
     */
    [[nodiscard]] static size_t fillDFS(std::vector<std::vector<int>>& grid,
                                        int start_x, int start_y,
                                        int target_color, int fill_color,
                                        Connectivity conn = Connectivity::Four);

private:
    /**
     * @brief Check if a position is within the bounds of the grid.
     *
     * @param x The x-coordinate to check.
     * @param y The y-coordinate to check.
     * @param rows The number of rows in the grid.
     * @param cols The number of columns in the grid.
     * @return true if the position is within bounds, false otherwise.
     */
    [[nodiscard]] static constexpr bool isInBounds(int x, int y, int rows,
                                                   int cols) noexcept {
        return x >= 0 && x < rows && y >= 0 && y < cols;
    }

    /**
     * @brief Get the directions for the specified connectivity.
     *
     * @param conn The type of connectivity (4-way or 8-way).
     * @return A vector of direction pairs.
     */
    [[nodiscard]] static auto getDirections(Connectivity conn)
        -> std::vector<std::pair<int, int>>;

    /**
     * @brief Validate grid and coordinates before processing.
     *
     * @tparam GridType The type of grid
     * @param grid The 2D grid to validate.
     * @param start_x The starting x-coordinate.
     * @param start_y The starting y-coordinate.
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     */
    template <Grid GridType>
    static void validateInput(const GridType& grid, int start_x, int start_y);

    /**
     * @brief Extended validation for additional input parameters
     *
     * @tparam GridType The type of grid
     * @param grid The 2D grid to validate
     * @param start_x The starting x-coordinate
     * @param start_y The starting y-coordinate
     * @param target_color The color to be replaced
     * @param fill_color The color to fill with
     * @param config The configuration options
     * @throws std::invalid_argument If any parameters are invalid
     */
    template <Grid GridType>
    static void validateExtendedInput(
        const GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        const FloodFillConfig& config);

    /**
     * @brief Validate grid size and dimensions
     *
     * @tparam GridType The type of grid
     * @param grid The grid to validate
     * @throws std::invalid_argument If grid dimensions exceed maximum limits
     */
    template <Grid GridType>
    static void validateGridSize(const GridType& grid);

    /**
     * @brief Process a row of grid data using SIMD instructions
     *
     * @tparam T Type of grid element
     * @param row Pointer to the row data
     * @param start_idx Starting index in the row
     * @param length Number of elements to process
     * @param target_color Color to be replaced
     * @param fill_color Color to fill with
     * @return Number of cells filled
     */
    template <typename T>
    [[nodiscard]] static size_t processRowSIMD(T* row, int start_idx,
                                               int length, T target_color,
                                               T fill_color);

    /**
     * @brief Process a block of the grid for block-based flood fill
     *
     * @tparam GridType The type of grid
     * @param grid The grid to process
     * @param blockX X coordinate of the block's top-left corner
     * @param blockY Y coordinate of the block's top-left corner
     * @param blockSize Size of the block
     * @param target_color Color to be replaced
     * @param fill_color Color to fill with
     * @param conn Connectivity type
     * @param borderQueue Queue to store border pixels
     * @return Number of cells filled in the block
     */
    template <Grid GridType>
    [[nodiscard]] static size_t processBlock(
        GridType& grid, int blockX, int blockY, int blockSize,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color, Connectivity conn,
        std::queue<std::pair<int, int>>& borderQueue);
};

template <Grid GridType>
void FloodFill::validateInput(const GridType& grid, int start_x, int start_y) {
    if (grid.empty() || grid[0].empty()) {
        THROW_INVALID_ARGUMENT("Grid cannot be empty");
    }

    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    if (!isInBounds(start_x, start_y, rows, cols)) {
        THROW_INVALID_ARGUMENT("Starting coordinates out of bounds");
    }
}

template <Grid GridType>
void FloodFill::validateExtendedInput(
    const GridType& grid, int start_x, int start_y,
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
    const size_t max_dimension = std::numeric_limits<int>::max() / 2;

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
        const size_t first_row_size = grid[0].size();
        for (size_t i = 1; i < grid.size(); ++i) {
            if (grid[i].size() != first_row_size) {
                THROW_INVALID_ARGUMENT("Grid has non-uniform row sizes");
            }
        }
    }
}

template <Grid GridType>
size_t FloodFill::fillBFS(
    GridType& grid, int start_x, int start_y,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color, Connectivity conn) {
    LOG_F(INFO, "Starting BFS Flood Fill at ({}, {})", start_x, start_y);

    size_t filled_cells = 0;  // Counter for filled cells

    try {
        validateInput(grid, start_x, start_y);

        if (grid[start_x][start_y] != target_color ||
            target_color == fill_color) {
            LOG_F(
                WARNING,
                "Start position does not match target color or target color is "
                "the same as fill color.");
            return filled_cells;
        }

        int rows = static_cast<int>(grid.size());
        int cols = static_cast<int>(grid[0].size());
        const auto directions = getDirections(conn);  // Now returns vector
        std::queue<std::pair<int, int>> toVisitQueue;

        toVisitQueue.emplace(start_x, start_y);
        grid[start_x][start_y] = fill_color;
        filled_cells++;  // Count filled cells

        while (!toVisitQueue.empty()) {
            auto [x, y] = toVisitQueue.front();
            toVisitQueue.pop();
            LOG_F(INFO, "Filling position ({}, {})", x, y);

            // Now we can directly iterate over the vector
            for (const auto& [dx, dy] : directions) {
                int newX = x + dx;
                int newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[newX][newY] == target_color) {
                    grid[newX][newY] = fill_color;
                    filled_cells++;  // Count filled cells
                    toVisitQueue.emplace(newX, newY);
                    LOG_F(INFO, "Adding position ({}, {}) to queue", newX,
                          newY);
                }
            }
        }

        return filled_cells;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fillBFS: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

template <Grid GridType>
size_t FloodFill::fillDFS(
    GridType& grid, int start_x, int start_y,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color, Connectivity conn) {
    LOG_F(INFO, "Starting DFS Flood Fill at ({}, {})", start_x, start_y);

    size_t filled_cells = 0;  // Counter for filled cells

    try {
        validateInput(grid, start_x, start_y);

        if (grid[start_x][start_y] != target_color ||
            target_color == fill_color) {
            LOG_F(
                WARNING,
                "Start position does not match target color or target color is "
                "the same as fill color.");
            return filled_cells;
        }

        int rows = static_cast<int>(grid.size());
        int cols = static_cast<int>(grid[0].size());
        auto directions = getDirections(conn);
        std::stack<std::pair<int, int>> toVisitStack;

        toVisitStack.emplace(start_x, start_y);
        grid[start_x][start_y] = fill_color;
        filled_cells++;  // Count filled cells

        while (!toVisitStack.empty()) {
            auto [x, y] = toVisitStack.top();
            toVisitStack.pop();
            LOG_F(INFO, "Filling position ({}, {})", x, y);

            for (auto [dx, dy] : directions) {
                int newX = x + dx;
                int newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[newX][newY] == target_color) {
                    grid[newX][newY] = fill_color;
                    filled_cells++;  // Count filled cells
                    toVisitStack.emplace(newX, newY);
                    LOG_F(INFO, "Adding position ({}, {}) to stack", newX,
                          newY);
                }
            }
        }

        return filled_cells;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fillDFS: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

template <Grid GridType>
size_t FloodFill::fillParallel(
    GridType& grid, int start_x, int start_y,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color,
    const FloodFillConfig& config) {
    LOG_F(INFO, "Starting Parallel Flood Fill at ({}, {}) with {} threads",
          start_x, start_y, config.numThreads);

    size_t filled_cells = 0;  // Counter for filled cells

    try {
        // Enhanced validation with the extended input function
        validateExtendedInput(grid, start_x, start_y, target_color, fill_color,
                              config);

        if (grid[start_x][start_y] != target_color ||
            target_color == fill_color) {
            LOG_F(
                WARNING,
                "Start position does not match target color or target color is "
                "the same as fill color.");
            return filled_cells;
        }

        int rows = static_cast<int>(grid.size());
        int cols = static_cast<int>(grid[0].size());
        auto directions = getDirections(config.connectivity);

        // First BFS phase to find initial points to process in parallel
        std::vector<std::pair<int, int>> seeds;
        std::queue<std::pair<int, int>> queue;
        std::vector<std::vector<bool>> visited(rows,
                                               std::vector<bool>(cols, false));

        queue.emplace(start_x, start_y);
        visited[start_x][start_y] = true;
        grid[start_x][start_y] = fill_color;
        filled_cells++;  // Count filled cells

        // Find seed points for parallel processing
        while (!queue.empty() && seeds.size() < config.numThreads) {
            auto [x, y] = queue.front();
            queue.pop();

            // Add current point as a seed if it's not the starting point
            if (x != start_x || y != start_y) {
                seeds.emplace_back(x, y);
            }

            // Explore neighbors to find more potential seeds
            for (auto [dx, dy] : directions) {
                int newX = x + dx;
                int newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[newX][newY] == target_color && !visited[newX][newY]) {
                    visited[newX][newY] = true;
                    grid[newX][newY] = fill_color;
                    filled_cells++;  // Count filled cells
                    queue.emplace(newX, newY);
                }
            }
        }

        // If we didn't find enough seeds, use what we have
        if (seeds.empty()) {
            LOG_F(INFO,
                  "Area too small for parallel fill, using single thread");
            return filled_cells;  // Already filled by the seed finding phase
        }

        // Use mutex to protect concurrent access to the grid
        std::mutex gridMutex;
        std::atomic<bool> shouldTerminate{false};
        std::atomic<size_t> threadFilledCells{0};

        // Worker function for each thread
        auto worker = [&](const std::pair<int, int>& seed) {
            std::queue<std::pair<int, int>> localQueue;
            localQueue.push(seed);
            size_t localFilledCells = 0;

            while (!localQueue.empty() && !shouldTerminate) {
                auto [x, y] = localQueue.front();
                localQueue.pop();

                for (auto [dx, dy] : directions) {
                    int newX = x + dx;
                    int newY = y + dy;

                    if (isInBounds(newX, newY, rows, cols)) {
                        std::lock_guard<std::mutex> lock(gridMutex);
                        if (grid[newX][newY] == target_color) {
                            grid[newX][newY] = fill_color;
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
        LOG_F(ERROR, "Exception in fillParallel: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGOTFLOOD_GPP