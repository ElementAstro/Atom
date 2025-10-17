#include "atom/utils/container/container.hpp"
#include "atom/utils/container/ranges.hpp"
#include "atom/utils/container/span.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(container, m) {
    m.doc() = "Container utilities module for the atom package";

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

    // Container utility functions
    m.def(
        "is_subset",
        [](const std::vector<int>& subset, const std::vector<int>& superset)
            -> bool { return atom::utils::isSubset(subset, superset); },
        py::arg("subset"), py::arg("superset"),
        R"(Check if one container is a subset of another container.

          Args:
              subset: The container to check if it's a subset.
              superset: The container to check against.

          Returns:
              True if subset is a subset of superset, False otherwise.

          Examples:
              >>> from atom.utils import container
              >>> container.is_subset([1, 2], [1, 2, 3, 4])
              True
          )");

    m.def(
        "is_subset_str",
        [](const std::vector<std::string>& subset,
           const std::vector<std::string>& superset) -> bool {
            return atom::utils::isSubset(subset, superset);
        },
        py::arg("subset"), py::arg("superset"),
        R"(Check if one string container is a subset of another.

          Args:
              subset: The string container to check if it's a subset.
              superset: The string container to check against.

          Returns:
              True if subset is a subset of superset, False otherwise.
          )");

    m.def(
        "contains",
        [](const std::vector<int>& container, int value) -> bool {
            return atom::utils::contains(container, value);
        },
        py::arg("container"), py::arg("value"),
        R"(Check if a container contains a specific element.

          Args:
              container: The container to search in.
              value: The value to search for.

          Returns:
              True if the value is found, False otherwise.

          Examples:
              >>> container.contains([1, 2, 3], 2)
              True
          )");

    m.def(
        "contains_str",
        [](const std::vector<std::string>& container, const std::string& value)
            -> bool { return atom::utils::contains(container, value); },
        py::arg("container"), py::arg("value"),
        R"(Check if a string container contains a specific string.

          Args:
              container: The string container to search in.
              value: The string value to search for.

          Returns:
              True if the value is found, False otherwise.
          )");

    m.def(
        "intersection",
        [](const std::vector<int>& container1,
           const std::vector<int>& container2) -> std::vector<int> {
            auto result = atom::utils::intersection(container1, container2);
            return std::vector<int>(result.begin(), result.end());
        },
        py::arg("container1"), py::arg("container2"),
        R"(Return intersection of two containers.

          Args:
              container1: The first container.
              container2: The second container.

          Returns:
              A list containing elements present in both containers.

          Examples:
              >>> container.intersection([1, 2, 3], [2, 3, 4])
              [2, 3]
          )");

    m.def(
        "intersection_str",
        [](const std::vector<std::string>& container1,
           const std::vector<std::string>& container2)
            -> std::vector<std::string> {
            auto result = atom::utils::intersection(container1, container2);
            return std::vector<std::string>(result.begin(), result.end());
        },
        py::arg("container1"), py::arg("container2"),
        R"(Return intersection of two string containers.

          Args:
              container1: The first string container.
              container2: The second string container.

          Returns:
              A list containing strings present in both containers.
          )");

    // Range utility functions
    m.def(
        "find_element",
        [](const std::vector<int>& container, int value) -> py::object {
            auto result = atom::utils::findElement(container, value);
            if (result) {
                return py::cast(*result);
            }
            return py::none();
        },
        py::arg("container"), py::arg("value"),
        R"(Find an element in a container.

          Args:
              container: The container to search in.
              value: The value to find.

          Returns:
              The found value or None if not found.

          Examples:
              >>> container.find_element([1, 2, 3], 2)
              2
          )");

    m.def(
        "slice",
        [](const std::vector<int>& container, size_t start,
           size_t end) -> std::vector<int> {
            return atom::utils::slice(container, start, end);
        },
        py::arg("container"), py::arg("start"), py::arg("end"),
        R"(Slice a container into a new container.

          Args:
              container: The input container.
              start: The starting index of the slice.
              end: The ending index of the slice (exclusive).

          Returns:
              A new container containing the sliced elements.

          Examples:
              >>> container.slice([1, 2, 3, 4, 5], 1, 4)
              [2, 3, 4]
          )");

    m.def(
        "slice_str",
        [](const std::vector<std::string>& container, size_t start,
           size_t end) -> std::vector<std::string> {
            return atom::utils::slice(container, start, end);
        },
        py::arg("container"), py::arg("start"), py::arg("end"),
        R"(Slice a string container into a new container.

          Args:
              container: The input string container.
              start: The starting index of the slice.
              end: The ending index of the slice (exclusive).

          Returns:
              A new container containing the sliced strings.
          )");

    // Span utility functions
    m.def(
        "sum_span",
        [](const std::vector<int>& data) -> int {
            std::span<const int> span_data(data);
            return atom::utils::sum(span_data);
        },
        py::arg("data"),
        R"(Compute the sum of elements in a container.

          Args:
              data: The container containing the elements.

          Returns:
              The sum of all elements.

          Examples:
              >>> container.sum_span([1, 2, 3, 4])
              10
          )");

    m.def(
        "sum_span_float",
        [](const std::vector<double>& data) -> double {
            std::span<const double> span_data(data);
            return atom::utils::sum(span_data);
        },
        py::arg("data"),
        R"(Compute the sum of float elements in a container.

          Args:
              data: The container containing the float elements.

          Returns:
              The sum of all elements.
          )");

    m.def(
        "contains_span",
        [](const std::vector<int>& data, int value) -> bool {
            std::span<const int> span_data(data);
            return atom::utils::contains(span_data, value);
        },
        py::arg("data"), py::arg("value"),
        R"(Check if a container contains a specific value using span.

          Args:
              data: The container to search.
              value: The value to find.

          Returns:
              True if the value is found, False otherwise.
          )");

    m.def(
        "filter_span",
        [](const std::vector<int>& data,
           py::function predicate) -> std::vector<int> {
            std::span<const int> span_data(data);
            return atom::utils::filterSpan(span_data, [predicate](int value) {
                return predicate(value).cast<bool>();
            });
        },
        py::arg("data"), py::arg("predicate"),
        R"(Filter elements in a container based on a predicate.

          Args:
              data: The container to filter.
              predicate: The predicate function to apply.

          Returns:
              A list containing the filtered elements.

          Examples:
              >>> container.filter_span([1, 2, 3, 4], lambda x: x > 2)
              [3, 4]
          )");

    m.def(
        "count_if_span",
        [](const std::vector<int>& data, py::function predicate) -> size_t {
            std::span<const int> span_data(data);
            return atom::utils::countIfSpan(span_data, [predicate](int value) {
                return predicate(value).cast<bool>();
            });
        },
        py::arg("data"), py::arg("predicate"),
        R"(Count the number of elements that satisfy a predicate.

          Args:
              data: The container to search.
              predicate: The predicate function to apply.

          Returns:
              The number of elements that satisfy the predicate.

          Examples:
              >>> container.count_if_span([1, 2, 3, 4], lambda x: x > 2)
              2
          )");

    // Utility functions for Python lists
    m.def(
        "unique",
        [](const std::vector<int>& container) -> std::vector<int> {
            auto hash_set = atom::utils::toHashSet(container);
            return std::vector<int>(hash_set.begin(), hash_set.end());
        },
        py::arg("container"),
        R"(Remove duplicates from a container.

          Args:
              container: The input container.

          Returns:
              A new container with unique elements.

          Examples:
              >>> container.unique([1, 2, 2, 3, 3, 4])
              [1, 2, 3, 4]  # Order may vary
          )");

    m.def(
        "unique_str",
        [](const std::vector<std::string>& container)
            -> std::vector<std::string> {
            auto hash_set = atom::utils::toHashSet(container);
            return std::vector<std::string>(hash_set.begin(), hash_set.end());
        },
        py::arg("container"),
        R"(Remove duplicates from a string container.

          Args:
              container: The input string container.

          Returns:
              A new container with unique strings.
          )");
}
