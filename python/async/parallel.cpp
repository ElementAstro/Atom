#include "atom/async/parallel.hpp"

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Helper function for SIMD operations with numpy arrays
template <typename T>
void simd_add_arrays(py::array_t<T> a, py::array_t<T> b,
                     py::array_t<T> result) {
    py::buffer_info a_info = a.request();
    py::buffer_info b_info = b.request();
    py::buffer_info result_info = result.request();

    // Check dimensions
    if (a_info.ndim != 1 || b_info.ndim != 1 || result_info.ndim != 1) {
        throw std::invalid_argument("Number of dimensions must be 1");
    }

    if (a_info.shape[0] != b_info.shape[0] ||
        a_info.shape[0] != result_info.shape[0]) {
        throw std::invalid_argument("Input shapes must match");
    }

    size_t size = a_info.shape[0];

    // Get pointers to the data
    T* a_ptr = static_cast<T*>(a_info.ptr);
    T* b_ptr = static_cast<T*>(b_info.ptr);
    T* result_ptr = static_cast<T*>(result_info.ptr);

    // Call SimdOps add function
    atom::async::SimdOps::add(a_ptr, b_ptr, result_ptr, size);
}

template <typename T>
void simd_multiply_arrays(py::array_t<T> a, py::array_t<T> b,
                          py::array_t<T> result) {
    py::buffer_info a_info = a.request();
    py::buffer_info b_info = b.request();
    py::buffer_info result_info = result.request();

    // Check dimensions
    if (a_info.ndim != 1 || b_info.ndim != 1 || result_info.ndim != 1) {
        throw std::invalid_argument("Number of dimensions must be 1");
    }

    if (a_info.shape[0] != b_info.shape[0] ||
        a_info.shape[0] != result_info.shape[0]) {
        throw std::invalid_argument("Input shapes must match");
    }

    size_t size = a_info.shape[0];

    // Get pointers to the data
    T* a_ptr = static_cast<T*>(a_info.ptr);
    T* b_ptr = static_cast<T*>(b_info.ptr);
    T* result_ptr = static_cast<T*>(result_info.ptr);

    // Call SimdOps multiply function
    atom::async::SimdOps::multiply(a_ptr, b_ptr, result_ptr, size);
}

template <typename T>
T simd_dot_product(py::array_t<T> a, py::array_t<T> b) {
    py::buffer_info a_info = a.request();
    py::buffer_info b_info = b.request();

    // Check dimensions
    if (a_info.ndim != 1 || b_info.ndim != 1) {
        throw std::invalid_argument("Number of dimensions must be 1");
    }

    if (a_info.shape[0] != b_info.shape[0]) {
        throw std::invalid_argument("Input shapes must match");
    }

    size_t size = a_info.shape[0];

    // Get pointers to the data
    T* a_ptr = static_cast<T*>(a_info.ptr);
    T* b_ptr = static_cast<T*>(b_info.ptr);

    // Call SimdOps dotProduct function
    return atom::async::SimdOps::dotProduct(a_ptr, b_ptr, size);
}

// Python wrappers for Parallel class template functions
template <typename T>
py::list parallel_map_list(const std::vector<T>& items,
                           const py::function& func, size_t num_threads = 0) {
    auto result = atom::async::Parallel::map(
        items.begin(), items.end(),
        [&func](const T& item) {
            return func(item).template cast<py::object>();
        },
        num_threads);

    py::list py_result;
    for (const auto& item : result) {
        py_result.append(item);
    }
    return py_result;
}

template <typename T>
std::vector<T> parallel_filter_list(const std::vector<T>& items,
                                    const py::function& pred,
                                    size_t num_threads = 0) {
    return atom::async::Parallel::filter(
        items.begin(), items.end(),
        [&pred](const T& item) { return pred(item).template cast<bool>(); },
        num_threads);
}

template <typename T, typename U>
U parallel_reduce_list(const std::vector<T>& items, U init,
                       const py::function& binary_op, size_t num_threads = 0) {
    return atom::async::Parallel::reduce(
        items.begin(), items.end(), init,
        [&binary_op](U acc, const T& item) {
            return binary_op(acc, item).template cast<U>();
        },
        num_threads);
}

