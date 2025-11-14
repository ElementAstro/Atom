#include "atom/algorithm/math/gpu_math.hpp"
#include "atom/algorithm/core/simd_utils.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(gpu_math, m) {
    m.doc() = R"pbdoc(
        GPU-Accelerated Mathematical Operations
        -------------------------------------

        This module provides GPU-accelerated mathematical operations using OpenCL
        and SIMD-optimized fallbacks for high-performance computing.

        Features:
        - GPU-accelerated vector operations (addition, multiplication, dot product)
        - GPU-accelerated matrix operations (multiplication, transpose)
        - Statistical computations with GPU acceleration
        - Prime number generation using GPU sieve algorithms
        - SIMD-optimized fallbacks when GPU is not available
        - Automatic fallback to CPU implementations for small datasets

        Examples:
            >>> from atom.algorithm.gpu_math import GPUMath, SIMDCapabilities
            >>>
            >>> # Check GPU availability
            >>> gpu = GPUMath.get_instance()
            >>> if gpu.initialize():
            ...     print("GPU acceleration available")
            ... else:
            ...     print("Using CPU fallback")
            >>>
            >>> # Vector operations
            >>> a = [1.0, 2.0, 3.0, 4.0]
            >>> b = [5.0, 6.0, 7.0, 8.0]
            >>> result = gpu.vector_add(a, b)
            >>>
            >>> # Check SIMD capabilities
            >>> if SIMDCapabilities.has_avx2():
            ...     print("AVX2 SIMD available")
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

    // GPU Math class bindings
    py::class_<atom::algorithm::gpu::GPUMath>(m, "GPUMath", R"pbdoc(
        GPU-accelerated mathematical operations using OpenCL.

        This singleton class provides access to GPU-accelerated mathematical
        operations with automatic fallback to CPU implementations when GPU
        is not available or for small datasets.
    )pbdoc")
        .def_static("get_instance", &atom::algorithm::gpu::GPUMath::getInstance,
                    py::return_value_policy::reference,
                    "Get the singleton instance of GPUMath.")

        .def(
            "initialize", &atom::algorithm::gpu::GPUMath::initialize,
            "Initialize GPU math operations. Returns True if GPU is available.")

        .def("is_available", &atom::algorithm::gpu::GPUMath::isAvailable,
             "Check if GPU acceleration is currently available.")

        .def(
            "vector_add",
            [](atom::algorithm::gpu::GPUMath& self, const std::vector<float>& a,
               const std::vector<float>& b) { return self.vectorAdd(a, b); },
            py::arg("a"), py::arg("b"),
            R"pbdoc(
        GPU-accelerated vector addition.

        Args:
            a: First input vector
            b: Second input vector (must be same size as a)

        Returns:
            Vector containing element-wise sum of a and b

        Raises:
            ValueError: If vector sizes don't match
            RuntimeError: If GPU is not available and no fallback
        )pbdoc")

        .def(
            "vector_multiply",
            [](atom::algorithm::gpu::GPUMath& self, const std::vector<float>& a,
               const std::vector<float>& b) {
                return self.vectorMultiply(a, b);
            },
            py::arg("a"), py::arg("b"),
            R"pbdoc(
        GPU-accelerated element-wise vector multiplication.

        Args:
            a: First input vector
            b: Second input vector (must be same size as a)

        Returns:
            Vector containing element-wise product of a and b
        )pbdoc")

        .def(
            "dot_product",
            [](atom::algorithm::gpu::GPUMath& self, const std::vector<float>& a,
               const std::vector<float>& b) { return self.dotProduct(a, b); },
            py::arg("a"), py::arg("b"),
            R"pbdoc(
        GPU-accelerated dot product computation.

        Automatically uses CPU implementation for small vectors (< 1024 elements)
        for optimal performance.

        Args:
            a: First input vector
            b: Second input vector (must be same size as a)

        Returns:
            Dot product of the two vectors
        )pbdoc")

        .def(
            "matrix_multiply",
            [](atom::algorithm::gpu::GPUMath& self, const std::vector<float>& a,
               const std::vector<float>& b, size_t rows_a, size_t cols_a,
               size_t cols_b) {
                return self.matrixMultiply(a, b, rows_a, cols_a, cols_b);
            },
            py::arg("a"), py::arg("b"), py::arg("rows_a"), py::arg("cols_a"),
            py::arg("cols_b"),
            R"pbdoc(
        GPU-accelerated matrix multiplication.

        Matrices are stored in row-major order as flat vectors.

        Args:
            a: First matrix (rows_a × cols_a elements)
            b: Second matrix (cols_a × cols_b elements)
            rows_a: Number of rows in matrix a
            cols_a: Number of columns in matrix a (must equal rows in matrix b)
            cols_b: Number of columns in matrix b

        Returns:
            Result matrix (rows_a × cols_b elements) in row-major order
        )pbdoc")

        .def(
            "matrix_transpose",
            [](atom::algorithm::gpu::GPUMath& self,
               const std::vector<float>& matrix, size_t rows, size_t cols) {
                return self.matrixTranspose(matrix, rows, cols);
            },
            py::arg("matrix"), py::arg("rows"), py::arg("cols"),
            R"pbdoc(
        GPU-accelerated matrix transpose.

        Args:
            matrix: Input matrix in row-major order
            rows: Number of rows in input matrix
            cols: Number of columns in input matrix

        Returns:
            Transposed matrix (cols × rows) in row-major order
        )pbdoc")

        .def(
            "generate_primes",
            [](atom::algorithm::gpu::GPUMath& self, uint32_t limit) {
                return self.generatePrimes(limit);
            },
            py::arg("limit"),
            R"pbdoc(
        GPU-accelerated prime number generation using sieve algorithm.

        Args:
            limit: Upper limit for prime generation

        Returns:
            Vector of prime numbers up to the specified limit
        )pbdoc")

        .def(
            "calculate_mean",
            [](atom::algorithm::gpu::GPUMath& self,
               const std::vector<float>& data) {
                return self.calculateMean(data);
            },
            py::arg("data"),
            R"pbdoc(
        GPU-accelerated statistical mean calculation.

        Args:
            data: Input data vector

        Returns:
            Arithmetic mean of the input data
        )pbdoc")

        .def(
            "calculate_variance",
            [](atom::algorithm::gpu::GPUMath& self,
               const std::vector<float>& data) {
                return self.calculateVariance(data);
            },
            py::arg("data"),
            R"pbdoc(
        GPU-accelerated variance calculation.

        Args:
            data: Input data vector

        Returns:
            Variance of the input data
        )pbdoc");

    // SIMD Capabilities class
    py::class_<atom::algorithm::simd::SIMDCapabilities>(m, "SIMDCapabilities",
                                                        R"pbdoc(
        SIMD capability detection and information.

        Provides runtime detection of available SIMD instruction sets
        for optimal performance selection.
    )pbdoc")
        .def_static("has_sse2",
                    &atom::algorithm::simd::SIMDCapabilities::hasSSE2,
                    "Check if SSE2 instructions are available.")
        .def_static("has_avx2",
                    &atom::algorithm::simd::SIMDCapabilities::hasAVX2,
                    "Check if AVX2 instructions are available.")
        .def_static("has_avx512",
                    &atom::algorithm::simd::SIMDCapabilities::hasAVX512,
                    "Check if AVX-512 instructions are available.")
        .def_static("has_neon",
                    &atom::algorithm::simd::SIMDCapabilities::hasNEON,
                    "Check if ARM NEON instructions are available.");

    // Memory Operations
    py::class_<atom::algorithm::simd::MemoryOps>(m, "SIMDMemoryOps", R"pbdoc(
        SIMD-optimized memory operations.

        Provides high-performance memory operations using SIMD instructions
        when data alignment and size requirements are met.
    )pbdoc")
        .def_static(
            "copy",
            [](py::array_t<uint8_t> dest, py::array_t<uint8_t> src) {
                py::buffer_info dest_info = dest.request();
                py::buffer_info src_info = src.request();

                if (dest_info.size != src_info.size) {
                    throw std::invalid_argument("Array sizes must match");
                }

                atom::algorithm::simd::MemoryOps::copy(
                    dest_info.ptr, src_info.ptr, dest_info.size);
            },
            py::arg("dest"), py::arg("src"),
            R"pbdoc(
        SIMD-optimized memory copy.

        Uses the fastest available SIMD instructions when alignment
        and size requirements are met, falls back to standard memcpy otherwise.

        Args:
            dest: Destination array
            src: Source array (must be same size as dest)
        )pbdoc");
}
