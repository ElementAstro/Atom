#include "flood.hpp"

#include <queue>
#include <stack>

#include "atom/log/loguru.hpp"

auto FloodFill::isInBounds(int x_coord, int y_coord, int rows,
                           int cols) -> bool {
    return x_coord >= 0 && x_coord < rows && y_coord >= 0 && y_coord < cols;
}

auto FloodFill::getDirections(Connectivity conn)
    -> std::vector<std::pair<int, int>> {
    if (conn == Connectivity::Four) {
        return {
            {-1, 0},  // 上
            {1, 0},   // 下
            {0, -1},  // 左
            {0, 1}    // 右
        };
    }
    return {
        {-1, 0},   // 上
        {1, 0},    // 下
        {0, -1},   // 左
        {0, 1},    // 右
        {-1, -1},  // 左上
        {-1, 1},   // 右上
        {1, -1},   // 左下
        {1, 1}     // 右下
    };
}

void FloodFill::fillBFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
                        Connectivity conn) {
    LOG_F(INFO, "Starting BFS Flood Fill at ({}, {})", start_x, start_y);
    if (grid[start_x][start_y] != target_color || target_color == fill_color) {
        LOG_F(WARNING,
              "Start position does not match target color or target color is "
              "the same as fill color.");
        return;
    }

    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    auto directions = getDirections(conn);
    std::queue<std::pair<int, int>> toVisitQueue;

    toVisitQueue.emplace(start_x, start_y);
    grid[start_x][start_y] = fill_color;

    while (!toVisitQueue.empty()) {
        auto [x, y] = toVisitQueue.front();
        toVisitQueue.pop();
        LOG_F(INFO, "Filling position ({}, {})", x, y);

        for (auto [dx, dy] : directions) {
            int newX = x + dx;
            int newY = y + dy;

            if (isInBounds(newX, newY, rows, cols) &&
                grid[newX][newY] == target_color) {
                grid[newX][newY] = fill_color;
                toVisitQueue.emplace(newX, newY);
                LOG_F(INFO, "Adding position ({}, {}) to queue", newX, newY);
            }
        }
    }
}

void FloodFill::fillDFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
                        Connectivity conn) {
    LOG_F(INFO, "Starting DFS Flood Fill at ({}, {})", start_x, start_y);
    if (grid[start_x][start_y] != target_color || target_color == fill_color) {
        LOG_F(WARNING,
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
                LOG_F(INFO, "Adding position ({}, {}) to stack", newX, newY);
            }
        }
    }
}