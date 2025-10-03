#include "atom/system/hardware/device.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(device, m) {
    m.doc() = R"pbdoc(
        Hardware Device Management Module
        ---------------------------------

        This module provides functions for enumerating and managing hardware devices
        in the system, including USB devices, serial ports, and Bluetooth devices.

        Examples:
            >>> from atom.system import device
            >>> 
            >>> # Enumerate USB devices
            >>> usb_devices = device.enumerate_usb_devices()
            >>> for dev in usb_devices:
            ...     print(f"USB Device: {dev.description} at {dev.address}")
            >>> 
            >>> # Enumerate serial ports
            >>> serial_ports = device.enumerate_serial_ports()
            >>> for port in serial_ports:
            ...     print(f"Serial Port: {port.description} at {port.address}")
            >>> 
            >>> # Enumerate Bluetooth devices
            >>> bt_devices = device.enumerate_bluetooth_devices()
            >>> for dev in bt_devices:
            ...     print(f"Bluetooth Device: {dev.description} at {dev.address}")
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

    // DeviceInfo structure binding
    py::class_<atom::system::DeviceInfo>(
        m, "DeviceInfo",
        R"(Structure to hold device information.

This class contains basic information about a hardware device,
including its description and address/identifier.

Attributes:
    description: Device description or name.
    address: Device address or identifier.

Examples:
    >>> from atom.system import device
    >>> devices = device.enumerate_usb_devices()
    >>> for dev in devices:
    ...     print(f"Device: {dev.description}")
    ...     print(f"Address: {dev.address}")
)")
        .def(py::init<>(),
             "Constructs an empty DeviceInfo object.")
        .def(py::init<const std::string&, const std::string&>(),
             py::arg("description"), py::arg("address"),
             "Constructs a DeviceInfo object with description and address.")
        .def_readwrite("description", &atom::system::DeviceInfo::description,
                      R"(Device description or name.

This field contains a human-readable description of the device,
such as the device name, model, or manufacturer information.

Examples:
    >>> dev = device.DeviceInfo("USB Mass Storage", "/dev/sdb1")
    >>> print(dev.description)  # "USB Mass Storage"
)")
        .def_readwrite("address", &atom::system::DeviceInfo::address,
                      R"(Device address or identifier.

This field contains the device address, path, or unique identifier
that can be used to access or reference the device.

Examples:
    >>> dev = device.DeviceInfo("USB Mass Storage", "/dev/sdb1")
    >>> print(dev.address)  # "/dev/sdb1"
)")
        .def("__repr__", [](const atom::system::DeviceInfo& self) {
            return "<DeviceInfo(description='" + self.description + 
                   "', address='" + self.address + "')>";
        })
        .def("__str__", [](const atom::system::DeviceInfo& self) {
            return self.description + " at " + self.address;
        });

    // USB device enumeration
    m.def("enumerate_usb_devices", &atom::system::enumerateUsbDevices,
          R"(Enumerate all USB devices in the system.

This function scans the system for all connected USB devices and returns
their information including device descriptions and addresses.

Returns:
    List of DeviceInfo objects representing USB devices.

Raises:
    RuntimeError: If the enumeration fails or USB subsystem is not available.

Examples:
    >>> from atom.system import device
    >>> usb_devices = device.enumerate_usb_devices()
    >>> print(f"Found {len(usb_devices)} USB devices:")
    >>> for dev in usb_devices:
    ...     print(f"  {dev.description} at {dev.address}")

Note:
    This function may require appropriate permissions to access USB device
    information on some systems. On Linux, it typically requires access to
    /sys/bus/usb/devices or similar system directories.
)");

    // Serial port enumeration
    m.def("enumerate_serial_ports", &atom::system::enumerateSerialPorts,
          R"(Enumerate all serial ports in the system.

This function scans the system for all available serial ports (COM ports
on Windows, /dev/tty* on Unix-like systems) and returns their information.

Returns:
    List of DeviceInfo objects representing serial ports.

Raises:
    RuntimeError: If the enumeration fails or serial subsystem is not available.

Examples:
    >>> from atom.system import device
    >>> serial_ports = device.enumerate_serial_ports()
    >>> print(f"Found {len(serial_ports)} serial ports:")
    >>> for port in serial_ports:
    ...     print(f"  {port.description} at {port.address}")
    >>> 
    >>> # Filter for specific port types
    >>> usb_serial = [p for p in serial_ports if "USB" in p.description]
    >>> print(f"USB serial ports: {len(usb_serial)}")

Note:
    The availability and naming of serial ports varies by platform:
    - Windows: COM1, COM2, etc.
    - Linux: /dev/ttyUSB0, /dev/ttyACM0, /dev/ttyS0, etc.
    - macOS: /dev/cu.*, /dev/tty.*
)");

    // Bluetooth device enumeration
    m.def("enumerate_bluetooth_devices", &atom::system::enumerateBluetoothDevices,
          R"(Enumerate all Bluetooth devices in the system.

This function scans for discoverable Bluetooth devices in the vicinity
and returns their information including device names and addresses.

Returns:
    List of DeviceInfo objects representing Bluetooth devices.

Raises:
    RuntimeError: If the enumeration fails, Bluetooth is not available,
                 or Bluetooth adapter is not enabled.

Examples:
    >>> from atom.system import device
    >>> try:
    ...     bt_devices = device.enumerate_bluetooth_devices()
    ...     print(f"Found {len(bt_devices)} Bluetooth devices:")
    ...     for dev in bt_devices:
    ...         print(f"  {dev.description} at {dev.address}")
    ... except RuntimeError as e:
    ...     print(f"Bluetooth enumeration failed: {e}")

Note:
    Bluetooth device enumeration may take several seconds to complete as it
    involves scanning for nearby devices. The function may require:
    - Bluetooth adapter to be enabled
    - Appropriate permissions (may require administrator/root privileges)
    - Target devices to be in discoverable mode
    
    On some systems, this operation may require user confirmation or
    may be restricted by security policies.
)");

    // Utility functions for device management
    m.def("get_device_count", []() -> py::dict {
        auto usb_devices = atom::system::enumerateUsbDevices();
        auto serial_ports = atom::system::enumerateSerialPorts();
        auto bt_devices = atom::system::enumerateBluetoothDevices();
        
        py::dict result;
        result["usb"] = usb_devices.size();
        result["serial"] = serial_ports.size();
        result["bluetooth"] = bt_devices.size();
        result["total"] = usb_devices.size() + serial_ports.size() + bt_devices.size();
        
        return result;
    }, R"(Get count of devices by type.

Returns:
    Dictionary containing device counts by type and total count.

Examples:
    >>> from atom.system import device
    >>> counts = device.get_device_count()
    >>> print(f"USB devices: {counts['usb']}")
    >>> print(f"Serial ports: {counts['serial']}")
    >>> print(f"Bluetooth devices: {counts['bluetooth']}")
    >>> print(f"Total devices: {counts['total']}")
)");

    m.def("find_devices_by_description", [](const std::string& pattern) -> py::list {
        py::list result;
        
        // Search USB devices
        auto usb_devices = atom::system::enumerateUsbDevices();
        for (const auto& dev : usb_devices) {
            if (dev.description.find(pattern) != std::string::npos) {
                result.append(dev);
            }
        }
        
        // Search serial ports
        auto serial_ports = atom::system::enumerateSerialPorts();
        for (const auto& dev : serial_ports) {
            if (dev.description.find(pattern) != std::string::npos) {
                result.append(dev);
            }
        }
        
        // Search Bluetooth devices
        auto bt_devices = atom::system::enumerateBluetoothDevices();
        for (const auto& dev : bt_devices) {
            if (dev.description.find(pattern) != std::string::npos) {
                result.append(dev);
            }
        }
        
        return result;
    }, py::arg("pattern"),
          R"(Find devices by description pattern.

Args:
    pattern: String pattern to search for in device descriptions.

Returns:
    List of DeviceInfo objects whose descriptions contain the pattern.

Examples:
    >>> from atom.system import device
    >>> # Find all USB devices
    >>> usb_devices = device.find_devices_by_description("USB")
    >>> 
    >>> # Find Arduino devices
    >>> arduino_devices = device.find_devices_by_description("Arduino")
    >>> 
    >>> # Find serial devices
    >>> serial_devices = device.find_devices_by_description("Serial")
)");

    m.def("get_all_devices", []() -> py::list {
        py::list result;
        
        auto usb_devices = atom::system::enumerateUsbDevices();
        for (const auto& dev : usb_devices) {
            result.append(dev);
        }
        
        auto serial_ports = atom::system::enumerateSerialPorts();
        for (const auto& dev : serial_ports) {
            result.append(dev);
        }
        
        auto bt_devices = atom::system::enumerateBluetoothDevices();
        for (const auto& dev : bt_devices) {
            result.append(dev);
        }
        
        return result;
    }, R"(Get all devices from all categories.

Returns:
    List of all DeviceInfo objects from USB, serial, and Bluetooth categories.

Examples:
    >>> from atom.system import device
    >>> all_devices = device.get_all_devices()
    >>> print(f"Total devices found: {len(all_devices)}")
    >>> for dev in all_devices:
    ...     print(f"  {dev}")
)");
}
