#include "atom/system/storage.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(storage, m) {
    m.doc() = "Storage monitoring module for the atom package";

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

    // StorageMonitor class binding
    py::class_<atom::system::StorageMonitor>(
        m, "StorageMonitor",
        R"(Class for monitoring storage space changes.

This class can monitor the storage space usage of all mounted devices and
trigger registered callback functions when storage space changes.

Examples:
    >>> from atom.system import storage
    >>> # Create a storage monitor
    >>> monitor = storage.StorageMonitor()
    >>> 
    >>> # Define a callback function
    >>> def on_storage_change(path):
    ...     print(f"Storage change detected at: {path}")
    ... 
    >>> # Register the callback
    >>> monitor.register_callback(on_storage_change)
    >>> 
    >>> # Start monitoring
    >>> monitor.start_monitoring()
)")
        .def(py::init<>(),
             "Default constructor. Creates a new StorageMonitor instance.")
        .def("register_callback",
             &atom::system::StorageMonitor::registerCallback,
             py::arg("callback"),
             R"(Registers a callback function.

The callback function will be triggered when storage space changes.

Args:
    callback: The callback function to be triggered when storage space changes.
              The function should accept a single string parameter (the path).

Examples:
    >>> def storage_callback(path):
    ...     print(f"Storage changed at: {path}")
    ...
    >>> monitor.register_callback(storage_callback)
)")
        .def("start_monitoring", &atom::system::StorageMonitor::startMonitoring,
             R"(Starts storage space monitoring.

Returns:
    True if monitoring started successfully, otherwise False.

Examples:
    >>> success = monitor.start_monitoring()
    >>> if success:
    ...     print("Storage monitoring started successfully")
    ... else:
    ...     print("Failed to start storage monitoring")
)")
        .def("stop_monitoring", &atom::system::StorageMonitor::stopMonitoring,
             R"(Stops storage space monitoring.

Examples:
    >>> monitor.stop_monitoring()
    >>> print("Storage monitoring stopped")
)")
        .def("is_running", &atom::system::StorageMonitor::isRunning,
             R"(Checks if monitoring is running.

Returns:
    True if monitoring is running, otherwise False.

Examples:
    >>> if monitor.is_running():
    ...     print("Storage monitoring is active")
    ... else:
    ...     print("Storage monitoring is not active")
)")
        .def("trigger_callbacks",
             &atom::system::StorageMonitor::triggerCallbacks, py::arg("path"),
             R"(Triggers all registered callback functions.

This method manually triggers all registered callbacks with the specified path.

Args:
    path: The storage space path to pass to the callbacks.

Examples:
    >>> monitor.trigger_callbacks("/mnt/usb")
)")
        .def("is_new_media_inserted",
             &atom::system::StorageMonitor::isNewMediaInserted, py::arg("path"),
             R"(Checks if new storage media is inserted at the specified path.

Args:
    path: The storage space path to check.

Returns:
    True if new storage media is inserted, otherwise False.

Examples:
    >>> if monitor.is_new_media_inserted("/mnt/usb"):
    ...     print("New USB drive detected")
)")
        .def("list_all_storage", &atom::system::StorageMonitor::listAllStorage,
             R"(Lists all mounted storage spaces.

This method prints information about all mounted storage spaces to the console.

Examples:
    >>> monitor.list_all_storage()
)")
        .def("list_files", &atom::system::StorageMonitor::listFiles,
             py::arg("path"),
             R"(Lists the files in the specified path.

This method prints information about files in the specified path to the console.

Args:
    path: The storage space path to list files from.

Examples:
    >>> monitor.list_files("/mnt/usb")
)")
        .def("add_storage_path", &atom::system::StorageMonitor::addStoragePath,
             py::arg("path"),
             R"(Dynamically adds a storage path to be monitored.

Args:
    path: The storage path to add.

Examples:
    >>> monitor.add_storage_path("/mnt/external")
    >>> print("Added external drive to monitoring list")
)")
        .def("remove_storage_path",
             &atom::system::StorageMonitor::removeStoragePath, py::arg("path"),
             R"(Dynamically removes a storage path from monitoring.

Args:
    path: The storage path to remove.

Examples:
    >>> monitor.remove_storage_path("/mnt/external")
    >>> print("Removed external drive from monitoring list")
)")
        .def("get_storage_status",
             &atom::system::StorageMonitor::getStorageStatus,
             R"(Gets the current storage status.

Returns:
    A string representation of the storage status.

Examples:
    >>> status = monitor.get_storage_status()
    >>> print(f"Current storage status: {status}")
)");

