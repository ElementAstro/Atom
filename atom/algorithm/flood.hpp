#ifndef ATOM_ALGORITHM_FLOOD_GPP
#define ATOM_ALGORITHM_FLOOD_GPP

#include <atomic>
#include <concepts>
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

#include "atom/algorithm/rust_numeric.hpp"
#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>

/**
 * @enum Connectivity
 * @brief Enum to specify the type of connectivity for flood fill.
 */
enum class Connectivity {
    Four,  ///< 4-way connectivity (up, down, left, right)
    Eight  ///< 8-way connectivity (up, down, left, right, and diagonals)
};

// Static assertion to ensure enum values are as expected
static_assert(static_cast<std::int32_t>(Connectivity::Four) == 0 &&
                  static_cast<std::int32_t>(Connectivity::Eight) == 1,
              "Connectivity enum values must be 0 and 1");

/**
 * @concept Grid
 * @brief Concept that defines requirements for a type to be used as a grid.
 */
template <typename T>
concept Grid = requires(T t, std::size_t i, std::size_t j) {
    { t[i] } -> std::ranges::random_access_range;
    { t[i][j] } -> std::convertible_to<typename T::value_type::value_type>;
    requires std::is_default_constructible_v<T>;
    // { t.size() } -> std::convertible_to<usize>;
    { t.empty() } -> std::same_as<bool>;
    // requires(!t.empty() ? t[0].size() > 0 : true);
};

/**
 * @concept SIMDCompatibleGrid
 * @brief Concept that defines requirements for a type to be used with SIMD
 * operations.
 */
template <typename T>
concept SIMDCompatibleGrid =
    Grid<T> &&
    (std::same_as<typename T::value_type::value_type, atom::algorithm::i32> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::f32> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::f64> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::u8> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::u32>);

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
        u32 numThreads = static_cast<u32>(std::thread::hardware_concurrency());
        bool useSIMD = true;
        bool useBlockProcessing = true;
        u32 blockSize = 32;  // Size of cache-friendly blocks
        f32 loadBalancingFactor =
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
    [[nodiscard]] static usize fillBFS(
        GridType& grid, i32 start_x, i32 start_y,
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
    [[nodiscard]] static usize fillDFS(
        GridType& grid, i32 start_x, i32 start_y,
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
    [[nodiscard]] static usize fillParallel(
        GridType& grid, i32 start_x, i32 start_y,
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
    [[nodiscard]] static usize fillSIMD(
        GridType& grid, i32 start_x, i32 start_y,
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
        GridType& grid, i32 start_x, i32 start_y,
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
    [[nodiscard]] static usize fillBlockOptimized(
        GridType& grid, i32 start_x, i32 start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        const FloodFillConfig& config);

    /**
     * @brief Specialized BFS flood fill method for
     * std::vector<std::vector<i32>>
     * @return Number of cells filled
     */
    [[nodiscard]] static usize fillBFS(std::vector<std::vector<i32>>& grid,
                                       i32 start_x, i32 start_y,
                                       i32 target_color, i32 fill_color,
                                       Connectivity conn = Connectivity::Four);

    /**
     * @brief Specialized DFS flood fill method for
     * std::vector<std::vector<i32>>
     * @return Number of cells filled
     */
    [[nodiscard]] static usize fillDFS(std::vector<std::vector<i32>>& grid,
                                       i32 start_x, i32 start_y,
                                       i32 target_color, i32 fill_color,
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
    [[nodiscard]] static constexpr bool isInBounds(i32 x, i32 y, i32 rows,
                                                   i32 cols) noexcept {
        return x >= 0 && x < rows && y >= 0 && y < cols;
    }

    /**
     * @brief Get the directions for the specified connectivity.
     *
     * @param conn The type of connectivity (4-way or 8-way).
     * @return A vector of direction pairs.
     */
    [[nodiscard]] static auto getDirections(Connectivity conn)
        -> std::vector<std::pair<i32, i32>>;

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
    static void validateInput(const GridType& grid, i32 start_x, i32 start_y);

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
        const GridType& grid, i32 start_x, i32 start_y,
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
    [[nodiscard]] static usize processRowSIMD(T* row, i32 start_idx, i32 length,
                                              T target_color, T fill_color);

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
    [[nodiscard]] static usize processBlock(
        GridType& grid, i32 blockX, i32 blockY, i32 blockSize,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color, Connectivity conn,
        std::queue<std::pair<i32, i32>>& borderQueue);
};

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

        // First BFS phase to find initial points to process in parallel
        std::vector<std::pair<i32, i32>> seeds;
        std::queue<std::pair<i32, i32>> queue;
        std::vector<std::vector<bool>> visited(
            static_cast<usize>(rows),
            std::vector<bool>(static_cast<usize>(cols), false));

        queue.emplace(start_x, start_y);
        visited[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            true;
        grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            fill_color;
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
                i32 newX = x + dx;
                i32 newY = y + dy;

                if (isInBounds(newX, newY, rows, cols) &&
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] ==
                        target_color &&
                    !visited[static_cast<usize>(newX)]
                            [static_cast<usize>(newY)]) {
                    visited[static_cast<usize>(newX)]
                           [static_cast<usize>(newY)] = true;
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] =
                        fill_color;
                    filled_cells++;  // Count filled cells
                    queue.emplace(newX, newY);
                }
            }
        }

        // If we didn't find enough seeds, use what we have
        if (seeds.empty()) {
            spdlog::info(
                "Area too small for parallel fill, using single thread");
            return filled_cells;  // Already filled by the seed finding phase
        }

        // Use mutex to protect concurrent access to the grid
        std::mutex gridMutex;
        std::atomic<bool> shouldTerminate{false};
        std::atomic<usize> threadFilledCells{0};

        // Worker function for each thread
        auto worker = [&](const std::pair<i32, i32>& seed) {
            std::queue<std::pair<i32, i32>> localQueue;
            localQueue.push(seed);
            usize localFilledCells = 0;

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

#endif  // ATOM_ALGORITHM_FLOOD_GPP