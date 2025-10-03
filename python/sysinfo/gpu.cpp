#include "atom/sysinfo/gpu.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(gpu, m) {
    m.doc() = "GPU and monitor information module for the atom package";

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

    // MonitorInfo structure binding
    py::class_<MonitorInfo>(m, "MonitorInfo",
                           R"(Information about a connected monitor/display.

This class provides detailed information about a connected monitor including
model name, identifier, resolution, and refresh rate.

Examples:
    >>> from atom.sysinfo import gpu
    >>> # Get information about all connected monitors
    >>> monitors = gpu.get_all_monitors_info()
    >>> for monitor in monitors:
    ...     print(f"Monitor: {monitor.model}")
    ...     print(f"Resolution: {monitor.width}x{monitor.height}")
    ...     print(f"Refresh rate: {monitor.refresh_rate}Hz")
    ...     print(f"Identifier: {monitor.identifier}")
)")
        .def(py::init<>(), "Constructs a new MonitorInfo object.")
        .def_readwrite("model", &MonitorInfo::model, 
                       "Monitor model name")
        .def_readwrite("identifier", &MonitorInfo::identifier,
                       "Monitor identifier string")
        .def_readwrite("width", &MonitorInfo::width,
                       "Screen width in pixels")
        .def_readwrite("height", &MonitorInfo::height,
                       "Screen height in pixels")
        .def_readwrite("refresh_rate", &MonitorInfo::refreshRate,
                       "Refresh rate in Hz")
        .def("__repr__", [](const MonitorInfo& info) {
            return "<MonitorInfo model='" + info.model + "'" +
                   " resolution=" + std::to_string(info.width) + "x" + 
                   std::to_string(info.height) +
                   " refresh_rate=" + std::to_string(info.refreshRate) + "Hz>";
        });

    // GPU information functions
    m.def("get_gpu_info", &getGPUInfo,
          R"(Get GPU information from the system.

Returns:
    String containing formatted GPU information including graphics card details,
    driver information, and other GPU-related system information.

Examples:
    >>> from atom.sysinfo import gpu
    >>> # Get GPU information
    >>> gpu_info = gpu.get_gpu_info()
    >>> print("GPU Information:")
    >>> print(gpu_info)
)");

    m.def("get_all_monitors_info", &getAllMonitorsInfo,
          R"(Get information for all connected monitors.

Returns:
    List of MonitorInfo objects containing details about each connected monitor
    including model, resolution, refresh rate, and identifier.

Examples:
    >>> from atom.sysinfo import gpu
    >>> # Get information about all monitors
    >>> monitors = gpu.get_all_monitors_info()
    >>> print(f"Found {len(monitors)} monitor(s):")
    >>> 
    >>> for i, monitor in enumerate(monitors):
    ...     print(f"Monitor {i+1}:")
    ...     print(f"  Model: {monitor.model}")
    ...     print(f"  Resolution: {monitor.width}x{monitor.height}")
    ...     print(f"  Refresh Rate: {monitor.refresh_rate}Hz")
    ...     print(f"  Identifier: {monitor.identifier}")
)");


}
