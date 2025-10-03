#include "atom/algorithm/graphics/flood.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(flood_fill, m) {
    m.doc() = R"pbdoc(
        Flood Fill Algorithms with NumPy Integration
        -------------------------------------------

        This module provides high-performance flood fill algorithms optimized for
        image processing and computer graphics applications.

        Features:
        - BFS and DFS flood fill implementations
        - 4-way and 8-way connectivity options
        - SIMD optimizations for large images
        - Parallel processing support
        - Direct NumPy array integration
        - Boundary checking and validation

        Applications:
        - Paint bucket tool implementation
        - Connected component analysis
        - Region segmentation
        - Game development (territory marking)
        - Medical image processing

        Examples:
            >>> from atom.algorithm.flood_fill import flood_fill_bfs, Connectivity
            >>> import numpy as np
            >>> 
            >>> # Create a simple 2D grid
            >>> grid = np.array([[1, 1, 0, 0],
            ...                  [1, 0, 0, 1],
            ...                  [0, 0, 1, 1],
            ...                  [0, 1, 1, 1]], dtype=np.int32)
            >>> 
            >>> # Fill starting from position (0, 0), changing 1s to 2s
            >>> filled_count = flood_fill_bfs(grid, 0, 0, target_color=1, 
            ...                              fill_color=2, connectivity=Connectivity.Four)
            >>> print(f"Filled {filled_count} cells")
            >>> print(grid)
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::RuntimeError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // Connectivity enum
    py::enum_<Connectivity>(m, "Connectivity", R"pbdoc(
        Connectivity options for flood fill algorithms.

        Four: 4-way connectivity (up, down, left, right)
        Eight: 8-way connectivity (includes diagonals)
    )pbdoc")
        .value("Four", Connectivity::Four, "4-way connectivity")
        .value("Eight", Connectivity::Eight, "8-way connectivity");

    // BFS flood fill with NumPy integration
    m.def("flood_fill_bfs", [](py::array_t<int32_t> grid, int start_x, int start_y,
                              int32_t target_color, int32_t fill_color,
                              Connectivity conn) {
        py::buffer_info buf = grid.request();
        
        if (buf.ndim != 2) {
            throw std::invalid_argument("Grid must be a 2D array");
        }
        
        int height = static_cast<int>(buf.shape[0]);
        int width = static_cast<int>(buf.shape[1]);
        int32_t* ptr = static_cast<int32_t*>(buf.ptr);
        
        // Convert NumPy array to std::vector<std::vector<int32_t>>
        std::vector<std::vector<int32_t>> cpp_grid(height, std::vector<int32_t>(width));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                cpp_grid[y][x] = ptr[y * width + x];
            }
        }
        
        // Perform flood fill
        size_t filled_count = atom::algorithm::FloodFill::fillBFS(
            cpp_grid, start_x, start_y, target_color, fill_color, conn);
        
        // Copy result back to NumPy array
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                ptr[y * width + x] = cpp_grid[y][x];
            }
        }
        
        return filled_count;
    }, py::arg("grid"), py::arg("start_x"), py::arg("start_y"), 
       py::arg("target_color"), py::arg("fill_color"), 
       py::arg("connectivity") = Connectivity::Four,
    R"pbdoc(
    Perform breadth-first search flood fill on a 2D grid.
    
    Args:
        grid: 2D NumPy array of integers (modified in-place)
        start_x: Starting X coordinate (row)
        start_y: Starting Y coordinate (column)
        target_color: Color value to replace
        fill_color: New color value
        connectivity: Connectivity type (Four or Eight)
        
    Returns:
        Number of cells that were filled
        
    Examples:
        >>> grid = np.array([[1, 1, 0], [1, 0, 1], [0, 1, 1]], dtype=np.int32)
        >>> count = flood_fill_bfs(grid, 0, 0, 1, 2)
        >>> print(f"Filled {count} cells")
    )pbdoc");

    // DFS flood fill with NumPy integration
    m.def("flood_fill_dfs", [](py::array_t<int32_t> grid, int start_x, int start_y,
                              int32_t target_color, int32_t fill_color,
                              Connectivity conn) {
        py::buffer_info buf = grid.request();
        
        if (buf.ndim != 2) {
            throw std::invalid_argument("Grid must be a 2D array");
        }
        
        int height = static_cast<int>(buf.shape[0]);
        int width = static_cast<int>(buf.shape[1]);
        int32_t* ptr = static_cast<int32_t*>(buf.ptr);
        
        // Convert NumPy array to std::vector<std::vector<int32_t>>
        std::vector<std::vector<int32_t>> cpp_grid(height, std::vector<int32_t>(width));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                cpp_grid[y][x] = ptr[y * width + x];
            }
        }
        
        // Perform flood fill
        size_t filled_count = atom::algorithm::FloodFill::fillDFS(
            cpp_grid, start_x, start_y, target_color, fill_color, conn);
        
        // Copy result back to NumPy array
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                ptr[y * width + x] = cpp_grid[y][x];
            }
        }
        
        return filled_count;
    }, py::arg("grid"), py::arg("start_x"), py::arg("start_y"), 
       py::arg("target_color"), py::arg("fill_color"), 
       py::arg("connectivity") = Connectivity::Four,
    R"pbdoc(
    Perform depth-first search flood fill on a 2D grid.
    
    Args:
        grid: 2D NumPy array of integers (modified in-place)
        start_x: Starting X coordinate (row)
        start_y: Starting Y coordinate (column)
        target_color: Color value to replace
        fill_color: New color value
        connectivity: Connectivity type (Four or Eight)
        
    Returns:
        Number of cells that were filled
        
    Examples:
        >>> grid = np.array([[1, 1, 0], [1, 0, 1], [0, 1, 1]], dtype=np.int32)
        >>> count = flood_fill_dfs(grid, 0, 0, 1, 2)
        >>> print(f"Filled {count} cells")
    )pbdoc");

    // Find connected components
    m.def("find_connected_components", [](py::array_t<int32_t> grid,
                                         Connectivity conn) {
        py::buffer_info buf = grid.request();
        
        if (buf.ndim != 2) {
            throw std::invalid_argument("Grid must be a 2D array");
        }
        
        int height = static_cast<int>(buf.shape[0]);
        int width = static_cast<int>(buf.shape[1]);
        int32_t* ptr = static_cast<int32_t*>(buf.ptr);
        
        // Create result array for component labels
        py::array_t<int32_t> labels = py::array_t<int32_t>({height, width});
        py::buffer_info labels_buf = labels.request();
        int32_t* labels_ptr = static_cast<int32_t*>(labels_buf.ptr);
        
        // Initialize labels to -1 (unvisited)
        std::fill(labels_ptr, labels_ptr + height * width, -1);
        
        int32_t component_id = 0;
        std::vector<size_t> component_sizes;
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (labels_ptr[y * width + x] == -1 && ptr[y * width + x] != 0) {
                    // Found unvisited non-zero cell, start new component
                    std::vector<std::vector<int32_t>> temp_grid(height, std::vector<int32_t>(width));
                    std::vector<std::vector<int32_t>> temp_labels(height, std::vector<int32_t>(width, -1));
                    
                    // Copy data to temporary grids
                    for (int ty = 0; ty < height; ++ty) {
                        for (int tx = 0; tx < width; ++tx) {
                            temp_grid[ty][tx] = ptr[ty * width + tx];
                            temp_labels[ty][tx] = labels_ptr[ty * width + tx];
                        }
                    }
                    
                    // Perform flood fill to find component
                    int32_t target_value = ptr[y * width + x];
                    size_t component_size = atom::algorithm::FloodFill::fillBFS(
                        temp_grid, y, x, target_value, component_id, conn);
                    
                    // Copy labels back
                    for (int ty = 0; ty < height; ++ty) {
                        for (int tx = 0; tx < width; ++tx) {
                            if (temp_grid[ty][tx] == component_id) {
                                labels_ptr[ty * width + tx] = component_id;
                            }
                        }
                    }
                    
                    component_sizes.push_back(component_size);
                    component_id++;
                }
            }
        }
        
        return py::make_tuple(labels, component_sizes);
    }, py::arg("grid"), py::arg("connectivity") = Connectivity::Four,
    R"pbdoc(
    Find all connected components in a 2D grid.
    
    Args:
        grid: 2D NumPy array of integers
        connectivity: Connectivity type (Four or Eight)
        
    Returns:
        Tuple of (labels, sizes) where:
        - labels: 2D array with component IDs for each cell
        - sizes: List of component sizes
        
    Examples:
        >>> grid = np.array([[1, 1, 0, 2], [1, 0, 0, 2], [0, 0, 2, 2]], dtype=np.int32)
        >>> labels, sizes = find_connected_components(grid)
        >>> print(f"Found {len(sizes)} components with sizes: {sizes}")
    )pbdoc");

    // Utility functions
    m.def("create_test_grid", [](int width, int height, double fill_ratio, int seed) {
        std::mt19937 gen(seed);
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        py::array_t<int32_t> grid = py::array_t<int32_t>({height, width});
        py::buffer_info buf = grid.request();
        int32_t* ptr = static_cast<int32_t*>(buf.ptr);
        
        for (int i = 0; i < height * width; ++i) {
            ptr[i] = (dis(gen) < fill_ratio) ? 1 : 0;
        }
        
        return grid;
    }, py::arg("width"), py::arg("height"), py::arg("fill_ratio") = 0.5, py::arg("seed") = 42,
    R"pbdoc(
    Create a test grid with random values for testing flood fill algorithms.
    
    Args:
        width: Grid width
        height: Grid height
        fill_ratio: Probability of cell being 1 vs 0
        seed: Random seed
        
    Returns:
        2D NumPy array with random 0s and 1s
    )pbdoc");
}
