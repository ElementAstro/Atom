#include "atom/utils/uuid.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(uuid, m) {
    m.doc() = "UUID generation and manipulation module for the atom package";

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

    // UuidError enum
    py::enum_<atom::utils::UuidError>(m, "UuidError",
                                      R"(Error types for UUID operations.

Attributes:
    INVALID_FORMAT: The UUID string has an invalid format
    INVALID_LENGTH: The UUID string has an incorrect length
    INVALID_CHARACTER: The UUID string contains invalid characters
    CONVERSION_FAILED: Failed to convert the UUID string
    INTERNAL_ERROR: An internal error occurred during UUID operations
)")
        .value("INVALID_FORMAT", atom::utils::UuidError::InvalidFormat)
        .value("INVALID_LENGTH", atom::utils::UuidError::InvalidLength)
        .value("INVALID_CHARACTER", atom::utils::UuidError::InvalidCharacter)
        .value("CONVERSION_FAILED", atom::utils::UuidError::ConversionFailed)
        .value("INTERNAL_ERROR", atom::utils::UuidError::InternalError);

    // UUID class binding
    py::class_<atom::utils::UUID>(
        m, "UUID",
        R"(Represents a Universally Unique Identifier (UUID).

This class provides methods for generating, comparing, and manipulating UUIDs
with enhanced C++20 features, robust error handling and performance optimizations.

Args:
    data: Optional data to initialize the UUID with.
          If not provided, a random UUID will be generated.

Examples:
    >>> from atom.utils import UUID
    >>> # Generate a random UUID
    >>> uuid1 = UUID()
    >>> # Create a UUID from a string
    >>> uuid2 = UUID.from_string("550e8400-e29b-41d4-a716-446655440000")
    >>> # Generate a UUID v4 (random)
    >>> uuid3 = UUID.generate_v4()
)")
        .def(py::init<>(), "Constructs a new UUID with a random value.")
        .def(py::init<const std::array<uint8_t, 16>&>(), py::arg("data"),
             "Constructs a UUID from a given 16-byte array.")
        .def(py::init<std::span<const uint8_t>>(), py::arg("bytes"),
             R"(Constructs a UUID from a span of bytes.

Args:
    bytes: A span of bytes, must be exactly 16 bytes.

Raises:
    ValueError: If span size is not 16 bytes.
)")
        .def("to_string", &atom::utils::UUID::toString,
             R"(Converts the UUID to a string representation.

Returns:
    A string representation of the UUID.

Examples:
    >>> uuid.to_string()
    '550e8400-e29b-41d4-a716-446655440000'
)")
        .def_static(
            "from_string",
            [](std::string_view str) {
                auto result = atom::utils::UUID::fromString(str);
                if (!result.has_value()) {
                    switch (static_cast<int>(result.error())) {
                        case static_cast<int>(
                            atom::utils::UuidError::InvalidFormat):
                            throw py::value_error("Invalid UUID format");
                        case static_cast<int>(
                            atom::utils::UuidError::InvalidLength):
                            throw py::value_error("Invalid UUID length");
                        case static_cast<int>(
                            atom::utils::UuidError::InvalidCharacter):
                            throw py::value_error("Invalid character in UUID");
                        case static_cast<int>(
                            atom::utils::UuidError::ConversionFailed):
                            throw py::value_error("Failed to convert UUID");
                        default:
                            throw std::runtime_error(
                                "Internal error creating UUID");
                    }
                }
                return result.value();
            },
            py::arg("str"),
            R"(Creates a UUID from a string representation.

Args:
    str: A string representation of a UUID.

Returns:
    A UUID object.

Raises:
    ValueError: If the string is not a valid UUID.

Examples:
    >>> UUID.from_string("550e8400-e29b-41d4-a716-446655440000")
)")
        .def("get_data", &atom::utils::UUID::getData,
             R"(Retrieves the underlying data of the UUID.

Returns:
    A bytes object representing the 16 bytes of the UUID.
)")
        .def("version", &atom::utils::UUID::version,
             R"(Gets the version of the UUID.

Returns:
    The version number of the UUID (1, 3, 4, or 5).
)")
        .def("variant", &atom::utils::UUID::variant,
             R"(Gets the variant of the UUID.

Returns:
    The variant number of the UUID.
)")
        .def_static("generate_v1", &atom::utils::UUID::generateV1,
                    R"(Generates a version 1, time-based UUID.

Returns:
    A version 1 UUID.

Raises:
    RuntimeError: If the generation fails.
)")
        .def_static(
            "generate_v3", &atom::utils::UUID::generateV3,
            py::arg("namespace_uuid"), py::arg("name"),
            R"(Generates a version 3 UUID using the MD5 hashing algorithm.

Args:
    namespace_uuid: The namespace UUID.
    name: The name from which to generate the UUID.

Returns:
    A version 3 UUID.

Raises:
    RuntimeError: If the hash generation fails.
)")
        .def_static("generate_v4", &atom::utils::UUID::generateV4,
                    R"(Generates a version 4, random UUID.

Returns:
    A version 4 UUID.

Raises:
    RuntimeError: If the random generator fails.
)")
        .def_static(
            "generate_v5", &atom::utils::UUID::generateV5,
            py::arg("namespace_uuid"), py::arg("name"),
            R"(Generates a version 5 UUID using the SHA-1 hashing algorithm.

Args:
    namespace_uuid: The namespace UUID.
    name: The name from which to generate the UUID.

Returns:
    A version 5 UUID.

Raises:
    RuntimeError: If the hash generation fails.
)")
        .def_static("is_valid_uuid", &atom::utils::UUID::isValidUUID,
                    py::arg("str"),
                    R"(Checks if a string is a valid UUID format.

Args:
    str: The string to check.

Returns:
    True if valid UUID format, False otherwise.
)")
        .def("__eq__", &atom::utils::UUID::operator==, py::arg("other"),
             "Compares this UUID with another for equality.")
        .def("__ne__", &atom::utils::UUID::operator!=, py::arg("other"),
             "Compares this UUID with another for inequality.")
        .def("__lt__", &atom::utils::UUID::operator<, py::arg("other"),
             "Defines a less-than comparison for UUIDs.")
        .def("__str__", &atom::utils::UUID::toString,
             "Returns the string representation of the UUID.")
        .def(
            "__repr__",
            [](const atom::utils::UUID& self) {
                return "UUID('" + self.toString() + "')";
            },
            "Returns a printable representation of the UUID object.")
        .def(
            "__hash__",
            [](const atom::utils::UUID& self) {
                return std::hash<atom::utils::UUID>{}(self);
            },
            "Returns a hash value for the UUID suitable for use in "
            "dictionaries.");

