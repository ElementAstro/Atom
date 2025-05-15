#include "atom/sysinfo/bios.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(bios, m) {
    m.doc() = "BIOS information and management module for the atom package";

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

    // BiosInfoData struct binding
    py::class_<BiosInfoData>(m, "BiosInfoData",
                             R"(Structure containing BIOS information.

This class provides detailed information about the system's BIOS, including
version, manufacturer, release date, and other properties.

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get BIOS info
    >>> bios_info = bios.BiosInfo.get_instance().get_bios_info()
    >>> print(f"BIOS Version: {bios_info.version}")
    >>> print(f"Manufacturer: {bios_info.manufacturer}")
    >>> print(f"Release Date: {bios_info.release_date}")
)")
        .def(py::init<>(), "Constructs a new BiosInfoData object.")
        .def_readwrite("version", &BiosInfoData::version, "BIOS version string")
        .def_readwrite("manufacturer", &BiosInfoData::manufacturer,
                       "BIOS manufacturer name")
        .def_readwrite("release_date", &BiosInfoData::releaseDate,
                       "BIOS release date as string")
        .def_readwrite("serial_number", &BiosInfoData::serialNumber,
                       "BIOS serial number")
        .def_readwrite("characteristics", &BiosInfoData::characteristics,
                       "BIOS characteristics as string")
        .def_readwrite("is_upgradeable", &BiosInfoData::isUpgradeable,
                       "Whether the BIOS can be upgraded")
        .def_readwrite("last_update", &BiosInfoData::lastUpdate,
                       "Timestamp of the last BIOS update")
        .def("is_valid", &BiosInfoData::isValid,
             "Check if the BIOS information is valid")
        .def("to_string", &BiosInfoData::toString,
             "Get a string representation of the BIOS information")
        .def("__repr__", [](const BiosInfoData& info) {
            return "<BiosInfoData version='" + info.version +
                   "' manufacturer='" + info.manufacturer + "' release_date='" +
                   info.releaseDate + "'>";
        });

    // BiosHealthStatus struct binding
    py::class_<BiosHealthStatus>(
        m, "BiosHealthStatus",
        R"(Structure containing BIOS health status information.

This class provides health status information about the system's BIOS, including
whether it's healthy, age in days, and any warnings or errors.

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check BIOS health
    >>> health = bios.BiosInfo.get_instance().check_health()
    >>> print(f"BIOS healthy: {health.is_healthy}")
    >>> if health.warnings:
    ...     print("Warnings:")
    ...     for warning in health.warnings:
    ...         print(f"- {warning}")
)")
        .def(py::init<>(), "Constructs a new BiosHealthStatus object.")
        .def_readwrite("is_healthy", &BiosHealthStatus::isHealthy,
                       "Whether the BIOS is in a healthy state")
        .def_readwrite("bios_age_in_days", &BiosHealthStatus::biosAgeInDays,
                       "Age of the BIOS in days since release")
        .def_readwrite("last_check_time", &BiosHealthStatus::lastCheckTime,
                       "Timestamp of the last health check")
        .def_readwrite("warnings", &BiosHealthStatus::warnings,
                       "List of warning messages")
        .def_readwrite("errors", &BiosHealthStatus::errors,
                       "List of error messages")
        .def("__repr__", [](const BiosHealthStatus& status) {
            return "<BiosHealthStatus is_healthy=" +
                   std::string(status.isHealthy ? "True" : "False") +
                   " age_in_days=" + std::to_string(status.biosAgeInDays) +
                   " warnings=" + std::to_string(status.warnings.size()) +
                   " errors=" + std::to_string(status.errors.size()) + ">";
        });

    // BiosUpdateInfo struct binding
    py::class_<BiosUpdateInfo>(m, "BiosUpdateInfo",
                               R"(Structure containing BIOS update information.

This class provides information about available BIOS updates.

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check for BIOS updates
    >>> update_info = bios.BiosInfo.get_instance().check_for_updates()
    >>> if update_info.update_available:
    ...     print(f"New version available: {update_info.latest_version}")
    ...     print(f"Download URL: {update_info.update_url}")
    ... else:
    ...     print("BIOS is up to date")
)")
        .def(py::init<>(), "Constructs a new BiosUpdateInfo object.")
        .def_readwrite("current_version", &BiosUpdateInfo::currentVersion,
                       "Current BIOS version")
        .def_readwrite("latest_version", &BiosUpdateInfo::latestVersion,
                       "Latest available BIOS version")
        .def_readwrite("update_available", &BiosUpdateInfo::updateAvailable,
                       "Whether an update is available")
        .def_readwrite("update_url", &BiosUpdateInfo::updateUrl,
                       "URL to download the BIOS update")
        .def_readwrite("release_notes", &BiosUpdateInfo::releaseNotes,
                       "Release notes for the latest version")
        .def("__repr__", [](const BiosUpdateInfo& info) {
            return "<BiosUpdateInfo current='" + info.currentVersion +
                   "' latest='" + info.latestVersion + "' update_available=" +
                   (info.updateAvailable ? "True" : "False") + ">";
        });

    // BiosInfo class binding - as a singleton
    py::class_<BiosInfo, std::unique_ptr<BiosInfo, py::nodelete>>(
        m, "BiosInfo",
        R"(Class for retrieving and managing BIOS information.

This singleton class provides methods to retrieve BIOS information, check health status,
look for updates, and perform BIOS-related operations.

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get the singleton instance
    >>> bios_mgr = bios.BiosInfo.get_instance()
    >>> 
    >>> # Get basic BIOS information
    >>> info = bios_mgr.get_bios_info()
    >>> print(f"BIOS version: {info.version}")
    >>> print(f"Manufacturer: {info.manufacturer}")
)")
        .def_static(
            "get_instance",
            []() {
                return std::unique_ptr<BiosInfo, py::nodelete>(
                    &BiosInfo::getInstance());
            },
            R"(Get the singleton instance of BiosInfo.

Returns:
    Reference to the BiosInfo singleton.

Examples:
    >>> from atom.sysinfo import bios
    >>> bios_mgr = bios.BiosInfo.get_instance()
)",
            py::return_value_policy::reference)
        .def("get_bios_info", &BiosInfo::getBiosInfo,
             py::arg("force_update") = false,
             R"(Get BIOS information.

Args:
    force_update: Whether to force a refresh of the BIOS information (default: False)

Returns:
    BiosInfoData object containing BIOS information

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get cached BIOS info
    >>> info = bios.BiosInfo.get_instance().get_bios_info()
    >>> print(f"BIOS version: {info.version}")
    >>> 
    >>> # Force update and get fresh info
    >>> info = bios.BiosInfo.get_instance().get_bios_info(True)
)",
             py::return_value_policy::reference)
        .def("refresh_bios_info", &BiosInfo::refreshBiosInfo,
             R"(Force a refresh of the BIOS information.

Returns:
    Boolean indicating success or failure.

Examples:
    >>> from atom.sysinfo import bios
    >>> # Refresh BIOS information
    >>> if bios.BiosInfo.get_instance().refresh_bios_info():
    ...     print("BIOS information refreshed successfully")
    ... else:
    ...     print("Failed to refresh BIOS information")
)")
        .def("check_health", &BiosInfo::checkHealth,
             R"(Check the health status of the BIOS.

Returns:
    BiosHealthStatus object containing health information

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check BIOS health
    >>> health = bios.BiosInfo.get_instance().check_health()
    >>> if health.is_healthy:
    ...     print("BIOS is healthy")
    ... else:
    ...     print("BIOS has issues:")
    ...     for error in health.errors:
    ...         print(f"- {error}")
)")
        .def("check_for_updates", &BiosInfo::checkForUpdates,
             R"(Check for available BIOS updates.

Returns:
    BiosUpdateInfo object containing update information

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check for BIOS updates
    >>> update_info = bios.BiosInfo.get_instance().check_for_updates()
    >>> if update_info.update_available:
    ...     print(f"New version available: {update_info.latest_version}")
    ...     print(f"Download URL: {update_info.update_url}")
    ... else:
    ...     print("BIOS is up to date")
)")
        .def("get_smbios_data", &BiosInfo::getSMBIOSData,
             R"(Get raw SMBIOS data.

Returns:
    List of strings containing SMBIOS data entries

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get SMBIOS data
    >>> smbios_data = bios.BiosInfo.get_instance().get_smbios_data()
    >>> for entry in smbios_data:
    ...     print(entry)
)")
        .def("set_secure_boot", &BiosInfo::setSecureBoot, py::arg("enable"),
             R"(Enable or disable Secure Boot in BIOS.

Args:
    enable: Whether to enable (True) or disable (False) Secure Boot

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import bios
    >>> # Enable Secure Boot
    >>> try:
    ...     success = bios.BiosInfo.get_instance().set_secure_boot(True)
    ...     if success:
    ...         print("Secure Boot enabled successfully")
    ...     else:
    ...         print("Failed to enable Secure Boot")
    ... except Exception as e:
    ...     print(f"Error: {e}")
)")
        .def("set_uefi_boot", &BiosInfo::setUEFIBoot, py::arg("enable"),
             R"(Enable or disable UEFI Boot in BIOS.

Args:
    enable: Whether to enable (True) or disable (False) UEFI Boot

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import bios
    >>> # Enable UEFI Boot
    >>> try:
    ...     success = bios.BiosInfo.get_instance().set_uefi_boot(True)
    ...     if success:
    ...         print("UEFI Boot enabled successfully")
    ...     else:
    ...         print("Failed to enable UEFI Boot")
    ... except Exception as e:
    ...     print(f"Error: {e}")
)")
        .def("backup_bios_settings", &BiosInfo::backupBiosSettings,
             py::arg("filepath"),
             R"(Backup BIOS settings to a file.

Args:
    filepath: Path where to save the backup file

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import bios
    >>> # Backup BIOS settings
    >>> try:
    ...     success = bios.BiosInfo.get_instance().backup_bios_settings("bios_backup.bin")
    ...     if success:
    ...         print("BIOS settings backed up successfully")
    ...     else:
    ...         print("Failed to backup BIOS settings")
    ... except Exception as e:
    ...     print(f"Error: {e}")
)")
        .def("restore_bios_settings", &BiosInfo::restoreBiosSettings,
             py::arg("filepath"),
             R"(Restore BIOS settings from a backup file.

Args:
    filepath: Path to the backup file

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import bios
    >>> # Restore BIOS settings
    >>> try:
    ...     success = bios.BiosInfo.get_instance().restore_bios_settings("bios_backup.bin")
    ...     if success:
    ...         print("BIOS settings restored successfully")
    ...     else:
    ...         print("Failed to restore BIOS settings")
    ... except Exception as e:
    ...     print(f"Error: {e}")
)")
        .def(
            "is_secure_boot_supported",
            static_cast<bool (BiosInfo::*)()>(&BiosInfo::isSecureBootSupported),
            R"(Check if Secure Boot is supported by the system.

Returns:
    Boolean indicating whether Secure Boot is supported

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check if Secure Boot is supported
    >>> if bios.BiosInfo.get_instance().is_secure_boot_supported():
    ...     print("Secure Boot is supported on this system")
    ... else:
    ...     print("Secure Boot is not supported on this system")
)");

    // Utility functions
    m.def(
        "get_bios_version",
        []() { return BiosInfo::getInstance().getBiosInfo().version; },
        R"(Get the current BIOS version.

Returns:
    String containing the BIOS version

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get BIOS version
    >>> version = bios.get_bios_version()
    >>> print(f"BIOS version: {version}")
)");

    m.def(
        "get_bios_manufacturer",
        []() { return BiosInfo::getInstance().getBiosInfo().manufacturer; },
        R"(Get the BIOS manufacturer.

Returns:
    String containing the BIOS manufacturer name

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get BIOS manufacturer
    >>> manufacturer = bios.get_bios_manufacturer()
    >>> print(f"BIOS manufacturer: {manufacturer}")
)");

    m.def(
        "get_bios_release_date",
        []() { return BiosInfo::getInstance().getBiosInfo().releaseDate; },
        R"(Get the BIOS release date.

Returns:
    String containing the BIOS release date

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get BIOS release date
    >>> release_date = bios.get_bios_release_date()
    >>> print(f"BIOS release date: {release_date}")
)");

    m.def(
        "is_bios_outdated",
        [](int max_age_days) {
            auto health = BiosInfo::getInstance().checkHealth();
            return health.biosAgeInDays > max_age_days;
        },
        py::arg("max_age_days") = 365,
        R"(Check if the BIOS is outdated based on age.

Args:
    max_age_days: Maximum acceptable age in days (default: 365)

Returns:
    Boolean indicating whether the BIOS is outdated

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check if BIOS is more than 2 years old
    >>> if bios.is_bios_outdated(730):
    ...     print("BIOS is more than 2 years old, consider updating")
    ... else:
    ...     print("BIOS is relatively recent")
)");

    m.def(
        "check_bios_update",
        []() {
            auto update_info = BiosInfo::getInstance().checkForUpdates();
            return py::make_tuple(update_info.updateAvailable,
                                  update_info.currentVersion,
                                  update_info.latestVersion);
        },
        R"(Check if a BIOS update is available.

Returns:
    Tuple containing (is_update_available, current_version, latest_version)

Examples:
    >>> from atom.sysinfo import bios
    >>> # Check for BIOS updates
    >>> update_available, current_version, latest_version = bios.check_bios_update()
    >>> if update_available:
    ...     print(f"BIOS update available: {current_version} -> {latest_version}")
    ... else:
    ...     print(f"BIOS is up to date: {current_version}")
)");

    // Context manager for BIOS information access
    py::class_<py::object>(m, "BiosInfoContext")
        .def(py::init([]() {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             "Create a context manager for BIOS information access")
        .def("__enter__",
             [](py::object& self) {
                 // Get BIOS info
                 BiosInfo& bios_info = BiosInfo::getInstance();
                 bios_info.refreshBiosInfo();
                 self.attr("info") = py::cast(bios_info.getBiosInfo());
                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 return py::bool_(false);  // Don't suppress exceptions
             })
        .def_property_readonly(
            "info", [](py::object& self) { return self.attr("info"); },
            "Get the BiosInfoData object");

    // Factory function for BIOS info context
    m.def(
        "get_bios_info_context", [&m]() { return m.attr("BiosInfoContext")(); },
        R"(Create a context manager for BIOS information access.

Returns:
    A context manager that provides access to BIOS information

Examples:
    >>> from atom.sysinfo import bios
    >>> # Use as a context manager to get BIOS info
    >>> with bios.get_bios_info_context() as ctx:
    ...     info = ctx.info
    ...     print(f"BIOS version: {info.version}")
    ...     print(f"Manufacturer: {info.manufacturer}")
)");

    // Helper function for detailed BIOS summary
    m.def(
        "get_bios_summary",
        []() {
            auto& bios_info = BiosInfo::getInstance();
            auto info = bios_info.getBiosInfo();
            auto health = bios_info.checkHealth();
            auto update = bios_info.checkForUpdates();

            py::dict summary;
            summary["version"] = info.version;
            summary["manufacturer"] = info.manufacturer;
            summary["release_date"] = info.releaseDate;
            summary["age_in_days"] = health.biosAgeInDays;
            summary["is_healthy"] = health.isHealthy;
            summary["warnings"] = health.warnings;
            summary["errors"] = health.errors;
            summary["update_available"] = update.updateAvailable;
            summary["latest_version"] = update.latestVersion;
            summary["is_upgradeable"] = info.isUpgradeable;

            return summary;
        },
        R"(Get a comprehensive summary of BIOS information.

Returns:
    Dictionary containing BIOS details, health status, and update information

Examples:
    >>> from atom.sysinfo import bios
    >>> # Get BIOS summary
    >>> summary = bios.get_bios_summary()
    >>> print(f"BIOS version: {summary['version']}")
    >>> print(f"Manufacturer: {summary['manufacturer']}")
    >>> print(f"Age: {summary['age_in_days']} days")
    >>> 
    >>> if summary['update_available']:
    ...     print(f"Update available: {summary['latest_version']}")
    >>> 
    >>> if summary['warnings']:
    ...     print("Warnings:")
    ...     for warning in summary['warnings']:
    ...         print(f"- {warning}")
)");
}