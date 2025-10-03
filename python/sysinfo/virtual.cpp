#include "atom/sysinfo/virtual.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(virtual, m) {
    m.doc() = "Virtualization and container detection module for the atom package";

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

    // Hypervisor detection functions
    m.def("get_hypervisor_vendor", &getHypervisorVendor,
          R"(Retrieve the vendor information of the hypervisor.

Uses CPUID instruction to get the hypervisor vendor string.
Common vendors include VMware, VirtualBox, Hyper-V, KVM, and others.

Returns:
    String containing the vendor string of the hypervisor, or empty if no hypervisor is detected.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check hypervisor vendor
    >>> vendor = virtual.get_hypervisor_vendor()
    >>> if vendor:
    ...     print(f"Running under hypervisor: {vendor}")
    ... else:
    ...     print("No hypervisor detected")
)");

    m.def("is_virtual_machine", &isVirtualMachine,
          R"(Detect if the system is running inside a virtual machine.

Uses various techniques, including CPUID instruction, to determine if the current
system is a virtual machine.

Returns:
    Boolean indicating whether the system is a virtual machine.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check if running in VM
    >>> if virtual.is_virtual_machine():
    ...     print("Running in a virtual machine")
    ...     vm_type = virtual.get_virtualization_type()
    ...     print(f"Virtualization type: {vm_type}")
    ... else:
    ...     print("Running on physical hardware")
)");

    // Individual detection methods
    m.def("check_bios", &checkBIOS,
          R"(Check BIOS information to identify if the system is a virtual machine.

Inspects the BIOS information for signs that indicate the presence of a virtual machine,
such as manufacturer strings, version information, and other BIOS characteristics
commonly associated with virtualization.

Returns:
    Boolean indicating whether BIOS information suggests a virtual machine.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check BIOS for VM indicators
    >>> if virtual.check_bios():
    ...     print("BIOS indicates virtual machine")
)");

    m.def("check_network_adapter", &checkNetworkAdapter,
          R"(Check the network adapter for common virtual machine adapters.

Looks for network adapters that are commonly used by virtual machines, such as
"VMware Virtual Ethernet Adapter", "VirtualBox Host-Only Adapter", or other
virtualization-specific MAC address prefixes.

Returns:
    Boolean indicating whether a virtual machine network adapter is found.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check network adapters for VM indicators
    >>> if virtual.check_network_adapter():
    ...     print("Virtual network adapter detected")
)");

    m.def("check_disk", &checkDisk,
          R"(Check disk information for identifiers commonly used by virtual machines.

Inspects the disk information to find identifiers that are typically associated with
virtual machine disks, such as specific model names, serial numbers, or device paths
that indicate virtualized storage.

Returns:
    Boolean indicating whether virtual machine disk identifiers are found.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check disk for VM indicators
    >>> if virtual.check_disk():
    ...     print("Virtual disk detected")
)");

    m.def("check_graphics_card", &checkGraphicsCard,
          R"(Check the graphics card device for signs of virtualization.

Examines the graphics card device to determine if it is a type commonly used by
virtual machines, such as virtual GPU adapters or basic display adapters with
limited capabilities typical of virtualized environments.

Returns:
    Boolean indicating whether a virtual machine graphics card is detected.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check graphics card for VM indicators
    >>> if virtual.check_graphics_card():
    ...     print("Virtual graphics card detected")
)");

    m.def("check_processes", &checkProcesses,
          R"(Check for the presence of common virtual machine processes.

Scans the system processes to identify any that are typically associated with
virtual machines, such as VMware Tools, VirtualBox Guest Additions, or other
virtualization management services.

Returns:
    Boolean indicating whether virtual machine processes are found.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check processes for VM indicators
    >>> if virtual.check_processes():
    ...     print("Virtual machine processes detected")
)");

    m.def("check_pci_bus", &checkPCIBus,
          R"(Check PCI bus devices for virtualization indicators.

Inspects the PCI bus devices to see if any of them are known to be used by
virtual machines, including specific vendor IDs and device IDs that are
associated with virtualization platforms.

Returns:
    Boolean indicating whether virtual machine PCI bus devices are found.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check PCI bus for VM indicators
    >>> if virtual.check_pci_bus():
    ...     print("Virtual PCI devices detected")
)");

    m.def("check_time_drift", &checkTimeDrift,
          R"(Detect time drift and offset issues that may indicate a virtual machine.

Checks for irregularities in system time management, which can be a sign of running
inside a virtual machine. Measures timing discrepancies between different clock
sources that often occur in virtualized environments.

Returns:
    Boolean indicating whether time drift or offset issues are detected.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check for time drift indicators
    >>> if virtual.check_time_drift():
    ...     print("Time drift detected (possible VM)")
)");

    // Container detection functions
    m.def("is_docker_container", &isDockerContainer,
          R"(Detect if the system is running inside a Docker container.

Examines system characteristics specific to Docker containerization, such as
checking for the presence of /.dockerenv file, cgroup configurations, and
container-specific environment variables.

Returns:
    Boolean indicating whether running in a Docker container.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check if running in Docker
    >>> if virtual.is_docker_container():
    ...     print("Running in Docker container")
    ... else:
    ...     print("Not running in Docker")
)");

    m.def("is_container", &isContainer,
          R"(Detect if the system is running inside a container.

Checks for various container technologies including Docker, LXC/LXD, Kubernetes pods,
and other containerization solutions.

Returns:
    Boolean indicating whether running in any type of container.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Check if running in any container
    >>> if virtual.is_container():
    ...     print("Running in a container")
    ...     container_type = virtual.get_container_type()
    ...     print(f"Container type: {container_type}")
    ... else:
    ...     print("Not running in a container")
)");

    m.def("get_container_type", &getContainerType,
          R"(Get the container type if running in a containerized environment.

If the system is running in a container, this function attempts to identify
the specific container technology in use.

Returns:
    String containing the name of the container technology (e.g., "Docker", "LXC"),
    or empty string if not in a container.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Get container type
    >>> container_type = virtual.get_container_type()
    >>> if container_type:
    ...     print(f"Container type: {container_type}")
    ... else:
    ...     print("Not running in a container")
)");

    // Advanced detection functions
    m.def("get_virtualization_confidence", &getVirtualizationConfidence,
          R"(Comprehensive virtualization detection with confidence score.

Combines multiple detection methods to provide a more accurate assessment of whether
the system is running in a virtualized environment. The function weighs different
indicators based on their reliability to calculate an overall confidence score.

Returns:
    Float between 0.0 and 1.0, with higher values indicating greater likelihood of virtualization.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Get virtualization confidence score
    >>> confidence = virtual.get_virtualization_confidence()
    >>> print(f"Virtualization confidence: {confidence:.2f}")
    >>> if confidence > 0.8:
    ...     print("Very likely running in a virtual environment")
    >>> elif confidence > 0.5:
    ...     print("Possibly running in a virtual environment")
    >>> else:
    ...     print("Likely running on physical hardware")
)");

    m.def("get_virtualization_type", &getVirtualizationType,
          R"(Detect the specific type of virtualization technology in use.

Attempts to identify the specific virtualization platform or technology being used,
such as VMware, VirtualBox, Hyper-V, KVM/QEMU, Xen, or others.

Returns:
    String containing the name of the detected virtualization technology, or "Unknown"
    if the type cannot be determined.

Examples:
    >>> from atom.sysinfo import virtual
    >>> # Get specific virtualization type
    >>> vm_type = virtual.get_virtualization_type()
    >>> if vm_type != "Unknown":
    ...     print(f"Virtualization platform: {vm_type}")
    ... else:
    ...     print("Virtualization type could not be determined")
)");


}
