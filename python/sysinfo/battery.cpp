#include "atom/sysinfo/battery.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(battery, m) {
    m.doc() = "Battery information and monitoring module for the atom package";

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

    // BatteryInfo structure binding
    py::class_<BatteryInfo>(m, "BatteryInfo",
                            R"(Structure containing battery information.

This class provides detailed information about the system's battery, including charge level,
voltage, current, and other properties.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get basic battery information
    >>> info = battery.get_battery_info()
    >>> print(f"Battery present: {info.is_battery_present}")
    >>> print(f"Charge level: {info.battery_life_percent}%")
    >>> if info.is_charging:
    ...     print("Battery is charging")
)")
        .def(py::init<>(), "Constructs a new BatteryInfo object.")
        .def(py::init<const BatteryInfo&>(), py::arg("other"),
             "Copy constructor.")
        .def_readwrite("is_battery_present", &BatteryInfo::isBatteryPresent,
                       "Whether the battery is present")
        .def_readwrite("is_charging", &BatteryInfo::isCharging,
                       "Whether the battery is charging")
        .def_readwrite("battery_life_percent", &BatteryInfo::batteryLifePercent,
                       "Battery life percentage (0-100)")
        .def_readwrite("battery_life_time", &BatteryInfo::batteryLifeTime,
                       "Remaining battery life time in minutes")
        .def_readwrite("battery_full_life_time",
                       &BatteryInfo::batteryFullLifeTime,
                       "Full battery life time in minutes")
        .def_readwrite("energy_now", &BatteryInfo::energyNow,
                       "Current remaining energy in microjoules")
        .def_readwrite("energy_full", &BatteryInfo::energyFull,
                       "Total battery capacity in microjoules")
        .def_readwrite("energy_design", &BatteryInfo::energyDesign,
                       "Designed battery capacity in microjoules")
        .def_readwrite("voltage_now", &BatteryInfo::voltageNow,
                       "Current voltage in volts")
        .def_readwrite("current_now", &BatteryInfo::currentNow,
                       "Current battery current in amperes")
        .def_readwrite("temperature", &BatteryInfo::temperature,
                       "Battery temperature in Celsius")
        .def_readwrite("cycle_counts", &BatteryInfo::cycleCounts,
                       "Battery charge cycle counts")
        .def_readwrite("manufacturer", &BatteryInfo::manufacturer,
                       "Battery manufacturer")
        .def_readwrite("model", &BatteryInfo::model, "Battery model")
        .def_readwrite("serial_number", &BatteryInfo::serialNumber,
                       "Battery serial number")
        .def("get_battery_health", &BatteryInfo::getBatteryHealth,
             R"(Calculate battery health (0-100%).

Returns:
    Battery health percentage.
)")
        .def("get_estimated_time_remaining",
             &BatteryInfo::getEstimatedTimeRemaining,
             R"(Estimate remaining usage time.

Returns:
    Estimated time remaining in hours.
)")
        .def("__eq__", &BatteryInfo::operator==, py::arg("other"),
             "Compare two BatteryInfo objects for equality.")
        .def("__ne__", &BatteryInfo::operator!=, py::arg("other"),
             "Compare two BatteryInfo objects for inequality.")
        .def("__repr__", [](const BatteryInfo& info) {
            return "<BatteryInfo "
                   "present=" +
                   std::to_string(info.isBatteryPresent) +
                   " charging=" + std::to_string(info.isCharging) +
                   " level=" + std::to_string(info.batteryLifePercent) + "%" +
                   " voltage=" + std::to_string(info.voltageNow) + "V" +
                   " temperature=" + std::to_string(info.temperature) + "°C" +
                   ">";
        });

    // BatteryAlertSettings structure binding
    py::class_<BatteryAlertSettings>(m, "BatteryAlertSettings",
                                     R"(Settings for battery alerts.

This class defines thresholds for various battery-related alerts, such as low battery
level or high temperature.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Create custom alert settings
    >>> settings = battery.BatteryAlertSettings()
    >>> settings.low_battery_threshold = 25.0  # Set low battery alert to 25%
    >>> settings.high_temp_threshold = 40.0    # Set high temperature alert to 40°C
    >>> # Apply to battery manager
    >>> mgr = battery.BatteryManager.get_instance()
    >>> mgr.set_alert_settings(settings)
)")
        .def(py::init<>(), "Constructs a new BatteryAlertSettings object.")
        .def_readwrite("low_battery_threshold",
                       &BatteryAlertSettings::lowBatteryThreshold,
                       "Low battery warning threshold (percentage)")
        .def_readwrite("critical_battery_threshold",
                       &BatteryAlertSettings::criticalBatteryThreshold,
                       "Critical battery warning threshold (percentage)")
        .def_readwrite("high_temp_threshold",
                       &BatteryAlertSettings::highTempThreshold,
                       "High temperature warning threshold (Celsius)")
        .def_readwrite("low_health_threshold",
                       &BatteryAlertSettings::lowHealthThreshold,
                       "Low battery health warning threshold (percentage)");

    // BatteryStats structure binding
    py::class_<BatteryStats>(m, "BatteryStats",
                             R"(Statistics about battery usage.

This class provides statistics about battery usage, including power consumption,
discharge rate, and temperature ranges.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get battery usage statistics
    >>> mgr = battery.BatteryManager.get_instance()
    >>> stats = mgr.get_stats()
    >>> print(f"Average power consumption: {stats.average_power_consumption}W")
    >>> print(f"Average discharge rate: {stats.average_discharge_rate}%/h")
    >>> print(f"Battery health: {stats.battery_health}%")
)")
        .def(py::init<>(), "Constructs a new BatteryStats object.")
        .def_readwrite("average_power_consumption",
                       &BatteryStats::averagePowerConsumption,
                       "Average power consumption in watts")
        .def_readwrite("total_energy_consumed",
                       &BatteryStats::totalEnergyConsumed,
                       "Total energy consumed in watt-hours")
        .def_readwrite("average_discharge_rate",
                       &BatteryStats::averageDischargeRate,
                       "Average discharge rate in percent per hour")
        .def_readwrite("total_uptime", &BatteryStats::totalUptime,
                       "Total battery uptime")
        .def_readwrite("min_battery_level", &BatteryStats::minBatteryLevel,
                       "Minimum recorded battery level in percent")
        .def_readwrite("max_battery_level", &BatteryStats::maxBatteryLevel,
                       "Maximum recorded battery level in percent")
        .def_readwrite("min_temperature", &BatteryStats::minTemperature,
                       "Minimum recorded temperature in Celsius")
        .def_readwrite("max_temperature", &BatteryStats::maxTemperature,
                       "Maximum recorded temperature in Celsius")
        .def_readwrite("min_voltage", &BatteryStats::minVoltage,
                       "Minimum recorded voltage")
        .def_readwrite("max_voltage", &BatteryStats::maxVoltage,
                       "Maximum recorded voltage")
        .def_readwrite("avg_discharge_rate", &BatteryStats::avgDischargeRate,
                       "Average battery discharge rate")
        .def_readwrite("cycle_count", &BatteryStats::cycleCount,
                       "Number of battery charge cycles")
        .def_readwrite("battery_health", &BatteryStats::batteryHealth,
                       "Battery health percentage");

    // PowerPlan enum binding
    py::enum_<PowerPlan>(m, "PowerPlan", "Enumeration of power plan types")
        .value("BALANCED", PowerPlan::BALANCED, "Balanced power plan")
        .value("PERFORMANCE", PowerPlan::PERFORMANCE,
               "Performance-focused power plan")
        .value("POWER_SAVER", PowerPlan::POWER_SAVER, "Power-saving plan")
        .value("CUSTOM", PowerPlan::CUSTOM, "Custom power plan")
        .export_values();

    // Battery information functions
    m.def("get_battery_info", &getBatteryInfo,
          R"(Get basic battery information.

Returns:
    BatteryInfo object containing basic battery information.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get basic battery info
    >>> info = battery.get_battery_info()
    >>> print(f"Battery level: {info.battery_life_percent}%")
    >>> if info.is_charging:
    ...     print("Battery is charging")
    ... else:
    ...     print(f"Estimated time remaining: {info.get_estimated_time_remaining():.1f} hours")
)");

    m.def("get_detailed_battery_info", &getDetailedBatteryInfo,
          R"(Get detailed battery information.

Returns:
    BatteryInfo object containing detailed battery information including manufacturer,
    model, serial number, and more detailed power metrics.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get detailed battery info
    >>> info = battery.get_detailed_battery_info()
    >>> print(f"Battery model: {info.model}")
    >>> print(f"Manufacturer: {info.manufacturer}")
    >>> print(f"Voltage: {info.voltage_now}V")
    >>> print(f"Current: {info.current_now}A")
    >>> print(f"Temperature: {info.temperature}°C")
    >>> print(f"Battery health: {info.get_battery_health()}%")
)");

    // BatteryMonitor class binding
    py::class_<BatteryMonitor>(m, "BatteryMonitor",
                               R"(Monitor battery status changes.

This class provides static methods to start and stop battery monitoring.

Examples:
    >>> from atom.sysinfo import battery
    >>> import time
    >>>
    >>> # Define callback function for battery updates
    >>> def on_battery_update(info):
    ...     print(f"Battery level: {info.battery_life_percent}%")
    ...     print(f"Charging: {info.is_charging}")
    ...
    >>> # Start monitoring with 2 second interval
    >>> battery.BatteryMonitor.start_monitoring(on_battery_update, 2000)
    >>>
    >>> # Let it run for a while
    >>> time.sleep(10)
    >>>
    >>> # Stop monitoring
    >>> battery.BatteryMonitor.stop_monitoring()
)")
        .def_static(
            "start_monitoring",
            [](py::function callback, unsigned int interval_ms) {
                BatteryMonitor::startMonitoring(
                    [callback](const BatteryInfo& info) {
                        py::gil_scoped_acquire acquire;
                        try {
                            callback(info);
                        } catch (py::error_already_set& e) {
                            PyErr_Print();
                        }
                    },
                    interval_ms);
            },
            py::arg("callback"), py::arg("interval_ms") = 1000,
            R"(Start monitoring battery status.

Args:
    callback: Callback function to handle battery status updates.
    interval_ms: Monitoring interval in milliseconds (default: 1000).

Examples:
    >>> from atom.sysinfo import battery
    >>> # Define a callback function
    >>> def on_battery_update(info):
    ...     print(f"Battery update - Level: {info.battery_life_percent}%")
    ...
    >>> # Start monitoring with 1 second intervals
    >>> battery.BatteryMonitor.start_monitoring(on_battery_update)
)")
        .def_static("stop_monitoring", &BatteryMonitor::stopMonitoring,
                    R"(Stop monitoring battery status.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Stop any active battery monitoring
    >>> battery.BatteryMonitor.stop_monitoring()
)");

    // BatteryManager class binding - as a singleton
    py::class_<BatteryManager, std::unique_ptr<BatteryManager, py::nodelete>>(
        m, "BatteryManager",
        R"(Battery management class for monitoring and statistics.

This singleton class provides advanced battery monitoring, statistics collection,
and alert functionality.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get the singleton instance
    >>> manager = battery.BatteryManager.get_instance()
    >>>
    >>> # Set up alert callback
    >>> def on_battery_alert(alert_msg, info):
    ...     print(f"Battery alert: {alert_msg}")
    ...     print(f"Current level: {info.battery_life_percent}%")
    ...
    >>> manager.set_alert_callback(on_battery_alert)
    >>>
    >>> # Start monitoring
    >>> manager.start_monitoring(5000)  # Check every 5 seconds
)")
        .def_static(
            "get_instance",
            []() {
                return std::unique_ptr<BatteryManager, py::nodelete>(
                    &BatteryManager::getInstance());
            },
            R"(Get the singleton instance of BatteryManager.

Returns:
    Reference to the BatteryManager singleton.

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
)",
            py::return_value_policy::reference)
        .def(
            "set_alert_callback",
            [](BatteryManager& self, py::function py_callback) {
                self.setAlertCallback([py_callback](AlertType alert_type,
                                                    const BatteryInfo& info) {
                    py::gil_scoped_acquire acquire;
                    try {
                        std::string alert_msg;
                        switch (alert_type) {
                            case AlertType::LOW_BATTERY:
                                alert_msg = "LOW_BATTERY";
                                break;
                            case AlertType::CRITICAL_BATTERY:
                                alert_msg = "CRITICAL_BATTERY";
                                break;
                            case AlertType::HIGH_TEMPERATURE:
                                alert_msg = "HIGH_TEMPERATURE";
                                break;
                            case AlertType::LOW_BATTERY_HEALTH:
                                alert_msg = "LOW_BATTERY_HEALTH";
                                break;
                            default:
                                alert_msg = "UNKNOWN_ALERT";
                                break;
                        }
                        py_callback(alert_msg, info);
                    } catch (py::error_already_set& e) {
                        PyErr_Print();
                    }
                });
            },
            py::arg("callback"),
            R"(Set a callback function for battery alerts.

Args:
    callback: Function to call when a battery alert is triggered.
              The callback receives two arguments: alert message (str)
              and battery info (BatteryInfo).

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
    >>>
    >>> def alert_handler(alert_msg, info):
    ...     print(f"Alert: {alert_msg}")
    ...     print(f"Battery level: {info.battery_life_percent}%")
    ...
    >>> mgr.set_alert_callback(alert_handler)
)")
        .def("set_alert_settings", &BatteryManager::setAlertSettings,
             py::arg("settings"),
             R"(Configure alert thresholds.

Args:
    settings: BatteryAlertSettings object with desired threshold values.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Create custom alert settings
    >>> settings = battery.BatteryAlertSettings()
    >>> settings.low_battery_threshold = 25.0
    >>> settings.high_temp_threshold = 42.0
    >>>
    >>> # Apply settings
    >>> mgr = battery.BatteryManager.get_instance()
    >>> mgr.set_alert_settings(settings)
)")
        .def("get_stats", &BatteryManager::getStats,
             R"(Get battery usage statistics.

Returns:
    BatteryStats object with usage statistics.

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
    >>> stats = mgr.get_stats()
    >>> print(f"Average power consumption: {stats.average_power_consumption}W")
    >>> print(f"Battery health: {stats.battery_health}%")
)")
        .def("start_recording", &BatteryManager::startRecording,
             py::arg("log_file") = "",
             R"(Start recording battery history data.

Args:
    log_file: Optional path to a log file. If empty, data is only stored in memory.

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
    >>> # Record to a file
    >>> mgr.start_recording("battery_history.log")
    >>> # Or just store in memory
    >>> mgr.start_recording()
)")
        .def("stop_recording", &BatteryManager::stopRecording,
             R"(Stop recording battery history data.

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
    >>> mgr.stop_recording()
)")
        .def("start_monitoring", &BatteryManager::startMonitoring,
             py::arg("interval_ms") = 10000,
             R"(Start monitoring battery status.

Args:
    interval_ms: Monitoring interval in milliseconds (default: 10000).

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
    >>> # Monitor every 5 seconds
    >>> mgr.start_monitoring(5000)
)")
        .def("stop_monitoring", &BatteryManager::stopMonitoring,
             R"(Stop monitoring battery status.

Examples:
    >>> from atom.sysinfo import battery
    >>> mgr = battery.BatteryManager.get_instance()
    >>> mgr.stop_monitoring()
)")
        .def("get_history", &BatteryManager::getHistory,
             py::arg("max_entries") = 0,
             R"(Get battery history data.

Args:
    max_entries: Maximum number of history entries to return (0 = all).

Returns:
    List of tuples (timestamp, BatteryInfo) representing the battery history.

Examples:
    >>> from atom.sysinfo import battery
    >>> import datetime
    >>>
    >>> mgr = battery.BatteryManager.get_instance()
    >>> # Get the last 10 history entries
    >>> history = mgr.get_history(10)
    >>>
    >>> for timestamp, info in history:
    ...     # Convert timestamp to readable format
    ...     time_str = datetime.datetime.fromtimestamp(
    ...         timestamp.timestamp()
    ...     ).strftime("%H:%M:%S")
    ...     print(f"[{time_str}] Level: {info.battery_life_percent}%")
)");

    // PowerPlanManager class binding
    py::class_<PowerPlanManager>(
        m, "PowerPlanManager",
        R"(Utility class for managing system power plans.

This class provides static methods to get and set the current power plan.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get current power plan
    >>> current_plan = battery.PowerPlanManager.get_current_power_plan()
    >>> print(f"Current power plan: {current_plan}")
    >>>
    >>> # Switch to power saver
    >>> success = battery.PowerPlanManager.set_power_plan(battery.PowerPlan.POWER_SAVER)
    >>> if success:
    ...     print("Successfully switched to power saver mode")
)")
        .def_static("set_power_plan", &PowerPlanManager::setPowerPlan,
                    py::arg("plan"),
                    R"(Set the system power plan.

Args:
    plan: The PowerPlan to activate.

Returns:
    Boolean indicating success or failure.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Switch to performance mode
    >>> success = battery.PowerPlanManager.set_power_plan(battery.PowerPlan.PERFORMANCE)
    >>> if success:
    ...     print("Switched to performance mode")
    ... else:
    ...     print("Failed to switch power plan")
)")
        .def_static("get_current_power_plan",
                    &PowerPlanManager::getCurrentPowerPlan,
                    R"(Get the current system power plan.

Returns:
    The current PowerPlan.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get current power plan
    >>> plan = battery.PowerPlanManager.get_current_power_plan()
    >>> if plan == battery.PowerPlan.BALANCED:
    ...     print("System is using balanced power plan")
    ... elif plan == battery.PowerPlan.POWER_SAVER:
    ...     print("System is in power saving mode")
)")
        .def_static("get_available_power_plans",
                    &PowerPlanManager::getAvailablePowerPlans,
                    R"(Get a list of available power plans on the system.

Returns:
    List of power plan names available on the system.

Examples:
    >>> from atom.sysinfo import battery
    >>> # List available power plans
    >>> plans = battery.PowerPlanManager.get_available_power_plans()
    >>> print("Available power plans:")
    >>> for plan in plans:
    ...     print(f"- {plan}")
)");

    // Additional utility functions
    m.def(
        "is_charging",
        []() {
            auto infoOpt = getBatteryInfo();
            return infoOpt.has_value() ? infoOpt->isCharging : false;
        },
        R"(Check if battery is currently charging.

Returns:
    Boolean indicating whether the battery is charging.

Examples:
    >>> from atom.sysinfo import battery
    >>> if battery.is_charging():
    ...     print("Battery is charging")
    ... else:
    ...     print("Battery is discharging")
)");

    m.def(
        "get_battery_level",
        []() {
            auto infoOpt = getBatteryInfo();
            if (infoOpt.has_value()) {
                const auto& info = infoOpt.value();
                if (!info.isBatteryPresent) {
                    return -1.0f;
                }
                return info.batteryLifePercent;
            }
            return -1.0f;  // No battery info available
        },
        R"(Get current battery level.

Returns:
    Battery level as percentage (0-100), or -1 if no battery is present.

Examples:
    >>> from atom.sysinfo import battery
    >>> level = battery.get_battery_level()
    >>> if level >= 0:
    ...     print(f"Battery level: {level}%")
    ... else:
    ...     print("No battery detected")
)");

    m.def(
        "get_estimated_runtime",
        []() {
            auto infoOpt = getBatteryInfo();
            if (infoOpt.has_value()) {
                const auto& info = infoOpt.value();
                if (!info.isBatteryPresent || info.isCharging) {
                    return -1.0f;
                }
                return info.getEstimatedTimeRemaining();
            }
            return -1.0f;  // No battery info or not applicable
        },
        R"(Get estimated remaining battery runtime in hours.

Returns:
    Estimated runtime in hours, or -1 if battery is charging or not present.

Examples:
    >>> from atom.sysinfo import battery
    >>> runtime = battery.get_estimated_runtime()
    >>> if runtime > 0:
    ...     print(f"Estimated runtime: {runtime:.1f} hours")
    ... else:
    ...     print("Cannot estimate runtime (battery charging or not present) ")
)");

    m.def(
        "is_battery_present",
        []() {
            auto infoOpt = getBatteryInfo();
            return infoOpt.has_value() ? infoOpt->isBatteryPresent : false;
        },
        R"(Check if a battery is present in the system.

Returns:
    Boolean indicating whether a battery is present.

Examples:
    >>> from atom.sysinfo import battery
    >>> if battery.is_battery_present():
    ...     print("Battery is present")
    ... else:
    ...     print("No battery detected (desktop system) ")
)");

    m.def(
        "get_battery_health",
        []() {
            BatteryResult result = getDetailedBatteryInfo();
            if (std::holds_alternative<BatteryInfo>(result)) {
                const auto& info = std::get<BatteryInfo>(result);
                if (!info.isBatteryPresent) {
                    return -1.0f;
                }
                return info.getBatteryHealth();
            }
            return -1.0f;  // Error or no battery
        },
        R"(Get battery health percentage.

Returns:
    Battery health as percentage (0-100), or -1 if no battery is present.

Examples:
    >>> from atom.sysinfo import battery
    >>> health = battery.get_battery_health()
    >>> if health >= 0:
    ...     print(f"Battery health: {health}%")
    ...     if health < 70:
    ...         print("Battery may need replacement soon")
    ... else:
    ...     print("No battery detected")
)");

    // Context manager for monitoring battery
    py::class_<py::object>(m, "BatteryMonitorContext")
        .def(py::init([](py::function callback, unsigned int interval_ms) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("callback"), py::arg("interval_ms") = 1000,
             "Create a context manager for battery monitoring")
        .def("__enter__",
             [](py::object& self, py::function callback,
                unsigned int interval_ms) {
                 // Store the callback and install battery monitor
                 self.attr("callback") = callback;

                 BatteryMonitor::startMonitoring(
                     [callback](const BatteryInfo& info) {
                         py::gil_scoped_acquire acquire;
                         try {
                             callback(info);
                         } catch (py::error_already_set& e) {
                             PyErr_Print();
                         }
                     },
                     interval_ms);

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 BatteryMonitor::stopMonitoring();
                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for battery monitor context
    m.def(
        "monitor_battery",
        [&m](py::function callback, unsigned int interval_ms) {
            return m.attr("BatteryMonitorContext")(callback, interval_ms);
        },
        py::arg("callback"), py::arg("interval_ms") = 1000,
        R"(Create a context manager for temporary battery monitoring.

This function returns a context manager that sets up battery monitoring with the
provided callback and cleans up when the context is exited.

Args:
    callback: Function to call with battery updates (receives BatteryInfo)
    interval_ms: Update interval in milliseconds (default: 1000)

Returns:
    A context manager for battery monitoring

Examples:
    >>> from atom.sysinfo import battery
    >>> import time
    >>>
    >>> def process_battery_info(info):
    ...     print(f"Battery level: {info.battery_life_percent}%")
    ...
    >>> # Use as a context manager
    >>> with battery.monitor_battery(process_battery_info, 2000):
    ...     print("Monitoring battery for 10 seconds...")
    ...     time.sleep(10)
    ...
    >>> print("Monitoring stopped")
)");

    // Helper function to check if battery level is low
    m.def(
        "is_battery_low",
        [](float threshold) {
            auto infoOpt = getBatteryInfo();
            if (infoOpt.has_value()) {
                const auto& info = infoOpt.value();
                return info.isBatteryPresent && !info.isCharging &&
                       info.batteryLifePercent < threshold;
            }
            return false;  // If no battery info, assume not low
        },
        py::arg("threshold") = 20.0f,
        R"(Check if battery level is below the specified threshold.

Args:
    threshold: Battery percentage threshold (default: 20.0)

Returns:
    Boolean indicating whether battery is low.

Examples:
    >>> from atom.sysinfo import battery
    >>> # Check if battery is below 15%
    >>> if battery.is_battery_low(15.0):
    ...     print("Warning: Battery level is low!")
)");

    // Function to format battery time remaining in a human-readable format
    m.def(
        "format_time_remaining",
        [](bool include_seconds) {
            auto infoOpt = getBatteryInfo();
            if (infoOpt.has_value()) {
                const auto& info = infoOpt.value();
                if (!info.isBatteryPresent || info.isCharging) {
                    return std::string("N/A");
                }

                float hours = info.getEstimatedTimeRemaining();
                if (hours <= 0) {
                    return std::string("Unknown");
                }

                int whole_hours = static_cast<int>(hours);
                int minutes = static_cast<int>((hours - whole_hours) * 60);

                std::string result_str;
                if (whole_hours > 0) {
                    result_str += std::to_string(whole_hours) + "h ";
                }

                result_str += std::to_string(minutes) + "m";

                if (include_seconds) {
                    int seconds =
                        static_cast<int>((hours - whole_hours) * 3600) % 60;
                    result_str += " " + std::to_string(seconds) + "s";
                }

                return result_str;
            }
            return std::string("N/A");  // If no battery info
        },
        py::arg("include_seconds") = false,
        R"(Format battery time remaining as a human-readable string.

Args:
    include_seconds: Whether to include seconds in the output (default: False)

Returns:
    String representation of time remaining (e.g., "2h 15m")

Examples:
    >>> from atom.sysinfo import battery
    >>> # Get formatted time remaining
    >>> time_str = battery.format_time_remaining()
    >>> print(f"Time remaining: {time_str}")
)");
}
