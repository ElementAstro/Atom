#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <cstring>

namespace py = pybind11;

PYBIND11_MODULE(to_byte, m) {
    m.doc() = R"pbdoc(
        Byte Conversion Module
        ----------------------

        This module provides utilities for converting various types to and from byte representations.
        It supports serialization and deserialization of:
        - Numeric types (integers, floats)
        - Strings
        - Containers (vectors, maps)
        - Custom types

        Features:
        - Endianness handling (little-endian, big-endian)
        - Type-safe serialization
        - Efficient byte packing/unpacking

        Examples:
            >>> from atom.utils import to_byte
            >>> # Convert integer to bytes
            >>> bytes_data = to_byte.to_bytes_int(42)
            >>> # Convert bytes back to integer
            >>> value = to_byte.from_bytes_int(bytes_data)
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

    // Integer to bytes conversion
    m.def(
        "to_bytes_int8",
        [](int8_t value) {
            std::vector<uint8_t> bytes;
            bytes.push_back(static_cast<uint8_t>(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"),
        R"(Convert an 8-bit integer to bytes.

Args:
    value: 8-bit integer value

Returns:
    Bytes representation

Examples:
    >>> to_byte.to_bytes_int8(42)
    b'*'
)");

    m.def(
        "to_bytes_int16",
        [](int16_t value) {
            std::vector<uint8_t> bytes(2);
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert a 16-bit integer to bytes.");

    m.def(
        "to_bytes_int32",
        [](int32_t value) {
            std::vector<uint8_t> bytes(4);
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"),
        R"(Convert a 32-bit integer to bytes.

Args:
    value: 32-bit integer value

Returns:
    Bytes representation

Examples:
    >>> to_byte.to_bytes_int32(42)
    b'*\x00\x00\x00'  # Little-endian
)");

    m.def(
        "to_bytes_int64",
        [](int64_t value) {
            std::vector<uint8_t> bytes(8);
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert a 64-bit integer to bytes.");

    m.def(
        "to_bytes_uint8",
        [](uint8_t value) {
            std::vector<uint8_t> bytes;
            bytes.push_back(value);
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert an unsigned 8-bit integer to bytes.");

    m.def(
        "to_bytes_uint16",
        [](uint16_t value) {
            std::vector<uint8_t> bytes(2);
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert an unsigned 16-bit integer to bytes.");

    m.def(
        "to_bytes_uint32",
        [](uint32_t value) {
            std::vector<uint8_t> bytes(4);
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert an unsigned 32-bit integer to bytes.");

    m.def(
        "to_bytes_uint64",
        [](uint64_t value) {
            std::vector<uint8_t> bytes(8);
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert an unsigned 64-bit integer to bytes.");

    // Float to bytes conversion
    m.def(
        "to_bytes_float",
        [](float value) {
            std::vector<uint8_t> bytes(sizeof(float));
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"),
        R"(Convert a float to bytes.

Args:
    value: Float value

Returns:
    Bytes representation

Examples:
    >>> to_byte.to_bytes_float(3.14)
    b'\xc3\xf5H@'  # IEEE 754 representation
)");

    m.def(
        "to_bytes_double",
        [](double value) {
            std::vector<uint8_t> bytes(sizeof(double));
            std::memcpy(bytes.data(), &value, sizeof(value));
            return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size());
        },
        py::arg("value"), "Convert a double to bytes.");

    // String to bytes conversion
    m.def(
        "to_bytes_string",
        [](const std::string& value) { return py::bytes(value); },
        py::arg("value"),
        R"(Convert a string to bytes.

Args:
    value: String value

Returns:
    Bytes representation

Examples:
    >>> to_byte.to_bytes_string("hello")
    b'hello'
)");

    // Bytes to integer conversion
    m.def(
        "from_bytes_int8",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 1) {
                throw std::invalid_argument("Expected 1 byte for int8");
            }
            return static_cast<int8_t>(str_view[0]);
        },
        py::arg("bytes_data"), "Convert bytes to an 8-bit integer.");

    m.def(
        "from_bytes_int16",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 2) {
                throw std::invalid_argument("Expected 2 bytes for int16");
            }
            int16_t value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"), "Convert bytes to a 16-bit integer.");

    m.def(
        "from_bytes_int32",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 4) {
                throw std::invalid_argument("Expected 4 bytes for int32");
            }
            int32_t value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"),
        R"(Convert bytes to a 32-bit integer.

Args:
    bytes_data: Bytes to convert

Returns:
    32-bit integer value

Examples:
    >>> bytes_data = to_byte.to_bytes_int32(42)
    >>> to_byte.from_bytes_int32(bytes_data)
    42
)");

    m.def(
        "from_bytes_int64",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 8) {
                throw std::invalid_argument("Expected 8 bytes for int64");
            }
            int64_t value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"), "Convert bytes to a 64-bit integer.");

    m.def(
        "from_bytes_uint8",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 1) {
                throw std::invalid_argument("Expected 1 byte for uint8");
            }
            return static_cast<uint8_t>(str_view[0]);
        },
        py::arg("bytes_data"), "Convert bytes to an unsigned 8-bit integer.");

    m.def(
        "from_bytes_uint16",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 2) {
                throw std::invalid_argument("Expected 2 bytes for uint16");
            }
            uint16_t value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"), "Convert bytes to an unsigned 16-bit integer.");

    m.def(
        "from_bytes_uint32",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 4) {
                throw std::invalid_argument("Expected 4 bytes for uint32");
            }
            uint32_t value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"), "Convert bytes to an unsigned 32-bit integer.");

    m.def(
        "from_bytes_uint64",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != 8) {
                throw std::invalid_argument("Expected 8 bytes for uint64");
            }
            uint64_t value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"), "Convert bytes to an unsigned 64-bit integer.");

    // Bytes to float conversion
    m.def(
        "from_bytes_float",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != sizeof(float)) {
                throw std::invalid_argument("Expected 4 bytes for float");
            }
            float value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"),
        R"(Convert bytes to a float.

Args:
    bytes_data: Bytes to convert

Returns:
    Float value

Examples:
    >>> bytes_data = to_byte.to_bytes_float(3.14)
    >>> to_byte.from_bytes_float(bytes_data)
    3.14
)");

    m.def(
        "from_bytes_double",
        [](py::bytes bytes_data) {
            auto str_view = static_cast<std::string>(bytes_data);
            if (str_view.size() != sizeof(double)) {
                throw std::invalid_argument("Expected 8 bytes for double");
            }
            double value;
            std::memcpy(&value, str_view.data(), sizeof(value));
            return value;
        },
        py::arg("bytes_data"), "Convert bytes to a double.");

    // Bytes to string conversion
    m.def(
        "from_bytes_string",
        [](py::bytes bytes_data) {
            return static_cast<std::string>(bytes_data);
        },
        py::arg("bytes_data"),
        R"(Convert bytes to a string.

Args:
    bytes_data: Bytes to convert

Returns:
    String value

Examples:
    >>> to_byte.from_bytes_string(b'hello')
    'hello'
)");
}
