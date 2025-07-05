#include "atom/web/utils.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

PYBIND11_MODULE(utils, m) {
    m.doc() = "Network utilities module for the atom package";

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

    // Windows-specific initialization
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

    // Port checking functions
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

    // Port scanning functions
    m.def("scan_port", &atom::web::scanPort, py::arg("host"), py::arg("port"),
          py::arg("timeout") = std::chrono::milliseconds(2000),
          R"(Scan a specific port on a given host to check if it's open.

Args:
    host: The hostname or IP address to scan.
    port: The port number to scan (0-65535).
    timeout: The maximum time to wait for a connection (default: 2000 ms).

Returns:
    bool: True if the port is open, False otherwise.

Examples:
    >>> from atom.web.utils import scan_port
    >>> scan_port("example.com", 80)
    True  # Port 80 is open on example.com
    >>> scan_port("example.com", 8080, 1000)  # With 1 second timeout
    False  # Port 8080 is closed
)");

    m.def("scan_port_range", &atom::web::scanPortRange, py::arg("host"),
          py::arg("start_port"), py::arg("end_port"),
          py::arg("timeout") = std::chrono::milliseconds(1000),
          R"(Scan a range of ports on a given host to find open ones.

Args:
    host: The hostname or IP address to scan.
    start_port: The beginning of the port range to scan.
    end_port: The end of the port range to scan.
    timeout: The maximum time to wait for each connection attempt (default: 1000 ms).

Returns:
    list[int]: List of open ports.

Examples:
    >>> from atom.web.utils import scan_port_range
    >>> scan_port_range("example.com", 80, 85)
    [80, 443]  # Only these ports are open in the range
)");

    m.def(
        "scan_port_range_async",
        [](const std::string& host, uint16_t start_port, uint16_t end_port,
           std::chrono::milliseconds timeout) {
            auto future = atom::web::scanPortRangeAsync(host, start_port,
                                                        end_port, timeout);
            return future.get();  // Wait for the result and return it
        },
        py::arg("host"), py::arg("start_port"), py::arg("end_port"),
        py::arg("timeout") = std::chrono::milliseconds(1000),
        R"(Asynchronously scan a range of ports on a given host.

This function scans ports in a separate thread for better performance with large port ranges.

Args:
    host: The hostname or IP address to scan.
    start_port: The beginning of the port range to scan.
    end_port: The end of the port range to scan.
    timeout: The maximum time to wait for each connection attempt (default: 1000 ms).

Returns:
    list[int]: List of open ports.

Examples:
    >>> from atom.web.utils import scan_port_range_async
    >>> scan_port_range_async("example.com", 80, 100)  # Scan ports 80-100
    [80, 443]  # Only these ports are open in the range
)");

    // DNS and IP address functions
    m.def("get_ip_addresses", &atom::web::getIPAddresses, py::arg("hostname"),
          R"(Get IP addresses for a given hostname through DNS resolution.

Args:
    hostname: The hostname to resolve.

Returns:
    list[str]: List of IP addresses.

Examples:
    >>> from atom.web.utils import get_ip_addresses
    >>> get_ip_addresses("example.com")
    ['93.184.216.34', '2606:2800:220:1:248:1893:25c8:1946']
)");

    m.def("get_local_ip_addresses", &atom::web::getLocalIPAddresses,
          R"(Get all local IP addresses of the machine.

Returns:
    list[str]: List of local IP addresses.

Examples:
    >>> from atom.web.utils import get_local_ip_addresses
    >>> get_local_ip_addresses()
    ['192.168.1.5', '127.0.0.1', '::1']
)");

    m.def("check_internet_connectivity", &atom::web::checkInternetConnectivity,
          R"(Check if the device has active internet connectivity.

This function attempts to connect to well-known internet hosts to determine if
internet connectivity is available.

Returns:
    bool: True if internet is available, False otherwise.

Examples:
    >>> from atom.web.utils import check_internet_connectivity
    >>> check_internet_connectivity()
    True  # Internet is available
)");

    // Address information functions
    m.def(
        "addr_info_to_string",
        [](const std::string& hostname, const std::string& service,
           bool json_format) {
            auto addrInfo = atom::web::getAddrInfo(hostname, service);
            return atom::web::addrInfoToString(addrInfo.get(), json_format);
        },
        py::arg("hostname"), py::arg("service"), py::arg("json_format") = false,
        R"(Convert address information for a hostname and service to a string.

This function retrieves address information for a hostname and service and converts it
to a human-readable or JSON string representation.

Args:
    hostname: The hostname to resolve.
    service: The service to resolve (can be name like "http" or port number like "80").
    json_format: If True, output in JSON format (default: False).

Returns:
    str: String representation of the address information.

Raises:
    RuntimeError: If getaddrinfo fails.
    ValueError: If hostname or service is empty.

Examples:
    >>> from atom.web.utils import addr_info_to_string
    >>> print(addr_info_to_string("example.com", "http"))
    Family: AF_INET, Type: SOCK_STREAM, Protocol: IPPROTO_TCP, Address: 93.184.216.34:80
    >>> print(addr_info_to_string("example.com", "80", True))
    {"family":"AF_INET","type":"SOCK_STREAM","protocol":"IPPROTO_TCP","address":"93.184.216.34:80"}
)");

    // More advanced address info functions
    m.def(
        "compare_addr_info",
        [](const std::string& hostname1, const std::string& service1,
           const std::string& hostname2, const std::string& service2) {
            auto addr1 = atom::web::getAddrInfo(hostname1, service1);
            auto addr2 = atom::web::getAddrInfo(hostname2, service2);
            return atom::web::compareAddrInfo(addr1.get(), addr2.get());
        },
        py::arg("hostname1"), py::arg("service1"), py::arg("hostname2"),
        py::arg("service2"),
        R"(Compare two address information structures for equality.

This function resolves two hostname/service pairs and compares their address information
structures for equality.

Args:
    hostname1: The first hostname to resolve.
    service1: The first service to resolve.
    hostname2: The second hostname to resolve.
    service2: The second service to resolve.

Returns:
    bool: True if the structures are equal, False otherwise.

Raises:
    RuntimeError: If getaddrinfo fails.
    ValueError: If any hostname or service is empty.

Examples:
    >>> from atom.web.utils import compare_addr_info
    >>> compare_addr_info("example.com", "http", "example.com", "80")
    True  # These resolve to the same address information
    >>> compare_addr_info("example.com", "http", "google.com", "http")
    False  # These resolve to different address information
)");

    m.def(
        "filter_addr_info_by_family",
        [](const std::string& hostname, const std::string& service,
           int family) {
            auto addrInfo = atom::web::getAddrInfo(hostname, service);
            auto filtered = atom::web::filterAddrInfo(addrInfo.get(), family);
            return atom::web::addrInfoToString(filtered.get());
        },
        py::arg("hostname"), py::arg("service"), py::arg("family"),
        R"(Filter address information by family.

This function retrieves address information for a hostname and service and filters it
by the specified family (e.g., AF_INET for IPv4, AF_INET6 for IPv6).

Args:
    hostname: The hostname to resolve.
    service: The service to resolve.
    family: The family to filter by (e.g., socket.AF_INET, socket.AF_INET6).

Returns:
    str: String representation of the filtered address information.

Raises:
    RuntimeError: If getaddrinfo fails.
    ValueError: If hostname or service is empty.

Examples:
    >>> import socket
    >>> from atom.web.utils import filter_addr_info_by_family
    >>> filter_addr_info_by_family("example.com", "http", socket.AF_INET)
    'Family: AF_INET, Type: SOCK_STREAM, Protocol: IPPROTO_TCP, Address: 93.184.216.34:80'
)");

    // Convenience functions for common network tasks
    m.def(
        "is_host_reachable",
        [](const std::string& host, uint16_t port,
           std::chrono::milliseconds timeout) {
            return atom::web::scanPort(host, port, timeout);
        },
        py::arg("host"), py::arg("port") = 80,
        py::arg("timeout") = std::chrono::milliseconds(2000),
        R"(Check if a host is reachable by attempting to connect to a specific port.

This is a convenience alias for scan_port to provide a more intuitive name for
checking if a host is reachable.

Args:
    host: The hostname or IP address to check.
    port: The port to connect to (default: 80).
    timeout: The maximum time to wait for a connection (default: 2000 ms).

Returns:
    bool: True if the host is reachable, False otherwise.

Examples:
    >>> from atom.web.utils import is_host_reachable
    >>> is_host_reachable("example.com")
    True  # Host is reachable via port 80
    >>> is_host_reachable("example.com", 22, 1000)  # Try SSH port with 1s timeout
    False  # Host doesn't accept SSH connections
)");

    m.def(
        "find_open_port",
        [](uint16_t start_port, uint16_t end_port) {
            for (uint16_t port = start_port; port <= end_port; ++port) {
                if (!atom::web::isPortInUse(port)) {
                    return static_cast<int>(port);
                }
            }
            return -1;  // No open ports in range
        },
        py::arg("start_port") = 8000, py::arg("end_port") = 9000,
        R"(Find the first available open port in a range.

Args:
    start_port: The beginning of the port range to check (default: 8000).
    end_port: The end of the port range to check (default: 9000).

Returns:
    int: The first open port in the range, or -1 if no ports are available.

Examples:
    >>> from atom.web.utils import find_open_port
    >>> port = find_open_port(8000, 8100)
    >>> if port != -1:
    ...     print(f"Found open port: {port}")
    ... else:
    ...     print("No open ports available in range")
    Found open port: 8012
)");

    m.def(
        "hostname_to_ip", &atom::web::getIPAddresses, py::arg("hostname"),
        R"(Convert a hostname to its IP addresses (alias for get_ip_addresses).

Args:
    hostname: The hostname to resolve.

Returns:
    list[str]: List of IP addresses.

Examples:
    >>> from atom.web.utils import hostname_to_ip
    >>> hostname_to_ip("example.com")
    ['93.184.216.34', '2606:2800:220:1:248:1893:25c8:1946']
)");
}
