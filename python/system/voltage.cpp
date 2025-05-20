#include "atom/system/voltage.hpp"

#include <pybind11/functional.h>  // Added for py::cpp_function and advanced lambda wrapping
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace py::literals;

PYBIND11_MODULE(voltage, m) {
    m.doc() = "Voltage and power source monitoring module for the atom package";

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

    // PowerSourceType enum
    py::enum_<atom::system::PowerSourceType>(
        m, "PowerSourceType",
        R"(Enumeration representing the type of power source.)")
        .value("AC", atom::system::PowerSourceType::AC,
               "AC power source (e.g., wall outlet)")
        .value("BATTERY", atom::system::PowerSourceType::Battery,
               "Battery power source")
        .value("USB", atom::system::PowerSourceType::USB, "USB power source")
        .value("UNKNOWN", atom::system::PowerSourceType::Unknown,
               "Unknown power source type")
        .export_values();

    // PowerSourceInfo struct
    py::class_<atom::system::PowerSourceInfo>(
        m, "PowerSourceInfo",
        R"(Structure containing information about a power source.

This class provides information about a specific power source, such as its type,
voltage, current, and charge status.

Examples:
    >>> from atom.system import voltage
    >>> # Get voltage monitor
    >>> monitor = voltage.get_voltage_monitor()
    >>> # Get information about all power sources
    >>> sources = monitor.get_all_power_sources()
    >>> for source in sources:
    ...     print(f"Power source: {source.name}")
    ...     print(f"Type: {source.type}")
    ...     if source.voltage:
    ...         print(f"Voltage: {source.voltage} V")
    ...     if source.charge_percent:
    ...         print(f"Charge: {source.charge_percent}%")
)")
        .def(py::init<>(), "Constructs a new PowerSourceInfo object.")
        .def_readwrite(
            "name", &atom::system::PowerSourceInfo::name,
            "The name of the power source (e.g., 'Battery 1', 'AC Adapter')")
        .def_readwrite("type", &atom::system::PowerSourceInfo::type,
                       "The type of power source")
        .def_readwrite(
            "voltage", &atom::system::PowerSourceInfo::voltage,
            "The voltage of the power source in volts (V), if available")
        .def_readwrite(
            "current", &atom::system::PowerSourceInfo::current,
            "The current of the power source in amperes (A), if available")
        .def_readwrite("charge_percent",
                       &atom::system::PowerSourceInfo::chargePercent,
                       "The charge percentage of the power source (e.g., for "
                       "batteries), if available")
        .def_readwrite("is_charging",
                       &atom::system::PowerSourceInfo::isCharging,
                       "A boolean indicating whether the power source is "
                       "currently charging, if available")
        .def("to_string", &atom::system::PowerSourceInfo::toString,
             "Returns a string representation of the PowerSourceInfo")
        .def("__repr__", &atom::system::PowerSourceInfo::toString);

    // VoltageMonitor abstract base class
    py::class_<atom::system::VoltageMonitor> voltage_monitor(
        m, "VoltageMonitor",
        R"(Abstract base class for voltage monitors.

This class provides an interface for retrieving voltage and power source information
from the underlying system.

Examples:
    >>> from atom.system import voltage
    >>> # Get a platform-specific voltage monitor instance
    >>> monitor = voltage.get_voltage_monitor()
    >>> # Get input voltage
    >>> input_v = monitor.get_input_voltage()
    >>> if input_v:
    ...     print(f"Input voltage: {input_v} V")
    >>> # Get battery voltage
    >>> battery_v = monitor.get_battery_voltage()
    >>> if battery_v:
    ...     print(f"Battery voltage: {battery_v} V")
)");

    // Define VoltageMonitor methods
    voltage_monitor
        .def("get_input_voltage",
             &atom::system::VoltageMonitor::getInputVoltage,
             R"(Gets the input voltage in volts (V).

Returns:
    An optional float representing the input voltage, or None if not available.
)")
        .def("get_battery_voltage",
             &atom::system::VoltageMonitor::getBatteryVoltage,
             R"(Gets the battery voltage in volts (V).

Returns:
    An optional float representing the battery voltage, or None if not available.
)")
        .def("get_all_power_sources",
             &atom::system::VoltageMonitor::getAllPowerSources,
             R"(Gets information about all available power sources.

Returns:
    A list of PowerSourceInfo objects, each representing a power source.
)")
        .def("get_platform_name",
             &atom::system::VoltageMonitor::getPlatformName,
             R"(Gets the name of the platform the monitor is running on.

Returns:
    A string representing the platform name (e.g., 'Windows', 'MacOS', 'Linux').
)");

    // Factory function to create a platform-specific voltage monitor
    m.def(
        "get_voltage_monitor",
        []() { return atom::system::VoltageMonitor::create(); },
        R"(Creates a platform-specific VoltageMonitor implementation.

Returns:
    A VoltageMonitor instance appropriate for the current platform.

Examples:
    >>> from atom.system import voltage
    >>> # Get a voltage monitor for the current platform
    >>> monitor = voltage.get_voltage_monitor()
    >>> print(f"Running on platform: {monitor.get_platform_name()}")
)",
        py::return_value_policy::take_ownership);

    // Utility function to convert PowerSourceType to string
    m.def("power_source_type_to_string", &atom::system::powerSourceTypeToString,
          py::arg("type"),
          R"(Converts a PowerSourceType enum value to a string representation.

Args:
    type: The PowerSourceType enum value to convert.

Returns:
    A string representing the power source type.

Examples:
    >>> from atom.system import voltage
    >>> # Convert PowerSourceType.AC to string
    >>> type_str = voltage.power_source_type_to_string(voltage.PowerSourceType.AC)
    >>> print(type_str)  # Outputs: "AC"
)");

    // Helper function to get the current power source
    m.def(
        "get_current_power_source",
        []() {
            auto monitor = atom::system::VoltageMonitor::create();
            auto sources = monitor->getAllPowerSources();

            // Try to find the currently active power source
            for (const auto& source : sources) {
                // Battery that is discharging (not charging and has charge) is
                // likely the current source
                if (source.type == atom::system::PowerSourceType::Battery &&
                    source.isCharging.has_value() &&
                    !source.isCharging.value() &&
                    source.chargePercent.has_value() &&
                    source.chargePercent.value() > 0) {
                    return source;
                }

                // AC that is providing power is likely the current source
                if (source.type == atom::system::PowerSourceType::AC &&
                    source.voltage.has_value() && source.voltage.value() > 0) {
                    return source;
                }
            }

            // If we couldn't determine the active source, return the first one
            // if available
            if (!sources.empty()) {
                return sources[0];
            }

            // Otherwise, return an empty info object
            return atom::system::PowerSourceInfo();
        },
        R"(Gets information about the current (active) power source.

Returns:
    A PowerSourceInfo object representing the current power source.

Examples:
    >>> from atom.system import voltage
    >>> # Get the current power source
    >>> source = voltage.get_current_power_source()
    >>> print(f"Current power source: {source.name}")
    >>> if source.type == voltage.PowerSourceType.BATTERY:
    ...     print(f"Battery level: {source.charge_percent}%")
    ... elif source.type == voltage.PowerSourceType.AC:
    ...     print("Running on AC power")
)");

    // Helper function to check if currently running on battery
    m.def(
        "is_on_battery",
        []() {
            auto monitor = atom::system::VoltageMonitor::create();
            auto sources = monitor->getAllPowerSources();

            for (const auto& source : sources) {
                if (source.type == atom::system::PowerSourceType::Battery &&
                    source.isCharging.has_value() &&
                    !source.isCharging.value() &&
                    source.chargePercent.has_value() &&
                    source.chargePercent.value() > 0) {
                    return true;
                }
            }

            return false;
        },
        R"(Checks if the system is currently running on battery power.

Returns:
    True if the system is running on battery power, False otherwise.

Examples:
    >>> from atom.system import voltage
    >>> # Check if running on battery
    >>> if voltage.is_on_battery():
    ...     print("System is running on battery power")
    ... else:
    ...     print("System is running on AC power")
)");

    // Helper function to get battery level
    m.def(
        "get_battery_level",
        []() -> std::optional<int> {
            auto monitor = atom::system::VoltageMonitor::create();
            auto sources = monitor->getAllPowerSources();

            for (const auto& source : sources) {
                if (source.type == atom::system::PowerSourceType::Battery &&
                    source.chargePercent.has_value()) {
                    return source.chargePercent;
                }
            }

            return std::nullopt;
        },
        R"(Gets the current battery charge level in percent.

Returns:
    An optional integer representing the battery level (0-100), or None if not available.

Examples:
    >>> from atom.system import voltage
    >>> # Get battery level
    >>> level = voltage.get_battery_level()
    >>> if level is not None:
    ...     print(f"Battery level: {level}%")
    ... else:
    ...     print("Battery level information not available")
)");

    // Helper function to check battery status
    m.def(
        "get_battery_status",
        []() {
            auto monitor = atom::system::VoltageMonitor::create();
            auto sources = monitor->getAllPowerSources();

            py::dict result;
            result["found"] = false;

            for (const auto& source : sources) {
                if (source.type == atom::system::PowerSourceType::Battery) {
                    result["found"] = true;
                    result["name"] = source.name;

                    if (source.chargePercent.has_value()) {
                        result["level"] = source.chargePercent.value();
                    }

                    if (source.isCharging.has_value()) {
                        result["is_charging"] = source.isCharging.value();
                    }

                    if (source.voltage.has_value()) {
                        result["voltage"] = source.voltage.value();
                    }

                    if (source.current.has_value()) {
                        result["current"] = source.current.value();
                    }

                    break;
                }
            }

            return result;
        },
        R"(Gets comprehensive information about the system's battery.

Returns:
    A dictionary containing battery status information.

Examples:
    >>> from atom.system import voltage
    >>> # Get battery status
    >>> status = voltage.get_battery_status()
    >>> if status["found"]:
    ...     print(f"Battery: {status['name']}")
    ...     if "level" in status:
    ...         print(f"Level: {status['level']}%")
    ...     if "is_charging" in status:
    ...         state = "Charging" if status["is_charging"] else "Discharging"
    ...         print(f"State: {state}")
    ... else:
    ...     print("No battery found")
)");

    // Python-friendly method for getting voltage monitor by platform name
    m.def(
        "get_voltage_monitor_for_platform",
        [](const std::string& platform_name) {
            auto monitor = atom::system::VoltageMonitor::create();

            // Check if requested platform matches current platform
            if (platform_name == monitor->getPlatformName() ||
                platform_name.empty() || platform_name == "auto") {
                return monitor;
            }

            // If platform doesn't match, throw an error
            throw std::runtime_error("Requested platform '" + platform_name +
                                     "' doesn't match current platform '" +
                                     monitor->getPlatformName() + "'");
        },
        py::arg("platform_name") = "auto",
        R"(Creates a voltage monitor for a specific platform.

Args:
    platform_name: The name of the platform to create a monitor for.
                   Use "auto" or empty string for automatic detection.

Returns:
    A VoltageMonitor instance for the specified platform.

Raises:
    RuntimeError: If the requested platform doesn't match the current platform.

Examples:
    >>> from atom.system import voltage
    >>> # Get a voltage monitor with automatic platform detection
    >>> monitor = voltage.get_voltage_monitor_for_platform()
    >>> # Explicitly request Windows platform (only works on Windows)
    >>> try:
    ...     win_monitor = voltage.get_voltage_monitor_for_platform("Windows")
    ...     print("Got Windows voltage monitor")
    ... except RuntimeError as e:
    ...     print(f"Error: {e}")
)",
        py::return_value_policy::take_ownership);

    // Convenience class for monitoring voltage changes
    py::class_<py::object>(m, "VoltageMonitorContext")
        .def(py::init([](double check_interval, py::function callback) {
                 return py::object();  // Actual implementation in __enter__
             }),
             py::arg("check_interval") = 1.0, py::arg("callback"),
             "Create a context manager for monitoring voltage changes")
        .def(
            "__enter__",
            [](py::object& self, double check_interval, py::function callback) {
                // Import needed Python modules
                py::module_ threading = py::module_::import("threading");
                py::module_ time = py::module_::import("time");

                // Create a voltage monitor
                auto monitor = atom::system::VoltageMonitor::create();
                self.attr("monitor") =
                    py::cast(std::move(monitor),
                             py::return_value_policy::take_ownership);

                // Get initial readings
                auto init_monitor = py::cast<atom::system::VoltageMonitor*>(
                    self.attr("monitor"));

                self.attr("running") = py::bool_(true);
                self.attr("check_interval") = py::float_(check_interval);
                self.attr("callback") = callback;

                // Start a monitoring thread
                auto monitor_func =
                    [](py::object self_thread_arg) {     // Renamed 'self' to
                                                         // 'self_thread_arg' to
                                                         // avoid confusion
                        py::gil_scoped_release release;  // Release GIL

                        auto monitor_ptr = py::cast<
                            atom::system::VoltageMonitor*>(  // Renamed
                                                             // 'monitor' to
                                                             // 'monitor_ptr'
                            self_thread_arg.attr("monitor"));
                        double interval = py::cast<double>(
                            self_thread_arg.attr("check_interval"));

                        // Store the last readings
                        std::optional<double> last_input_voltage =
                            monitor_ptr->getInputVoltage();
                        std::optional<double> last_battery_voltage =
                            monitor_ptr->getBatteryVoltage();
                        auto last_sources = monitor_ptr->getAllPowerSources();

                        while (
                            py::cast<bool>(self_thread_arg.attr("running"))) {
                            // Sleep for the specified interval
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(
                                    static_cast<int>(interval * 1000)));

                            if (!py::cast<bool>(
                                    self_thread_arg.attr("running")))
                                break;

                            // Get new readings
                            std::optional<double> input_voltage =
                                monitor_ptr->getInputVoltage();
                            std::optional<double> battery_voltage =
                                monitor_ptr->getBatteryVoltage();
                            auto sources = monitor_ptr->getAllPowerSources();

                            // Check for changes
                            bool changed = false;

                            if (input_voltage != last_input_voltage) {
                                changed = true;
                            }

                            if (battery_voltage != last_battery_voltage) {
                                changed = true;
                            }

                            if (sources.size() != last_sources.size()) {
                                changed = true;
                            } else {
                                for (size_t i = 0; i < sources.size(); ++i) {
                                    if (sources[i].voltage !=
                                            last_sources[i].voltage ||
                                        sources[i].current !=
                                            last_sources[i].current ||
                                        sources[i].chargePercent !=
                                            last_sources[i].chargePercent ||
                                        sources[i].isCharging !=
                                            last_sources[i].isCharging) {
                                        changed = true;
                                        break;
                                    }
                                }
                            }

                            // If changes detected, call the callback
                            if (changed) {
                                py::gil_scoped_acquire
                                    acquire;  // Reacquire GIL for Python calls
                                try {
                                    self_thread_arg.attr("callback")(
                                        input_voltage,  // Use self_thread_arg
                                        battery_voltage, sources);
                                } catch (py::error_already_set& e) {
                                    // Log but don't propagate the error
                                    PyErr_Print();
                                }

                                // Update last readings
                                last_input_voltage = input_voltage;
                                last_battery_voltage = battery_voltage;
                                last_sources = sources;
                            }
                        }
                    };

                // Create and start the thread
                self.attr("thread") = threading.attr("Thread")(
                    "target"_a = py::cpp_function(monitor_func),
                    "args"_a = py::make_tuple(self));
                self.attr("thread").attr("daemon") = py::bool_(true);
                self.attr("thread").attr("start")();

                return self;
            })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 self.attr("running") = py::bool_(false);

                 // Wait for the thread to finish (with timeout)
                 py::module_ time = py::module_::import("time");
                 py::object thread = self.attr("thread");

                 if (py::hasattr(thread, "is_alive") &&
                     py::cast<bool>(thread.attr("is_alive")())) {
                     double timeout = 1.0;  // 1 second timeout
                     thread.attr("join")(timeout);
                 }

                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for voltage monitor context
    m.def(
        "monitor_voltage_changes",
        [m](double check_interval, py::function callback) {  // Capture m
            return m.attr("VoltageMonitorContext")(check_interval, callback);
        },
        py::arg("check_interval") = 1.0, py::arg("callback"),
        R"(Creates a context manager for monitoring voltage changes.

This function returns a context manager that periodically checks for changes in voltage
and power source information and calls the provided callback when changes are detected.

Args:
    check_interval: How often to check for changes, in seconds (default: 1.0).
    callback: Function to call when changes are detected. The callback receives three
              arguments: input_voltage (optional float), battery_voltage (optional float),
              and sources (list of PowerSourceInfo).

Returns:
    A context manager for voltage monitoring.

Examples:
    >>> from atom.system import voltage
    >>> import time
    >>> 
    >>> # Define a callback function
    >>> def on_voltage_change(input_v, battery_v, sources):
    ...     print(f"Voltage change detected!")
    ...     if input_v:
    ...         print(f"Input voltage: {input_v} V")
    ...     if battery_v:
    ...         print(f"Battery voltage: {battery_v} V")
    ...     for source in sources:
    ...         print(f"Source: {source.name}, Type: {source.type}")
    ... 
    >>> # Use as a context manager
    >>> with voltage.monitor_voltage_changes(0.5, on_voltage_change):
    ...     print("Monitoring voltage changes for 10 seconds...")
    ...     time.sleep(10)
    ... 
    >>> print("Monitoring stopped")
)");
}