#if ATOM_USE_SIMD
    // FastUUID class binding if SIMD is enabled
    py::class_<atom::utils::FastUUID>(
        m, "FastUUID",
        R"(High-performance UUID implementation using SIMD instructions.

This class provides a faster implementation of UUIDs using SIMD instructions
when available on the platform.

Args:
    data: Optional data to initialize the UUID with.
          If not provided, a default UUID will be created.

Examples:
    >>> from atom.utils import FastUUID
    >>> # Create a FastUUID
    >>> uuid = FastUUID()
    >>> # Create from string
    >>> uuid = FastUUID.from_str("550e8400-e29b-41d4-a716-446655440000")
)")
        .def(py::init<>(), "Default constructor")
        .def(py::init<const FastUUID&>(), py::arg("other"), "Copy constructor")
        .def(py::init<uint64_t, uint64_t>(), py::arg("x"), py::arg("y"),
             "Construct from two 64-bit integers")
        .def(py::init<const uint8_t*>(), py::arg("bytes"),
             "Construct from byte array")
        .def(py::init<std::string_view>(), py::arg("bytes"),
             "Construct from string containing 16 bytes of UUID data")
        .def_static("from_str", &atom::utils::FastUUID::fromStrFactory,
                    py::arg("s"), "Create UUID from string representation")
        .def("bytes",
             py::overload_cast<>(&atom::utils::FastUUID::bytes, py::const_),
             "Get raw bytes of UUID")
        .def("str",
             py::overload_cast<>(&atom::utils::FastUUID::str, py::const_),
             "Get string representation of UUID")
        .def(
            "__eq__",
            [](const atom::utils::FastUUID& self,
               const atom::utils::FastUUID& other) { return self == other; },
            py::arg("other"), "Equality comparison")
        .def(
            "__ne__",
            [](const atom::utils::FastUUID& self,
               const atom::utils::FastUUID& other) { return self != other; },
            py::arg("other"), "Not-equal comparison")
        .def(
            "__lt__",
            [](const atom::utils::FastUUID& self,
               const atom::utils::FastUUID& other) { return self < other; },
            py::arg("other"), "Less-than comparison")
        .def(
            "__gt__",
            [](const atom::utils::FastUUID& self,
               const atom::utils::FastUUID& other) { return self > other; },
            py::arg("other"), "Greater-than comparison")
        .def(
            "__le__",
            [](const atom::utils::FastUUID& self,
               const atom::utils::FastUUID& other) { return self <= other; },
            py::arg("other"), "Less-than-or-equal comparison")
        .def(
            "__ge__",
            [](const atom::utils::FastUUID& self,
               const atom::utils::FastUUID& other) { return self >= other; },
            py::arg("other"), "Greater-than-or-equal comparison")
        .def(
            "__str__",
            [](const atom::utils::FastUUID& self) { return self.str(); },
            "Returns the string representation of the UUID")
        .def(
            "__repr__",
            [](const atom::utils::FastUUID& self) {
                return "FastUUID('" + self.str() + "')";
            },
            "Returns a printable representation of the FastUUID object")
        .def(
            "__hash__",
            [](const atom::utils::FastUUID& self) { return self.hash(); },
            "Returns a hash value for the FastUUID suitable for use in "
            "dictionaries");
