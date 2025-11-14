#include "atom/utils/container/ranges.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(ranges, m) {
    m.doc() = R"pbdoc(
        Ranges Utilities Module
        -----------------------

        This module provides range-based utilities and generators for working with sequences.
        It includes functions for filtering, transforming, and manipulating ranges.

        Features:
        - Range filtering
        - Range transformation
        - Range chunking
        - Range zipping
        - Range enumeration

        Examples:
            >>> from atom.utils import ranges
            >>> # Filter a range
            >>> filtered = ranges.filter_range([1, 2, 3, 4, 5], lambda x: x > 2)
            >>> list(filtered)  # [3, 4, 5]
            >>>
            >>> # Transform a range
            >>> transformed = ranges.transform_range([1, 2, 3], lambda x: x * 2)
            >>> list(transformed)  # [2, 4, 6]
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

    // Filter range function
    m.def(
        "filter_range",
        [](const std::vector<int>& range, py::function predicate) {
            std::vector<int> result;
            for (const auto& item : range) {
                if (predicate(item).cast<bool>()) {
                    result.push_back(item);
                }
            }
            return result;
        },
        py::arg("range"), py::arg("predicate"),
        R"(Filter a range based on a predicate function.

Args:
    range: Input range (list of integers)
    predicate: Function that takes an element and returns bool

Returns:
    Filtered list

Examples:
    >>> ranges.filter_range([1, 2, 3, 4, 5], lambda x: x > 2)
    [3, 4, 5]
)");

    m.def(
        "filter_range_float",
        [](const std::vector<double>& range, py::function predicate) {
            std::vector<double> result;
            for (const auto& item : range) {
                if (predicate(item).cast<bool>()) {
                    result.push_back(item);
                }
            }
            return result;
        },
        py::arg("range"), py::arg("predicate"),
        "Filter a range of floats based on a predicate function.");

    m.def(
        "filter_range_string",
        [](const std::vector<std::string>& range, py::function predicate) {
            std::vector<std::string> result;
            for (const auto& item : range) {
                if (predicate(item).cast<bool>()) {
                    result.push_back(item);
                }
            }
            return result;
        },
        py::arg("range"), py::arg("predicate"),
        "Filter a range of strings based on a predicate function.");

    // Transform range function
    m.def(
        "transform_range",
        [](const std::vector<int>& range, py::function transformer) {
            std::vector<py::object> result;
            for (const auto& item : range) {
                result.push_back(transformer(item));
            }
            return result;
        },
        py::arg("range"), py::arg("transformer"),
        R"(Transform a range using a function.

Args:
    range: Input range (list of integers)
    transformer: Function that transforms each element

Returns:
    Transformed list

Examples:
    >>> ranges.transform_range([1, 2, 3], lambda x: x * 2)
    [2, 4, 6]
)");

    m.def(
        "transform_range_float",
        [](const std::vector<double>& range, py::function transformer) {
            std::vector<py::object> result;
            for (const auto& item : range) {
                result.push_back(transformer(item));
            }
            return result;
        },
        py::arg("range"), py::arg("transformer"),
        "Transform a range of floats using a function.");

    m.def(
        "transform_range_string",
        [](const std::vector<std::string>& range, py::function transformer) {
            std::vector<py::object> result;
            for (const auto& item : range) {
                result.push_back(transformer(item));
            }
            return result;
        },
        py::arg("range"), py::arg("transformer"),
        "Transform a range of strings using a function.");

    // Chunk range function
    m.def(
        "chunk_range",
        [](const std::vector<int>& range, size_t chunk_size) {
            std::vector<std::vector<int>> result;
            for (size_t i = 0; i < range.size(); i += chunk_size) {
                std::vector<int> chunk;
                for (size_t j = i; j < std::min(i + chunk_size, range.size());
                     ++j) {
                    chunk.push_back(range[j]);
                }
                result.push_back(chunk);
            }
            return result;
        },
        py::arg("range"), py::arg("chunk_size"),
        R"(Split a range into chunks of specified size.

Args:
    range: Input range (list of integers)
    chunk_size: Size of each chunk

Returns:
    List of chunks

Examples:
    >>> ranges.chunk_range([1, 2, 3, 4, 5], 2)
    [[1, 2], [3, 4], [5]]
)");

    // Enumerate range function
    m.def(
        "enumerate_range",
        [](const std::vector<int>& range) {
            std::vector<std::pair<size_t, int>> result;
            for (size_t i = 0; i < range.size(); ++i) {
                result.push_back({i, range[i]});
            }
            return result;
        },
        py::arg("range"),
        R"(Enumerate a range with indices.

Args:
    range: Input range (list of integers)

Returns:
    List of (index, value) pairs

Examples:
    >>> ranges.enumerate_range([10, 20, 30])
    [(0, 10), (1, 20), (2, 30)]
)");

    m.def(
        "enumerate_range_string",
        [](const std::vector<std::string>& range) {
            std::vector<std::pair<size_t, std::string>> result;
            for (size_t i = 0; i < range.size(); ++i) {
                result.push_back({i, range[i]});
            }
            return result;
        },
        py::arg("range"), "Enumerate a range of strings with indices.");

    // Zip ranges function
    m.def(
        "zip_ranges",
        [](const std::vector<int>& range1, const std::vector<int>& range2) {
            std::vector<std::pair<int, int>> result;
            size_t min_size = std::min(range1.size(), range2.size());
            for (size_t i = 0; i < min_size; ++i) {
                result.push_back({range1[i], range2[i]});
            }
            return result;
        },
        py::arg("range1"), py::arg("range2"),
        R"(Zip two ranges together.

Args:
    range1: First range (list of integers)
    range2: Second range (list of integers)

Returns:
    List of pairs from both ranges

Examples:
    >>> ranges.zip_ranges([1, 2, 3], [4, 5, 6])
    [(1, 4), (2, 5), (3, 6)]
)");

    // Take range function
    m.def(
        "take_range",
        [](const std::vector<int>& range, size_t count) {
            std::vector<int> result;
            size_t take_count = std::min(count, range.size());
            for (size_t i = 0; i < take_count; ++i) {
                result.push_back(range[i]);
            }
            return result;
        },
        py::arg("range"), py::arg("count"),
        R"(Take the first n elements from a range.

Args:
    range: Input range (list of integers)
    count: Number of elements to take

Returns:
    List with first n elements

Examples:
    >>> ranges.take_range([1, 2, 3, 4, 5], 3)
    [1, 2, 3]
)");

    // Skip range function
    m.def(
        "skip_range",
        [](const std::vector<int>& range, size_t count) {
            std::vector<int> result;
            size_t skip_count = std::min(count, range.size());
            for (size_t i = skip_count; i < range.size(); ++i) {
                result.push_back(range[i]);
            }
            return result;
        },
        py::arg("range"), py::arg("count"),
        R"(Skip the first n elements from a range.

Args:
    range: Input range (list of integers)
    count: Number of elements to skip

Returns:
    List without first n elements

Examples:
    >>> ranges.skip_range([1, 2, 3, 4, 5], 2)
    [3, 4, 5]
)");

    // Reverse range function
    m.def(
        "reverse_range",
        [](const std::vector<int>& range) {
            std::vector<int> result(range.rbegin(), range.rend());
            return result;
        },
        py::arg("range"),
        R"(Reverse a range.

Args:
    range: Input range (list of integers)

Returns:
    Reversed list

Examples:
    >>> ranges.reverse_range([1, 2, 3, 4, 5])
    [5, 4, 3, 2, 1]
)");

    // Flatten range function
    m.def(
        "flatten_range",
        [](const std::vector<std::vector<int>>& range) {
            std::vector<int> result;
            for (const auto& subrange : range) {
                result.insert(result.end(), subrange.begin(), subrange.end());
            }
            return result;
        },
        py::arg("range"),
        R"(Flatten a nested range.

Args:
    range: Nested range (list of lists)

Returns:
    Flattened list

Examples:
    >>> ranges.flatten_range([[1, 2], [3, 4], [5]])
    [1, 2, 3, 4, 5]
)");
}
