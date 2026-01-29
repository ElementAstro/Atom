"""Disk information bindings module."""

# All disk-related bindings are combined in a single module file (disk.cpp)
# This module provides access to disk/storage device information, monitoring,
# and security functionality.

from .disk import (
    DiskInfo,
    SecurityPolicy,
    StorageDevice,
    add_device_to_whitelist,
    calculate_disk_usage_percentage,
    check_disk_space,
    format_size,
    get_available_drives,
    get_device_serial_number,
    get_disk_health,
    get_disk_info,
    get_disk_summary,
    get_disk_usage,
    get_drive_model,
    get_file_system_type,
    get_largest_disk,
    get_most_free_disk,
    get_storage_device_models,
    get_storage_devices,
    is_device_in_whitelist,
    is_disk_low_space,
    monitor_devices,
    remove_device_from_whitelist,
    scan_disk_for_threats,
    set_disk_read_only,
    start_device_monitoring,
)

__all__ = [
    # Classes/Structures
    "DiskInfo",
    "StorageDevice",
    "SecurityPolicy",
    # Disk Information Functions
    "get_disk_info",
    "get_disk_usage",
    "get_drive_model",
    # Storage Device Functions
    "get_storage_devices",
    "get_storage_device_models",
    "get_available_drives",
    "get_device_serial_number",
    "get_disk_health",
    # Disk Utility Functions
    "calculate_disk_usage_percentage",
    "get_file_system_type",
    # Security Functions
    "add_device_to_whitelist",
    "remove_device_from_whitelist",
    "is_device_in_whitelist",
    "set_disk_read_only",
    "scan_disk_for_threats",
    # Monitoring Functions
    "start_device_monitoring",
    "monitor_devices",
    # Helper Functions
    "format_size",
    "get_disk_summary",
    "is_disk_low_space",
    "check_disk_space",
    "get_largest_disk",
    "get_most_free_disk",
]
