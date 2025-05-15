#include "atom/algorithm/matrix_compress.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(matrix_compress, m) {
    m.doc() = "Matrix compression module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const MatrixCompressException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const MatrixDecompressException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // CompressedData type alias for clarity in Python
    using CompressedData = atom::algorithm::MatrixCompressor::CompressedData;
    using Matrix = atom::algorithm::MatrixCompressor::Matrix;

    // Register MatrixCompressor class
    py::class_<atom::algorithm::MatrixCompressor>(
        m, "MatrixCompressor",
        R"(A class for compressing and decompressing matrices using run-length encoding.

This class provides static methods to compress and decompress matrices, as well as
various utility functions for working with compressed matrices.

Examples:
    >>> from atom.algorithm.matrix_compress import MatrixCompressor
    >>> # Create a simple matrix
    >>> matrix = [['A', 'A', 'B'], ['B', 'C', 'C']]
    >>> # Compress it
    >>> compressed = MatrixCompressor.compress(matrix)
    >>> # Decompress it back
    >>> decompressed = MatrixCompressor.decompress(compressed, 2, 3)
)")
        .def_static("compress", &atom::algorithm::MatrixCompressor::compress,
                    py::arg("matrix"),
                    R"(Compresses a matrix using run-length encoding.

Args:
    matrix: The matrix to compress (list of lists of characters)

Returns:
    The compressed data (list of (char, count) pairs)

Raises:
    RuntimeError: If compression fails
)")
        .def_static("compress_parallel",
                    &atom::algorithm::MatrixCompressor::compressParallel,
                    py::arg("matrix"), py::arg("thread_count") = 0,
                    R"(Compresses a large matrix using multiple threads.

Args:
    matrix: The matrix to compress (list of lists of characters)
    thread_count: Number of threads to use (0 for system default)

Returns:
    The compressed data (list of (char, count) pairs)

Raises:
    RuntimeError: If compression fails
)")
        .def_static("decompress",
                    &atom::algorithm::MatrixCompressor::decompress,
                    py::arg("compressed"), py::arg("rows"), py::arg("cols"),
                    R"(Decompresses data into a matrix.

Args:
    compressed: The compressed data (list of (char, count) pairs)
    rows: The number of rows in the decompressed matrix
    cols: The number of columns in the decompressed matrix

Returns:
    The decompressed matrix (list of lists of characters)

Raises:
    RuntimeError: If decompression fails
)")
        .def_static("decompress_parallel",
                    &atom::algorithm::MatrixCompressor::decompressParallel,
                    py::arg("compressed"), py::arg("rows"), py::arg("cols"),
                    py::arg("thread_count") = 0,
                    R"(Decompresses a large matrix using multiple threads.

Args:
    compressed: The compressed data (list of (char, count) pairs)
    rows: The number of rows in the decompressed matrix
    cols: The number of columns in the decompressed matrix
    thread_count: Number of threads to use (0 for system default)

Returns:
    The decompressed matrix (list of lists of characters)

Raises:
    RuntimeError: If decompression fails
)")
        .def_static(
            "print_matrix",
            [](const py::object& matrix) {
                // Convert Python matrix to C++ matrix and print it
                try {
                    Matrix cpp_matrix;
                    for (auto row : matrix) {
                        std::vector<char> cpp_row;
                        for (auto item : row) {
                            // Get character from Python object (string or char)
                            std::string item_str = py::str(item);
                            if (!item_str.empty()) {
                                cpp_row.push_back(item_str[0]);
                            }
                        }
                        cpp_matrix.push_back(cpp_row);
                    }
                    atom::algorithm::MatrixCompressor::printMatrix(cpp_matrix);
                } catch (const py::error_already_set& e) {
                    throw;
                } catch (const std::exception& e) {
                    throw py::value_error("Invalid matrix format: " +
                                          std::string(e.what()));
                }
            },
            py::arg("matrix"),
            R"(Prints the matrix to the standard output.

Args:
    matrix: The matrix to print (list of lists of characters)
)")
        .def_static("generate_random_matrix",
                    &atom::algorithm::MatrixCompressor::generateRandomMatrix,
                    py::arg("rows"), py::arg("cols"),
                    py::arg("charset") = "ABCD",
                    R"(Generates a random matrix.

Args:
    rows: The number of rows in the matrix
    cols: The number of columns in the matrix
    charset: The set of characters to use for generating the matrix (default: "ABCD")

Returns:
    The generated random matrix (list of lists of characters)

Raises:
    ValueError: If rows or cols are not positive
)")
        .def_static("save_compressed_to_file",
                    &atom::algorithm::MatrixCompressor::saveCompressedToFile,
                    py::arg("compressed"), py::arg("filename"),
                    R"(Saves the compressed data to a file.

Args:
    compressed: The compressed data to save (list of (char, count) pairs)
    filename: The name of the file to save the data to

Raises:
    RuntimeError: If the file cannot be opened
)")
        .def_static("load_compressed_from_file",
                    &atom::algorithm::MatrixCompressor::loadCompressedFromFile,
                    py::arg("filename"),
                    R"(Loads compressed data from a file.

Args:
    filename: The name of the file to load the data from

Returns:
    The loaded compressed data (list of (char, count) pairs)

Raises:
    RuntimeError: If the file cannot be opened
)")
        .def_static(
            "calculate_compression_ratio",
            [](const py::object& matrix, const CompressedData& compressed) {
                // Convert Python matrix to C++ matrix for calculation
                try {
                    Matrix cpp_matrix;
                    for (auto row : matrix) {
                        std::vector<char> cpp_row;
                        for (auto item : row) {
                            // Get character from Python object (string or char)
                            std::string item_str = py::str(item);
                            if (!item_str.empty()) {
                                cpp_row.push_back(item_str[0]);
                            }
                        }
                        cpp_matrix.push_back(cpp_row);
                    }
                    return atom::algorithm::MatrixCompressor::
                        calculateCompressionRatio(cpp_matrix, compressed);
                } catch (const py::error_already_set& e) {
                    throw;
                } catch (const std::exception& e) {
                    throw py::value_error("Invalid matrix format: " +
                                          std::string(e.what()));
                }
            },
            py::arg("matrix"), py::arg("compressed"),
            R"(Calculates the compression ratio.

Args:
    matrix: The original matrix (list of lists of characters)
    compressed: The compressed data (list of (char, count) pairs)

Returns:
    The compression ratio (compressed size / original size)
)")
        .def_static(
            "downsample",
            [](const py::object& matrix, int factor) {
                // Convert Python matrix to C++ matrix for downsampling
                try {
                    Matrix cpp_matrix;
                    for (auto row : matrix) {
                        std::vector<char> cpp_row;
                        for (auto item : row) {
                            // Get character from Python object (string or char)
                            std::string item_str = py::str(item);
                            if (!item_str.empty()) {
                                cpp_row.push_back(item_str[0]);
                            }
                        }
                        cpp_matrix.push_back(cpp_row);
                    }
                    return atom::algorithm::MatrixCompressor::downsample(
                        cpp_matrix, factor);
                } catch (const py::error_already_set& e) {
                    throw;
                } catch (const std::exception& e) {
                    throw py::value_error("Invalid matrix format or factor: " +
                                          std::string(e.what()));
                }
            },
            py::arg("matrix"), py::arg("factor"),
            R"(Downsamples a matrix by a given factor.

Args:
    matrix: The matrix to downsample (list of lists of characters)
    factor: The downsampling factor

Returns:
    The downsampled matrix (list of lists of characters)

Raises:
    ValueError: If factor is not positive
)")
        .def_static(
            "upsample",
            [](const py::object& matrix, int factor) {
                // Convert Python matrix to C++ matrix for upsampling
                try {
                    Matrix cpp_matrix;
                    for (auto row : matrix) {
                        std::vector<char> cpp_row;
                        for (auto item : row) {
                            // Get character from Python object (string or char)
                            std::string item_str = py::str(item);
                            if (!item_str.empty()) {
                                cpp_row.push_back(item_str[0]);
                            }
                        }
                        cpp_matrix.push_back(cpp_row);
                    }
                    return atom::algorithm::MatrixCompressor::upsample(
                        cpp_matrix, factor);
                } catch (const py::error_already_set& e) {
                    throw;
                } catch (const std::exception& e) {
                    throw py::value_error("Invalid matrix format or factor: " +
                                          std::string(e.what()));
                }
            },
            py::arg("matrix"), py::arg("factor"),
            R"(Upsamples a matrix by a given factor.

Args:
    matrix: The matrix to upsample (list of lists of characters)
    factor: The upsampling factor

Returns:
    The upsampled matrix (list of lists of characters)

Raises:
    ValueError: If factor is not positive
)")
        .def_static(
            "calculate_mse",
            [](const py::object& matrix1, const py::object& matrix2) {
                // Convert Python matrices to C++ matrices for MSE calculation
                try {
                    Matrix cpp_matrix1, cpp_matrix2;

                    // Convert first matrix
                    for (auto row : matrix1) {
                        std::vector<char> cpp_row;
                        for (auto item : row) {
                            std::string item_str = py::str(item);
                            if (!item_str.empty()) {
                                cpp_row.push_back(item_str[0]);
                            }
                        }
                        cpp_matrix1.push_back(cpp_row);
                    }

                    // Convert second matrix
                    for (auto row : matrix2) {
                        std::vector<char> cpp_row;
                        for (auto item : row) {
                            std::string item_str = py::str(item);
                            if (!item_str.empty()) {
                                cpp_row.push_back(item_str[0]);
                            }
                        }
                        cpp_matrix2.push_back(cpp_row);
                    }

                    return atom::algorithm::MatrixCompressor::calculateMSE(
                        cpp_matrix1, cpp_matrix2);
                } catch (const py::error_already_set& e) {
                    throw;
                } catch (const std::exception& e) {
                    throw py::value_error("Error calculating MSE: " +
                                          std::string(e.what()));
                }
            },
            py::arg("matrix1"), py::arg("matrix2"),
            R"(Calculates the mean squared error (MSE) between two matrices.

Args:
    matrix1: The first matrix (list of lists of characters)
    matrix2: The second matrix (list of lists of characters)

Returns:
    The mean squared error

Raises:
    ValueError: If matrices have different dimensions
)");

