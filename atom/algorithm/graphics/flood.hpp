#ifndef ATOM_ALGORITHM_GRAPHICS_FLOOD_HPP
#define ATOM_ALGORITHM_GRAPHICS_FLOOD_HPP

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

#include "../core/rust_numeric.hpp"
#include "atom/error/exception.hpp"
#include "grid_concepts.hpp"

#include <spdlog/spdlog.h>

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

}  // namespace atom::algorithm

// Template implementations
#include "flood_impl.hpp"

#endif  // ATOM_ALGORITHM_GRAPHICS_FLOOD_HPP
