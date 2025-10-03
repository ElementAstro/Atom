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
    m.doc() = R"pbdoc(
        High-Performance Parallel Computing Module
        -----------------------------------------

        This module provides high-performance parallel algorithms and SIMD-optimized
        operations for efficient computation on multi-core systems.

        Features:
          - Parallel implementations of map, filter, reduce, sort algorithms
          - SIMD-optimized vector operations (AVX, AVX2, AVX512, NEON)
          - Thread configuration and affinity control
          - Platform-specific optimizations for Windows, macOS, and Linux
          - C++20 coroutine support for asynchronous parallel operations
          - NumPy integration for high-performance array operations

        The module includes:
          - Parallel: Main parallel algorithms class
          - SimdOps: SIMD-optimized operations
          - ThreadConfig: Thread configuration and optimization
          - Task: C++20 coroutine task support

        Example:
            >>> from atom.async.parallel import Parallel, SimdOps
            >>> import numpy as np
            >>>
            >>> # Parallel map operation
            >>> result = Parallel.map([1, 2, 3, 4], lambda x: x * x, num_threads=4)
            >>> print(result)  # [1, 4, 9, 16]
            >>>
            >>> # SIMD vector addition
            >>> a = np.array([1.0, 2.0, 3.0, 4.0])
            >>> b = np.array([5.0, 6.0, 7.0, 8.0])
            >>> result = np.zeros_like(a)
            >>> SimdOps.add(a, b, result)
            >>> print(result)  # [6.0, 8.0, 10.0, 12.0]
    )pbdoc";

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

    // Define ThreadConfig Priority enum
    py::enum_<atom::async::Parallel::ThreadConfig::Priority>(
        m, "ThreadPriority",
        R"pbdoc(
        Thread priority levels for parallel operations.

        Different priority levels affect how the operating system schedules
        the threads used in parallel computations.
        )pbdoc")
        .value("LOWEST", atom::async::Parallel::ThreadConfig::Priority::Lowest,
               "Lowest thread priority")
        .value("LOW", atom::async::Parallel::ThreadConfig::Priority::Low,
               "Low thread priority")
        .value("NORMAL", atom::async::Parallel::ThreadConfig::Priority::Normal,
               "Normal thread priority (default)")
        .value("HIGH", atom::async::Parallel::ThreadConfig::Priority::High,
               "High thread priority")
        .value("HIGHEST", atom::async::Parallel::ThreadConfig::Priority::Highest,
               "Highest thread priority")
        .export_values();

    // ThreadConfig class binding
    py::class_<atom::async::Parallel::ThreadConfig>(
        m, "ThreadConfig",
        R"pbdoc(
        Thread configuration and optimization utilities.

        This class provides platform-specific thread optimization functions
        for setting CPU affinity and thread priority to improve performance
        in parallel computations.
        )pbdoc")
        .def_static(
            "set_thread_affinity",
            &atom::async::Parallel::ThreadConfig::setThreadAffinity,
            py::arg("cpu_id"),
            R"pbdoc(
            Set the CPU affinity for the current thread.

            Args:
                cpu_id: CPU core ID to bind the thread to

            Returns:
                bool: True if successful, False otherwise

            Examples:
                >>> ThreadConfig.set_thread_affinity(0)  # Bind to CPU core 0
                True
            )pbdoc")
        .def_static(
            "set_thread_priority",
            &atom::async::Parallel::ThreadConfig::setThreadPriority,
            py::arg("priority"),
            R"pbdoc(
            Set the priority for the current thread.

            Args:
                priority: Thread priority level

            Returns:
                bool: True if successful, False otherwise

            Examples:
                >>> ThreadConfig.set_thread_priority(ThreadPriority.HIGH)
                True
            )pbdoc");

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
                    [](py::list items, py::function comp, size_t num_threads) {
                        sort_fixed(items, comp, num_threads);
                    },
                    py::arg("items"),
                    py::arg("comp"),
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

    // Utility functions
    m.def(
        "hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); },
        R"pbdoc(
        Get the number of hardware threads available.

        Returns:
            Number of concurrent threads supported by the implementation

        Examples:
            >>> from atom.async.parallel import hardware_concurrency
            >>> print(f"Available threads: {hardware_concurrency()}")
        )pbdoc")

    .def(
        "benchmark_parallel_operations",
        [](size_t data_size, size_t num_threads) -> py::dict {
            using namespace std::chrono;

            py::dict results;

            // Generate test data
            std::vector<int> data(data_size);
            std::iota(data.begin(), data.end(), 1);

            // Benchmark parallel map
            auto start = high_resolution_clock::now();
            auto map_result = atom::async::Parallel::map(
                data.begin(), data.end(),
                [](int x) { return x * x; },
                num_threads);
            auto end = high_resolution_clock::now();
            auto map_duration = duration_cast<microseconds>(end - start);

            results[py::str("map_time_us")] = map_duration.count();
            results[py::str("map_throughput")] = (data_size * 1000000.0) / map_duration.count();

            // Benchmark parallel filter
            start = high_resolution_clock::now();
            auto filter_result = atom::async::Parallel::filter(
                data.begin(), data.end(),
                [](int x) { return x % 2 == 0; },
                num_threads);
            end = high_resolution_clock::now();
            auto filter_duration = duration_cast<microseconds>(end - start);

            results[py::str("filter_time_us")] = filter_duration.count();
            results[py::str("filter_throughput")] = (data_size * 1000000.0) / filter_duration.count();
            results[py::str("filtered_count")] = filter_result.size();

            // Benchmark parallel reduce
            start = high_resolution_clock::now();
            auto reduce_result = atom::async::Parallel::reduce(
                data.begin(), data.end(), 0,
                [](int acc, int x) { return acc + x; },
                num_threads);
            end = high_resolution_clock::now();
            auto reduce_duration = duration_cast<microseconds>(end - start);

            results[py::str("reduce_time_us")] = reduce_duration.count();
            results[py::str("reduce_throughput")] = (data_size * 1000000.0) / reduce_duration.count();
            results[py::str("reduce_result")] = reduce_result;

            results[py::str("data_size")] = data_size;
            results[py::str("num_threads")] = num_threads;

            return results;
        },
        py::arg("data_size") = 1000000, py::arg("num_threads") = std::thread::hardware_concurrency(),
        R"pbdoc(
        Benchmark parallel operations performance.

        Args:
            data_size: Size of test data (default: 1,000,000)
            num_threads: Number of threads to use (default: hardware concurrency)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_parallel_operations(100000, 4)
            >>> print(f"Map throughput: {results['map_throughput']:.2f} ops/sec")
        )pbdoc")

    .def(
        "parallel_matrix_multiply",
        [](py::array_t<double> a, py::array_t<double> b, size_t num_threads) -> py::array_t<double> {
            py::buffer_info a_info = a.request();
            py::buffer_info b_info = b.request();

            if (a_info.ndim != 2 || b_info.ndim != 2) {
                throw std::invalid_argument("Input arrays must be 2D");
            }

            size_t rows_a = a_info.shape[0];
            size_t cols_a = a_info.shape[1];
            size_t rows_b = b_info.shape[0];
            size_t cols_b = b_info.shape[1];

            if (cols_a != rows_b) {
                throw std::invalid_argument("Matrix dimensions don't match for multiplication");
            }

            auto result = py::array_t<double>({rows_a, cols_b});
            py::buffer_info result_info = result.request();

            double* a_ptr = static_cast<double*>(a_info.ptr);
            double* b_ptr = static_cast<double*>(b_info.ptr);
            double* result_ptr = static_cast<double*>(result_info.ptr);

            // Parallel matrix multiplication
            atom::async::Parallel::for_each(
                std::views::iota(0UL, rows_a).begin(),
                std::views::iota(0UL, rows_a).end(),
                [=](size_t i) {
                    for (size_t j = 0; j < cols_b; ++j) {
                        double sum = 0.0;
                        for (size_t k = 0; k < cols_a; ++k) {
                            sum += a_ptr[i * cols_a + k] * b_ptr[k * cols_b + j];
                        }
                        result_ptr[i * cols_b + j] = sum;
                    }
                },
                num_threads);

            return result;
        },
        py::arg("a"), py::arg("b"), py::arg("num_threads") = std::thread::hardware_concurrency(),
        R"pbdoc(
        Perform parallel matrix multiplication.

        Args:
            a: First matrix (2D numpy array)
            b: Second matrix (2D numpy array)
            num_threads: Number of threads to use (default: hardware concurrency)

        Returns:
            numpy.ndarray: Result of matrix multiplication

        Examples:
            >>> import numpy as np
            >>> a = np.random.rand(100, 50)
            >>> b = np.random.rand(50, 75)
            >>> result = parallel_matrix_multiply(a, b, 4)
            >>> print(result.shape)  # (100, 75)
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

    // SIMD feature detection
#ifdef ATOM_SIMD_AVX512
    m.attr("HAS_AVX512") = true;
#else
    m.attr("HAS_AVX512") = false;
#endif

#ifdef ATOM_SIMD_AVX2
    m.attr("HAS_AVX2") = true;
#else
    m.attr("HAS_AVX2") = false;
#endif

#ifdef ATOM_SIMD_AVX
    m.attr("HAS_AVX") = true;
#else
    m.attr("HAS_AVX") = false;
#endif

#ifdef ATOM_SIMD_NEON
    m.attr("HAS_NEON") = true;
#else
    m.attr("HAS_NEON") = false;
#endif

    // Platform detection
#ifdef ATOM_PLATFORM_WINDOWS
    m.attr("PLATFORM") = "Windows";
#elif defined(ATOM_PLATFORM_APPLE)
    m.attr("PLATFORM") = "macOS";
#elif defined(ATOM_PLATFORM_LINUX)
    m.attr("PLATFORM") = "Linux";
#else
    m.attr("PLATFORM") = "Unknown";
#endif
}
