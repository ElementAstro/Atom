#include "atom/algorithm/pathfinding.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>

namespace py = pybind11;

PYBIND11_MODULE(pathfinding, m) {
    m.doc() = "Pathfinding algorithms module for the atom package";

    // Register Point struct
    py::class_<atom::algorithm::Point>(
        m, "Point",
        R"(Represents a 2D point with integer coordinates.

Examples:
    >>> from atom.algorithm.pathfinding import Point
    >>> p = Point(1, 2)
    >>> print(p.x, p.y)
    1 2
)")
        .def(py::init<>(), "Default constructor")
        .def(py::init<int, int>(), py::arg("x"), py::arg("y"),
             "Constructs a Point object with the given x and y coordinates.")
        .def_readwrite("x", &atom::algorithm::Point::x, "X coordinate")
        .def_readwrite("y", &atom::algorithm::Point::y, "Y coordinate")
        .def("__eq__",
             [](const atom::algorithm::Point& a,
                const atom::algorithm::Point& b) {
                 return a.x == b.x && a.y == b.y;
             })
        .def("__ne__",
             [](const atom::algorithm::Point& a,
                const atom::algorithm::Point& b) {
                 return a.x != b.x || a.y != b.y;
             })
        .def("__repr__",
             [](const atom::algorithm::Point& p) {
                 return "Point(" + std::to_string(p.x) + ", " +
                        std::to_string(p.y) + ")";
             })
        .def("__hash__", [](const atom::algorithm::Point& p) {
            return std::hash<atom::algorithm::Point>{}(p);
        });

    // Register GridMap class
    py::class_<atom::algorithm::GridMap>(
        m, "GridMap",
        R"(Represents a 2D grid map with obstacles for pathfinding.)")
        .def(py::init<int, int>(), py::arg("width"), py::arg("height"),
             "Constructs an empty GridMap with specified width and height.")
        // 修复：改用 vector<uint8_t> 而不是 vector<bool>
        .def(py::init([](const std::vector<bool>& obstacles, int width,
                         int height) {
                 // 将 vector<bool> 转换为 vector<uint8_t>
                 std::vector<uint8_t> temp_obstacles;
                 temp_obstacles.reserve(obstacles.size());
                 for (bool val : obstacles) {
                     temp_obstacles.push_back(val ? 1 : 0);
                 }
                 // 使用 span<const uint8_t> 构造函数
                 return atom::algorithm::GridMap(
                     std::span<const uint8_t>(temp_obstacles), width, height);
             }),
             py::arg("obstacles"), py::arg("width"), py::arg("height"),
             "Constructs a GridMap with predefined obstacles.")
        .def("neighbors", &atom::algorithm::GridMap::neighbors, py::arg("p"),
             R"(Get all valid neighboring points.

Args:
    p: The point to find neighbors for

Returns:
    List of valid neighboring points
)")
        .def("cost", &atom::algorithm::GridMap::cost, py::arg("from"),
             py::arg("to"),
             R"(Calculate the cost of moving from one point to another.

Args:
    from: Starting point
    to: Ending point

Returns:
    Movement cost (typically 1.0 for adjacent cells)
)")
        .def("is_valid", &atom::algorithm::GridMap::isValid, py::arg("p"),
             R"(Check if a point is within the map boundaries.

Args:
    p: Point to check

Returns:
    True if the point is within boundaries, False otherwise
)")
        .def("set_obstacle", &atom::algorithm::GridMap::setObstacle,
             py::arg("p"), py::arg("is_obstacle"),
             R"(Set or remove an obstacle at the specified point.

Args:
    p: The point to modify
    is_obstacle: True to add an obstacle, False to remove it
)")
        .def("has_obstacle", &atom::algorithm::GridMap::hasObstacle,
             py::arg("p"),
             R"(Check if a point contains an obstacle.

Args:
    p: Point to check

Returns:
    True if the point has an obstacle, False otherwise
)")
        .def("get_width", &atom::algorithm::GridMap::getWidth,
             "Get the width of the grid")
        .def("get_height", &atom::algorithm::GridMap::getHeight,
             "Get the height of the grid");

    // Register heuristic functions
    py::module_ heuristics =
        m.def_submodule("heuristics", "Heuristic functions for pathfinding");

    heuristics.def("manhattan", &atom::algorithm::heuristics::manhattan,
                   py::arg("a"), py::arg("b"),
                   R"(Calculates the Manhattan distance between two points.

The Manhattan distance is the sum of the absolute differences of their Cartesian coordinates.

Args:
    a: First point
    b: Second point

Returns:
    Manhattan distance between points
)");

    heuristics.def("euclidean", &atom::algorithm::heuristics::euclidean,
                   py::arg("a"), py::arg("b"),
                   R"(Calculates the Euclidean distance between two points.

The Euclidean distance is the straight-line distance between two points.

Args:
    a: First point
    b: Second point

Returns:
    Euclidean distance between points
)");

    heuristics.def("diagonal", &atom::algorithm::heuristics::diagonal,
                   py::arg("a"), py::arg("b"),
                   R"(Calculates the diagonal distance between two points.

This combines Manhattan distance with diagonal shortcuts.

Args:
    a: First point
    b: Second point

Returns:
    Diagonal distance between points
)");

    heuristics.def("zero", &atom::algorithm::heuristics::zero, py::arg("a"),
                   py::arg("b"),
                   R"(Always returns zero distance (for Dijkstra's algorithm).

Args:
    a: First point (ignored)
    b: Second point (ignored)

Returns:
    Always returns 0.0
)");

    // Register PathFinder class and HeuristicType enum
    py::enum_<atom::algorithm::PathFinder::HeuristicType>(
        m, "HeuristicType",
        "Enum for selecting which heuristic to use for pathfinding")
        .value("MANHATTAN",
               atom::algorithm::PathFinder::HeuristicType::Manhattan)
        .value("EUCLIDEAN",
               atom::algorithm::PathFinder::HeuristicType::Euclidean)
        .value("DIAGONAL", atom::algorithm::PathFinder::HeuristicType::Diagonal)
        .export_values();

    // PathFinder static methods (using lambda to handle templates and
    // std::optional)
    m.def(
        "find_grid_path",
        [](const atom::algorithm::GridMap& map,
           const atom::algorithm::Point& start,
           const atom::algorithm::Point& goal,
           atom::algorithm::PathFinder::HeuristicType heuristicType) {
            auto path = atom::algorithm::PathFinder::findGridPath(
                map, start, goal, heuristicType);
            if (path) {
                return *path;
            } else {
                // Return empty list if no path found
                return std::vector<atom::algorithm::Point>();
            }
        },
        py::arg("map"), py::arg("start"), py::arg("goal"),
        py::arg("heuristic_type") =
            atom::algorithm::PathFinder::HeuristicType::Manhattan,
        R"(Find a path on a grid map using A* algorithm.

Args:
    map: The GridMap to search in
    start: Starting point
    goal: Goal point
    heuristic_type: Type of heuristic to use (default: MANHATTAN)

Returns:
    List of points from start to goal, or empty list if no path exists

Examples:
    >>> from atom.algorithm.pathfinding import GridMap, Point, find_grid_path, HeuristicType
    >>> grid = GridMap(10, 10)
    >>> # Add some obstacles
    >>> grid.set_obstacle(Point(2, 2), True)
    >>> grid.set_obstacle(Point(2, 3), True)
    >>> grid.set_obstacle(Point(2, 4), True)
    >>> # Find a path
    >>> path = find_grid_path(grid, Point(1, 1), Point(5, 5), HeuristicType.DIAGONAL)
)");

    // Alternative method without explicit GridMap (for simple cases)
    m.def(
        "find_path_with_obstacles",
        [](const std::vector<std::vector<bool>>& obstacles,
           const atom::algorithm::Point& start,
           const atom::algorithm::Point& goal,
           atom::algorithm::PathFinder::HeuristicType heuristicType) {
            if (obstacles.empty() || obstacles[0].empty()) {
                throw std::invalid_argument("Obstacle grid cannot be empty");
            }

            int height = obstacles.size();
            int width = obstacles[0].size();

            // 修复：使用 vector<uint8_t> 替代 vector<char>
            std::vector<uint8_t> flat_obstacles;
            flat_obstacles.reserve(width * height);

            for (const auto& row : obstacles) {
                if (static_cast<int>(row.size()) != width) {
                    throw std::invalid_argument(
                        "All rows must have the same width");
                }
                for (bool val : row) {
                    flat_obstacles.push_back(val ? 1 : 0);
                }
            }

            // 创建GridMap使用span<const uint8_t>构造函数
            atom::algorithm::GridMap map(
                std::span<const uint8_t>(flat_obstacles), width, height);

            auto path = atom::algorithm::PathFinder::findGridPath(
                map, start, goal, heuristicType);

            return path.value_or(std::vector<atom::algorithm::Point>());
        },
        py::arg("obstacles"), py::arg("start"), py::arg("goal"),
        py::arg("heuristic_type") =
            atom::algorithm::PathFinder::HeuristicType::Manhattan,
        R"(Find a path using a 2D grid of obstacles.

Args:
    obstacles: 2D grid of boolean values (True = obstacle, False = free)
    start: Starting point
    goal: Goal point
    heuristic_type: Type of heuristic to use (default: MANHATTAN)

Returns:
    List of points from start to goal, or empty list if no path exists

Examples:
    >>> from atom.algorithm.pathfinding import Point, find_path_with_obstacles, HeuristicType
    >>> # Create a simple obstacle grid (5x5)
    >>> obstacles = [
    ...     [False, False, False, False, False],
    ...     [False, False, True,  False, False],
    ...     [False, False, True,  False, False],
    ...     [False, False, True,  False, False],
    ...     [False, False, False, False, False]
    ... ]
    >>> path = find_path_with_obstacles(obstacles, Point(0, 0), Point(4, 4))
)");
}
