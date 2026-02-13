#include "atom/utils/core/anyutils.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(anyutils, m) {
    m.doc() = R"pbdoc(
        Any Utilities Module
        --------------------

        This module provides utilities for converting various types to different formats:
        - JSON serialization (toJson)
        - XML serialization (toXml)
        - YAML serialization (toYaml)
        - TOML serialization (toToml)
        - String conversion (toString)

        These functions support various data types including:
        - Basic types (int, float, bool, string)
        - Containers (list, dict, tuple)
        - Custom objects

        Examples:
            >>> from atom.utils import anyutils
            >>> # Convert to JSON
            >>> json_str = anyutils.to_json([1, 2, 3])
            >>> print(json_str)  # "[1, 2, 3]"
            >>>
            >>> # Convert to XML
            >>> xml_str = anyutils.to_xml({"key": "value"}, "root")
            >>> print(xml_str)  # "<root><key>value</key></root>"
            >>>
            >>> # Convert to YAML
            >>> yaml_str = anyutils.to_yaml({"key": "value"}, "config")
            >>> print(yaml_str)  # "config:\n  key: value\n"
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

    // JSON conversion functions
    m.def(
        "to_json_int",
        [](int value, bool pretty) { return toJson(value, pretty); },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert an integer to JSON string.

Args:
    value: Integer value to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    JSON string representation

Examples:
    >>> anyutils.to_json_int(42)
    '42'
)");

    m.def(
        "to_json_float",
        [](double value, bool pretty) { return toJson(value, pretty); },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert a float to JSON string.

Args:
    value: Float value to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    JSON string representation

Examples:
    >>> anyutils.to_json_float(3.14)
    '3.14'
)");

    m.def(
        "to_json_bool",
        [](bool value, bool pretty) { return toJson(value, pretty); },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert a boolean to JSON string.

Args:
    value: Boolean value to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    JSON string representation

Examples:
    >>> anyutils.to_json_bool(True)
    'true'
)");

    m.def(
        "to_json_string",
        [](const std::string& value, bool pretty) {
            return toJson(value, pretty);
        },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert a string to JSON string.

Args:
    value: String value to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    JSON string representation with proper escaping

Examples:
    >>> anyutils.to_json_string("hello")
    '"hello"'
)");

    m.def(
        "to_json_list_int",
        [](const std::vector<int>& value, bool pretty) {
            return toJson(value, pretty);
        },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert a list of integers to JSON string.

Args:
    value: List of integers to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    JSON array string

Examples:
    >>> anyutils.to_json_list_int([1, 2, 3])
    '[1, 2, 3]'
)");

    m.def(
        "to_json_list_float",
        [](const std::vector<double>& value, bool pretty) {
            return toJson(value, pretty);
        },
        py::arg("value"), py::arg("pretty") = false,
        "Convert a list of floats to JSON string.");

    m.def(
        "to_json_list_string",
        [](const std::vector<std::string>& value, bool pretty) {
            return toJson(value, pretty);
        },
        py::arg("value"), py::arg("pretty") = false,
        "Convert a list of strings to JSON string.");

    m.def(
        "to_json_map",
        [](const std::map<std::string, std::string>& value, bool pretty) {
            atom::utils::HashMap<std::string, std::string> map;
            for (const auto& [k, v] : value) {
                map[k] = v;
            }
            return toJson(map, pretty);
        },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert a dictionary to JSON string.

Args:
    value: Dictionary to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    JSON object string

Examples:
    >>> anyutils.to_json_map({"key": "value"})
    '{"key": "value"}'
)");

    // XML conversion functions
    m.def(
        "to_xml_int",
        [](int value, const std::string& tag) { return toXml(value, tag); },
        py::arg("value"), py::arg("tag"),
        R"(Convert an integer to XML string.

Args:
    value: Integer value to convert
    tag: XML tag name

Returns:
    XML string representation

Examples:
    >>> anyutils.to_xml_int(42, "number")
    '<number>42</number>'
)");

    m.def(
        "to_xml_float",
        [](double value, const std::string& tag) { return toXml(value, tag); },
        py::arg("value"), py::arg("tag"), "Convert a float to XML string.");

    m.def(
        "to_xml_bool",
        [](bool value, const std::string& tag) { return toXml(value, tag); },
        py::arg("value"), py::arg("tag"), "Convert a boolean to XML string.");

    m.def(
        "to_xml_string",
        [](const std::string& value, const std::string& tag) {
            return toXml(value, tag);
        },
        py::arg("value"), py::arg("tag"),
        R"(Convert a string to XML string.

Args:
    value: String value to convert
    tag: XML tag name

Returns:
    XML string with proper escaping

Examples:
    >>> anyutils.to_xml_string("hello", "message")
    '<message>hello</message>'
)");

    m.def(
        "to_xml_list_int",
        [](const std::vector<int>& value, const std::string& tag) {
            return toXml(value, tag);
        },
        py::arg("value"), py::arg("tag"),
        "Convert a list of integers to XML string.");

    m.def(
        "to_xml_list_string",
        [](const std::vector<std::string>& value, const std::string& tag) {
            return toXml(value, tag);
        },
        py::arg("value"), py::arg("tag"),
        "Convert a list of strings to XML string.");

    // YAML conversion functions
    m.def(
        "to_yaml_int",
        [](int value, const std::string& key) { return toYaml(value, key); },
        py::arg("value"), py::arg("key"),
        R"(Convert an integer to YAML string.

Args:
    value: Integer value to convert
    key: YAML key name

Returns:
    YAML string representation

Examples:
    >>> anyutils.to_yaml_int(42, "number")
    'number: 42\n'
)");

    m.def(
        "to_yaml_float",
        [](double value, const std::string& key) { return toYaml(value, key); },
        py::arg("value"), py::arg("key"), "Convert a float to YAML string.");

    m.def(
        "to_yaml_bool",
        [](bool value, const std::string& key) { return toYaml(value, key); },
        py::arg("value"), py::arg("key"), "Convert a boolean to YAML string.");

    m.def(
        "to_yaml_string",
        [](const std::string& value, const std::string& key) {
            return toYaml(value, key);
        },
        py::arg("value"), py::arg("key"),
        R"(Convert a string to YAML string.

Args:
    value: String value to convert
    key: YAML key name

Returns:
    YAML string with proper quoting if needed

Examples:
    >>> anyutils.to_yaml_string("hello", "message")
    'message: hello\n'
)");

    m.def(
        "to_yaml_list_int",
        [](const std::vector<int>& value, const std::string& key) {
            return toYaml(value, key);
        },
        py::arg("value"), py::arg("key"),
        "Convert a list of integers to YAML string.");

    m.def(
        "to_yaml_list_string",
        [](const std::vector<std::string>& value, const std::string& key) {
            return toYaml(value, key);
        },
        py::arg("value"), py::arg("key"),
        "Convert a list of strings to YAML string.");

    // TOML conversion functions
    m.def(
        "to_toml_int",
        [](int value, const std::string& key) { return toToml(value, key); },
        py::arg("value"), py::arg("key"),
        R"(Convert an integer to TOML string.

Args:
    value: Integer value to convert
    key: TOML key name

Returns:
    TOML string representation

Examples:
    >>> anyutils.to_toml_int(42, "number")
    'number = 42\n'
)");

    m.def(
        "to_toml_float",
        [](double value, const std::string& key) { return toToml(value, key); },
        py::arg("value"), py::arg("key"), "Convert a float to TOML string.");

    m.def(
        "to_toml_bool",
        [](bool value, const std::string& key) { return toToml(value, key); },
        py::arg("value"), py::arg("key"), "Convert a boolean to TOML string.");

    m.def(
        "to_toml_string",
        [](const std::string& value, const std::string& key) {
            return toToml(value, key);
        },
        py::arg("value"), py::arg("key"),
        R"(Convert a string to TOML string.

Args:
    value: String value to convert
    key: TOML key name

Returns:
    TOML string with proper escaping

Examples:
    >>> anyutils.to_toml_string("hello", "message")
    'message = "hello"\n'
)");

    m.def(
        "to_toml_list_int",
        [](const std::vector<int>& value, const std::string& key) {
            return toToml(value, key);
        },
        py::arg("value"), py::arg("key"),
        "Convert a list of integers to TOML string.");

    m.def(
        "to_toml_list_string",
        [](const std::vector<std::string>& value, const std::string& key) {
            return toToml(value, key);
        },
        py::arg("value"), py::arg("key"),
        "Convert a list of strings to TOML string.");

    // Pair conversion
    m.def(
        "to_string_pair",
        [](const std::pair<int, int>& value, bool pretty) {
            return toString(value, pretty);
        },
        py::arg("value"), py::arg("pretty") = false,
        R"(Convert a pair to string.

Args:
    value: Pair to convert
    pretty: Whether to use pretty printing (default: False)

Returns:
    String representation of the pair

Examples:
    >>> anyutils.to_string_pair((1, 2))
    '(1, 2)'
)");
}
