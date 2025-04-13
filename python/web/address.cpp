#include "atom/web/address.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <memory>

namespace py = pybind11;

PYBIND11_MODULE(address, m) {
    m.doc() = "Network address implementation module for the atom package";

    // 注册异常转换
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::web::InvalidAddressFormat& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::web::AddressRangeError& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const atom::web::AddressException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // 基类 Address 绑定
    py::class_<atom::web::Address, std::unique_ptr<atom::web::Address>>(
        m, "Address",
        R"(Base class for network addresses.

This abstract class provides a common interface for different types of network addresses,
including IPv4, IPv6, and Unix domain socket addresses.

Examples:
    >>> from atom.web import Address
    >>> addr = Address.create_from_string("192.168.1.1")
    >>> addr.get_type()
    'ipv4'
)")
        .def("parse", &atom::web::Address::parse, py::arg("address"),
             R"(Parse an address string.

Args:
    address: The address string to parse.

Returns:
    bool: True if address was parsed successfully.

Raises:
    ValueError: If the address format is invalid.
)")
        .def("print_address_type", &atom::web::Address::printAddressType,
             "Print the type of address.")
        .def("is_in_range", &atom::web::Address::isInRange, py::arg("start"),
             py::arg("end"),
             R"(Check if the address is within a specified range.

Args:
    start: The start address of the range.
    end: The end address of the range.

Returns:
    bool: True if the address is within the range.

Raises:
    IndexError: If the range is invalid.
)")
        .def("to_binary", &atom::web::Address::toBinary,
             "Convert the address to its binary representation.")
        .def("get_address", &atom::web::Address::getAddress,
             "Get the address as a string.")
        .def("is_equal", &atom::web::Address::isEqual, py::arg("other"),
             "Check if this address equals another address.")
        .def("get_type", &atom::web::Address::getType,
             "Get the address type (e.g., 'ipv4', 'ipv6', 'unixdomain').")
        .def("get_network_address", &atom::web::Address::getNetworkAddress,
             py::arg("mask"),
             R"(Get the network address for the given subnet mask.

Args:
    mask: The subnet mask.

Returns:
    str: The network address.

Raises:
    ValueError: If the mask format is invalid.
)")
        .def("get_broadcast_address", &atom::web::Address::getBroadcastAddress,
             py::arg("mask"),
             R"(Get the broadcast address for the given subnet mask.

Args:
    mask: The subnet mask.

Returns:
    str: The broadcast address.

Raises:
    ValueError: If the mask format is invalid.
)")
        .def("is_same_subnet", &atom::web::Address::isSameSubnet,
             py::arg("other"), py::arg("mask"),
             R"(Check if two addresses are in the same subnet.

Args:
    other: Another address to compare with.
    mask: The subnet mask.

Returns:
    bool: True if the addresses are in the same subnet.

Raises:
    ValueError: If the mask format is invalid.
)")
        .def("to_hex", &atom::web::Address::toHex,
             "Convert the address to its hexadecimal representation.")
        .def_static(
            "create_from_string",
            [](std::string_view addressStr) {
                auto addr = atom::web::Address::createFromString(addressStr);
                if (!addr) {
                    throw py::value_error("Invalid address format");
                }
                return addr;
            },
            py::arg("address_str"),
            R"(Create an appropriate address object from a string.

Args:
    address_str: The address string to parse.

Returns:
    Address: An address object of the appropriate type.

Raises:
    ValueError: If the address format is invalid or cannot be determined.
)")
        // Python-specific methods
        .def("__str__", &atom::web::Address::getAddress,
             "Get string representation of the address.")
        .def(
            "__eq__",
            [](const atom::web::Address& self,
               const atom::web::Address& other) { return self.isEqual(other); },
            py::arg("other"), "Compare two addresses for equality.");

    // IPv4 类绑定
    py::class_<atom::web::IPv4, atom::web::Address,
               std::unique_ptr<atom::web::IPv4>>(
        m, "IPv4",
        R"(Class representing an IPv4 address.

This class handles operations specific to IPv4 addresses, including parsing, validation,
and subnet calculations.

Args:
    address (str): The IPv4 address string to initialize with.

Examples:
    >>> from atom.web import IPv4
    >>> addr = IPv4("192.168.1.1")
    >>> addr.to_binary()
    '11000000101010000000000100000001'
)")
        .def(py::init<std::string_view>(), py::arg("address"),
             "Create an IPv4 address from a string.")
        .def(py::init<>(), "Create an uninitialized IPv4 address.")
        .def("parse", &atom::web::IPv4::parse, py::arg("address"),
             "Parse an IPv4 address string.")
        .def(
            "parse_cidr", &atom::web::IPv4::parseCIDR, py::arg("cidr"),
            R"(Parse an IPv4 address in CIDR notation (e.g., '192.168.1.0/24').

Args:
    cidr: The CIDR notation string.

Returns:
    bool: True if successful.

Raises:
    ValueError: If the CIDR format is invalid.
)")
        .def_static("get_prefix_length", &atom::web::IPv4::getPrefixLength,
                    py::arg("cidr"),
                    R"(Get the prefix length from a CIDR notation.

Args:
    cidr: The CIDR notation string.

Returns:
    Optional[int]: The prefix length, or None if invalid.
)")
        .def_static("is_valid_ipv4", &atom::web::IPv4::isValidIPv4,
                    py::arg("address"),
                    R"(Validate an IPv4 address string.

Args:
    address: The IPv4 address string.

Returns:
    bool: True if the address is valid.
)");

    // IPv6 类绑定
    py::class_<atom::web::IPv6, atom::web::Address,
               std::unique_ptr<atom::web::IPv6>>(
        m, "IPv6",
        R"(Class representing an IPv6 address.

This class handles operations specific to IPv6 addresses, including parsing, validation,
and subnet calculations.

Args:
    address (str): The IPv6 address string to initialize with.

Examples:
    >>> from atom.web import IPv6
    >>> addr = IPv6("2001:db8::1")
    >>> addr.to_hex()
    '20010db8000000000000000000000001'
)")
        .def(py::init<std::string_view>(), py::arg("address"),
             "Create an IPv6 address from a string.")
        .def(py::init<>(), "Create an uninitialized IPv6 address.")
        .def("parse", &atom::web::IPv6::parse, py::arg("address"),
             "Parse an IPv6 address string.")
        .def("parse_cidr", &atom::web::IPv6::parseCIDR, py::arg("cidr"),
             R"(Parse an IPv6 address in CIDR notation (e.g., '2001:db8::/32').

Args:
    cidr: The CIDR notation string.

Returns:
    bool: True if successful.

Raises:
    ValueError: If the CIDR format is invalid.
)")
        .def_static("get_prefix_length", &atom::web::IPv6::getPrefixLength,
                    py::arg("cidr"),
                    R"(Get the prefix length from a CIDR notation.

Args:
    cidr: The CIDR notation string.

Returns:
    Optional[int]: The prefix length, or None if invalid.
)")
        .def_static("is_valid_ipv6", &atom::web::IPv6::isValidIPv6,
                    py::arg("address"),
                    R"(Validate an IPv6 address string.

Args:
    address: The IPv6 address string.

Returns:
    bool: True if the address is valid.
)");

    // UnixDomain 类绑定
    py::class_<atom::web::UnixDomain, atom::web::Address,
               std::unique_ptr<atom::web::UnixDomain>>(
        m, "UnixDomain",
        R"(Class representing a Unix domain socket address.

This class handles operations specific to Unix domain socket addresses, including parsing
and path validation.

Args:
    path (str): The Unix domain socket path to initialize with.

Examples:
    >>> from atom.web import UnixDomain
    >>> addr = UnixDomain("/tmp/socket.sock")
    >>> addr.get_type()
    'unixdomain'
)")
        .def(py::init<std::string_view>(), py::arg("path"),
             "Create a Unix domain socket address from a path.")
        .def(py::init<>(),
             "Create an uninitialized Unix domain socket address.")
        .def("parse", &atom::web::UnixDomain::parse, py::arg("path"),
             "Parse a Unix domain socket path.")
        .def_static("is_valid_path", &atom::web::UnixDomain::isValidPath,
                    py::arg("path"),
                    R"(Validate a Unix domain socket path.

Args:
    path: The path to validate.

Returns:
    bool: True if the path is valid.
)");

    // 便利函数
    m.def(
        "parse_address",
        [](std::string_view address) {
            auto addr = atom::web::Address::createFromString(address);
            if (!addr) {
                throw py::value_error("Invalid address format");
            }
            return addr;
        },
        py::arg("address"),
        R"(Parse an address string into the appropriate address type.

Args:
    address: The address string to parse.

Returns:
    Address: An address object of the appropriate type.

Raises:
    ValueError: If the address format is invalid.

Examples:
    >>> from atom.web import parse_address
    >>> addr = parse_address("192.168.1.1")
    >>> isinstance(addr, IPv4)
    True
)");

    m.def(
        "is_valid_address",
        [](std::string_view address) {
            try {
                auto addr = atom::web::Address::createFromString(address);
                return addr != nullptr;
            } catch (...) {
                return false;
            }
        },
        py::arg("address"),
        R"(Check if an address string is valid.

Args:
    address: The address string to check.

Returns:
    bool: True if the address format is valid.

Examples:
    >>> from atom.web import is_valid_address
    >>> is_valid_address("192.168.1.1")
    True
    >>> is_valid_address("not-an-address")
    False
)");
}