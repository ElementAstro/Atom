#include "atom/algorithm/core/simd_utils.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(simd_utils, m) {
    m.doc() = R"pbdoc(
        SIMD Utilities
        --------------

        This module provides SIMD-accelerated operations for numerical computations.
        The actual SIMD instruction set used (AVX512, AVX2, SSE, NEON) depends on
        compile-time flags and CPU capabilities.

        All operations automatically fall back to scalar implementations when
        SIMD instructions are not available.
    )pbdoc";

    using namespace atom::algorithm::simd;

    // VectorWidth constants
    py::class_<VectorWidth>(m, "VectorWidth",
                            "SIMD vector width constants for different "
                            "instruction sets")
        .def_readonly_static("AVX512_F32", &VectorWidth::AVX512_F32,
                             "AVX-512 float32 vector width (16)")
        .def_readonly_static("AVX512_F64", &VectorWidth::AVX512_F64,
                             "AVX-512 float64 vector width (8)")
        .def_readonly_static("AVX2_F32", &VectorWidth::AVX2_F32,
                             "AVX2 float32 vector width (8)")
        .def_readonly_static("AVX2_F64", &VectorWidth::AVX2_F64,
                             "AVX2 float64 vector width (4)")
        .def_readonly_static("SSE_F32", &VectorWidth::SSE_F32,
                             "SSE float32 vector width (4)")
        .def_readonly_static("SSE_F64", &VectorWidth::SSE_F64,
                             "SSE float64 vector width (2)")
        .def_readonly_static("NEON_F32", &VectorWidth::NEON_F32,
                             "NEON float32 vector width (4)")
        .def_readonly_static("NEON_F64", &VectorWidth::NEON_F64,
                             "NEON float64 vector width (2)");

    // MathOps - SIMD-optimized mathematical operations
    m.def(
        "vector_add_f32",
        [](py::array_t<float> a, py::array_t<float> b) {
            if (a.size() != b.size()) {
                throw std::invalid_argument(
                    "Input arrays must have the same size");
            }

            auto result = py::array_t<float>(a.size());
            auto a_ptr = a.data();
            auto b_ptr = b.data();
            auto result_ptr = result.mutable_data();

            MathOps::vectorAdd(a_ptr, b_ptr, result_ptr, a.size());

            return result;
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
        SIMD-optimized vector addition for float32 arrays.

        Args:
            a: First input array
            b: Second input array

        Returns:
            Element-wise sum of a and b

        Raises:
            ValueError: If input arrays have different sizes
    )pbdoc");

    m.def(
        "vector_add_f64",
        [](py::array_t<double> a, py::array_t<double> b) {
            if (a.size() != b.size()) {
                throw std::invalid_argument(
                    "Input arrays must have the same size");
            }

            auto result = py::array_t<double>(a.size());
            auto a_ptr = a.data();
            auto b_ptr = b.data();
            auto result_ptr = result.mutable_data();

            MathOps::vectorAdd(a_ptr, b_ptr, result_ptr, a.size());

            return result;
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
        SIMD-optimized vector addition for float64 arrays.

        Args:
            a: First input array
            b: Second input array

        Returns:
            Element-wise sum of a and b

        Raises:
            ValueError: If input arrays have different sizes
    )pbdoc");

    m.def(
        "dot_product_f32",
        [](py::array_t<float> a, py::array_t<float> b) -> float {
            if (a.size() != b.size()) {
                throw std::invalid_argument(
                    "Input arrays must have the same size");
            }

            auto a_ptr = a.data();
            auto b_ptr = b.data();

            return MathOps::dotProduct(a_ptr, b_ptr, a.size());
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
        SIMD-optimized dot product for float32 arrays.

        Args:
            a: First input array
            b: Second input array

        Returns:
            Dot product of a and b

        Raises:
            ValueError: If input arrays have different sizes
    )pbdoc");

    m.def(
        "dot_product_f64",
        [](py::array_t<double> a, py::array_t<double> b) -> double {
            if (a.size() != b.size()) {
                throw std::invalid_argument(
                    "Input arrays must have the same size");
            }

            auto a_ptr = a.data();
            auto b_ptr = b.data();

            return MathOps::dotProduct(a_ptr, b_ptr, a.size());
        },
        py::arg("a"), py::arg("b"),
        R"pbdoc(
        SIMD-optimized dot product for float64 arrays.

        Args:
            a: First input array
            b: Second input array

        Returns:
            Dot product of a and b

        Raises:
            ValueError: If input arrays have different sizes
    )pbdoc");

    // Optimal vector width functions
    m.def("get_optimal_vector_width_f32", &getOptimalVectorWidth<float>,
          "Get optimal SIMD vector width for float32 on this platform");

    m.def("get_optimal_vector_width_f64", &getOptimalVectorWidth<double>,
          "Get optimal SIMD vector width for float64 on this platform");

    // SIMD capability detection
#ifdef ATOM_SIMD_AVX512
    m.attr("simd_capability") = "AVX512";
#elif defined(ATOM_SIMD_AVX2)
    m.attr("simd_capability") = "AVX2";
#elif defined(ATOM_SIMD_SSE42)
    m.attr("simd_capability") = "SSE4.2";
#elif defined(ATOM_SIMD_SSE41)
    m.attr("simd_capability") = "SSE4.1";
#elif defined(ATOM_SIMD_SSE2)
    m.attr("simd_capability") = "SSE2";
#elif defined(ATOM_SIMD_NEON)
    m.attr("simd_capability") = "NEON";
#else
    m.attr("simd_capability") = "SCALAR";
#endif

    // MemoryOps bindings - simplified for Python
    m.def(
        "optimized_copy",
        [](py::array src) {
            auto result = py::array(src.dtype(), src.shape());
            MemoryOps::copy(result.mutable_data(), src.data(), src.nbytes());
            return result;
        },
        py::arg("src"),
        "SIMD-optimized memory copy. Returns a copy of the input array.");

    m.def(
        "optimized_fill",
        [](py::array arr, uint8_t value) {
            MemoryOps::fill(arr.mutable_data(), value, arr.nbytes());
            return arr;
        },
        py::arg("arr"), py::arg("value"),
        "SIMD-optimized memory fill. Fills array with the given byte value "
        "(modifies in-place).");
}
