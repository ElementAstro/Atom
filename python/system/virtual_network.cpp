#include "atom/system/network/virtual_network.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(virtual_network, m) {
    m.doc() = R"pbdoc(
        Virtual Network Adapter Management Module
        -----------------------------------------

        This module provides functionality for creating, configuring, and managing
        virtual network adapters. It supports creating virtual network interfaces
        with custom IP configurations and DNS settings.

        Examples:
            >>> from atom.system import virtual_network
            >>> 
            >>> # Create adapter configuration
            >>> config = virtual_network.VirtualAdapterConfig()
            >>> config.adapter_name = "MyVirtualAdapter"
            >>> config.hardware_id = "TAP0901"
            >>> config.description = "Virtual Network Adapter for Testing"
            >>> config.ip_address = "192.168.100.1"
            >>> config.subnet_mask = "255.255.255.0"
            >>> config.gateway = "192.168.100.254"
            >>> config.primary_dns = "8.8.8.8"
            >>> config.secondary_dns = "8.8.4.4"
            >>> 
            >>> # Create virtual network adapter
            >>> adapter = virtual_network.VirtualNetworkAdapter()
            >>> if adapter.create(config):
            ...     print("Virtual adapter created successfully")
            >>> else:
            ...     print(f"Failed to create adapter: {adapter.get_last_error_message()}")

        Warning:
            Creating and managing virtual network adapters typically requires
            administrator/root privileges and may affect system network configuration.
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

    // VirtualAdapterConfig structure binding
    py::class_<VirtualAdapterConfig>(
        m, "VirtualAdapterConfig",
        R"(Configuration structure for virtual network adapter.

This class contains all the configuration parameters needed to create
and configure a virtual network adapter, including network settings
and DNS configuration.

Attributes:
    adapter_name: Name of the virtual adapter.
    hardware_id: Hardware ID for the adapter.
    description: Description of the adapter.
    ip_address: IP address to assign.
    subnet_mask: Subnet mask to assign.
    gateway: Default gateway to assign.
    primary_dns: Primary DNS server.
    secondary_dns: Secondary DNS server.

Examples:
    >>> config = virtual_network.VirtualAdapterConfig()
    >>> config.adapter_name = "TestAdapter"
    >>> config.ip_address = "10.0.0.1"
    >>> config.subnet_mask = "255.255.255.0"
)")
        .def(py::init<>(),
             "Constructs an empty VirtualAdapterConfig object.")
        .def_readwrite("adapter_name", &VirtualAdapterConfig::adapterName,
                      R"(Name of the virtual adapter.

This is the display name that will appear in network interface listings.
Should be unique and descriptive.

Examples:
    >>> config.adapter_name = "MyVPN_Adapter"
)")
        .def_readwrite("hardware_id", &VirtualAdapterConfig::hardwareID,
                      R"(Hardware ID for the adapter.

This identifies the type of virtual adapter to create. Common values:
- "TAP0901" for TAP adapters
- "ROOT\\NET" for generic network adapters

Examples:
    >>> config.hardware_id = "TAP0901"
)")
        .def_readwrite("description", &VirtualAdapterConfig::description,
                      R"(Description of the adapter.

A human-readable description that appears in network adapter properties.

Examples:
    >>> config.description = "Virtual Adapter for VPN Connection"
)")
        .def_readwrite("ip_address", &VirtualAdapterConfig::ipAddress,
                      R"(IP address to assign to the adapter.

The IPv4 address that will be assigned to this virtual interface.

Examples:
    >>> config.ip_address = "192.168.1.100"
)")
        .def_readwrite("subnet_mask", &VirtualAdapterConfig::subnetMask,
                      R"(Subnet mask to assign to the adapter.

The subnet mask that defines the network portion of the IP address.

Examples:
    >>> config.subnet_mask = "255.255.255.0"  # /24 network
)")
        .def_readwrite("gateway", &VirtualAdapterConfig::gateway,
                      R"(Default gateway to assign to the adapter.

The IP address of the default gateway for this network interface.

Examples:
    >>> config.gateway = "192.168.1.1"
)")
        .def_readwrite("primary_dns", &VirtualAdapterConfig::primaryDNS,
                      R"(Primary DNS server address.

The IP address of the primary DNS server for name resolution.

Examples:
    >>> config.primary_dns = "8.8.8.8"  # Google DNS
)")
        .def_readwrite("secondary_dns", &VirtualAdapterConfig::secondaryDNS,
                      R"(Secondary DNS server address.

The IP address of the secondary DNS server for backup name resolution.

Examples:
    >>> config.secondary_dns = "8.8.4.4"  # Google DNS secondary
)")
        .def("__repr__", [](const VirtualAdapterConfig& self) {
            return "<VirtualAdapterConfig(name='" + 
                   std::string(self.adapterName.begin(), self.adapterName.end()) + "')>";
        });

    // VirtualNetworkAdapter class binding
    py::class_<VirtualNetworkAdapter>(
        m, "VirtualNetworkAdapter",
        R"(Main class for managing virtual network adapters.

This class provides an interface for creating, configuring, and removing
virtual network adapters. It uses the Pimpl idiom to hide implementation
details and provide a clean interface.

Examples:
    >>> adapter = virtual_network.VirtualNetworkAdapter()
    >>> 
    >>> # Create a new virtual adapter
    >>> config = virtual_network.VirtualAdapterConfig()
    >>> # ... configure settings ...
    >>> if adapter.create(config):
    ...     print("Adapter created successfully")
    >>> 
    >>> # Configure IP settings
    >>> success = adapter.configure_ip(
    ...     "MyAdapter", "10.0.0.1", "255.255.255.0", "10.0.0.254"
    ... )
    >>> 
    >>> # Remove the adapter when done
    >>> adapter.remove("MyAdapter")

Warning:
    Virtual network adapter operations typically require administrator
    privileges and may affect system network configuration.
)")
        .def(py::init<>(),
             "Construct a new Virtual Network Adapter manager.")
        .def("create", &VirtualNetworkAdapter::Create,
             py::arg("config"),
             R"(Creates a virtual network adapter.

Args:
    config: Configuration parameters for the adapter.

Returns:
    True if creation succeeded, False otherwise.

Examples:
    >>> config = virtual_network.VirtualAdapterConfig()
    >>> config.adapter_name = "TestAdapter"
    >>> config.hardware_id = "TAP0901"
    >>> config.ip_address = "192.168.100.1"
    >>> config.subnet_mask = "255.255.255.0"
    >>> 
    >>> adapter = virtual_network.VirtualNetworkAdapter()
    >>> if adapter.create(config):
    ...     print("Virtual adapter created successfully")
    >>> else:
    ...     error = adapter.get_last_error_message()
    ...     print(f"Failed to create adapter: {error}")

Note:
    - Requires administrator privileges on Windows
    - May require specific drivers to be installed
    - Adapter name must be unique on the system
    - Some antivirus software may interfere with virtual adapters
)")
        .def("remove", &VirtualNetworkAdapter::Remove,
             py::arg("adapter_name"),
             R"(Removes a virtual network adapter.

Args:
    adapter_name: Name of the adapter to remove.

Returns:
    True if removal succeeded, False otherwise.

Examples:
    >>> if adapter.remove("TestAdapter"):
    ...     print("Adapter removed successfully")
    >>> else:
    ...     error = adapter.get_last_error_message()
    ...     print(f"Failed to remove adapter: {error}")

Warning:
    Removing a virtual adapter will disconnect any active connections
    using that adapter and may disrupt network services.
)")
        .def("configure_ip", &VirtualNetworkAdapter::ConfigureIP,
             py::arg("adapter_name"), py::arg("ip_address"), 
             py::arg("subnet_mask"), py::arg("gateway"),
             R"(Configures IP settings for an adapter.

Args:
    adapter_name: Name of the adapter to configure.
    ip_address: IP address to assign.
    subnet_mask: Subnet mask to assign.
    gateway: Default gateway to assign.

Returns:
    True if configuration succeeded, False otherwise.

Examples:
    >>> success = adapter.configure_ip(
    ...     "MyAdapter",
    ...     "10.0.0.100",
    ...     "255.255.255.0", 
    ...     "10.0.0.1"
    ... )
    >>> if success:
    ...     print("IP configuration applied")
    >>> else:
    ...     print(f"Configuration failed: {adapter.get_last_error_message()}")

Note:
    - The adapter must exist before configuring IP settings
    - Previous IP configuration will be replaced
    - Changes take effect immediately
    - May require administrator privileges
)")
        .def("configure_dns", &VirtualNetworkAdapter::ConfigureDNS,
             py::arg("adapter_name"), py::arg("primary_dns"), py::arg("secondary_dns"),
             R"(Configures DNS settings for an adapter.

Args:
    adapter_name: Name of the adapter to configure.
    primary_dns: Primary DNS server address.
    secondary_dns: Secondary DNS server address.

Returns:
    True if configuration succeeded, False otherwise.

Examples:
    >>> success = adapter.configure_dns(
    ...     "MyAdapter",
    ...     "8.8.8.8",      # Google DNS
    ...     "1.1.1.1"       # Cloudflare DNS
    ... )
    >>> if success:
    ...     print("DNS configuration applied")

Note:
    - DNS settings are applied per-adapter
    - Secondary DNS is optional but recommended
    - Changes may require network restart to take full effect
)")
        .def("get_last_error_message", &VirtualNetworkAdapter::GetLastErrorMessage,
             R"(Gets the last error message.

Returns:
    Last error message as a string.

Examples:
    >>> if not adapter.create(config):
    ...     error = adapter.get_last_error_message()
    ...     print(f"Error: {error}")
    ...     
    ...     # Common error messages:
    ...     # - "Access denied" - Need administrator privileges
    ...     # - "Driver not found" - Virtual adapter driver not installed
    ...     # - "Name already exists" - Adapter name is not unique

Note:
    Error messages are platform-specific and may contain technical details
    about the underlying system error that occurred.
)");

    // Utility functions for virtual network management
    m.def("create_simple_adapter", [](const std::string& name, const std::string& ip, 
                                     const std::string& mask) -> bool {
        VirtualAdapterConfig config;
        
        // Convert strings to wide strings for Windows API
        config.adapterName = std::wstring(name.begin(), name.end());
        config.hardwareID = L"TAP0901";  // Default TAP adapter
        config.description = L"Simple Virtual Adapter";
        config.ipAddress = std::wstring(ip.begin(), ip.end());
        config.subnetMask = std::wstring(mask.begin(), mask.end());
        config.gateway = L"";  // No gateway by default
        config.primaryDNS = L"8.8.8.8";
        config.secondaryDNS = L"8.8.4.4";
        
        VirtualNetworkAdapter adapter;
        return adapter.Create(config);
    }, py::arg("name"), py::arg("ip_address"), py::arg("subnet_mask"),
          R"(Create a simple virtual adapter with basic configuration.

Args:
    name: Name for the virtual adapter.
    ip_address: IP address to assign.
    subnet_mask: Subnet mask to use.

Returns:
    True if the adapter was created successfully, False otherwise.

Examples:
    >>> # Create a simple test adapter
    >>> success = virtual_network.create_simple_adapter(
    ...     "TestNet", "192.168.200.1", "255.255.255.0"
    ... )
    >>> if success:
    ...     print("Simple adapter created")

Note:
    This is a convenience function that creates a TAP adapter with
    default settings and Google DNS servers.
)");

    m.def("remove_adapter_by_name", [](const std::string& name) -> bool {
        VirtualNetworkAdapter adapter;
        std::wstring wide_name(name.begin(), name.end());
        return adapter.Remove(wide_name);
    }, py::arg("adapter_name"),
          R"(Remove a virtual adapter by name.

Args:
    adapter_name: Name of the adapter to remove.

Returns:
    True if the adapter was removed successfully, False otherwise.

Examples:
    >>> if virtual_network.remove_adapter_by_name("TestNet"):
    ...     print("Adapter removed successfully")

Note:
    This is a convenience function for removing adapters without
    creating a VirtualNetworkAdapter instance.
)");

    m.def("get_adapter_info", [](const std::string& name) -> py::dict {
        py::dict info;
        info["name"] = name;
        info["exists"] = false;  // Would need system query to determine
        info["type"] = "virtual";
        
        // Note: Actual implementation would query system for adapter details
        // This is a placeholder that shows the expected structure
        
        return info;
    }, py::arg("adapter_name"),
          R"(Get information about a virtual adapter.

Args:
    adapter_name: Name of the adapter to query.

Returns:
    Dictionary containing adapter information.

Examples:
    >>> info = virtual_network.get_adapter_info("MyAdapter")
    >>> print(f"Adapter exists: {info['exists']}")
    >>> print(f"Type: {info['type']}")

Note:
    This function provides basic information about virtual adapters.
    Detailed implementation would query the system for actual adapter status.
)");
}
