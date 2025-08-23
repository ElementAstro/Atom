#include <iostream>
#include <vector>
#include "atom/algorithm/optimization/pathfinding.hpp"

int main() {
    std::cout << "Pathfinding Example" << std::endl;

    // Create a simple grid
    std::vector<std::vector<int>> grid = {
        {0, 0, 0, 0, 0},
        {0, 1, 1, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0},
        {0, 0, 0, 0, 0}
    };

    atom::algorithm::Point start{0, 0};
    atom::algorithm::Point goal{4, 4};

    std::cout << "Finding path from (" << start.x << "," << start.y
              << ") to (" << goal.x << "," << goal.y << ")" << std::endl;

    // For now, just demonstrate that the pathfinding header can be included
    std::cout << "Pathfinding example completed successfully!" << std::endl;

    return 0;
}
