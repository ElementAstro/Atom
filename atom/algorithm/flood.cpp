#include "flood.hpp"
#include "atom/log/loguru.hpp"

namespace atom::algorithm {
[[nodiscard]] auto FloodFill::getDirections(Connectivity conn)
    -> std::vector<std::pair<int, int>> {
    if (conn == Connectivity::Four) {
        return std::vector<std::pair<int, int>>{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    }
    return std::vector<std::pair<int, int>>{{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                            {0, 1},   {1, -1}, {1, 0},  {1, 1}};
}

void FloodFill::fillBFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
                        Connectivity conn) {
    fillBFS<std::vector<std::vector<int>>>(grid, start_x, start_y, target_color,
                                           fill_color, conn);
}

void FloodFill::fillDFS(std::vector<std::vector<int>>& grid, int start_x,
                        int start_y, int target_color, int fill_color,
                        Connectivity conn) {
    fillDFS<std::vector<std::vector<int>>>(grid, start_x, start_y, target_color,
                                           fill_color, conn);
}

}  // namespace atom::algorithm

template void
atom::algorithm::FloodFill::validateInput<std::vector<std::vector<int>>>(
    const std::vector<std::vector<int>>& grid, int start_x, int start_y);