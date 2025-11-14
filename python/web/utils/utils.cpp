#include "atom/web/utils.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

/**
 * @brief Registers exception translations for the web utilities module.
 *
 * This function sets up proper exception handling to translate C++ exceptions
 * to appropriate Python exceptions for better error reporting.
 *
 * @param m The pybind11 module to register exceptions for
 */
void registerExceptionTranslations(py::module_& m) {
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
}

/**
 * @brief Binds system initialization functions to Python.
 *
 * This function creates Python bindings for system-level initialization
 * functions, particularly Windows-specific networking setup.
 *
 * @param m The pybind11 module to bind to
 */
void bindSystemInitialization(py::module_& m) {
    m.def("initialize_windows_socket_api",
          &atom::web::initializeWindowsSocketAPI,
          R"(Initialize networking subsystem (Windows-specific).

This function initializes the Windows Socket API, which is necessary for network operations on Windows.
On other platforms, this function does nothing and returns True.

Returns:
    bool: True if initialization succeeded, False otherwise.

Raises:
    RuntimeError: If initialization fails with a specific error message.

Examples:
    >>> from atom.web.utils import initialize_windows_socket_api
    >>> initialize_windows_socket_api()
    True
)");
}

/**
 * @brief Binds port utility functions to Python.
 *
 * This function creates Python bindings for port-related utility functions
 * including port checking, process management, and async operations.
 *
 * @param m The pybind11 module to bind to
 */
void bindPortUtilities(py::module_& m) {
    // Basic port checking
    m.def(
        "is_port_in_use", [](int port) { return atom::web::isPortInUse(port); },
        py::arg("port"),
        R"(Check if a port is in use.

This function checks if a port is in use by attempting to bind a socket to the port.
If the socket can be bound, the port is not in use.

Args:
    port: The port number to check (0-65535).

Returns:
    bool: True if the port is in use, False otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If socket operations fail.

Examples:
    >>> from atom.web.utils import is_port_in_use
    >>> is_port_in_use(8080)
    False  # Port is available
)");

    // Process management on ports
    m.def(
        "check_and_kill_program_on_port",
        [](int port) { return atom::web::checkAndKillProgramOnPort(port); },
        py::arg("port"),
        R"(Check if there is any program running on the specified port and kill it if found.

This function checks if there is any program running on the specified port by querying the system.
If a program is found, it will be terminated.

Args:
    port: The port number to check (0-65535).

Returns:
    bool: True if a program was found and terminated, False otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If socket operations fail.
    OSError: If process termination fails.

Examples:
    >>> from atom.web.utils import check_and_kill_program_on_port
    >>> check_and_kill_program_on_port(8080)
    True  # Program found and killed
)");

    m.def(
        "get_process_id_on_port",
        [](int port) { return atom::web::getProcessIDOnPort(port); },
        py::arg("port"),
        R"(Get the process ID of the program running on a specific port.

Args:
    port: The port number to check (0-65535).

Returns:
    Optional[int]: The process ID if found, None otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If command execution fails.

Examples:
    >>> from atom.web.utils import get_process_id_on_port
    >>> pid = get_process_id_on_port(8080)
    >>> if pid is not None:
    ...     print(f"Process with ID {pid} is using port 8080")
    ... else:
    ...     print("No process is using port 8080")
)");

    // Async port checking
    m.def(
        "is_port_in_use_async",
        [](int port) {
            auto future = atom::web::isPortInUseAsync(port);
            return future.get();  // Wait for the result and return it
        },
        py::arg("port"),
        R"(Asynchronously check if a port is in use.

This function checks if a port is in use in a separate thread and returns the result.

Args:
    port: The port number to check (0-65535).

Returns:
    bool: True if the port is in use, False otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If socket operations fail.

Examples:
    >>> from atom.web.utils import is_port_in_use_async
    >>> is_port_in_use_async(8080)
    False  # Port is available
)");
}

/**
 * @brief Binds DNS resolution functions to Python.
 *
 * This function creates Python bindings for DNS-related utilities including
 * hostname resolution, local IP discovery, and DNS cache management.
 *
 * @param m The pybind11 module to bind to
 */
