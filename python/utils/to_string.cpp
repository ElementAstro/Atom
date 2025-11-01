#include "atom/utils/to_string.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

PYBIND11_MODULE(to_string, m) {
    m.doc() =
        "Object to string conversion utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::ToStringException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ToStringException class
    py::register_exception<atom::utils::ToStringException>(
        m, "ToStringException",
        R"(Exception raised when string conversion fails.

        This exception is thrown when the toString functions encounter
        errors during the conversion process.
        )");

    // Core toString functions
    m.def(
        "to_string",
        [](const std::string& value) -> std::string {
            return atom::utils::toString(value);
        },
        py::arg("value"),
        R"(Convert a string to string (identity function).

          Args:
              value: The string value to convert.

          Returns:
              The same string value.

          Examples:
              >>> from atom.utils import to_string
              >>> result = to_string.to_string("hello")
              >>> print(result)  # "hello"
          )");

    m.def(
        "to_string",
        [](char value) -> std::string { return atom::utils::toString(value); },
        py::arg("value"),
        R"(Convert a character to string.

          Args:
              value: The character value to convert.

          Returns:
              A string containing the single character.

          Examples:
              >>> result = to_string.to_string('A')
              >>> print(result)  # "A"
          )");

    m.def(
        "to_string",
        [](int value) -> std::string { return atom::utils::toString(value); },
        py::arg("value"),
        R"(Convert an integer to string.

          Args:
              value: The integer value to convert.

          Returns:
              String representation of the integer.

          Examples:
              >>> result = to_string.to_string(42)
              >>> print(result)  # "42"
          )");

    m.def(
        "to_string",
        [](double value) -> std::string {
            return atom::utils::toString(value);
        },
        py::arg("value"),
        R"(Convert a float to string.

          Args:
              value: The float value to convert.

          Returns:
              String representation of the float.

          Examples:
              >>> result = to_string.to_string(3.14159)
              >>> print(result)  # "3.141590"
          )");

    m.def(
        "to_string",
        [](bool value) -> std::string { return atom::utils::toString(value); },
        py::arg("value"),
        R"(Convert a boolean to string.

          Args:
              value: The boolean value to convert.

          Returns:
              "1" for true, "0" for false.

          Examples:
              >>> result = to_string.to_string(True)
              >>> print(result)  # "1"
          )");

    m.def(
        "to_string_list",
        [](const std::vector<std::string>& container,
           const std::string& separator = ", ") -> std::string {
            return atom::utils::toString(container, separator);
        },
        py::arg("container"), py::arg("separator") = ", ",
        R"(Convert a list of strings to a formatted string.

          Args:
              container: The list of strings to convert.
              separator: The separator to use between elements.

          Returns:
              Formatted string representation of the list.

          Examples:
              >>> result = to_string.to_string_list(["a", "b", "c"])
              >>> print(result)  # "[a, b, c]"
          )");

    m.def(
        "to_string_int_list",
        [](const std::vector<int>& container,
           const std::string& separator = ", ") -> std::string {
            return atom::utils::toString(container, separator);
        },
        py::arg("container"), py::arg("separator") = ", ",
        R"(Convert a list of integers to a formatted string.

          Args:
              container: The list of integers to convert.
              separator: The separator to use between elements.

          Returns:
              Formatted string representation of the list.

          Examples:
              >>> result = to_string.to_string_int_list([1, 2, 3])
              >>> print(result)  # "[1, 2, 3]"
          )");

    m.def(
        "to_string_float_list",
        [](const std::vector<double>& container,
           const std::string& separator = ", ") -> std::string {
            return atom::utils::toString(container, separator);
        },
        py::arg("container"), py::arg("separator") = ", ",
        R"(Convert a list of floats to a formatted string.

          Args:
              container: The list of floats to convert.
              separator: The separator to use between elements.

          Returns:
              Formatted string representation of the list.

          Examples:
              >>> result = to_string.to_string_float_list([1.1, 2.2, 3.3])
              >>> print(result)  # "[1.100000, 2.200000, 3.300000]"
          )");

    m.def(
        "to_string_dict",
        [](const std::map<std::string, std::string>& container,
           const std::string& separator = ", ") -> std::string {
            return atom::utils::toString(container, separator);
        },
        py::arg("container"), py::arg("separator") = ", ",
        R"(Convert a dictionary to a formatted string.

          Args:
              container: The dictionary to convert.
              separator: The separator to use between key-value pairs.

          Returns:
              Formatted string representation of the dictionary.

          Examples:
              >>> result = to_string.to_string_dict({"a": "1", "b": "2"})
              >>> print(result)  # "{a: 1, b: 2}"
          )");

    m.def(
        "join_command_line",
        [](const std::vector<std::string>& args) -> std::string {
            if (args.empty()) {
                return "";
            }
            std::string result;
            bool first = true;
            for (const auto& arg : args) {
                if (!first) {
                    result += " ";
                }
                first = false;
                result += atom::utils::toString(arg);
            }
            return result;
        },
        py::arg("args"),
        R"(Join multiple arguments into a single command line string.

          Args:
              args: List of arguments to join.

          Returns:
              Space-separated command line string.

          Examples:
              >>> result = to_string.join_command_line(["ls", "-la", "/tmp"])
              >>> print(result)  # "ls -la /tmp"
          )");

    m.def(
        "to_string_array",
        [](const std::vector<std::string>& array,
           const std::string& separator = " ") -> std::string {
            return atom::utils::toStringArray(array, separator);
        },
        py::arg("array"), py::arg("separator") = " ",
        R"(Convert an array to string with specified separator.

          Args:
              array: The array to convert.
              separator: The separator to use between elements.

          Returns:
              String representation of the array without brackets.

          Examples:
              >>> result = to_string.to_string_array(["a", "b", "c"], "-")
              >>> print(result)  # "a-b-c"
          )");

    m.def(
        "to_string_range",
        [](const std::vector<int>& container,
           const std::string& separator = ", ") -> std::string {
            return atom::utils::toStringRange(container.begin(),
                                              container.end(), separator);
        },
        py::arg("container"), py::arg("separator") = ", ",
        R"(Convert a range to string with specified separator.

          Args:
              container: The container to convert.
              separator: The separator to use between elements.

          Returns:
              String representation of the range with brackets.

          Examples:
              >>> result = to_string.to_string_range([1, 2, 3], " | ")
              >>> print(result)  # "[1 | 2 | 3]"
          )");

    // Utility functions for Python objects
    m.def(
        "py_to_string",
        [](py::object obj) -> std::string {
            try {
                if (py::isinstance<py::str>(obj)) {
                    return obj.cast<std::string>();
                } else if (py::isinstance<py::int_>(obj)) {
                    return atom::utils::toString(obj.cast<int>());
                } else if (py::isinstance<py::float_>(obj)) {
                    return atom::utils::toString(obj.cast<double>());
                } else if (py::isinstance<py::bool_>(obj)) {
                    return atom::utils::toString(obj.cast<bool>());
                } else if (py::isinstance<py::list>(obj)) {
                    std::vector<std::string> vec;
                    for (auto item : obj) {
                        vec.push_back(py::str(item).cast<std::string>());
                    }
                    return atom::utils::toString(vec);
                } else if (py::isinstance<py::dict>(obj)) {
                    std::map<std::string, std::string> map;
                    for (auto item : obj) {
                        auto key = py::str(item.first).cast<std::string>();
                        auto value = py::str(item.second).cast<std::string>();
                        map[key] = value;
                    }
                    return atom::utils::toString(map);
                } else {
                    return py::str(obj).cast<std::string>();
                }
            } catch (const std::exception& e) {
                throw atom::utils::ToStringException(
                    std::string("Python object conversion failed: ") +
                    e.what());
            }
        },
        py::arg("obj"),
        R"(Convert a Python object to string using appropriate conversion.

          Args:
              obj: The Python object to convert.

          Returns:
              String representation of the Python object.

          Raises:
              ToStringException: If conversion fails.

          Examples:
              >>> result = to_string.py_to_string([1, 2, 3])
              >>> print(result)  # "[1, 2, 3]"
          )");

    // Constants for common separators
    m.attr("COMMA_SEPARATOR") = ", ";
    m.attr("SPACE_SEPARATOR") = " ";
    m.attr("NEWLINE_SEPARATOR") = "\n";
    m.attr("TAB_SEPARATOR") = "\t";
    m.attr("PIPE_SEPARATOR") = " | ";
}
