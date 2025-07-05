#include "atom/extra/boost/uuid.hpp"

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(uuid, m) {
    m.doc() = "UUID module for the atom package";

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

    // UUID class binding
    py::class_<atom::extra::boost::UUID>(
        m, "UUID",
        R"(A wrapper class for Boost.UUID providing various UUID operations.

This class generates, manipulates and compares UUIDs (Universally Unique Identifiers)
in various formats.

Args:
    str (optional): A string representation of a UUID.

Examples:
    >>> from atom.extra.boost import uuid
    >>> # Generate a random UUID (v4)
    >>> id1 = uuid.UUID()
    >>> print(id1.to_string())
    550e8400-e29b-41d4-a716-446655440000

    >>> # Create UUID from string
    >>> id2 = uuid.UUID("550e8400-e29b-41d4-a716-446655440000")
    >>> print(id2.format())
    {550e8400-e29b-41d4-a716-446655440000}
)")
        .def(py::init<>(),
             "Default constructor that generates a random UUID (v4).")
        .def(py::init<const std::string&>(), py::arg("str"),
             "Constructs a UUID from a string representation.")
        .def("to_string", &atom::extra::boost::UUID::toString,
             "Converts the UUID to a string representation.")
        .def("is_nil", &atom::extra::boost::UUID::isNil,
             "Checks if the UUID is nil (all zeros).")
        .def(py::self < py::self,
             "Checks if this UUID is less than another UUID.")
        .def(py::self <= py::self,
             "Checks if this UUID is less than or equal to another UUID.")
        .def(py::self > py::self,
             "Checks if this UUID is greater than another UUID.")
        .def(py::self >= py::self,
             "Checks if this UUID is greater than or equal to another UUID.")
        .def(py::self == py::self,
             "Checks if this UUID is equal to another UUID.")
        .def(py::self != py::self,
             "Checks if this UUID is not equal to another UUID.")
        .def("format", &atom::extra::boost::UUID::format,
             "Formats the UUID as a string enclosed in curly braces.")
        .def("to_bytes", &atom::extra::boost::UUID::toBytes,
             "Converts the UUID to a vector of bytes.")
        .def_static("from_bytes", &atom::extra::boost::UUID::fromBytes,
                    py::arg("bytes"),
                    R"(Constructs a UUID from a span of bytes.

Args:
    bytes: The vector of bytes (must be exactly 16 bytes).

Returns:
    The constructed UUID.

Raises:
    ValueError: If the vector size is not 16 bytes.
)")
        .def("to_uint64", &atom::extra::boost::UUID::toUint64,
             "Converts the UUID to a 64-bit unsigned integer.")
        .def_static("namespace_dns", &atom::extra::boost::UUID::namespaceDNS,
                    "Gets the DNS namespace UUID.")
        .def_static("namespace_url", &atom::extra::boost::UUID::namespaceURL,
                    "Gets the URL namespace UUID.")
        .def_static("namespace_oid", &atom::extra::boost::UUID::namespaceOID,
                    "Gets the OID namespace UUID.")
        .def_static(
            "v3", &atom::extra::boost::UUID::v3, py::arg("namespace_uuid"),
            py::arg("name"),
            R"(Generates a version 3 (MD5) UUID based on a namespace UUID and a name.

Args:
    namespace_uuid: The namespace UUID.
    name: The name.

Returns:
    The generated UUID.
)")
        .def_static(
            "v5", &atom::extra::boost::UUID::v5, py::arg("namespace_uuid"),
            py::arg("name"),
            R"(Generates a version 5 (SHA-1) UUID based on a namespace UUID and a name.

Args:
    namespace_uuid: The namespace UUID.
    name: The name.

Returns:
    The generated UUID.
)")
        .def("version", &atom::extra::boost::UUID::version,
             "Gets the version of the UUID.")
        .def("variant", &atom::extra::boost::UUID::variant,
             "Gets the variant of the UUID.")
        .def_static("v1", &atom::extra::boost::UUID::v1,
                    "Generates a version 1 (timestamp-based) UUID.")
        .def_static("v4", &atom::extra::boost::UUID::v4,
                    "Generates a version 4 (random) UUID.")
        .def("to_base64", &atom::extra::boost::UUID::toBase64,
             "Converts the UUID to a Base64 string representation.")
        .def("get_timestamp", &atom::extra::boost::UUID::getTimestamp,
             R"(Gets the timestamp from a version 1 UUID.

Returns:
    The timestamp as a datetime.datetime object.

Raises:
    RuntimeError: If the UUID is not version 1.
)")
        // Python-specific methods
        .def("__str__", &atom::extra::boost::UUID::toString,
             "String representation of the UUID.")
        .def(
            "__repr__",
            [](const atom::extra::boost::UUID& u) {
                return "UUID('" + u.toString() + "')";
            },
            "Official string representation of the UUID.")
        .def(
            "__hash__",
            [](const atom::extra::boost::UUID& u) {
                return std::hash<atom::extra::boost::UUID>{}(u);
            },
            "Returns hash value for using UUID as dictionary key.")
        .def("__int__", &atom::extra::boost::UUID::toUint64,
             "Converts the UUID to an integer.");

    // Module-level functions
    m.def("uuid1", &atom::extra::boost::UUID::v1,
          "Generates a version 1 (timestamp-based) UUID.");

    m.def("uuid3", &atom::extra::boost::UUID::v3, py::arg("namespace_uuid"),
          py::arg("name"),
          "Generates a version 3 (MD5) UUID based on a namespace UUID and a "
          "name.");

    m.def("uuid4", &atom::extra::boost::UUID::v4,
          "Generates a version 4 (random) UUID.");

    m.def("uuid5", &atom::extra::boost::UUID::v5, py::arg("namespace_uuid"),
          py::arg("name"),
          "Generates a version 5 (SHA-1) UUID based on a namespace UUID and a "
          "name.");

    // Constant namespaces
    m.attr("NAMESPACE_DNS") = atom::extra::boost::UUID::namespaceDNS();
    m.attr("NAMESPACE_URL") = atom::extra::boost::UUID::namespaceURL();
    m.attr("NAMESPACE_OID") = atom::extra::boost::UUID::namespaceOID();

    // Add UUID generation functions with easier names (Python-friendly API)
    m.def("generate_random", &atom::extra::boost::UUID::v4,
          "Generates a random UUID (same as uuid4).");

    m.def("generate_time_based", &atom::extra::boost::UUID::v1,
          "Generates a timestamp-based UUID (same as uuid1).");

    m.def(
        "parse",
        [](const std::string& s) { return atom::extra::boost::UUID(s); },
        py::arg("str"),
        R"(Parse a string into a UUID.

Args:
    str: A string representation of a UUID.

Returns:
    The constructed UUID.

Examples:
    >>> from atom.extra.boost import uuid
    >>> id = uuid.parse("550e8400-e29b-41d4-a716-446655440000")
)");

    // Helper functions
    m.def(
        "is_valid_uuid",
        [](const std::string& s) {
            try {
                atom::extra::boost::UUID uuid(s);
                return true;
            } catch (const std::exception&) {
                return false;
            }
        },
        py::arg("str"),
        R"(Check if a string is a valid UUID representation.

Args:
    str: A string to check.

Returns:
    True if the string is a valid UUID, False otherwise.

Examples:
    >>> from atom.extra.boost import uuid
    >>> uuid.is_valid_uuid("550e8400-e29b-41d4-a716-446655440000")
    True
    >>> uuid.is_valid_uuid("not-a-uuid")
    False
)");

    // Add version info
    m.attr("__version__") = "1.0.0";
}
