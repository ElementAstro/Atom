#include "atom/algorithm/math/matrix.hpp"

#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Template function to bind Matrix classes of different sizes
template <typename T, size_t Rows, size_t Cols>
void bindMatrix(py::module& m, const std::string& suffix) {
    using MatrixType = atom::algorithm::Matrix<T, Rows, Cols>;

    std::string class_name =
        "Matrix" + std::to_string(Rows) + "x" + std::to_string(Cols) + suffix;
    std::string class_doc =
        "Matrix class with " + std::to_string(Rows) + " rows and " +
        std::to_string(Cols) + " columns of " + suffix + " elements.\n\n" +
        "This is a compile-time fixed-size matrix optimized for performance.";

    py::class_<MatrixType>(m, class_name.c_str(), class_doc.c_str())
        // Constructors
        .def(py::init<>(), "Default constructor - creates zero matrix.")
        .def(py::init<const std::array<T, Rows * Cols>&>(), py::arg("data"),
             "Constructs matrix from array data in row-major order.")

        // Element access
        .def(
            "__call__",
            [](MatrixType& self, size_t row, size_t col) -> T& {
                if (row >= Rows || col >= Cols) {
                    throw py::index_error("Matrix index out of bounds");
                }
                return self(row, col);
            },
            py::arg("row"), py::arg("col"),
            py::return_value_policy::reference_internal,
            "Access matrix element at (row, col).")
        .def(
            "__call__",
            [](const MatrixType& self, size_t row, size_t col) -> T {
                if (row >= Rows || col >= Cols) {
                    throw py::index_error("Matrix index out of bounds");
                }
                return self(row, col);
            },
            py::arg("row"), py::arg("col"),
            "Access matrix element at (row, col) (const version).")

        // Properties
        .def_property_readonly(
            "rows", [](const MatrixType&) { return Rows; },
            "Number of rows in the matrix.")
        .def_property_readonly(
            "cols", [](const MatrixType&) { return Cols; },
            "Number of columns in the matrix.")
        .def_property_readonly(
            "size", [](const MatrixType&) { return Rows * Cols; },
            "Total number of elements in the matrix.")

        // Data access
        .def(
            "get_data",
            [](const MatrixType& self) {
                const auto& data = self.getData();
                return std::vector<T>(data.begin(), data.end());
            },
            "Returns matrix data as a list in row-major order.")
        .def(
            "set_data",
            [](MatrixType& self, const std::vector<T>& data) {
                if (data.size() != Rows * Cols) {
                    throw py::value_error(
                        "Data size must match matrix dimensions");
                }
                auto& matrix_data = self.getData();
                std::copy(data.begin(), data.end(), matrix_data.begin());
            },
            py::arg("data"), "Sets matrix data from a list in row-major order.")

        // Matrix operations (only for square matrices)
        .def(
            "trace",
            [](const MatrixType& self) -> T {
                static_assert(Rows == Cols,
                              "Trace only defined for square matrices");
                return self.trace();
            },
            "Computes the trace (sum of diagonal elements) - square matrices "
            "only.")
        .def(
            "determinant",
            [](const MatrixType& self) -> T {
                static_assert(Rows == Cols,
                              "Determinant only defined for square matrices");
                return self.determinant();
            },
            "Computes the determinant - square matrices only.")
        .def(
            "inverse",
            [](const MatrixType& self) -> MatrixType {
                static_assert(Rows == Cols,
                              "Inverse only defined for square matrices");
                return self.inverse();
            },
            "Computes the matrix inverse - square matrices only.")

        // Utility methods
        .def("print", &MatrixType::print, py::arg("width") = 8,
             py::arg("precision") = 2,
             "Prints the matrix to stdout with specified formatting.")
        .def("fill", &MatrixType::fill, py::arg("value"),
             "Fills all matrix elements with the specified value.")
        .def("frobenius_norm", &MatrixType::frobeniusNorm,
             "Computes the Frobenius norm of the matrix.")
        .def("max_element", &MatrixType::maxElement,
             "Finds the maximum element in the matrix.")
        .def("min_element", &MatrixType::minElement,
             "Finds the minimum element in the matrix.")
        .def(
            "is_symmetric",
            [](const MatrixType& self) -> bool {
                static_assert(Rows == Cols,
                              "Symmetry only defined for square matrices");
                return self.isSymmetric();
            },
            "Checks if the matrix is symmetric - square matrices only.")
        .def(
            "condition_number",
            [](const MatrixType& self) -> T {
                static_assert(
                    Rows == Cols,
                    "Condition number only defined for square matrices");
                return self.conditionNumber();
            },
            "Computes the condition number - square matrices only.")

        // Python special methods
        .def(
            "__str__",
            [](const MatrixType& self) {
                std::ostringstream oss;
                oss << "Matrix" << Rows << "x" << Cols << "(\n";
                for (size_t i = 0; i < Rows; ++i) {
                    oss << "  [";
                    for (size_t j = 0; j < Cols; ++j) {
                        oss << self(i, j);
                        if (j < Cols - 1)
                            oss << ", ";
                    }
                    oss << "]";
                    if (i < Rows - 1)
                        oss << ",";
                    oss << "\n";
                }
                oss << ")";
                return oss.str();
            },
            "String representation of the matrix.")
        .def(
            "__repr__",
            [suffix](const MatrixType&) {
                return "Matrix" + std::to_string(Rows) + "x" +
                       std::to_string(Cols) + suffix + "()";
            },
            "Representation string for the matrix.")

        // Arithmetic operators
        .def(py::self + py::self, "Matrix addition")
        .def(py::self - py::self, "Matrix subtraction")
        .def(py::self * T(), "Scalar multiplication")
        .def(T() * py::self, "Scalar multiplication (left)")
        .def(py::self / T(), "Scalar division")

        // Comparison operators
        .def(py::self == py::self, "Matrix equality comparison")
        .def(py::self != py::self, "Matrix inequality comparison")

        // NumPy integration
        .def(
            "to_numpy",
            [](const MatrixType& self) {
                auto result = py::array_t<T>({Rows, Cols});
                auto buf = result.request();
                T* ptr = static_cast<T*>(buf.ptr);
                const auto& data = self.getData();
                std::copy(data.begin(), data.end(), ptr);
                return result;
            },
            "Converts matrix to NumPy array.")
        .def_static(
            "from_numpy",
            [](py::array_t<T> input) {
                py::buffer_info buf = input.request();
                if (buf.ndim != 2 || buf.shape[0] != Rows ||
                    buf.shape[1] != Cols) {
                    throw py::value_error(
                        "Array dimensions must match matrix size");
                }
                T* ptr = static_cast<T*>(buf.ptr);
                std::array<T, Rows * Cols> data;
                std::copy(ptr, ptr + Rows * Cols, data.begin());
                return MatrixType(data);
            },
            py::arg("array"), "Creates matrix from NumPy array.");
}