void bindDnsUtilities(py::module_& m) {
    m.def("set_dns_cache_ttl", &atom::web::setDNSCacheTTL,
          py::arg("ttl_seconds"),
          R"(Set the Time-To-Live for DNS cache entries.

Args:
    ttl_seconds: The TTL duration in seconds.

Examples:
    >>> from atom.web.utils import set_dns_cache_ttl
    >>> import datetime
    >>> set_dns_cache_ttl(datetime.timedelta(minutes=5))
)");

    m.def("get_ip_addresses", &atom::web::getIPAddresses, py::arg("hostname"),
          R"(Get IP addresses for a given hostname through DNS resolution.

Args:
    hostname: The hostname to resolve.

Returns:
    List[str]: List of IP addresses associated with the hostname.

Examples:
    >>> from atom.web.utils import get_ip_addresses
    >>> ips = get_ip_addresses("google.com")
    >>> print(f"Google IPs: {ips}")
)");

    m.def("get_local_ip_addresses", &atom::web::getLocalIPAddresses,
          R"(Get all local IP addresses of the machine.

Returns:
    List[str]: List of local IP addresses (excluding loopback).

Examples:
    >>> from atom.web.utils import get_local_ip_addresses
    >>> local_ips = get_local_ip_addresses()
    >>> print(f"Local IPs: {local_ips}")
)");

    m.def("clear_dns_cache_expired_entries",
          &atom::web::clearDNSCacheExpiredEntries,
          R"(Clear expired entries from the DNS cache.

This function removes expired DNS cache entries to free memory and ensure
fresh lookups for expired hostnames.

Examples:
    >>> from atom.web.utils import clear_dns_cache_expired_entries
    >>> clear_dns_cache_expired_entries()
)");
}

/**
 * @brief Binds IP validation and utility functions to Python.
 *
 * This function creates Python bindings for IP address validation and utility
 * functions.
 *
 * @param m The pybind11 module to bind to
 */
void bindIpValidationUtilities(py::module_& m) {
    m.def("is_valid_ipv4", &atom::web::isValidIPv4, py::arg("ip_address"),
          R"(Check if an IP address is a valid IPv4 address.

Args:
    ip_address: The IP address string to validate.

Returns:
    bool: True if the address is a valid IPv4 address, False otherwise.

Examples:
    >>> from atom.web.utils import is_valid_ipv4
    >>> is_valid_ipv4("192.168.1.1")
    True
    >>> is_valid_ipv4("256.1.1.1")
    False
)");

    m.def("is_valid_ipv6", &atom::web::isValidIPv6, py::arg("ip_address"),
          R"(Check if an IP address is a valid IPv6 address.

Args:
    ip_address: The IP address string to validate.

Returns:
    bool: True if the address is a valid IPv6 address, False otherwise.

Examples:
    >>> from atom.web.utils import is_valid_ipv6
    >>> is_valid_ipv6("2001:db8::1")
    True
    >>> is_valid_ipv6("invalid::address")
    False
)");

    // Note: ipToString requires struct sockaddr which is complex to bind
    // It's a low-level function rarely used from Python, so we skip it
}

/**
 * @brief Binds socket utility functions to Python.
 *
 * This function creates Python bindings for socket-related utilities including
 * socket creation, binding, and connection operations.
 *
 * @param m The pybind11 module to bind to
 */
void bindSocketUtilities(py::module_& m) {
    m.def("create_socket", &atom::web::createSocket,
          R"(Create a new socket.

Returns:
    int: Socket file descriptor, -1 on error.

Raises:
    RuntimeError: If socket creation fails.

Examples:
    >>> from atom.web.utils import create_socket
    >>> sockfd = create_socket()
    >>> if sockfd != -1:
    ...     print(f"Socket created: {sockfd}")
)");

    m.def("bind_socket", &atom::web::bindSocket, py::arg("sockfd"),
          py::arg("port"),
          R"(Bind a socket to a specific port.

Args:
    sockfd: Socket file descriptor.
    port: Port number to bind to (0-65535).

Returns:
    bool: True if binding succeeded, False otherwise.

Examples:
    >>> from atom.web.utils import create_socket, bind_socket
    >>> sockfd = create_socket()
    >>> if bind_socket(sockfd, 8080):
    ...     print("Socket bound to port 8080")
)");

    m.def("set_socket_non_blocking", &atom::web::setSocketNonBlocking,
          py::arg("sockfd"),
          R"(Set a socket to non-blocking mode.

Args:
    sockfd: Socket file descriptor.

Returns:
    bool: True if setting succeeded, False otherwise.

Examples:
    >>> from atom.web.utils import create_socket, set_socket_non_blocking
    >>> sockfd = create_socket()
    >>> if set_socket_non_blocking(sockfd):
    ...     print("Socket set to non-blocking mode")
)");

    // Note: connectWithTimeout requires sockaddr which is complex to bind
    // We'll skip it for now as it's low-level and rarely used from Python
}

/**
 * @brief Binds port scanning functions to Python.
 *
 * This function creates Python bindings for port scanning utilities.
 *
 * @param m The pybind11 module to bind to
 */