#ifndef _WIN32
    m.def("monitor_udisk", &atom::system::monitorUdisk, py::arg("monitor"),
          R"(Monitors for USB disk insertion and removal events.

This function starts monitoring for USB disk events and notifies the provided
StorageMonitor instance when events occur.

Args:
    monitor: The StorageMonitor instance to notify of events.

Examples:
    >>> from atom.system import storage
    >>> monitor = storage.StorageMonitor()
    >>> # This would normally be called in a separate thread
    >>> # storage.monitor_udisk(monitor)
)");
#endif

    // Utility functions
    m.def(
        "create_storage_monitor_with_callback",
        [](const std::function<void(const std::string&)>& callback) {
            auto monitor = std::make_unique<atom::system::StorageMonitor>();
            monitor->registerCallback(callback);
            return monitor;
        },
        py::arg("callback"),
        R"(Creates a new StorageMonitor with a callback already registered.

This is a convenience function that creates a monitor and registers a callback in one step.

Args:
    callback: The callback function to register.

Returns:
    A new StorageMonitor instance with the callback registered.

Examples:
    >>> from atom.system import storage
    >>> def notify_storage_change(path):
    ...     print(f"Storage changed: {path}")
    ...
    >>> monitor = storage.create_storage_monitor_with_callback(notify_storage_change)
    >>> monitor.start_monitoring()
)",
        py::return_value_policy::take_ownership);

    m.def(
        "create_and_start_monitor",
        [](const std::function<void(const std::string&)>& callback) {
            auto monitor = std::make_unique<atom::system::StorageMonitor>();
            monitor->registerCallback(callback);
            if (!monitor->startMonitoring()) {
                throw std::runtime_error("Failed to start storage monitoring");
            }
            return monitor;
        },
        py::arg("callback"),
        R"(Creates a new StorageMonitor, registers a callback, and starts monitoring.

This is a convenience function that creates a monitor, registers a callback, and starts
monitoring in one step.

Args:
    callback: The callback function to register.

Returns:
    A new StorageMonitor instance that has already started monitoring.

Raises:
    RuntimeError: If monitoring fails to start.

Examples:
    >>> from atom.system import storage
    >>> try:
    ...     monitor = storage.create_and_start_monitor(
    ...         lambda path: print(f"Storage changed: {path}")
    ...     )
    ...     print("Monitoring started successfully")
    ... except RuntimeError as e:
    ...     print(f"Error: {e}")
)",
        py::return_value_policy::take_ownership);

    // Python-specific additional functionality
    m.def(
        "with_polling_callback",
        [](double interval_seconds) {
            return [interval_seconds](
                       const std::function<void(const std::string&)>&
                           user_callback) {
                return
                    [interval_seconds, user_callback](const std::string& path) {
                        // Call the user's callback
                        user_callback(path);

                        // Sleep for the interval
                        std::this_thread::sleep_for(std::chrono::milliseconds(
                            static_cast<int>(interval_seconds * 1000)));
                    };
            };
        },
        py::arg("interval_seconds") = 1.0,
        R"(Creates a decorator for storage callbacks that adds a polling interval.

This higher-order function creates a callback that calls the user's callback
and then waits for the specified interval before returning.

Args:
    interval_seconds: The interval in seconds to wait between callback executions.

Returns:
    A function that takes a user callback and returns a new callback with polling behavior.

Examples:
    >>> from atom.system import storage
    >>> # Create a polling callback factory with 2-second interval
    >>> polling = storage.with_polling_callback(2.0)
    >>> 
    >>> # Use it to decorate our actual callback
    >>> @polling
    ... def my_callback(path):
    ...     print(f"Storage changed: {path}")
    ... 
    >>> # Register the decorated callback
    >>> monitor = storage.StorageMonitor()
    >>> monitor.register_callback(my_callback)
)");

    // Simple context manager for storage monitoring
    py::class_<py::object>(m, "StorageMonitorContext",
                           "Context manager for storage monitoring")
        .def(py::init(
                 [](const std::function<void(const std::string&)>& callback) {
                     return py::object();  // Placeholder, actual implementation
                                           // in __enter__
                 }),
             py::arg("callback"),
             "Create a context manager for storage monitoring")
        .def("__enter__",
             [](py::object& self,
                const std::function<void(const std::string&)>& callback) {
                 auto monitor =
                     std::make_unique<atom::system::StorageMonitor>();
                 monitor->registerCallback(callback);
                 if (!monitor->startMonitoring()) {
                     throw std::runtime_error(
                         "Failed to start storage monitoring");
                 }
                 self.attr("_monitor") =
                     py::cast(std::move(monitor),
                              py::return_value_policy::take_ownership);
                 return self;
             })
        .def("__exit__", [](py::object& self, py::object exc_type,
                            py::object exc_val, py::object exc_tb) {
            if (py::hasattr(self, "_monitor")) {
                py::cast<atom::system::StorageMonitor*>(self.attr("_monitor"))
                    ->stopMonitoring();
            }
            return false;  // Don't suppress exceptions
        });

    // Module-level helper function to use the context manager
    m.def(
        "monitor_storage",
        [](const std::function<void(const std::string&)>& callback) {
            return m.attr("StorageMonitorContext")(callback);
        },
        py::arg("callback"),
        R"(Creates a context manager for storage monitoring.

This function creates a context manager that starts storage monitoring when entering
the context and stops it when exiting.

Args:
    callback: The callback function to register.

Returns:
    A context manager object.

Examples:
    >>> from atom.system import storage
    >>> def notify_change(path):
    ...     print(f"Storage changed: {path}")
    ... 
    >>> # Use as a context manager
    >>> with storage.monitor_storage(notify_change):
    ...     print("Monitoring storage...")
    ...     # Do something while monitoring
    ...     import time
    ...     time.sleep(10)
    ... # Monitoring automatically stops when exiting the context
)");
}