#endif

    // Standalone utility functions
    m.def("generate_unique_uuid", &atom::utils::generateUniqueUUID,
          R"(Generates a unique UUID and returns it as a string.

Returns:
    A unique UUID as a string.

Raises:
    RuntimeError: If UUID generation fails.

Examples:
    >>> from atom.utils import generate_unique_uuid
    >>> uuid_str = generate_unique_uuid()
)");

    m.def("get_mac", &atom::utils::getMAC,
          R"(Gets the MAC address of the system.

Returns:
    MAC address string or empty if not available.

Examples:
    >>> from atom.utils import get_mac
    >>> mac = get_mac()
)");

    m.def("get_cpu_serial", &atom::utils::getCPUSerial,
          R"(Gets CPU serial information.

Returns:
    CPU serial string or empty if not available.

Examples:
    >>> from atom.utils import get_cpu_serial
    >>> cpu_serial = get_cpu_serial()
)");

    m.def("format_uuid", &atom::utils::formatUUID, py::arg("uuid"),
          R"(Formats a UUID string with dashes.

Args:
    uuid: Raw UUID string.

Returns:
    Formatted UUID with dashes.

Examples:
    >>> from atom.utils import format_uuid
    >>> formatted = format_uuid("550e8400e29b41d4a716446655440000")
    >>> print(formatted)
    550e8400-e29b-41d4-a716-446655440000
)");

    // Additional utility functions specific to Python binding
    m.def(
        "uuid1", []() { return atom::utils::UUID::generateV1(); },
        R"(Generate a UUID based on the time and MAC address (version 1).

Returns:
    A new UUID object.

Raises:
    RuntimeError: If generation fails.

Examples:
    >>> from atom.utils import uuid1
    >>> u = uuid1()
)");

    m.def(
        "uuid3",
        [](const atom::utils::UUID& namespace_uuid, std::string_view name) {
            return atom::utils::UUID::generateV3(namespace_uuid, name);
        },
        py::arg("namespace_uuid"), py::arg("name"),
        R"(Generate a UUID using MD5 of namespace and name (version 3).

Args:
    namespace_uuid: The namespace UUID.
    name: The name string.

Returns:
    A new UUID object.

Raises:
    RuntimeError: If generation fails.

Examples:
    >>> from atom.utils import uuid3, UUID
    >>> namespace = UUID.from_string("6ba7b810-9dad-11d1-80b4-00c04fd430c8")  # DNS namespace
    >>> u = uuid3(namespace, "example.com")
)");

    m.def(
        "uuid4", []() { return atom::utils::UUID::generateV4(); },
        R"(Generate a random UUID (version 4).

Returns:
    A new UUID object.

Raises:
    RuntimeError: If generation fails.

Examples:
    >>> from atom.utils import uuid4
    >>> u = uuid4()
)");

    m.def(
        "uuid5",
        [](const atom::utils::UUID& namespace_uuid, std::string_view name) {
            return atom::utils::UUID::generateV5(namespace_uuid, name);
        },
        py::arg("namespace_uuid"), py::arg("name"),
        R"(Generate a UUID using SHA-1 of namespace and name (version 5).

Args:
    namespace_uuid: The namespace UUID.
    name: The name string.

Returns:
    A new UUID object.

Raises:
    RuntimeError: If generation fails.

Examples:
    >>> from atom.utils import uuid5, UUID
    >>> namespace = UUID.from_string("6ba7b810-9dad-11d1-80b4-00c04fd430c8")  # DNS namespace
    >>> u = uuid5(namespace, "example.com")
)");

    // Predefined namespace UUIDs
    m.attr("NAMESPACE_DNS") =
        atom::utils::UUID::fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
            .value();
    m.attr("NAMESPACE_URL") =
        atom::utils::UUID::fromString("6ba7b811-9dad-11d1-80b4-00c04fd430c8")
            .value();
    m.attr("NAMESPACE_OID") =
        atom::utils::UUID::fromString("6ba7b812-9dad-11d1-80b4-00c04fd430c8")
            .value();
    m.attr("NAMESPACE_X500") =
        atom::utils::UUID::fromString("6ba7b814-9dad-11d1-80b4-00c04fd430c8")
            .value();
}