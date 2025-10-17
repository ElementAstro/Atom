#include "atom/utils/conversion/to_any.hpp"
#include "atom/utils/conversion/to_byte.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

PYBIND11_MODULE(conversion, m) {
    m.doc() = "Type conversion and serialization utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::ParserException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::utils::SerializationException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Exception classes
    py::register_exception<atom::utils::ParserException>(m, "ParserException",
        R"(Exception raised when parsing fails.

        This exception is thrown when the Parser class encounters
        errors during parsing operations.
        )");

    py::register_exception<atom::utils::SerializationException>(m, "SerializationException",
        R"(Exception raised when serialization/deserialization fails.

        This exception is thrown when byte serialization operations
        encounter errors.
        )");

    // Parser class for to_any functionality
    py::class_<atom::utils::Parser>(m, "Parser",
        R"(A high-performance parser class for various parsing functionalities.

        The Parser class offers methods to parse literals and custom types
        with enhanced type safety and extensibility.

        Examples:
            >>> from atom.utils import conversion
            >>> parser = conversion.Parser()
            >>> result = parser.parse_literal("42")
            >>> print(result)  # 42 (as integer)
        )")
        .def(py::init<>(), "Create a new Parser instance")
        .def("parse_literal",
             [](atom::utils::Parser& self, const std::string& input) -> py::object {
                 auto result = self.parseLiteral(input);
                 if (result) {
                     const auto& any_val = *result;
                     const auto& type_info = any_val.type();

                     // Convert std::any to Python object
                     if (type_info == typeid(int)) {
                         return py::cast(std::any_cast<int>(any_val));
                     } else if (type_info == typeid(double)) {
                         return py::cast(std::any_cast<double>(any_val));
                     } else if (type_info == typeid(bool)) {
                         return py::cast(std::any_cast<bool>(any_val));
                     } else if (type_info == typeid(std::string)) {
                         return py::cast(std::any_cast<std::string>(any_val));
                     } else {
                         // For unknown types, try to convert to string
                         return py::str(input);
                     }
                 }
                 return py::none();
             },
             py::arg("input"),
             R"(Parse a literal string into an appropriate type.

             Args:
                 input: The input string to parse.

             Returns:
                 The parsed value with appropriate Python type, or None if parsing fails.

             Raises:
                 ParserException: If parsing encounters an error.

             Examples:
                 >>> parser.parse_literal("42")
                 42
                 >>> parser.parse_literal("3.14")
                 3.14
                 >>> parser.parse_literal("true")
                 True
             )")
        .def("parse_literal_with_default",
             [](atom::utils::Parser& self, const std::string& input, py::object default_value) -> py::object {
                 std::any default_any;

                 // Convert Python object to std::any
                 if (py::isinstance<py::int_>(default_value)) {
                     default_any = default_value.cast<int>();
                 } else if (py::isinstance<py::float_>(default_value)) {
                     default_any = default_value.cast<double>();
                 } else if (py::isinstance<py::bool_>(default_value)) {
                     default_any = default_value.cast<bool>();
                 } else if (py::isinstance<py::str>(default_value)) {
                     default_any = default_value.cast<std::string>();
                 } else {
                     default_any = default_value.cast<std::string>();
                 }

                 auto result = self.parseLiteralWithDefault(input, default_any);
                 const auto& type_info = result.type();

                 // Convert result back to Python
                 if (type_info == typeid(int)) {
                     return py::cast(std::any_cast<int>(result));
                 } else if (type_info == typeid(double)) {
                     return py::cast(std::any_cast<double>(result));
                 } else if (type_info == typeid(bool)) {
                     return py::cast(std::any_cast<bool>(result));
                 } else if (type_info == typeid(std::string)) {
                     return py::cast(std::any_cast<std::string>(result));
                 } else {
                     return default_value;
                 }
             },
             py::arg("input"), py::arg("default_value"),
             R"(Parse a literal string with a default value.

             Args:
                 input: The input string to parse.
                 default_value: The default value to return if parsing fails.

             Returns:
                 The parsed value or the default value if parsing fails.

             Examples:
                 >>> parser.parse_literal_with_default("invalid", 42)
                 42
             )");

    // Byte serialization functions
    m.def("serialize_int",
          [](int value) -> std::vector<uint8_t> {
              return atom::utils::serialize(value);
          },
          py::arg("value"),
          R"(Serialize an integer to bytes.

          Args:
              value: The integer value to serialize.

          Returns:
              A list of bytes representing the serialized integer.

          Examples:
              >>> conversion.serialize_int(42)
              [42, 0, 0, 0]  # Little-endian representation
          )");

    m.def("serialize_float",
          [](double value) -> std::vector<uint8_t> {
              return atom::utils::serialize(value);
          },
          py::arg("value"),
          R"(Serialize a float to bytes.

          Args:
              value: The float value to serialize.

          Returns:
              A list of bytes representing the serialized float.
          )");

    m.def("serialize_string",
          [](const std::string& value) -> std::vector<uint8_t> {
              return atom::utils::serialize(value);
          },
          py::arg("value"),
          R"(Serialize a string to bytes.

          Args:
              value: The string value to serialize.

          Returns:
              A list of bytes representing the serialized string.

          Examples:
              >>> conversion.serialize_string("hello")
              [5, 0, 0, 0, 0, 0, 0, 0, 104, 101, 108, 108, 111]  # Size + data
          )");

    m.def("serialize_bool",
          [](bool value) -> std::vector<uint8_t> {
              return atom::utils::serialize(value);
          },
          py::arg("value"),
          R"(Serialize a boolean to bytes.

          Args:
              value: The boolean value to serialize.

          Returns:
              A list of bytes representing the serialized boolean.
          )");

    m.def("serialize_int_list",
          [](const std::vector<int>& value) -> std::vector<uint8_t> {
              return atom::utils::serialize(value);
          },
          py::arg("value"),
          R"(Serialize a list of integers to bytes.

          Args:
              value: The list of integers to serialize.

          Returns:
              A list of bytes representing the serialized list.
          )");

    m.def("serialize_string_list",
          [](const std::vector<std::string>& value) -> std::vector<uint8_t> {
              return atom::utils::serialize(value);
          },
          py::arg("value"),
          R"(Serialize a list of strings to bytes.

          Args:
              value: The list of strings to serialize.

          Returns:
              A list of bytes representing the serialized list.
          )");

    // Deserialization functions
    m.def("deserialize_int",
          [](const std::vector<uint8_t>& bytes) -> int {
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserialize<int>(span, offset);
          },
          py::arg("bytes"),
          R"(Deserialize an integer from bytes.

          Args:
              bytes: The bytes to deserialize from.

          Returns:
              The deserialized integer value.

          Raises:
              SerializationException: If deserialization fails.
          )");

    m.def("deserialize_float",
          [](const std::vector<uint8_t>& bytes) -> double {
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserialize<double>(span, offset);
          },
          py::arg("bytes"),
          R"(Deserialize a float from bytes.

          Args:
              bytes: The bytes to deserialize from.

          Returns:
              The deserialized float value.
          )");

    m.def("deserialize_string",
          [](const std::vector<uint8_t>& bytes) -> std::string {
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserializeString(span, offset);
          },
          py::arg("bytes"),
          R"(Deserialize a string from bytes.

          Args:
              bytes: The bytes to deserialize from.

          Returns:
              The deserialized string value.
          )");

    m.def("deserialize_bool",
          [](const std::vector<uint8_t>& bytes) -> bool {
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserialize<bool>(span, offset);
          },
          py::arg("bytes"),
          R"(Deserialize a boolean from bytes.

          Args:
              bytes: The bytes to deserialize from.

          Returns:
              The deserialized boolean value.
          )");

    m.def("deserialize_int_list",
          [](const std::vector<uint8_t>& bytes) -> std::vector<int> {
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserializeVector<int>(span, offset);
          },
          py::arg("bytes"),
          R"(Deserialize a list of integers from bytes.

          Args:
              bytes: The bytes to deserialize from.

          Returns:
              The deserialized list of integers.
          )");

    m.def("deserialize_string_list",
          [](const std::vector<uint8_t>& bytes) -> std::vector<std::string> {
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserializeVector<std::string>(span, offset);
          },
          py::arg("bytes"),
          R"(Deserialize a list of strings from bytes.

          Args:
              bytes: The bytes to deserialize from.

          Returns:
              The deserialized list of strings.
          )");

    // File I/O functions
    m.def("save_to_file",
          &atom::utils::saveToFile,
          py::arg("data"), py::arg("filename"),
          R"(Save serialized data to a file.

          Args:
              data: The bytes to save.
              filename: The name of the file to write to.

          Raises:
              SerializationException: If the file cannot be opened for writing.
          )");

    m.def("load_from_file",
          &atom::utils::loadFromFile,
          py::arg("filename"),
          R"(Load serialized data from a file.

          Args:
              filename: The name of the file to read from.

          Returns:
              A list of bytes representing the loaded data.

          Raises:
              SerializationException: If the file cannot be opened for reading.
          )");

    // Utility functions for round-trip serialization
    m.def("round_trip_int",
          [](int value) -> int {
              auto bytes = atom::utils::serialize(value);
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserialize<int>(span, offset);
          },
          py::arg("value"),
          R"(Test round-trip serialization for an integer.

          Args:
              value: The integer value to test.

          Returns:
              The value after serialization and deserialization.

          Examples:
              >>> conversion.round_trip_int(42)
              42
          )");

    m.def("round_trip_string",
          [](const std::string& value) -> std::string {
              auto bytes = atom::utils::serialize(value);
              std::span<const uint8_t> span(bytes);
              size_t offset = 0;
              return atom::utils::deserializeString(span, offset);
          },
          py::arg("value"),
          R"(Test round-trip serialization for a string.

          Args:
              value: The string value to test.

          Returns:
              The value after serialization and deserialization.
          )");
}