#if ATOM_ENABLE_DEBUG
    // Add performance test function if debug is enabled
    m.def("performance_test", &atom::algorithm::performanceTest,
          py::arg("rows"), py::arg("cols"), py::arg("run_parallel") = true,
          R"(Runs a performance test on matrix compression and decompression.

Args:
    rows: The number of rows in the test matrix
    cols: The number of columns in the test matrix
    run_parallel: Whether to test parallel versions (default: True)
)");
#endif

    // Add utility functions for easier Python usage
    m.def("compress_matrix", &atom::algorithm::MatrixCompressor::compress,
          py::arg("matrix"),
          R"(Compresses a matrix using run-length encoding.

A convenience function that calls MatrixCompressor.compress.

Args:
    matrix: The matrix to compress (list of lists of characters)

Returns:
    The compressed data (list of (char, count) pairs)

Examples:
    >>> from atom.algorithm.matrix_compress import compress_matrix
    >>> matrix = [['A', 'A', 'B'], ['B', 'C', 'C']]
    >>> compressed = compress_matrix(matrix)
)");

    m.def("decompress_data", &atom::algorithm::MatrixCompressor::decompress,
          py::arg("compressed"), py::arg("rows"), py::arg("cols"),
          R"(Decompresses data into a matrix.

A convenience function that calls MatrixCompressor.decompress.

Args:
    compressed: The compressed data (list of (char, count) pairs)
    rows: The number of rows in the decompressed matrix
    cols: The number of columns in the decompressed matrix

Returns:
    The decompressed matrix (list of lists of characters)

Examples:
    >>> from atom.algorithm.matrix_compress import compress_matrix, decompress_data
    >>> matrix = [['A', 'A', 'B'], ['B', 'C', 'C']]
    >>> compressed = compress_matrix(matrix)
    >>> decompressed = decompress_data(compressed, 2, 3)
)");

    // Improved version with unchecked access for better performance
    m.def(
        "compress_numpy_array",
        [](py::array_t<char, py::array::c_style | py::array::forcecast> array) {
            py::buffer_info buf = array.request();

            if (buf.ndim != 2) {
                throw py::value_error("Input must be a 2D numpy array");
            }

            Matrix matrix(buf.shape[0], std::vector<char>(buf.shape[1]));

            // Use unchecked access for better performance
            auto r = array.unchecked<2>();

            for (py::ssize_t i = 0; i < r.shape(0); i++) {
                for (py::ssize_t j = 0; j < r.shape(1); j++) {
                    matrix[i][j] = r(i, j);
                }
            }

            return atom::algorithm::MatrixCompressor::compress(matrix);
        },
        py::arg("array"),
        R"(Compresses a 2D numpy array using run-length encoding.

Args:
    array: A 2D numpy array of characters to compress

Returns:
    The compressed data (list of (char, count) pairs)

Examples:
    >>> import numpy as np
    >>> from atom.algorithm.matrix_compress import compress_numpy_array
    >>> arr = np.array([['A', 'A', 'B'], ['B', 'C', 'C']], dtype='c')
    >>> compressed = compress_numpy_array(arr)
)");

    // Improved version with unchecked access for better performance
    m.def(
        "decompress_to_numpy",
        [](const CompressedData& compressed, int rows, int cols) {
            Matrix matrix = atom::algorithm::MatrixCompressor::decompress(
                compressed, rows, cols);

            py::array_t<char> result({rows, cols});

            // Use mutable_unchecked for direct, efficient write access
            auto r = result.mutable_unchecked<2>();

            for (py::ssize_t i = 0; i < rows; i++) {
                for (py::ssize_t j = 0; j < cols; j++) {
                    r(i, j) = matrix[i][j];
                }
            }

            return result;
        },
        py::arg("compressed"), py::arg("rows"), py::arg("cols"),
        R"(Decompresses data into a numpy array.

Args:
    compressed: The compressed data (list of (char, count) pairs)
    rows: The number of rows in the decompressed array
    cols: The number of columns in the decompressed array

Returns:
    A 2D numpy array containing the decompressed data

Examples:
    >>> import numpy as np
    >>> from atom.algorithm.matrix_compress import compress_numpy_array, decompress_to_numpy
    >>> arr = np.array([['A', 'A', 'B'], ['B', 'C', 'C']], dtype='c')
    >>> compressed = compress_numpy_array(arr)
    >>> decompressed = decompress_to_numpy(compressed, 2, 3)
)");

    // Add a vectorized version of the decompress function for better
    // performance
    m.def(
        "decompress_vectorized",
        [](const CompressedData& compressed, int rows, int cols) {
            // A vectorized version that directly outputs to a numpy array
            py::array_t<char> result({rows, cols});
            auto r = result.mutable_unchecked<2>();

            size_t idx = 0;
            for (const auto& pair : compressed) {
                char value = pair.first;
                int count = pair.second;

                for (int i = 0; i < count; i++) {
                    size_t row = idx / cols;
                    size_t col = idx % cols;

                    if (row < static_cast<size_t>(rows) &&
                        col < static_cast<size_t>(cols)) {
                        r(row, col) = value;
                    }

                    idx++;
                }
            }

            return result;
        },
        py::arg("compressed"), py::arg("rows"), py::arg("cols"),
        R"(Vectorized decompression of data into a numpy array.

This function is optimized for performance by directly writing to a numpy array.

Args:
    compressed: The compressed data (list of (char, count) pairs)
    rows: The number of rows in the decompressed array
    cols: The number of columns in the decompressed array

Returns:
    A 2D numpy array containing the decompressed data

Examples:
    >>> from atom.algorithm.matrix_compress import compress_matrix, decompress_vectorized
    >>> matrix = [['A', 'A', 'B'], ['B', 'C', 'C']]
    >>> compressed = compress_matrix(matrix)
    >>> decompressed = decompress_vectorized(compressed, 2, 3)
)");

    // Add a function to convert NumPy arrays to matrices
    m.def(
        "numpy_to_matrix",
        [](py::array_t<char, py::array::c_style | py::array::forcecast> array) {
            py::buffer_info buf = array.request();

            if (buf.ndim != 2) {
                throw py::value_error("Input must be a 2D numpy array");
            }

            Matrix matrix(buf.shape[0], std::vector<char>(buf.shape[1]));

            // Use unchecked access for better performance
            auto r = array.unchecked<2>();

            for (py::ssize_t i = 0; i < r.shape(0); i++) {
                for (py::ssize_t j = 0; j < r.shape(1); j++) {
                    matrix[i][j] = r(i, j);
                }
            }

            return matrix;
        },
        py::arg("array"),
        R"(Converts a 2D numpy array to a matrix format.

Args:
    array: A 2D numpy array of characters

Returns:
    A matrix (list of lists of characters)

Examples:
    >>> import numpy as np
    >>> from atom.algorithm.matrix_compress import numpy_to_matrix
    >>> arr = np.array([['A', 'A', 'B'], ['B', 'C', 'C']], dtype='c')
    >>> matrix = numpy_to_matrix(arr)
)");

    // Add a function to convert matrices to NumPy arrays
    m.def(
        "matrix_to_numpy",
        [](const Matrix& matrix) {
            if (matrix.empty()) {
                return py::array_t<char>(std::vector<ssize_t>{0, 0});
            }

            size_t rows = matrix.size();
            size_t cols = matrix[0].size();

            py::array_t<char> result({rows, cols});
            auto r = result.mutable_unchecked<2>();

            for (py::ssize_t i = 0; i < rows; i++) {
                for (py::ssize_t j = 0; j < cols; j++) {
                    r(i, j) = matrix[i][j];
                }
            }

            return result;
        },
        py::arg("matrix"),
        R"(Converts a matrix to a 2D numpy array.

Args:
    matrix: A matrix (list of lists of characters)

Returns:
    A 2D numpy array containing the same data

Examples:
    >>> from atom.algorithm.matrix_compress import matrix_to_numpy
    >>> matrix = [['A', 'A', 'B'], ['B', 'C', 'C']]
    >>> arr = matrix_to_numpy(matrix)
)");

    // Add a vectorized version that handles both compression methods
    m.def("vectorized_compress",
          py::vectorize([](char c1, char c2, char c3) -> py::tuple {
              std::vector<std::vector<char>> matrix = {{c1, c2, c3}};
              auto compressed =
                  atom::algorithm::MatrixCompressor::compress(matrix);
              return py::make_tuple(compressed.size(), compressed[0].first);
          }),
          R"(A vectorized version of compression for element-wise operations.

This demonstrates using py::vectorize for element-wise operations.

Args:
    c1, c2, c3: Input characters to compress as a row

Returns:
    A tuple containing (compressed_size, first_character)

Examples:
    >>> import numpy as np
    >>> from atom.algorithm.matrix_compress import vectorized_compress
    >>> a = np.array(['A', 'A', 'B'])
    >>> b = np.array(['B', 'C', 'C'])
    >>> c = np.array(['D', 'D', 'D'])
    >>> vectorized_compress(a, b, c)  # Element-wise operation
)");

    // Define a class with buffer protocol support for efficient NumPy interop
    struct CharMatrix {
        std::vector<char> data;
        size_t rows, cols;

        CharMatrix(const Matrix& matrix) : rows(matrix.size()), cols(0) {
            if (rows > 0) {
                cols = matrix[0].size();
                data.resize(rows * cols);

                for (size_t i = 0; i < rows; i++) {
                    for (size_t j = 0; j < cols; j++) {
                        data[i * cols + j] = matrix[i][j];
                    }
                }
            }
        }
    };

    py::class_<CharMatrix>(m, "CharMatrix", py::buffer_protocol())
        .def(py::init<Matrix>())
        .def_property_readonly("rows",
                               [](const CharMatrix& m) { return m.rows; })
        .def_property_readonly("cols",
                               [](const CharMatrix& m) { return m.cols; })
        .def_buffer([](CharMatrix& m) -> py::buffer_info {
            return py::buffer_info(
                m.data.data(),                         /* Pointer to buffer */
                sizeof(char),                          /* Size of one scalar */
                py::format_descriptor<char>::format(), /* Python struct-style
                                                          format descriptor */
                2,                /* Number of dimensions */
                {m.rows, m.cols}, /* Buffer dimensions */
                {sizeof(char) * m.cols, sizeof(char)}
                /* Strides (in bytes) for each index */
            );
        });

    m.def(
        "decompress_to_charmatrix",
        [](const CompressedData& compressed, int rows, int cols) {
            Matrix matrix = atom::algorithm::MatrixCompressor::decompress(
                compressed, rows, cols);
            return CharMatrix(matrix);
        },
        py::arg("compressed"), py::arg("rows"), py::arg("cols"),
        R"(Decompresses data into a CharMatrix object with buffer protocol support.

The CharMatrix supports the buffer protocol for efficient interfacing with NumPy.

Args:
    compressed: The compressed data (list of (char, count) pairs)
    rows: The number of rows in the decompressed matrix
    cols: The number of columns in the decompressed matrix

Returns:
    A CharMatrix object containing the decompressed data

Examples:
    >>> import numpy as np
    >>> from atom.algorithm.matrix_compress import compress_matrix, decompress_to_charmatrix
    >>> matrix = [['A', 'A', 'B'], ['B', 'C', 'C']]
    >>> compressed = compress_matrix(matrix)
    >>> char_matrix = decompress_to_charmatrix(compressed, 2, 3)
    >>> # Convert to numpy array efficiently using the buffer protocol
    >>> np_array = np.array(char_matrix)
)");
}
