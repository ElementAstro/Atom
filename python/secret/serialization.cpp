#include "atom/secret/serialization.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_serialization(py::module& m) {
    // JsonSerializer class
    py::class_<atom::secret::JsonSerializer>(m, "JsonSerializer",
                                             R"pbdoc(
        JSON serialization and deserialization utilities for password entries.

        Provides static methods for converting password entries, settings,
        and other data structures to and from JSON format.

        Example:
            >>> # Serialize a password entry
            >>> entry = PasswordEntry()
            >>> entry.username = "user@example.com"
            >>> result = JsonSerializer.serialize_entry(entry)
            >>> if result.is_success():
            ...     json_str = result.value()
            >>>
            >>> # Deserialize a password entry
            >>> result = JsonSerializer.deserialize_entry(json_str)
            >>> if result.is_success():
            ...     entry = result.value()
        )pbdoc")
        .def_static("serialize_entry",
                    &atom::secret::JsonSerializer::serializeEntry,
                    py::arg("entry"),
                    R"pbdoc(
            Serializes a PasswordEntry to JSON string.

            Args:
                entry: The password entry to serialize.

            Returns:
                Result[str]: JSON string or error message.
            )pbdoc")
        .def_static("deserialize_entry",
                    &atom::secret::JsonSerializer::deserializeEntry,
                    py::arg("json"),
                    R"pbdoc(
            Deserializes a PasswordEntry from JSON string.

            Args:
                json: The JSON string to deserialize.

            Returns:
                Result[PasswordEntry]: PasswordEntry or error message.
            )pbdoc")
        .def_static("serialize_entries",
                    &atom::secret::JsonSerializer::serializeEntries,
                    py::arg("entries"),
                    R"pbdoc(
            Serializes multiple PasswordEntry objects to JSON array.

            Args:
                entries: List of password entries to serialize.

            Returns:
                Result[str]: JSON string or error message.
            )pbdoc")
        .def_static("deserialize_entries",
                    &atom::secret::JsonSerializer::deserializeEntries,
                    py::arg("json"),
                    R"pbdoc(
            Deserializes multiple PasswordEntry objects from JSON array.

            Args:
                json: The JSON array string to deserialize.

            Returns:
                Result[list[PasswordEntry]]: List of PasswordEntry or error message.
            )pbdoc")
        .def_static("serialize_settings",
                    &atom::secret::JsonSerializer::serializeSettings,
                    py::arg("settings"),
                    R"pbdoc(
            Serializes a PasswordManagerSettings object to JSON.

            Args:
                settings: The settings to serialize.

            Returns:
                Result[str]: JSON string or error message.
            )pbdoc")
        .def_static("deserialize_settings",
                    &atom::secret::JsonSerializer::deserializeSettings,
                    py::arg("json"),
                    R"pbdoc(
            Deserializes a PasswordManagerSettings object from JSON.

            Args:
                json: The JSON string to deserialize.

            Returns:
                Result[PasswordManagerSettings]: PasswordManagerSettings or error message.
            )pbdoc")
        .def_static("unescape_string",
                    &atom::secret::JsonSerializer::unescapeString,
                    py::arg("str"),
                    R"pbdoc(
            Unescapes a JSON string (public utility).

            Args:
                str: Escaped JSON string.

            Returns:
                str: Unescaped string.
            )pbdoc");

    // SimpleJsonParser class
    py::class_<atom::secret::SimpleJsonParser>(m, "SimpleJsonParser",
                                               R"pbdoc(
        Simple JSON parser for basic JSON operations.

        This is a minimal implementation to avoid external dependencies.
        Provides basic JSON parsing utilities for extracting values.

        Example:
            >>> json_str = '{"name": "John", "age": 30}'
            >>> name = SimpleJsonParser.extract_string(json_str, "name")
            >>> age = SimpleJsonParser.extract_int(json_str, "age")
        )pbdoc")
        .def_static("extract_string",
                    &atom::secret::SimpleJsonParser::extractString,
                    py::arg("json"), py::arg("key"),
                    R"pbdoc(
            Extracts a string value from JSON.

            Args:
                json: JSON string.
                key: Key to extract.

            Returns:
                str: Extracted string value or empty string if not found.
            )pbdoc")
        .def_static("extract_int", &atom::secret::SimpleJsonParser::extractInt,
                    py::arg("json"), py::arg("key"),
                    R"pbdoc(
            Extracts an integer value from JSON.

            Args:
                json: JSON string.
                key: Key to extract.

            Returns:
                int: Extracted integer value or 0 if not found.
            )pbdoc")
        .def_static("extract_bool",
                    &atom::secret::SimpleJsonParser::extractBool,
                    py::arg("json"), py::arg("key"),
                    R"pbdoc(
            Extracts a boolean value from JSON.

            Args:
                json: JSON string.
                key: Key to extract.

            Returns:
                bool: Extracted boolean value or False if not found.
            )pbdoc")
        .def_static("extract_array",
                    &atom::secret::SimpleJsonParser::extractArray,
                    py::arg("json"), py::arg("key"),
                    R"pbdoc(
            Extracts an array value from JSON.

            Args:
                json: JSON string.
                key: Key to extract.

            Returns:
                str: Extracted array as string or empty string if not found.
            )pbdoc")
        .def_static("extract_object",
                    &atom::secret::SimpleJsonParser::extractObject,
                    py::arg("json"), py::arg("key"),
                    R"pbdoc(
            Extracts an object value from JSON.

            Args:
                json: JSON string.
                key: Key to extract.

            Returns:
                str: Extracted object as string or empty string if not found.
            )pbdoc")
        .def_static("is_valid_json",
                    &atom::secret::SimpleJsonParser::isValidJson,
                    py::arg("json"),
                    R"pbdoc(
            Checks if JSON string is valid.

            Args:
                json: JSON string to validate.

            Returns:
                bool: True if valid, False otherwise.
            )pbdoc");
}
