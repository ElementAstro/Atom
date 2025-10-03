#include "atom/sysinfo/sn.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(sn, m) {
    m.doc() = "Hardware serial number information module for the atom package";

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

    // HardwareInfo class binding
    py::class_<HardwareInfo>(m, "HardwareInfo",
                            R"(Hardware information class that provides access to system hardware serial numbers.

This class uses the PIMPL idiom to hide platform-specific implementation details.
It supports both Windows (via WMI) and Linux (via filesystem) platforms.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Create hardware info instance
    >>> hw_info = sn.HardwareInfo()
    >>> 
    >>> # Get various hardware serial numbers
    >>> bios_sn = hw_info.get_bios_serial_number()
    >>> motherboard_sn = hw_info.get_motherboard_serial_number()
    >>> cpu_sn = hw_info.get_cpu_serial_number()
    >>> disk_sns = hw_info.get_disk_serial_numbers()
    >>> 
    >>> print(f"BIOS Serial: {bios_sn}")
    >>> print(f"Motherboard Serial: {motherboard_sn}")
    >>> print(f"CPU Serial: {cpu_sn}")
    >>> print(f"Disk Serials: {disk_sns}")
)")
        .def(py::init<>(), "Constructs a new HardwareInfo object.")
        .def(py::init<const HardwareInfo&>(), py::arg("other"),
             "Copy constructor.")
        .def("get_bios_serial_number", &HardwareInfo::getBiosSerialNumber,
             R"(Get BIOS serial number.

Returns:
    String containing the BIOS serial number, or empty string if not available.

Examples:
    >>> from atom.sysinfo import sn
    >>> hw_info = sn.HardwareInfo()
    >>> bios_serial = hw_info.get_bios_serial_number()
    >>> if bios_serial:
    ...     print(f"BIOS Serial Number: {bios_serial}")
    ... else:
    ...     print("BIOS serial number not available")
)")
        .def("get_motherboard_serial_number", &HardwareInfo::getMotherboardSerialNumber,
             R"(Get motherboard serial number.

Returns:
    String containing the motherboard serial number, or empty string if not available.

Examples:
    >>> from atom.sysinfo import sn
    >>> hw_info = sn.HardwareInfo()
    >>> mb_serial = hw_info.get_motherboard_serial_number()
    >>> if mb_serial:
    ...     print(f"Motherboard Serial Number: {mb_serial}")
    ... else:
    ...     print("Motherboard serial number not available")
)")
        .def("get_cpu_serial_number", &HardwareInfo::getCpuSerialNumber,
             R"(Get CPU serial number.

Returns:
    String containing the CPU serial number, or empty string if not available.

Note:
    CPU serial numbers may not be available on all processors or may be
    disabled for privacy reasons.

Examples:
    >>> from atom.sysinfo import sn
    >>> hw_info = sn.HardwareInfo()
    >>> cpu_serial = hw_info.get_cpu_serial_number()
    >>> if cpu_serial:
    ...     print(f"CPU Serial Number: {cpu_serial}")
    ... else:
    ...     print("CPU serial number not available")
)")
        .def("get_disk_serial_numbers", &HardwareInfo::getDiskSerialNumbers,
             R"(Get disk serial numbers.

Returns:
    List of strings containing serial numbers for all detected storage devices.

Examples:
    >>> from atom.sysinfo import sn
    >>> hw_info = sn.HardwareInfo()
    >>> disk_serials = hw_info.get_disk_serial_numbers()
    >>> print(f"Found {len(disk_serials)} storage device(s):")
    >>> for i, serial in enumerate(disk_serials):
    ...     print(f"  Disk {i+1}: {serial}")
)")
        .def("__repr__", [](const HardwareInfo& info) {
            return "<HardwareInfo>";
        });

    // Convenience functions for quick access
    m.def("get_bios_serial", 
          []() {
              HardwareInfo hw;
              return hw.getBiosSerialNumber();
          },
          R"(Get BIOS serial number (convenience function).

Returns:
    String containing the BIOS serial number.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Quick access to BIOS serial
    >>> bios_serial = sn.get_bios_serial()
    >>> print(f"BIOS Serial: {bios_serial}")
)");

    m.def("get_motherboard_serial",
          []() {
              HardwareInfo hw;
              return hw.getMotherboardSerialNumber();
          },
          R"(Get motherboard serial number (convenience function).

Returns:
    String containing the motherboard serial number.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Quick access to motherboard serial
    >>> mb_serial = sn.get_motherboard_serial()
    >>> print(f"Motherboard Serial: {mb_serial}")
)");

    m.def("get_cpu_serial",
          []() {
              HardwareInfo hw;
              return hw.getCpuSerialNumber();
          },
          R"(Get CPU serial number (convenience function).

Returns:
    String containing the CPU serial number.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Quick access to CPU serial
    >>> cpu_serial = sn.get_cpu_serial()
    >>> print(f"CPU Serial: {cpu_serial}")
)");

    m.def("get_all_disk_serials",
          []() {
              HardwareInfo hw;
              return hw.getDiskSerialNumbers();
          },
          R"(Get all disk serial numbers (convenience function).

Returns:
    List of strings containing all disk serial numbers.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Quick access to all disk serials
    >>> disk_serials = sn.get_all_disk_serials()
    >>> for serial in disk_serials:
    ...     print(f"Disk Serial: {serial}")
)");

    m.def("get_hardware_summary",
          []() {
              HardwareInfo hw;
              py::dict summary;
              summary["bios_serial"] = hw.getBiosSerialNumber();
              summary["motherboard_serial"] = hw.getMotherboardSerialNumber();
              summary["cpu_serial"] = hw.getCpuSerialNumber();
              summary["disk_serials"] = hw.getDiskSerialNumbers();
              return summary;
          },
          R"(Get a comprehensive summary of all hardware serial numbers.

Returns:
    Dictionary containing all available hardware serial numbers.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Get complete hardware serial summary
    >>> summary = sn.get_hardware_summary()
    >>> print("Hardware Serial Numbers:")
    >>> print(f"  BIOS: {summary['bios_serial']}")
    >>> print(f"  Motherboard: {summary['motherboard_serial']}")
    >>> print(f"  CPU: {summary['cpu_serial']}")
    >>> print(f"  Disks: {summary['disk_serials']}")
)");

    m.def("has_hardware_serials",
          []() {
              HardwareInfo hw;
              bool has_bios = !hw.getBiosSerialNumber().empty();
              bool has_mb = !hw.getMotherboardSerialNumber().empty();
              bool has_cpu = !hw.getCpuSerialNumber().empty();
              bool has_disks = !hw.getDiskSerialNumbers().empty();
              return has_bios || has_mb || has_cpu || has_disks;
          },
          R"(Check if any hardware serial numbers are available.

Returns:
    Boolean indicating whether any hardware serial numbers could be retrieved.

Examples:
    >>> from atom.sysinfo import sn
    >>> # Check if hardware serials are available
    >>> if sn.has_hardware_serials():
    ...     print("Hardware serial numbers are available")
    ...     summary = sn.get_hardware_summary()
    ... else:
    ...     print("No hardware serial numbers available")
)");
}