// Template function to bind matrix multiplication for compatible dimensions
template <typename T, size_t RowsA, size_t ColsA_RowsB, size_t ColsB>
void bindMatrixMultiplication(py::module& m, const std::string& suffix) {
    using MatrixA = atom::algorithm::Matrix<T, RowsA, ColsA_RowsB>;
    using MatrixB = atom::algorithm::Matrix<T, ColsA_RowsB, ColsB>;
    using MatrixResult = atom::algorithm::Matrix<T, RowsA, ColsB>;

    std::string func_name = "multiply_" + std::to_string(RowsA) + "x" +
                            std::to_string(ColsA_RowsB) + "_" +
                            std::to_string(ColsA_RowsB) + "x" +
                            std::to_string(ColsB) + suffix;

    m.def(
        func_name.c_str(),
        [](const MatrixA& a, const MatrixB& b) -> MatrixResult {
            return a * b;
        },
        py::arg("a"), py::arg("b"),
        ("Matrix multiplication: " + std::to_string(RowsA) + "x" +
         std::to_string(ColsA_RowsB) + " * " + std::to_string(ColsA_RowsB) +
         "x" + std::to_string(ColsB))
            .c_str());
}

PYBIND11_MODULE(matrix, m) {
    m.doc() = R"pbdoc(
        Matrix Operations and Linear Algebra
        ------------------------------------

        This module provides compile-time fixed-size matrix classes optimized for performance.
        It supports common matrix operations, linear algebra functions, and NumPy integration.

        Features:
        - Compile-time fixed-size matrices for optimal performance
        - All standard matrix operations (addition, subtraction, multiplication)
        - Linear algebra operations (determinant, inverse, trace, norm)
        - NumPy array integration for seamless interoperability
        - Template specializations for common matrix sizes
        - Exception-safe operations with proper error handling

        Examples:
            >>> from atom.algorithm.matrix import Matrix3x3Double, identity3x3_double
            >>>
            >>> # Create matrices
            >>> mat1 = Matrix3x3Double()
            >>> mat1.fill(1.0)
            >>>
            >>> # Create identity matrix
            >>> identity = identity3x3_double()
            >>>
            >>> # Matrix operations
            >>> result = mat1 + identity
            >>> det = result.determinant()
            >>>
            >>> # NumPy integration
            >>> import numpy as np
            >>> np_array = result.to_numpy()
            >>> mat_from_np = Matrix3x3Double.from_numpy(np_array)
    )pbdoc";

    // Bind common matrix sizes for double precision
    bindMatrix<double, 2, 2>(m, "Double");
    bindMatrix<double, 3, 3>(m, "Double");
    bindMatrix<double, 4, 4>(m, "Double");
    bindMatrix<double, 2, 3>(m, "Double");
    bindMatrix<double, 3, 2>(m, "Double");
    bindMatrix<double, 3, 4>(m, "Double");
    bindMatrix<double, 4, 3>(m, "Double");

    // Bind common matrix sizes for float precision
    bindMatrix<float, 2, 2>(m, "Float");
    bindMatrix<float, 3, 3>(m, "Float");
    bindMatrix<float, 4, 4>(m, "Float");
    bindMatrix<float, 2, 3>(m, "Float");
    bindMatrix<float, 3, 2>(m, "Float");

    // Bind common matrix sizes for integer
    bindMatrix<int, 2, 2>(m, "Int");
    bindMatrix<int, 3, 3>(m, "Int");
    bindMatrix<int, 4, 4>(m, "Int");

    // Bind matrix multiplication functions for common combinations
    bindMatrixMultiplication<double, 2, 2, 2>(m, "_double");
    bindMatrixMultiplication<double, 3, 3, 3>(m, "_double");
    bindMatrixMultiplication<double, 4, 4, 4>(m, "_double");
    bindMatrixMultiplication<double, 2, 3, 2>(m, "_double");
    bindMatrixMultiplication<double, 3, 2, 3>(m, "_double");

    // Identity matrix factory functions
    m.def(
        "identity2x2_double",
        []() -> atom::algorithm::Matrix<double, 2, 2> {
            return atom::algorithm::identity<double, 2>();
        },
        "Creates a 2x2 double identity matrix.");
    m.def(
        "identity3x3_double",
        []() -> atom::algorithm::Matrix<double, 3, 3> {
            return atom::algorithm::identity<double, 3>();
        },
        "Creates a 3x3 double identity matrix.");
    m.def(
        "identity4x4_double",
        []() -> atom::algorithm::Matrix<double, 4, 4> {
            return atom::algorithm::identity<double, 4>();
        },
        "Creates a 4x4 double identity matrix.");
    m.def(
        "identity2x2_float",
        []() -> atom::algorithm::Matrix<float, 2, 2> {
            return atom::algorithm::identity<float, 2>();
        },
        "Creates a 2x2 float identity matrix.");
    m.def(
        "identity3x3_float",
        []() -> atom::algorithm::Matrix<float, 3, 3> {
            return atom::algorithm::identity<float, 3>();
        },
        "Creates a 3x3 float identity matrix.");
    m.def(
        "identity4x4_float",
        []() -> atom::algorithm::Matrix<float, 4, 4> {
            return atom::algorithm::identity<float, 4>();
        },
        "Creates a 4x4 float identity matrix.");

    // Random matrix factory functions
    m.def(
        "random_matrix2x2_double",
        [](double min, double max) {
            return atom::algorithm::randomMatrix<double, 2, 2>(min, max);
        },
        py::arg("min") = 0.0, py::arg("max") = 1.0,
        "Creates a 2x2 double matrix with random values.");
    m.def(
        "random_matrix3x3_double",
        [](double min, double max) {
            return atom::algorithm::randomMatrix<double, 3, 3>(min, max);
        },
        py::arg("min") = 0.0, py::arg("max") = 1.0,
        "Creates a 3x3 double matrix with random values.");
    m.def(
        "random_matrix4x4_double",
        [](double min, double max) {
            return atom::algorithm::randomMatrix<double, 4, 4>(min, max);
        },
        py::arg("min") = 0.0, py::arg("max") = 1.0,
        "Creates a 4x4 double matrix with random values.");

    // Transpose functions for common matrix sizes
    m.def(
        "transpose2x2_double",
        [](const atom::algorithm::Matrix<double, 2, 2>& mat) {
            return atom::algorithm::transpose(mat);
        },
        py::arg("matrix"), "Transposes a 2x2 double matrix.");

    m.def(
        "transpose3x3_double",
        [](const atom::algorithm::Matrix<double, 3, 3>& mat) {
            return atom::algorithm::transpose(mat);
        },
        py::arg("matrix"), "Transposes a 3x3 double matrix.");

    m.def(
        "transpose4x4_double",
        [](const atom::algorithm::Matrix<double, 4, 4>& mat) {
            return atom::algorithm::transpose(mat);
        },
        py::arg("matrix"), "Transposes a 4x4 double matrix.");

    m.def(
        "transpose2x3_double",
        [](const atom::algorithm::Matrix<double, 2, 3>& mat) {
            return atom::algorithm::transpose(mat);
        },
        py::arg("matrix"), "Transposes a 2x3 double matrix to 3x2.");

    m.def(
        "transpose3x2_double",
        [](const atom::algorithm::Matrix<double, 3, 2>& mat) {
            return atom::algorithm::transpose(mat);
        },
        py::arg("matrix"), "Transposes a 3x2 double matrix to 2x3.");

    // LU Decomposition functions
    m.def(
        "lu_decomposition2x2_double",
        [](const atom::algorithm::Matrix<double, 2, 2>& mat) {
            return atom::algorithm::luDecomposition(mat);
        },
        py::arg("matrix"), "Performs LU decomposition on a 2x2 double matrix.");

    m.def(
        "lu_decomposition3x3_double",
        [](const atom::algorithm::Matrix<double, 3, 3>& mat) {
            return atom::algorithm::luDecomposition(mat);
        },
        py::arg("matrix"), "Performs LU decomposition on a 3x3 double matrix.");

    m.def(
        "lu_decomposition4x4_double",
        [](const atom::algorithm::Matrix<double, 4, 4>& mat) {
            return atom::algorithm::luDecomposition(mat);
        },
        py::arg("matrix"), "Performs LU decomposition on a 4x4 double matrix.");

    // Singular Value Decomposition functions
    m.def(
        "svd2x2_double",
        [](const atom::algorithm::Matrix<double, 2, 2>& mat) {
            return atom::algorithm::singularValueDecomposition(mat);
        },
        py::arg("matrix"),
        "Performs SVD on a 2x2 double matrix, returns singular values.");

    m.def(
        "svd3x3_double",
        [](const atom::algorithm::Matrix<double, 3, 3>& mat) {
            return atom::algorithm::singularValueDecomposition(mat);
        },
        py::arg("matrix"),
        "Performs SVD on a 3x3 double matrix, returns singular values.");

    m.def(
        "svd4x4_double",
        [](const atom::algorithm::Matrix<double, 4, 4>& mat) {
            return atom::algorithm::singularValueDecomposition(mat);
        },
        py::arg("matrix"),
        "Performs SVD on a 4x4 double matrix, returns singular values.");
}
