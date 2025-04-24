#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <random>

#include "atom/algorithm/flood.hpp"

namespace py = pybind11;

// Helper functions for conversion between numpy arrays and C++ vectors
std::vector<std::vector<int>> numpy_to_vector(py::array_t<int> array) {
    auto buf = array.request();
    if (buf.ndim != 2)
        throw std::runtime_error("Number of dimensions must be 2");

    std::vector<std::vector<int>> result(buf.shape[0],
                                         std::vector<int>(buf.shape[1]));

    int *ptr = static_cast<int *>(buf.ptr);

    for (ssize_t i = 0; i < buf.shape[0]; i++) {
        for (ssize_t j = 0; j < buf.shape[1]; j++) {
            result[i][j] = ptr[i * buf.shape[1] + j];
        }
    }

    return result;
}

py::array_t<int> vector_to_numpy(const std::vector<std::vector<int>> &vec) {
    if (vec.empty())
        return py::array_t<int>(std::vector<ssize_t>{0, 0});

    ssize_t rows = vec.size();
    ssize_t cols = vec[0].size();

    py::array_t<int> result({rows, cols});
    auto buf = result.request();
    int *ptr = static_cast<int *>(buf.ptr);

    for (ssize_t i = 0; i < rows; i++) {
        for (ssize_t j = 0; j < cols; j++) {
            ptr[i * cols + j] = vec[i][j];
        }
    }

    return result;
}

