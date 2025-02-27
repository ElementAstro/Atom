#ifndef FLOOD_FILL_H
#define FLOOD_FILL_H

#include <atomic>
#include <concepts>
#include <mutex>
#include <queue>
#include <ranges>
#include <stack>
#include <thread>
#include <type_traits>
#include <vector>

#include "atom/log/loguru.hpp"

/**
 * @enum Connectivity
 * @brief Enum to specify the type of connectivity for flood fill.
 */
enum class Connectivity {
    Four,  ///< 4-way connectivity (up, down, left, right)
    Eight  ///< 8-way connectivity (up, down, left, right, and diagonals)
};

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

namespace atom::algorithm {

/**
 * @class FloodFill
 * @brief A class that provides static methods for performing flood fill
 * operations using BFS and DFS.
 */
class FloodFill {
public:
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
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     */
    template <Grid GridType>
    static void fillBFS(GridType& grid, int start_x, int start_y,
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
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     */
    template <Grid GridType>
    static void fillDFS(GridType& grid, int start_x, int start_y,
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
     * @param conn The type of connectivity to use (default is 4-way
     * connectivity).
     * @param num_threads Number of threads to use (default is hardware
     * concurrency).
     * @throws std::invalid_argument If grid is empty or coordinates are
     * invalid.
     */
    template <Grid GridType>
    static void fillParallel(
        GridType& grid, int start_x, int start_y,
        typename GridType::value_type::value_type target_color,
        typename GridType::value_type::value_type fill_color,
        Connectivity conn = Connectivity::Four,
        unsigned int num_threads = std::thread::hardware_concurrency());

    /**
     * @brief 为std::vector<std::vector<int>>特化的BFS洪水填充方法
     */
    static void fillBFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
                        Connectivity conn = Connectivity::Four);

    /**
     * @brief 为std::vector<std::vector<int>>特化的DFS洪水填充方法
     */
    static void fillDFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
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
};

}  // namespace atom::algorithm

namespace atom::algorithm {

template <Grid GridType>
void FloodFill::validateInput(const GridType& grid, int start_x, int start_y) {
    if (grid.empty() || grid[0].empty()) {
        throw std::invalid_argument("Grid cannot be empty");
    }

    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    if (!isInBounds(start_x, start_y, rows, cols)) {
        throw std::invalid_argument("Starting coordinates out of bounds");
    }
}

template <Grid GridType>
void FloodFill::fillBFS(GridType& grid, int start_x, int start_y,
                        typename GridType::value_type::value_type target_color,
                        typename GridType::value_type::value_type fill_color,
                        Connectivity conn) {
    LOG_F(INFO, "Starting BFS Flood Fill at ({}, {})", start_x, start_y);

    try {
        validateInput(grid, start_x, start_y);

        if (grid[start_x][start_y] != target_color ||
            target_color == fill_color) {
            LOG_F(
                WARNING,
                "Start position does not match target color or target color is "
                "the same as fill color.");
            return;
        }

        int rows = static_cast<int>(grid.size());
        int cols = static_cast<int>(grid[0].size());
        const auto directions = getDirections(conn);  // Now returns vector
        std::queue<std::pair<int, int>> toVisitQueue;

        toVisitQueue.emplace(start_x, start_y);
        grid[start_x][start_y] = fill_color;

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
                    toVisitQueue.emplace(newX, newY);
                    LOG_F(INFO, "Adding position ({}, {}) to queue", newX,
                          newY);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fillBFS: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

template <Grid GridType>
void FloodFill::fillDFS(GridType& grid, int start_x, int start_y,
                        typename GridType::value_type::value_type target_color,
                        typename GridType::value_type::value_type fill_color,
                        Connectivity conn) {
    LOG_F(INFO, "Starting DFS Flood Fill at ({}, {})", start_x, start_y);

    try {
        validateInput(grid, start_x, start_y);

        if (grid[start_x][start_y] != target_color ||
            target_color == fill_color) {
            LOG_F(
                WARNING,
                "Start position does not match target color or target color is "
                "the same as fill color.");
            return;
        }

        int rows = static_cast<int>(grid.size());
        int cols = static_cast<int>(grid[0].size());
        auto directions = getDirections(conn);
        std::stack<std::pair<int, int>> toVisitStack;

        toVisitStack.emplace(start_x, start_y);
        grid[start_x][start_y] = fill_color;

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
                    toVisitStack.emplace(newX, newY);
                    LOG_F(INFO, "Adding position ({}, {}) to stack", newX,
                          newY);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fillDFS: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

template <Grid GridType>
void FloodFill::fillParallel(
    GridType& grid, int start_x, int start_y,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color, Connectivity conn,
    unsigned int num_threads) {
    LOG_F(INFO, "Starting Parallel Flood Fill at ({}, {}) with {} threads",
          start_x, start_y, num_threads);

    try {
        validateInput(grid, start_x, start_y);

        if (grid[start_x][start_y] != target_color ||
            target_color == fill_color) {
            LOG_F(
                WARNING,
                "Start position does not match target color or target color is "
                "the same as fill color.");
            return;
        }

        int rows = static_cast<int>(grid.size());
        int cols = static_cast<int>(grid[0].size());
        auto directions = getDirections(conn);

        // First BFS phase to find initial points to process in parallel
        std::vector<std::pair<int, int>> seeds;
        std::queue<std::pair<int, int>> queue;
        std::vector<std::vector<bool>> visited(rows,
                                               std::vector<bool>(cols, false));

        queue.emplace(start_x, start_y);
        visited[start_x][start_y] = true;
        grid[start_x][start_y] = fill_color;

        // Find seed points for parallel processing
        while (!queue.empty() && seeds.size() < num_threads) {
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
                    queue.emplace(newX, newY);
                }
            }
        }

        // If we didn't find enough seeds, use what we have
        if (seeds.empty()) {
            LOG_F(INFO,
                  "Area too small for parallel fill, using single thread");
            return;  // Already filled by the seed finding phase
        }

        // Use mutex to protect concurrent access to the grid
        std::mutex gridMutex;
        std::atomic<bool> shouldTerminate{false};

        // Worker function for each thread
        auto worker = [&](const std::pair<int, int>& seed) {
            std::queue<std::pair<int, int>> localQueue;
            localQueue.push(seed);

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
                            localQueue.emplace(newX, newY);
                        }
                    }
                }
            }
        };

        // Launch worker threads
        std::vector<std::jthread> threads;
        threads.reserve(seeds.size());

        for (const auto& seed : seeds) {
            threads.emplace_back(worker, seed);
        }

        // No need to join explicitly as std::jthread automatically joins on
        // destruction

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fillParallel: {}", e.what());
        throw;  // Re-throw the exception after logging
    }
}

}  // namespace atom::algorithm

#endif  // FLOOD_FILL_H