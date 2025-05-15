#include "atom/sysinfo/disk.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(disk, m) {
    m.doc() = "Disk and storage information module for the atom package";

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

    // DiskInfo structure binding
    py::class_<DiskInfo>(m, "DiskInfo",
                         R"(Structure to represent disk information.

This class provides detailed information about a disk, including its mount point,
device path, model, filesystem type, and usage statistics.

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get information about all disks
    >>> disk_info_list = disk.get_disk_info()
    >>> for info in disk_info_list:
    ...     print(f"Path: {info.path}")
    ...     print(f"Device: {info.device_path}")
    ...     print(f"Total space: {info.total_space / (1024**3):.2f} GB")
    ...     print(f"Free space: {info.free_space / (1024**3):.2f} GB")
    ...     print(f"Usage: {info.usage_percent:.1f}%")
)")
        .def(py::init<>(), "Constructs a new DiskInfo object.")
        .def_readwrite("path", &DiskInfo::path, "Mount point or volume path")
        .def_readwrite("device_path", &DiskInfo::devicePath,
                       "Physical device path")
        .def_readwrite("model", &DiskInfo::model, "Disk model name")
        .def_readwrite("fs_type", &DiskInfo::fsType,
                       "File system type (e.g., NTFS, ext4)")
        .def_readwrite("total_space", &DiskInfo::totalSpace,
                       "Total space in bytes")
        .def_readwrite("free_space", &DiskInfo::freeSpace,
                       "Free space in bytes")
        .def_readwrite("usage_percent", &DiskInfo::usagePercent,
                       "Usage percentage (0-100)")
        .def_readwrite("is_removable", &DiskInfo::isRemovable,
                       "Whether the disk is removable")
        .def_property_readonly(
            "used_space",
            [](const DiskInfo& info) {
                return info.totalSpace - info.freeSpace;
            },
            "Used space in bytes (calculated from total_space - free_space)")
        .def("__repr__", [](const DiskInfo& info) {
            return "<DiskInfo path='" + info.path + "' model='" + info.model +
                   "' total=" +
                   std::to_string(info.totalSpace / (1024 * 1024 * 1024)) +
                   "GB" + " used=" + std::to_string(info.usagePercent) + "%>";
        });

    // StorageDevice structure binding
    py::class_<StorageDevice>(m, "StorageDevice",
                              R"(Structure to represent a storage device.

This class provides information about a physical storage device, including its
path, model, serial number, and size.

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get information about all storage devices
    >>> devices = disk.get_storage_devices()
    >>> for device in devices:
    ...     print(f"Device: {device.device_path}")
    ...     print(f"Model: {device.model}")
    ...     print(f"Size: {device.size_bytes / (1024**3):.2f} GB")
    ...     print(f"Removable: {device.is_removable}")
)")
        .def(py::init<>(), "Constructs a new StorageDevice object.")
        .def_readwrite("device_path", &StorageDevice::devicePath,
                       "Path to the device (e.g., /dev/sda)")
        .def_readwrite("model", &StorageDevice::model, "Device model name")
        .def_readwrite("serial_number", &StorageDevice::serialNumber,
                       "Serial number if available")
        .def_readwrite("size_bytes", &StorageDevice::sizeBytes, "Size in bytes")
        .def_readwrite("is_removable", &StorageDevice::isRemovable,
                       "Whether the device is removable")
        .def_property_readonly(
            "size_gb",
            [](const StorageDevice& device) {
                return static_cast<double>(device.sizeBytes) /
                       (1024 * 1024 * 1024);
            },
            "Size in gigabytes (calculated from size_bytes)")
        .def("__repr__", [](const StorageDevice& device) {
            return "<StorageDevice path='" + device.devicePath + "' model='" +
                   device.model + "' size=" +
                   std::to_string(device.sizeBytes / (1024 * 1024 * 1024)) +
                   "GB" + " removable=" +
                   std::string(device.isRemovable ? "True" : "False") + ">";
        });

    // SecurityPolicy enum binding
    py::enum_<SecurityPolicy>(m, "SecurityPolicy",
                              "Enumeration of device security policies")
        .value("DEFAULT", SecurityPolicy::DEFAULT, "System default policy")
        .value("READ_ONLY", SecurityPolicy::READ_ONLY, "Force read-only access")
        .value("SCAN_BEFORE_USE", SecurityPolicy::SCAN_BEFORE_USE,
               "Scan for malware before allowing access")
        .value("WHITELIST_ONLY", SecurityPolicy::WHITELIST_ONLY,
               "Only allow whitelisted devices")
        .value("QUARANTINE", SecurityPolicy::QUARANTINE,
               "Isolate device content")
        .export_values();

    // Function bindings
    m.def("get_disk_info", &getDiskInfo, py::arg("include_removable") = true,
          R"(Retrieves detailed disk information for all available disks.

This function scans the system for all available disks and returns
detailed information for each one, including usage, filesystem type,
and device model information.

Args:
    include_removable: Whether to include removable drives in the results (default: True)

Returns:
    List of DiskInfo objects for each available disk

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get info for all disks including removable
    >>> all_disks = disk.get_disk_info()
    >>> for d in all_disks:
    ...     print(f"{d.path}: {d.usage_percent:.1f}% used")
    >>> 
    >>> # Get only fixed disks (exclude removable)
    >>> fixed_disks = disk.get_disk_info(include_removable=False)
)");

    m.def("get_disk_usage", &getDiskUsage,
          R"(Retrieves the disk usage information for all available disks.

This function is a simplified version that focuses only on getting disk paths
and usage. For more detailed information, use get_disk_info() instead.

Returns:
    List of (path, usage_percent) tuples for each available disk

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get basic disk usage
    >>> usage_list = disk.get_disk_usage()
    >>> for path, percent in usage_list:
    ...     print(f"{path}: {percent:.1f}% used")
)");

    m.def("get_drive_model", &getDriveModel, py::arg("drive_path"),
          R"(Retrieves the model of a specified drive.

Args:
    drive_path: Path of the drive to query

Returns:
    String containing the model name of the drive

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get model name of a specific drive
    >>> model = disk.get_drive_model("C:")  # Windows
    >>> # Or on Linux/macOS
    >>> model = disk.get_drive_model("/dev/sda")
    >>> print(f"Drive model: {model}")
)");

    m.def("get_storage_devices", &getStorageDevices,
          py::arg("include_removable") = true,
          R"(Retrieves information about all connected storage devices.

Args:
    include_removable: Whether to include removable storage devices (default: True)

Returns:
    List of StorageDevice objects for each connected storage device

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get all storage devices
    >>> devices = disk.get_storage_devices()
    >>> for device in devices:
    ...     print(f"{device.model} ({device.size_bytes / (1024**3):.1f} GB)
    >>> 
    >>> # Get only fixed storage devices (exclude removable)
    >>> fixed_devices = disk.get_storage_devices(include_removable=False)
)");

    m.def("get_storage_device_models", &getStorageDeviceModels,
          R"(Legacy function that returns pairs of device paths and models.

Returns:
    List of (device_path, model) tuples for each storage device

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get device paths and models
    >>> device_models = disk.get_storage_device_models()
    >>> for path, model in device_models:
    ...     print(f"{path}: {model}")
)");

    m.def("get_available_drives", &getAvailableDrives,
          py::arg("include_removable") = true,
          R"(Retrieves a list of all available drives on the system.

Args:
    include_removable: Whether to include removable drives (default: True)

Returns:
    List of strings representing available drives

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get all available drives
    >>> drives = disk.get_available_drives()
    >>> print(f"Available drives: {', '.join(drives)}")
    >>> 
    >>> # Get only fixed drives
    >>> fixed_drives = disk.get_available_drives(include_removable=False)
)");

    m.def("calculate_disk_usage_percentage", &calculateDiskUsagePercentage,
          py::arg("total_space"), py::arg("free_space"),
          R"(Calculates the disk usage percentage.

Args:
    total_space: Total space on the disk, in bytes
    free_space: Free (available) space on the disk, in bytes

Returns:
    Disk usage percentage (0-100)

Examples:
    >>> from atom.sysinfo import disk
    >>> # Calculate usage percentage
    >>> total = 1000000000  # 1 GB
    >>> free = 250000000    # 250 MB
    >>> usage = disk.calculate_disk_usage_percentage(total, free)
    >>> print(f"Disk usage: {usage:.1f}%")
)");

    m.def("get_file_system_type", &getFileSystemType, py::arg("path"),
          R"(Retrieves the file system type for a specified path.

Args:
    path: Path to the disk or mount point

Returns:
    String containing the file system type (e.g., "NTFS", "ext4", "HFS+")

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get filesystem type
    >>> fs_type = disk.get_file_system_type("C:")  # Windows
    >>> # Or on Linux/macOS
    >>> fs_type = disk.get_file_system_type("/")
    >>> print(f"Filesystem type: {fs_type}")
)");

    m.def("add_device_to_whitelist", &addDeviceToWhitelist,
          py::arg("device_identifier"),
          R"(Adds a device to the security whitelist.

Args:
    device_identifier: Device identifier (serial number, UUID, etc.)

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import disk
    >>> # Add a device to the whitelist
    >>> success = disk.add_device_to_whitelist("SERIAL123456")
    >>> if success:
    ...     print("Device added to whitelist")
    ... else:
    ...     print("Failed to add device to whitelist")
)");

    m.def("remove_device_from_whitelist", &removeDeviceFromWhitelist,
          py::arg("device_identifier"),
          R"(Removes a device from the security whitelist.

Args:
    device_identifier: Device identifier (serial number, UUID, etc.)

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import disk
    >>> # Remove a device from the whitelist
    >>> success = disk.remove_device_from_whitelist("SERIAL123456")
    >>> if success:
    ...     print("Device removed from whitelist")
    ... else:
    ...     print("Failed to remove device from whitelist")
)");

    m.def("set_disk_read_only", &setDiskReadOnly, py::arg("path"),
          R"(Sets a disk to read-only mode for security.

Args:
    path: Path to the disk or mount point

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import disk
    >>> # Set a disk to read-only mode
    >>> try:
    ...     success = disk.set_disk_read_only("E:")  # Windows
    ...     if success:
    ...         print("Disk set to read-only mode")
    ...     else:
    ...         print("Failed to set disk to read-only mode")
    ... except Exception as e:
    ...     print(f"Error: {e}")
)");

    m.def("scan_disk_for_threats", &scanDiskForThreats, py::arg("path"),
          py::arg("scan_depth") = 0,
          R"(Scans a disk for malicious files.

Args:
    path: Path to the disk or mount point
    scan_depth: How many directory levels to scan (0 for unlimited)

Returns:
    Tuple of (success, threat_count) where:
        - success is a boolean indicating if the scan completed successfully
        - threat_count is the number of suspicious files found

Examples:
    >>> from atom.sysinfo import disk
    >>> # Scan a disk for threats
    >>> success, threats = disk.scan_disk_for_threats("E:")
    >>> if success:
    ...     if threats > 0:
    ...         print(f"Found {threats} suspicious files")
    ...     else:
    ...         print("No threats detected")
    ... else:
    ...     print("Scan failed")
)");

    m.def(
        "start_device_monitoring",
        [](py::function callback, SecurityPolicy policy) {
            return startDeviceMonitoring(
                [callback](const StorageDevice& device) {
                    py::gil_scoped_acquire acquire;
                    try {
                        callback(device);
                    } catch (py::error_already_set& e) {
                        PyErr_Print();
                    }
                },
                policy);
        },
        py::arg("callback"),
        py::arg("security_policy") = SecurityPolicy::DEFAULT,
        R"(Starts monitoring for device insertion events.

Args:
    callback: Function to call when a device is inserted
              The callback receives a StorageDevice object as its argument
    security_policy: Security policy to apply to new devices (default: DEFAULT)

Returns:
    A future object that can be used to stop monitoring

Examples:
    >>> from atom.sysinfo import disk
    >>> import time
    >>> 
    >>> # Define callback function
    >>> def on_device_inserted(device):
    ...     print(f"New device detected: {device.model}")
    ...     print(f"Path: {device.device_path}")
    ...     print(f"Size: {device.size_bytes / (1024**3):.1f} GB")
    ... 
    >>> # Start monitoring with read-only policy
    >>> future = disk.start_device_monitoring(
    ...     on_device_inserted, 
    ...     disk.SecurityPolicy.READ_ONLY
    ... )
    >>> 
    >>> # Let it run for a while
    >>> try:
    ...     print("Monitoring for devices. Insert a USB drive...")
    ...     time.sleep(30)  # Monitor for 30 seconds
    ... except KeyboardInterrupt:
    ...     print("Monitoring stopped by user")
)");

    m.def("get_device_serial_number", &getDeviceSerialNumber,
          py::arg("device_path"),
          R"(Gets the serial number of a storage device.

Args:
    device_path: Path to the device

Returns:
    Optional string containing the serial number if available

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get device serial number
    >>> serial = disk.get_device_serial_number("/dev/sda")
    >>> if serial:
    ...     print(f"Serial number: {serial}")
    ... else:
    ...     print("Serial number not available")
)");

    m.def("is_device_in_whitelist", &isDeviceInWhitelist,
          py::arg("device_identifier"),
          R"(Checks if a device is in the whitelist.

Args:
    device_identifier: Device identifier to check

Returns:
    Boolean indicating whether the device is in the whitelist

Examples:
    >>> from atom.sysinfo import disk
    >>> # Check if a device is whitelisted
    >>> if disk.is_device_in_whitelist("SERIAL123456"):
    ...     print("Device is in whitelist")
    ... else:
    ...     print("Device is not in whitelist")
)");

    m.def(
        "get_disk_health",
        [](const std::string& devicePath) {
            auto result = getDiskHealth(devicePath);
            if (std::holds_alternative<int>(result)) {
                return py::cast(std::get<int>(result));
            } else {
                return py::cast(std::get<std::string>(result));
            }
        },
        py::arg("device_path"),
        R"(Gets disk health information if available.

Args:
    device_path: Path to the device

Returns:
    Either an integer representing health percentage (0-100) or a string error message

Examples:
    >>> from atom.sysinfo import disk
    >>> # Check disk health
    >>> health = disk.get_disk_health("/dev/sda")
    >>> if isinstance(health, int):
    ...     print(f"Disk health: {health}%")
    ... else:
    ...     print(f"Error: {health}")
)");

    // Context manager for monitoring device events
    py::class_<py::object>(m, "DeviceMonitorContext")
        .def(py::init([](py::function callback, SecurityPolicy policy) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("callback"),
             py::arg("security_policy") = SecurityPolicy::DEFAULT,
             "Create a context manager for device insertion monitoring")
        .def(
            "__enter__",
            [](py::object& self, py::function callback, SecurityPolicy policy) {
                // Store the callback and start device monitoring
                self.attr("callback") = callback;

                auto future = startDeviceMonitoring(
                    [callback](const StorageDevice& device) {
                        py::gil_scoped_acquire acquire;
                        try {
                            callback(device);
                        } catch (py::error_already_set& e) {
                            PyErr_Print();
                        }
                    },
                    policy);

                // Store the future so we can cancel it in __exit__
                self.attr("future") = py::cast(std::move(future));

                return self;
            })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 py::object future = self.attr("future");

                 // Not all futures have a method to request cancellation
                 if (py::hasattr(future, "cancel")) {
                     future.attr("cancel")();
                 }

                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for device monitor context
    m.def(
        "monitor_devices",
        [&m](py::function callback, SecurityPolicy policy) {
            return m.attr("DeviceMonitorContext")(callback, policy);
        },
        py::arg("callback"),
        py::arg("security_policy") = SecurityPolicy::DEFAULT,
        R"(Create a context manager for device insertion monitoring.

This function returns a context manager that monitors for device insertions
and calls the provided callback when a device is inserted.

Args:
    callback: Function to call when a device is inserted
              The callback receives a StorageDevice object as its argument
    security_policy: Security policy to apply to new devices (default: DEFAULT)

Returns:
    A context manager for device monitoring

Examples:
    >>> from atom.sysinfo import disk
    >>> import time
    >>> 
    >>> # Define a callback function
    >>> def on_device_inserted(device):
    ...     print(f"New device: {device.model} ({device.size_bytes / (1024**3):.1f} GB)  
    >>> # Use as a context manager 
    >>> with disk.monitor_devices(on_device_inserted,
                                  disk.SecurityPolicy.READ_ONLY):
    ...     print("Monitoring for devices. Insert a USB drive...")
    ...     try:
    ...         time.sleep(30)  # Monitor for 30 seconds
    ...     except KeyboardInterrupt:
    ...         print("Monitoring stopped by user")
    ... 
    >>> print("Monitoring stopped")
)");

    // Additional utility functions
    m.def(
        "format_size",
        [](uint64_t size_bytes) {
            const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
            int unit_index = 0;
            double size = static_cast<double>(size_bytes);

            while (size >= 1024 && unit_index < 5) {
                size /= 1024;
                unit_index++;
            }

            char buffer[64];
            if (unit_index == 0) {
                snprintf(buffer, sizeof(buffer), "%.0f %s", size,
                         units[unit_index]);
            } else {
                snprintf(buffer, sizeof(buffer), "%.2f %s", size,
                         units[unit_index]);
            }

            return std::string(buffer);
        },
        py::arg("size_bytes"),
        R"(Format a size in bytes to a human-readable string.

Args:
    size_bytes: Size in bytes

Returns:
    String representation with appropriate unit (B, KB, MB, GB, TB, PB)

Examples:
    >>> from atom.sysinfo import disk
    >>> # Format different sizes
    >>> print(disk.format_size(1024))            # "1.00 KB"
    >>> print(disk.format_size(1536))            # "1.50 KB"
    >>> print(disk.format_size(1048576))         # "1.00 MB"
    >>> print(disk.format_size(1073741824))      # "1.00 GB"
    >>> print(disk.format_size(1099511627776))   # "1.00 TB"
)");

    m.def(
        "get_disk_summary",
        []() {
            auto disks = getDiskInfo();

            py::list result;
            for (const auto& disk : disks) {
                py::dict disk_info;
                disk_info["path"] = disk.path;
                disk_info["device_path"] = disk.devicePath;
                disk_info["model"] = disk.model;
                disk_info["fs_type"] = disk.fsType;
                disk_info["total_gb"] =
                    static_cast<double>(disk.totalSpace) / (1024 * 1024 * 1024);
                disk_info["free_gb"] =
                    static_cast<double>(disk.freeSpace) / (1024 * 1024 * 1024);
                disk_info["used_gb"] =
                    static_cast<double>(disk.totalSpace - disk.freeSpace) /
                    (1024 * 1024 * 1024);
                disk_info["usage_percent"] = disk.usagePercent;
                disk_info["is_removable"] = disk.isRemovable;

                result.append(disk_info);
            }

            return result;
        },
        R"(Get a summary of all disks in an easy-to-use format.

Returns:
    List of dictionaries containing disk information with pre-calculated values in GB

Examples:
    >>> from atom.sysinfo import disk
    >>> import pprint
    >>> # Get disk summary
    >>> summary = disk.get_disk_summary()
    >>> for disk_info in summary:
    ...     print(f"{disk_info['path']} ({disk_info['model']})
    ...     print(f"  {disk_info['used_gb']:.1f} GB used of {disk_info['total_gb']:.1f} GB")
    ...     print(f"  {disk_info['usage_percent']:.1f}% full")
)");

    m.def(
        "is_disk_low_space",
        [](const std::string& path, float threshold_percent) {
            auto disks = getDiskInfo();
            for (const auto& disk : disks) {
                if (disk.path == path) {
                    return disk.usagePercent > (100.0f - threshold_percent);
                }
            }
            return false;
        },
        py::arg("path"), py::arg("threshold_percent") = 10.0f,
        R"(Check if a disk is running low on space.

Args:
    path: Path to the disk or mount point
    threshold_percent: Free space threshold percentage (default: 10.0)

Returns:
    Boolean indicating whether free space is below the threshold

Examples:
    >>> from atom.sysinfo import disk
    >>> # Check if C: drive has less than 15% free space
    >>> if disk.is_disk_low_space("C:", 15.0):
    ...     print("Warning: Disk C: is running low on space!")
)");

    m.def(
        "check_disk_space",
        [](const std::string& path, uint64_t required_bytes) {
            auto disks = getDiskInfo();
            for (const auto& disk : disks) {
                if (disk.path == path) {
                    return disk.freeSpace >= required_bytes;
                }
            }
            return false;
        },
        py::arg("path"), py::arg("required_bytes"),
        R"(Check if a disk has enough free space.

Args:
    path: Path to the disk or mount point
    required_bytes: Required free space in bytes

Returns:
    Boolean indicating whether there is enough free space

Examples:
    >>> from atom.sysinfo import disk
    >>> # Check if there's enough space for a 1GB file
    >>> if disk.check_disk_space("C:", 1 * 1024 * 1024 * 1024):
    ...     print("There's enough space for the file")
    ... else:
    ...     print("Not enough disk space")
)");

    m.def(
        "get_largest_disk",
        []() -> py::object {
            auto disks = getDiskInfo();
            if (disks.empty()) {
                return py::none();
            }

            const DiskInfo* largest = &disks[0];
            for (const auto& disk : disks) {
                if (disk.totalSpace > largest->totalSpace) {
                    largest = &disk;
                }
            }

            return py::cast(*largest);
        },
        R"(Get the largest disk available on the system.

Returns:
    DiskInfo object for the largest disk, or None if no disks are available

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get the largest disk
    >>> largest = disk.get_largest_disk()
    >>> if largest:
    ...     print(f"Largest disk: {largest.path} ({largest.total_space / (1024**3):.1f} GB)
)");

    m.def(
        "get_most_free_disk",
        []() -> py::object {
            auto disks = getDiskInfo();
            if (disks.empty()) {
                return py::none();
            }

            const DiskInfo* most_free = &disks[0];
            for (const auto& disk : disks) {
                if (disk.freeSpace > most_free->freeSpace) {
                    most_free = &disk;
                }
            }

            return py::cast(*most_free);
        },
        R"(Get the disk with the most free space.

Returns:
    DiskInfo object for the disk with the most free space, or None if no disks are available

Examples:
    >>> from atom.sysinfo import disk
    >>> # Get the disk with the most free space
    >>> most_free = disk.get_most_free_disk()
    >>> if most_free:
    ...     print(f"Most free space: {most_free.path} ({most_free.free_space / (1024**3):.1f} GB free)
)");
}