PYBIND11_MODULE(flood_fill, m) {
    m.doc() = R"pbdoc(
        Flood Fill Algorithms
        --------------------

        This module provides various flood fill algorithms for 2D grids:
        
        - **fill_bfs**: Flood fill using Breadth-First Search
        - **fill_dfs**: Flood fill using Depth-First Search
        - **fill_parallel**: Flood fill using multiple threads

        Example:
            >>> import numpy as np
            >>> from atom.algorithm.flood_fill import fill_bfs, Connectivity
            >>> 
            >>> # Create a grid
            >>> grid = np.zeros((10, 10), dtype=np.int32)
            >>> grid[3:7, 3:7] = 1  # Create a square
            >>> 
            >>> # Fill the square
            >>> filled_grid = fill_bfs(grid, 5, 5, 1, 2, Connectivity.FOUR)
            >>> 
            >>> # Check result
            >>> assert np.all(filled_grid[3:7, 3:7] == 2)
    )pbdoc";

    // Expose Connectivity enum
    py::enum_<Connectivity>(m, "Connectivity",
                            "Connectivity type for flood fill")
        .value("FOUR", Connectivity::Four,
               "4-way connectivity (up, down, left, right)")
        .value("EIGHT", Connectivity::Eight,
               "8-way connectivity (including diagonals)")
        .export_values();

    // Register exception translator
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument &e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error &e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception &e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Expose FloodFill BFS method
    m.def(
        "fill_bfs",
        [](py::array_t<int> grid, int start_x, int start_y, int target_color,
           int fill_color, Connectivity conn) {
            // Convert numpy array to vector of vectors
            std::vector<std::vector<int>> cpp_grid = numpy_to_vector(grid);

            // Call C++ function
            atom::algorithm::FloodFill::fillBFS(cpp_grid, start_x, start_y,
                                                target_color, fill_color, conn);

            // Convert back to numpy array
            return vector_to_numpy(cpp_grid);
        },
        py::arg("grid"), py::arg("start_x"), py::arg("start_y"),
        py::arg("target_color"), py::arg("fill_color"),
        py::arg("connectivity") = Connectivity::Four,
        R"pbdoc(
        Perform flood fill using Breadth-First Search (BFS).

        Args:
            grid (numpy.ndarray): 2D grid to perform flood fill on
            start_x (int): Starting X coordinate
            start_y (int): Starting Y coordinate
            target_color (int): Color to be replaced
            fill_color (int): Color to fill with
            connectivity (Connectivity): Type of connectivity (FOUR or EIGHT)

        Returns:
            numpy.ndarray: Grid with flood fill applied

        Raises:
            ValueError: If grid is empty or coordinates are invalid
       )pbdoc");

    // Expose FloodFill DFS method
    m.def(
        "fill_dfs",
        [](py::array_t<int> grid, int start_x, int start_y, int target_color,
           int fill_color, Connectivity conn) {
            // Convert numpy array to vector of vectors
            std::vector<std::vector<int>> cpp_grid = numpy_to_vector(grid);

            // Call C++ function
            atom::algorithm::FloodFill::fillDFS(cpp_grid, start_x, start_y,
                                                target_color, fill_color, conn);

            // Convert back to numpy array
            return vector_to_numpy(cpp_grid);
        },
        py::arg("grid"), py::arg("start_x"), py::arg("start_y"),
        py::arg("target_color"), py::arg("fill_color"),
        py::arg("connectivity") = Connectivity::Four,
        R"pbdoc(
        Perform flood fill using Depth-First Search (DFS).

        Args:
            grid (numpy.ndarray): 2D grid to perform flood fill on
            start_x (int): Starting X coordinate
            start_y (int): Starting Y coordinate
            target_color (int): Color to be replaced
            fill_color (int): Color to fill with
            connectivity (Connectivity): Type of connectivity (FOUR or EIGHT)

        Returns:
            numpy.ndarray: Grid with flood fill applied

        Raises:
            ValueError: If grid is empty or coordinates are invalid
       )pbdoc");

    // Expose FloodFill Parallel method
    m.def(
        "fill_parallel",
        [](py::array_t<int> grid, int start_x, int start_y, int target_color,
           int fill_color, Connectivity conn, unsigned int num_threads) {
            // Convert numpy array to vector of vectors
            std::vector<std::vector<int>> cpp_grid = numpy_to_vector(grid);

            // Call C++ function
            atom::algorithm::FloodFill::fillParallel(cpp_grid, start_x, start_y,
                                                     target_color, fill_color,
                                                     conn, num_threads);

            // Convert back to numpy array
            return vector_to_numpy(cpp_grid);
        },
        py::arg("grid"), py::arg("start_x"), py::arg("start_y"),
        py::arg("target_color"), py::arg("fill_color"),
        py::arg("connectivity") = Connectivity::Four,
        py::arg("num_threads") = std::thread::hardware_concurrency(),
        R"pbdoc(
        Perform parallel flood fill using multiple threads.

        Args:
            grid (numpy.ndarray): 2D grid to perform flood fill on
            start_x (int): Starting X coordinate
            start_y (int): Starting Y coordinate
            target_color (int): Color to be replaced
            fill_color (int): Color to fill with
            connectivity (Connectivity): Type of connectivity (FOUR or EIGHT)
            num_threads (int): Number of threads to use (default: hardware concurrency)

        Returns:
            numpy.ndarray: Grid with flood fill applied

        Raises:
            ValueError: If grid is empty or coordinates are invalid
       )pbdoc");

    // Helper function to create a grid
    m.def(
        "create_grid",
        [](int rows, int cols, int value) {
            std::vector<std::vector<int>> grid(rows,
                                               std::vector<int>(cols, value));
            return vector_to_numpy(grid);
        },
        py::arg("rows"), py::arg("cols"), py::arg("value") = 0,
        R"pbdoc(
        Create a 2D grid filled with a single value.

        Args:
            rows (int): Number of rows
            cols (int): Number of columns
            value (int): Value to fill the grid with (default: 0)

        Returns:
            numpy.ndarray: Created grid
       )pbdoc");

    // Utility function to visualize a grid
    m.def(
        "visualize_grid",
        [](py::array_t<int> grid) {
            try {
                // Import matplotlib
                py::object plt = py::module::import("matplotlib.pyplot");

                // Create figure
                plt.attr("figure")();
                plt.attr("imshow")(grid, py::arg("cmap") = "viridis",
                                   py::arg("interpolation") = "nearest");
                plt.attr("colorbar")();
                plt.attr("grid")(true);
                plt.attr("show")();

                return true;
            } catch (py::error_already_set &e) {
                // Handle case where matplotlib is not available
                py::print("Error: Could not visualize grid.");
                py::print(
                    "Make sure matplotlib is installed: pip install "
                    "matplotlib");
                e.restore();
                return false;
            }
        },
        py::arg("grid"),
        R"pbdoc(
        Visualize a grid using matplotlib.

        Args:
            grid (numpy.ndarray): 2D grid to visualize

        Returns:
            bool: True if visualization was successful, False otherwise

        Note:
            Requires matplotlib to be installed.
       )pbdoc");

    // Additional utility for creating test patterns
    m.def(
        "create_maze_pattern",
        [](int rows, int cols, int wall_value, int path_value, float complexity,
           float density) {
            // Input validation
            if (rows < 5 || cols < 5) {
                throw std::invalid_argument(
                    "Rows and columns must be at least 5");
            }
            if (complexity < 0.0 || complexity > 1.0) {
                throw std::invalid_argument(
                    "Complexity must be between 0.0 and 1.0");
            }
            if (density < 0.0 || density > 1.0) {
                throw std::invalid_argument(
                    "Density must be between 0.0 and 1.0");
            }

            std::vector<std::vector<int>> maze(
                rows, std::vector<int>(cols, wall_value));

            int complexity_factor =
                static_cast<int>(complexity * (5 * (rows + cols)));
            complexity_factor = std::max(1, complexity_factor);

            int density_param =
                static_cast<int>(density * ((static_cast<float>(rows) / 2.0f) *
                                            (static_cast<float>(cols) / 2.0f)));

            for (int i = 1; i < rows - 1; i += 2) {
                for (int j = 1; j < cols - 1; j += 2) {
                    maze[i][j] = path_value;
                    // Create passages
                    if (i < rows - 2)
                        maze[i + 1][j] = path_value;
                    if (j < cols - 2)
                        maze[i][j + 1] = path_value;
                }
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> distribution_rows(0, rows - 1);
            std::uniform_int_distribution<int> distribution_cols(0, cols - 1);

            int walls_to_add = density_param + (complexity_factor / 10);

            for (int i = 0; i < walls_to_add; i++) {
                int x = distribution_rows(gen);
                int y = distribution_cols(gen);
                if (x > 0 && x < rows - 1 && y > 0 && y < cols - 1) {
                    maze[x][y] = wall_value;
                }
            }

            return vector_to_numpy(maze);
        },
        py::arg("rows") = 20, py::arg("cols") = 20, py::arg("wall_value") = 1,
        py::arg("path_value") = 0, py::arg("complexity") = 0.75,
        py::arg("density") = 0.50,
        R"pbdoc(
        Create a random maze pattern for testing flood fill algorithms.

        Args:
            rows (int): Number of rows (default: 20)
            cols (int): Number of columns (default: 20)
            wall_value (int): Value for walls (default: 1)
            path_value (int): Value for paths (default: 0)
            complexity (float): Complexity of the maze (0.0-1.0, default: 0.75)
            density (float): Density of walls (0.0-1.0, default: 0.5)

        Returns:
            numpy.ndarray: Maze pattern grid

        Raises:
            ValueError: If input parameters are invalid
       )pbdoc");

    // Add function to compare performance of different algorithms
    m.def(
        "compare_performance",
        [](py::array_t<int> grid, int start_x, int start_y, int target_color,
           int fill_color, Connectivity conn) {
            // Convert numpy array to vector of vectors
            std::vector<std::vector<int>> cpp_grid = numpy_to_vector(grid);
            std::vector<std::vector<int>> grid_copy1 = cpp_grid;
            std::vector<std::vector<int>> grid_copy2 = cpp_grid;

            // Import time for timing
            py::object time = py::module::import("time");

            // Time BFS
            double bfs_start = time.attr("time")().cast<double>();
            atom::algorithm::FloodFill::fillBFS(cpp_grid, start_x, start_y,
                                                target_color, fill_color, conn);
            double bfs_end = time.attr("time")().cast<double>();
            double bfs_time = bfs_end - bfs_start;

            // Time DFS
            double dfs_start = time.attr("time")().cast<double>();
            atom::algorithm::FloodFill::fillDFS(grid_copy1, start_x, start_y,
                                                target_color, fill_color, conn);
            double dfs_end = time.attr("time")().cast<double>();
            double dfs_time = dfs_end - dfs_start;

            // Time Parallel
            double parallel_start = time.attr("time")().cast<double>();
            atom::algorithm::FloodFill::fillParallel(
                grid_copy2, start_x, start_y, target_color, fill_color, conn);
            double parallel_end = time.attr("time")().cast<double>();
            double parallel_time = parallel_end - parallel_start;

            // Return timing results
            py::dict results;
            results["bfs_time"] = bfs_time;
            results["dfs_time"] = dfs_time;
            results["parallel_time"] = parallel_time;

            // Calculate speedup
            results["parallel_speedup_vs_bfs"] = bfs_time / parallel_time;
            results["parallel_speedup_vs_dfs"] = dfs_time / parallel_time;

            return results;
        },
        py::arg("grid"), py::arg("start_x"), py::arg("start_y"),
        py::arg("target_color"), py::arg("fill_color"),
        py::arg("connectivity") = Connectivity::Four,
        R"pbdoc(
        Compare performance of different flood fill algorithms.

        This function runs BFS, DFS, and parallel flood fill on the same grid
        and returns timing information.

        Args:
            grid (numpy.ndarray): 2D grid to perform flood fill on
            start_x (int): Starting X coordinate
            start_y (int): Starting Y coordinate
            target_color (int): Color to be replaced
            fill_color (int): Color to fill with
            connectivity (Connectivity): Type of connectivity (FOUR or EIGHT)

        Returns:
            dict: Dictionary with timing results and speedup information
       )pbdoc");

    // Helper for applying flood fill to images
    m.def(
        "fill_image",
        [](py::array_t<uint8_t> image, int start_x, int start_y,
           py::tuple target_color, py::tuple fill_color, Connectivity conn) {
            // Convert tuple to vector for target and fill colors
            std::vector<uint8_t> target_color_vec(3);
            std::vector<uint8_t> fill_color_vec(3);

            for (int i = 0; i < 3; i++) {
                target_color_vec[i] = target_color[i].cast<uint8_t>();
                fill_color_vec[i] = fill_color[i].cast<uint8_t>();
            }

            // Get image dimensions
            auto buf = image.request();
            if (buf.ndim != 3) {
                throw std::runtime_error(
                    "Image must be 3-dimensional (height, width, channels)");
            }

            // Create a copy of the image
            py::array_t<uint8_t> result_image =
                py::array_t<uint8_t>(buf.shape, buf.strides);
            auto result_buf = result_image.request();
            std::memcpy(result_buf.ptr, buf.ptr, buf.size * sizeof(uint8_t));

            // Get dimensions
            int height = buf.shape[0];
            int width = buf.shape[1];
            int channels = buf.shape[2];

            if (channels != 3) {
                throw std::runtime_error("Image must have 3 channels (RGB)");
            }

            // Create a mask for the flood fill
            std::vector<std::vector<int>> mask(height,
                                               std::vector<int>(width, 0));

            // Set 1s in the mask where target color matches - 修复void指针算术
            uint8_t *ptr = static_cast<uint8_t *>(buf.ptr);
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    bool matches = true;
                    for (int c = 0; c < 3; c++) {
                        if (ptr[y * buf.strides[0] / sizeof(uint8_t) +
                                x * buf.strides[1] / sizeof(uint8_t) +
                                c * buf.strides[2] / sizeof(uint8_t)] !=
                            target_color_vec[c]) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches) {
                        mask[y][x] = 1;
                    }
                }
            }

            // Apply flood fill to the mask
            atom::algorithm::FloodFill::fillBFS(mask, start_y, start_x, 1, 2,
                                                conn);

            // Apply the fill color to the result image where mask == 2 -
            // 修复void指针算术
            uint8_t *result_ptr = static_cast<uint8_t *>(result_buf.ptr);
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    if (mask[y][x] == 2) {
                        for (int c = 0; c < 3; c++) {
                            result_ptr
                                [y * result_buf.strides[0] / sizeof(uint8_t) +
                                 x * result_buf.strides[1] / sizeof(uint8_t) +
                                 c * result_buf.strides[2] / sizeof(uint8_t)] =
                                    fill_color_vec[c];
                        }
                    }
                }
            }

            return result_image;
        },
        py::arg("image"), py::arg("start_x"), py::arg("start_y"),
        py::arg("target_color"), py::arg("fill_color"),
        py::arg("connectivity") = Connectivity::Four,
        R"pbdoc(
        Apply flood fill to an RGB image.

        Args:
            image (numpy.ndarray): 3D array representing an RGB image
            start_x (int): Starting X coordinate
            start_y (int): Starting Y coordinate
            target_color (tuple): RGB tuple of the color to replace
            fill_color (tuple): RGB tuple of the color to fill with
            connectivity (Connectivity): Type of connectivity (FOUR or EIGHT)

        Returns:
            numpy.ndarray: Image with flood fill applied

        Raises:
            RuntimeError: If image is not 3D or doesn't have 3 channels
       )pbdoc");
}