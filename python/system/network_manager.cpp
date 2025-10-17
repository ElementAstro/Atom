#include "atom/system/network/network_manager.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(network_manager, m) {
    m.doc() = R"pbdoc(
        Network Management Module
        -------------------------

        This module provides comprehensive network interface and connection management
        capabilities, including interface enumeration, DNS management, and network
        monitoring functionality.

        Examples:
            >>> from atom.system import network_manager
            >>>
            >>> # Create network manager
            >>> manager = network_manager.NetworkManager()
            >>>
            >>> # Get network interfaces
            >>> interfaces = manager.get_network_interfaces()
            >>> for iface in interfaces:
            ...     print(f"Interface: {iface.get_name()}")
            ...     print(f"  MAC: {iface.get_mac()}")
            ...     print(f"  Status: {'UP' if iface.is_up() else 'DOWN'}")
            ...     for addr in iface.get_addresses():
            ...         print(f"  Address: {addr}")
            >>>
            >>> # DNS operations
            >>> dns_servers = network_manager.NetworkManager.get_dns_servers()
            >>> print(f"DNS servers: {dns_servers}")
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

    // NetworkConnection structure binding
    py::class_<atom::system::NetworkConnection>(
        m, "NetworkConnection",
        R"(Represents a network connection.

This class contains information about an active network connection,
including protocol, addresses, and port numbers.

Attributes:
    protocol: Protocol (TCP or UDP).
    local_address: Local IP address.
    remote_address: Remote IP address.
    local_port: Local port number.
    remote_port: Remote port number.

Examples:
    >>> conn = network_manager.NetworkConnection()
    >>> conn.protocol = "TCP"
    >>> conn.local_address = "127.0.0.1"
    >>> conn.local_port = 8080
)")
        .def(py::init<>(), "Constructs an empty NetworkConnection object.")
        .def_readwrite("protocol", &atom::system::NetworkConnection::protocol,
                       "Protocol (TCP or UDP)")
        .def_readwrite("local_address",
                       &atom::system::NetworkConnection::localAddress,
                       "Local IP address")
        .def_readwrite("remote_address",
                       &atom::system::NetworkConnection::remoteAddress,
                       "Remote IP address")
        .def_readwrite("local_port",
                       &atom::system::NetworkConnection::localPort,
                       "Local port number")
        .def_readwrite("remote_port",
                       &atom::system::NetworkConnection::remotePort,
                       "Remote port number")
        .def("__repr__",
             [](const atom::system::NetworkConnection& self) {
                 return "<NetworkConnection(protocol='" + self.protocol +
                        "', local=" + self.localAddress + ":" +
                        std::to_string(self.localPort) +
                        ", remote=" + self.remoteAddress + ":" +
                        std::to_string(self.remotePort) + ")>";
             })
        .def("__str__", [](const atom::system::NetworkConnection& self) {
            return self.protocol + " " + self.localAddress + ":" +
                   std::to_string(self.localPort) + " -> " +
                   self.remoteAddress + ":" + std::to_string(self.remotePort);
        });

    // NetworkInterface class binding
    py::class_<atom::system::NetworkInterface>(
        m, "NetworkInterface",
        R"(Represents a network interface.

This class provides information about a network interface including
its name, IP addresses, MAC address, and status.

Examples:
    >>> interface = interfaces[0]  # Get first interface
    >>> print(f"Name: {interface.get_name()}")
    >>> print(f"MAC: {interface.get_mac()}")
    >>> print(f"Up: {interface.is_up()}")
    >>> for addr in interface.get_addresses():
    ...     print(f"Address: {addr}")
)")
        .def(py::init<std::string, std::vector<std::string>, std::string,
                      bool>(),
             py::arg("name"), py::arg("addresses"), py::arg("mac"),
             py::arg("is_up"),
             "Constructs a NetworkInterface with specified parameters.")
        .def("get_name", &atom::system::NetworkInterface::getName,
             py::return_value_policy::reference_internal,
             R"(Gets the name of the network interface.

Returns:
    The name of the network interface.

Examples:
    >>> name = interface.get_name()
    >>> print(f"Interface name: {name}")
)")
        .def("get_addresses",
             static_cast<const std::vector<std::string>& (
                 atom::system::NetworkInterface::*)() const>(
                 &atom::system::NetworkInterface::getAddresses),
             py::return_value_policy::reference_internal,
             R"(Gets the IP addresses associated with the network interface.

Returns:
    List of IP addresses.

Examples:
    >>> addresses = interface.get_addresses()
    >>> for addr in addresses:
    ...     print(f"IP: {addr}")
)")
        .def("get_mac", &atom::system::NetworkInterface::getMac,
             py::return_value_policy::reference_internal,
             R"(Gets the MAC address of the network interface.

Returns:
    The MAC address.

Examples:
    >>> mac = interface.get_mac()
    >>> print(f"MAC address: {mac}")
)")
        .def("is_up", &atom::system::NetworkInterface::isUp,
             R"(Checks if the network interface is up.

Returns:
    True if the interface is up, False otherwise.

Examples:
    >>> if interface.is_up():
    ...     print("Interface is active")
    ... else:
    ...     print("Interface is down")
)")
        .def("__repr__",
             [](const atom::system::NetworkInterface& self) {
                 return "<NetworkInterface(name='" + self.getName() +
                        "', mac='" + self.getMac() +
                        "', up=" + (self.isUp() ? "True" : "False") + ")>";
             })
        .def("__str__", [](const atom::system::NetworkInterface& self) {
            return self.getName() + " (" + self.getMac() + ") - " +
                   (self.isUp() ? "UP" : "DOWN");
        });

    // NetworkManager class binding
    py::class_<atom::system::NetworkManager>(
        m, "NetworkManager",
        R"(Manages network interfaces and connections.

This class provides methods to enumerate network interfaces, manage
their state, and perform DNS operations.

Examples:
    >>> manager = network_manager.NetworkManager()
    >>> interfaces = manager.get_network_interfaces()
    >>> status = manager.get_interface_status("eth0")
)")
        .def(py::init<>(), "Constructs a NetworkManager object.")
        .def("get_network_interfaces",
             &atom::system::NetworkManager::getNetworkInterfaces,
             R"(Gets the list of network interfaces.

Returns:
    List of NetworkInterface objects.

Examples:
    >>> interfaces = manager.get_network_interfaces()
    >>> print(f"Found {len(interfaces)} interfaces")
    >>> for iface in interfaces:
    ...     print(f"  {iface.get_name()}: {iface.get_mac()}")
)")
        .def("get_interface_status",
             &atom::system::NetworkManager::getInterfaceStatus,
             py::arg("interface_name"),
             R"(Gets the status of a network interface.

Args:
    interface_name: The name of the network interface.

Returns:
    The status of the network interface as a string.

Examples:
    >>> status = manager.get_interface_status("eth0")
    >>> print(f"eth0 status: {status}")
)")
        .def("monitor_connection_status",
             &atom::system::NetworkManager::monitorConnectionStatus,
             R"(Monitors the connection status of network interfaces.

This method starts monitoring network interface status changes.
It runs in the background and can be used to detect network changes.

Examples:
    >>> manager.monitor_connection_status()
)")

        // Static methods
        .def_static("enable_interface",
                    &atom::system::NetworkManager::enableInterface,
                    py::arg("interface_name"),
                    R"(Enables a network interface.

Args:
    interface_name: The name of the network interface to enable.

Raises:
    RuntimeError: If the interface cannot be enabled.

Examples:
    >>> network_manager.NetworkManager.enable_interface("eth0")

Note:
    This operation typically requires administrator/root privileges.
)")
        .def_static("disable_interface",
                    &atom::system::NetworkManager::disableInterface,
                    py::arg("interface_name"),
                    R"(Disables a network interface.

Args:
    interface_name: The name of the network interface to disable.

Raises:
    RuntimeError: If the interface cannot be disabled.

Examples:
    >>> network_manager.NetworkManager.disable_interface("eth0")

Note:
    This operation typically requires administrator/root privileges.
)")
        .def_static("resolve_dns", &atom::system::NetworkManager::resolveDNS,
                    py::arg("hostname"),
                    R"(Resolves a DNS hostname to an IP address.

Args:
    hostname: The DNS hostname to resolve.

Returns:
    The resolved IP address as a string.

Raises:
    RuntimeError: If DNS resolution fails.

Examples:
    >>> ip = network_manager.NetworkManager.resolve_dns("google.com")
    >>> print(f"google.com resolves to: {ip}")
)")
        .def_static("get_dns_servers",
                    &atom::system::NetworkManager::getDNSServers,
                    R"(Gets the list of DNS servers.

Returns:
    List of DNS server addresses.

Examples:
    >>> dns_servers = network_manager.NetworkManager.get_dns_servers()
    >>> print("DNS servers:")
    >>> for server in dns_servers:
    ...     print(f"  {server}")
)")
        .def_static("set_dns_servers",
                    &atom::system::NetworkManager::setDNSServers,
                    py::arg("dns_servers"),
                    R"(Sets the list of DNS servers.

Args:
    dns_servers: List of DNS server addresses.

Raises:
    RuntimeError: If DNS servers cannot be set.

Examples:
    >>> dns_servers = ["8.8.8.8", "8.8.4.4"]
    >>> network_manager.NetworkManager.set_dns_servers(dns_servers)

Note:
    This operation typically requires administrator/root privileges.
)")
        .def_static("add_dns_server",
                    &atom::system::NetworkManager::addDNSServer, py::arg("dns"),
                    R"(Adds a DNS server to the list.

Args:
    dns: The DNS server address to add.

Raises:
    RuntimeError: If the DNS server cannot be added.

Examples:
    >>> network_manager.NetworkManager.add_dns_server("1.1.1.1")

Note:
    This operation typically requires administrator/root privileges.
)")
        .def_static("remove_dns_server",
                    &atom::system::NetworkManager::removeDNSServer,
                    py::arg("dns"),
                    R"(Removes a DNS server from the list.

Args:
    dns: The DNS server address to remove.

Raises:
    RuntimeError: If the DNS server cannot be removed.

Examples:
    >>> network_manager.NetworkManager.remove_dns_server("1.1.1.1")

Note:
    This operation typically requires administrator/root privileges.
)");

    // Standalone function for getting network connections by PID
    m.def("get_network_connections", &atom::system::getNetworkConnections,
          py::arg("pid"),
          R"(Gets the network connections of a process by its PID.

Args:
    pid: The process ID.

Returns:
    List of NetworkConnection objects representing the network connections.

Raises:
    RuntimeError: If the process is not found or connections cannot be retrieved.

Examples:
    >>> connections = network_manager.get_network_connections(1234)
    >>> print(f"Process 1234 has {len(connections)} connections:")
    >>> for conn in connections:
    ...     print(f"  {conn}")
)");
}