void bindPortScanningUtilities(py::module_& m) {
    m.def("scan_port", &atom::web::scanPort, py::arg("host"), py::arg("port"),
          py::arg("timeout") = std::chrono::milliseconds(2000),
          R"(Scan a specific port on a given host to check if it's open.

Args:
    host: The hostname or IP address to scan.
    port: The port number to scan (0-65535).
    timeout: Maximum time to wait for connection (default: 2000ms).

Returns:
    bool: True if the port is open, False otherwise.

Examples:
    >>> from atom.web.utils import scan_port
    >>> import datetime
    >>> if scan_port("google.com", 80):
    ...     print("Port 80 is open")
    >>> # With custom timeout
    >>> if scan_port("example.com", 443, datetime.timedelta(seconds=5)):
    ...     print("Port 443 is open")
)");

    m.def("scan_port_range", &atom::web::scanPortRange, py::arg("host"),
          py::arg("start_port"), py::arg("end_port"),
          py::arg("timeout") = std::chrono::milliseconds(1000),
          R"(Scan a range of ports on a given host to find open ones.

Args:
    host: The hostname or IP address to scan.
    start_port: The beginning of the port range to scan.
    end_port: The end of the port range to scan.
    timeout: Maximum time to wait for each connection (default: 1000ms).

Returns:
    List[int]: List of open ports.

Examples:
    >>> from atom.web.utils import scan_port_range
    >>> open_ports = scan_port_range("localhost", 8000, 9000)
    >>> print(f"Open ports: {open_ports}")
)");

    m.def(
        "scan_port_range_async",
        [](const std::string& host, uint16_t startPort, uint16_t endPort,
           std::chrono::milliseconds timeout) {
            auto future = atom::web::scanPortRangeAsync(host, startPort,
                                                        endPort, timeout);
            return future.get();  // Wait for the result and return it
        },
        py::arg("host"), py::arg("start_port"), py::arg("end_port"),
        py::arg("timeout") = std::chrono::milliseconds(1000),
        R"(Asynchronously scan a range of ports on a given host.

Args:
    host: The hostname or IP address to scan.
    start_port: The beginning of the port range to scan.
    end_port: The end of the port range to scan.
    timeout: Maximum time to wait for each connection (default: 1000ms).

Returns:
    List[int]: List of open ports.

Examples:
    >>> from atom.web.utils import scan_port_range_async
    >>> open_ports = scan_port_range_async("localhost", 8000, 9000)
    >>> print(f"Open ports: {open_ports}")
)");
}

/**
 * @brief Binds network connectivity functions to Python.
 *
 * This function creates Python bindings for network connectivity utilities.
 *
 * @param m The pybind11 module to bind to
 */
void bindNetworkConnectivityUtilities(py::module_& m) {
    m.def("check_internet_connectivity", &atom::web::checkInternetConnectivity,
          R"(Check if the device has active internet connectivity.

Returns:
    bool: True if internet is available, False otherwise.

Note:
    This function tests connectivity by attempting to connect to reliable
    DNS servers (8.8.8.8, 1.1.1.1, 208.67.222.222) on port 53.

Examples:
    >>> from atom.web.utils import check_internet_connectivity
    >>> if check_internet_connectivity():
    ...     print("Internet connection available")
    ... else:
    ...     print("No internet connection")
)");
}

void init_utils(py::module_& m) {
    m.attr("__doc__") = R"pbdoc(
        Network Utilities Module
        -----------------------

        This module provides comprehensive network utilities for the atom package,
        including DNS resolution, IP validation, port management, socket operations,
        and connectivity testing.

        Key Features:
        - DNS resolution and caching
        - IP address validation (IPv4/IPv6)
        - Port scanning and management
        - Socket operations and utilities
        - Network connectivity testing
        - Process management on ports
        - Address information utilities

        Categories:
        - DNS Operations: hostname resolution, local IP discovery, cache management
        - IP Validation: IPv4/IPv6 address validation and conversion
        - Port Operations: scanning, process management, availability checking
        - Socket Operations: creation, binding, connection utilities
        - Network Testing: connectivity checks and diagnostics

        Examples:
            >>> from atom.web.utils import *
            >>>
            >>> # DNS operations
            >>> ips = get_ip_addresses("google.com")
            >>> local_ips = get_local_ip_addresses()
            >>>
            >>> # IP validation
            >>> is_valid = is_valid_ipv4("192.168.1.1")
            >>>
            >>> # Port operations
            >>> port_busy = is_port_in_use(8080)
            >>>
            >>> # Network connectivity
            >>> has_internet = check_internet_connectivity()
    )pbdoc";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind different categories of network utilities
    bindSystemInitialization(m);
    bindPortUtilities(m);
    bindDnsUtilities(m);                  // DNS functions
    bindIpValidationUtilities(m);         // IP validation functions
    bindSocketUtilities(m);               // Socket operations
    bindPortScanningUtilities(m);         // Port scanning functions
    bindNetworkConnectivityUtilities(m);  // Network connectivity functions
}
