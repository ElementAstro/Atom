#ifndef FLOOD_FILL_H
#define FLOOD_FILL_H

#include <vector>

/**
 * @enum Connectivity
 * @brief Enum to specify the type of connectivity for flood fill.
 */
enum class Connectivity {
    Four,  ///< 4-way connectivity (up, down, left, right)
    Eight  ///< 8-way connectivity (up, down, left, right, and diagonals)
};

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
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param conn The type of connectivity to use (default is 4-way
     * connectivity).
     */
    static void fillBFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
                        Connectivity conn = Connectivity::Four);

    /**
     * @brief Perform flood fill using Depth-First Search (DFS).
     *
     * @param grid The 2D grid to perform the flood fill on.
     * @param start_x The starting x-coordinate for the flood fill.
     * @param start_y The starting y-coordinate for the flood fill.
     * @param target_color The color to be replaced.
     * @param fill_color The color to fill with.
     * @param conn The type of connectivity to use (default is 4-way
     * connectivity).
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
    static bool isInBounds(int x, int y, int rows, int cols);

    /**
     * @brief Get the directions for the specified connectivity.
     *
     * @param conn The type of connectivity (4-way or 8-way).
     * @return A vector of pairs representing the directions.
     */
    static std::vector<std::pair<int, int>> getDirections(Connectivity conn);
};

#endif  // FLOOD_FILL_H