void for_each_fixed(py::list items, const py::function& func,
                    size_t num_threads) {
    std::vector<py::object> vec;
    vec.reserve(items.size());
    for (auto item : items) {
        vec.push_back(py::reinterpret_borrow<py::object>(item));
    }

    atom::async::Parallel::for_each(
        vec.begin(), vec.end(), [&func](const py::object& item) { func(item); },
        num_threads);
}

void sort_fixed(py::list items, py::function comp, size_t num_threads) {
    std::vector<py::object> vec;
    vec.reserve(items.size());
    for (auto item : items) {
        vec.push_back(py::reinterpret_borrow<py::object>(item));
    }

    atom::async::Parallel::sort(
        vec.begin(), vec.end(),
        [&comp](const py::object& a, const py::object& b) {
            return comp(a, b).cast<bool>();
        },
        num_threads);

    // 更新原始列表
    for (size_t i = 0; i < vec.size(); ++i) {
        items[i] = vec[i];
    }
}

PYBIND11_MODULE(parallel, m) {
    m.doc() = "Parallel computing module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Parallel class binding
    py::class_<atom::async::Parallel>(
        m, "Parallel",
        R"(High-performance parallel algorithms library.

This class provides parallel implementations of common algorithms like map, filter,
reduce, and sort for improved performance on multi-core systems.

Examples:
    >>> from atom.async import Parallel
    >>> Parallel.map([1, 2, 3, 4], lambda x: x * 2)
    [2, 4, 6, 8]
)")
        .def_static(
            "for_each",
            &for_each_fixed,  // 使用修复后的函数
            py::arg("items"), py::arg("func"), py::arg("num_threads") = 0,
            R"(Applies a function to each element in a sequence in parallel.

Args:
    items: A sequence of elements.
    func: Function to apply to each element.
    num_threads: Number of threads to use (0 = hardware concurrency).

Examples:
    >>> items = [1, 2, 3, 4]
    >>> Parallel.for_each(items, lambda x: print(x * 2))
)")
        .def_static(
            "map", &parallel_map_list<py::object>, py::arg("items"),
            py::arg("func"), py::arg("num_threads") = 0,
            R"(Maps a function over a sequence in parallel and returns results.

Args:
    items: A sequence of elements.
    func: Function to apply to each element.
    num_threads: Number of threads to use (0 = hardware concurrency).

Returns:
    List of results from applying the function to each element.

Examples:
    >>> from atom.async import Parallel
    >>> Parallel.map([1, 2, 3, 4], lambda x: x * 2)
    [2, 4, 6, 8]
)")
        .def_static(
            "filter", &parallel_filter_list<py::object>, py::arg("items"),
            py::arg("predicate"), py::arg("num_threads") = 0,
            R"(Filters elements in a sequence in parallel based on a predicate.

Args:
    items: A sequence of elements.
    predicate: Function that returns True for elements to keep.
    num_threads: Number of threads to use (0 = hardware concurrency).

Returns:
    List of elements that satisfy the predicate.

Examples:
    >>> from atom.async import Parallel
    >>> Parallel.filter([1, 2, 3, 4, 5, 6], lambda x: x % 2 == 0)
    [2, 4, 6]
)")
        .def_static("reduce", &parallel_reduce_list<py::object, py::object>,
                    py::arg("items"), py::arg("init"), py::arg("binary_op"),
                    py::arg("num_threads") = 0,
                    R"(Reduces a sequence in parallel using a binary operation.

Args:
    items: A sequence of elements.
    init: Initial value.
    binary_op: Binary operation to apply (takes accumulated value and item).
    num_threads: Number of threads to use (0 = hardware concurrency).

Returns:
    Result of the reduction.

Examples:
    >>> from atom.async import Parallel
    >>> Parallel.reduce([1, 2, 3, 4], 0, lambda acc, x: acc + x)
    10
)")
        .def_static("sort",
                    &sort_fixed,
                    py::arg("items"),
                    py::arg("comp") = py::cpp_function(
                        [](const py::object& a, const py::object& b) {
                            return py::bool_(PyObject_RichCompareBool(
                                                 a.ptr(), b.ptr(), Py_LT) == 1);
                        }),
                    py::arg("num_threads") = 0,
                    R"(Sorts a sequence in parallel.

Args:
    items: A sequence of elements (sorted in-place).
    comp: Comparison function (default: less than).
    num_threads: Number of threads to use (0 = hardware concurrency).

Examples:
    >>> from atom.async import Parallel
    >>> items = [3, 1, 4, 2]
    >>> Parallel.sort(items)
    >>> items
    [1, 2, 3, 4]
    >>> Parallel.sort(items, lambda a, b: b < a)  # Reverse sort
    >>> items
    [4, 3, 2, 1]
)");

    // SimdOps class binding
    py::class_<atom::async::SimdOps>(
        m, "SimdOps",
        R"(SIMD-enabled operations for high-performance computing.

This class provides optimizations using SIMD (Single Instruction, Multiple Data)
instructions for common vector operations like addition, multiplication and dot product.

Examples:
    >>> import numpy as np
    >>> from atom.async import SimdOps
    >>> a = np.array([1.0, 2.0, 3.0])
    >>> b = np.array([4.0, 5.0, 6.0])
    >>> result = np.zeros_like(a)
    >>> SimdOps.add(a, b, result)
    >>> result
    array([5., 7., 9.])
)")
        .def_static(
            "add", &simd_add_arrays<float>, py::arg("a"), py::arg("b"),
            py::arg("result"),
            R"(Adds two arrays element-wise using SIMD instructions if possible.

Args:
    a: First array (numpy.ndarray).
    b: Second array (numpy.ndarray).
    result: Output array for results (numpy.ndarray).

Examples:
    >>> import numpy as np
    >>> from atom.async import SimdOps
    >>> a = np.array([1.0, 2.0, 3.0])
    >>> b = np.array([4.0, 5.0, 6.0])
    >>> result = np.zeros_like(a)
    >>> SimdOps.add(a, b, result)
    >>> result
    array([5., 7., 9.])
)")
        .def_static("add", &simd_add_arrays<double>, py::arg("a"), py::arg("b"),
                    py::arg("result"))
        .def_static("add", &simd_add_arrays<int32_t>, py::arg("a"),
                    py::arg("b"), py::arg("result"))
        .def_static("add", &simd_add_arrays<int64_t>, py::arg("a"),
                    py::arg("b"), py::arg("result"))
        .def_static(
            "multiply", &simd_multiply_arrays<float>, py::arg("a"),
            py::arg("b"), py::arg("result"),
            R"(Multiplies two arrays element-wise using SIMD instructions if possible.

Args:
    a: First array (numpy.ndarray).
    b: Second array (numpy.ndarray).
    result: Output array for results (numpy.ndarray).

Examples:
    >>> import numpy as np
    >>> from atom.async import SimdOps
    >>> a = np.array([1.0, 2.0, 3.0])
    >>> b = np.array([4.0, 5.0, 6.0])
    >>> result = np.zeros_like(a)
    >>> SimdOps.multiply(a, b, result)
    >>> result
    array([4., 10., 18.])
)")
        .def_static("multiply", &simd_multiply_arrays<double>, py::arg("a"),
                    py::arg("b"), py::arg("result"))
        .def_static("multiply", &simd_multiply_arrays<int32_t>, py::arg("a"),
                    py::arg("b"), py::arg("result"))
        .def_static("multiply", &simd_multiply_arrays<int64_t>, py::arg("a"),
                    py::arg("b"), py::arg("result"))
        .def_static(
            "dot_product", &simd_dot_product<float>, py::arg("a"), py::arg("b"),
            R"(Calculates the dot product of two vectors using SIMD if possible.

Args:
    a: First array (numpy.ndarray).
    b: Second array (numpy.ndarray).

Returns:
    Dot product result.

Examples:
    >>> import numpy as np
    >>> from atom.async import SimdOps
    >>> a = np.array([1.0, 2.0, 3.0])
    >>> b = np.array([4.0, 5.0, 6.0])
    >>> SimdOps.dot_product(a, b)
    32.0
)")
        .def_static("dot_product", &simd_dot_product<double>, py::arg("a"),
                    py::arg("b"))
        .def_static("dot_product", &simd_dot_product<int32_t>, py::arg("a"),
                    py::arg("b"))
        .def_static("dot_product", &simd_dot_product<int64_t>, py::arg("a"),
                    py::arg("b"));
}
