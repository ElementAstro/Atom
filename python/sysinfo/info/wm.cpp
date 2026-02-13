#include "atom/sysinfo/wm.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(wm, m) {
    m.doc() =
        "Window manager and desktop environment information module for the "
        "atom package";

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

    // SystemInfo structure binding
    py::class_<SystemInfo>(
        m, "SystemInfo",
        R"(System desktop environment and window manager information.

This class provides information about the current desktop environment,
window manager, theme settings, and related system components.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get system information
    >>> sys_info = wm.get_system_info()
    >>> print(f"Desktop Environment: {sys_info.desktop_environment}")
    >>> print(f"Window Manager: {sys_info.window_manager}")
    >>> print(f"WM Theme: {sys_info.wm_theme}")
    >>> print(f"Icons: {sys_info.icons}")
    >>> print(f"Font: {sys_info.font}")
    >>> print(f"Cursor: {sys_info.cursor}")
)")
        .def(py::init<>(), "Constructs a new SystemInfo object.")
        .def_readwrite("desktop_environment", &SystemInfo::desktopEnvironment,
                       "Desktop environment (e.g., 'Fluent', 'GNOME', 'KDE')")
        .def_readwrite(
            "window_manager", &SystemInfo::windowManager,
            "Window manager (e.g., 'Desktop Window Manager', 'i3', 'bspwm')")
        .def_readwrite("wm_theme", &SystemInfo::wmTheme,
                       "Window manager theme information")
        .def_readwrite("icons", &SystemInfo::icons,
                       "Icon theme or icon information")
        .def_readwrite("font", &SystemInfo::font, "System font information")
        .def_readwrite("cursor", &SystemInfo::cursor,
                       "Cursor theme information")
        .def("__repr__", [](const SystemInfo& info) {
            return "<SystemInfo desktop='" + info.desktopEnvironment + "'" +
                   " wm='" + info.windowManager + "'" + " theme='" +
                   info.wmTheme + "'>";
        });

    // System information function
    m.def("get_system_info", &getSystemInfo,
          R"(Retrieve system desktop environment and window manager information.

Returns:
    SystemInfo object containing desktop environment details.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get complete system information
    >>> sys_info = wm.get_system_info()
    >>>
    >>> print("Desktop Environment Information:")
    >>> print(f"  Desktop Environment: {sys_info.desktop_environment}")
    >>> print(f"  Window Manager: {sys_info.window_manager}")
    >>> print(f"  WM Theme: {sys_info.wm_theme}")
    >>> print(f"  Icons: {sys_info.icons}")
    >>> print(f"  Font: {sys_info.font}")
    >>> print(f"  Cursor: {sys_info.cursor}")
)");

    // Convenience functions for specific information
    m.def(
        "get_desktop_environment",
        []() { return getSystemInfo().desktopEnvironment; },
        R"(Get the name of the current desktop environment.

Returns:
    String containing the desktop environment name.

Examples:
    >>> from atom.sysinfo import wm
    >>> de = wm.get_desktop_environment()
    >>> print(f"Desktop Environment: {de}")
)");

    m.def(
        "get_window_manager", []() { return getSystemInfo().windowManager; },
        R"(Get the name of the current window manager.

Returns:
    String containing the window manager name.

Examples:
    >>> from atom.sysinfo import wm
    >>> wm_name = wm.get_window_manager()
    >>> print(f"Window Manager: {wm_name}")
)");

    m.def(
        "get_wm_theme", []() { return getSystemInfo().wmTheme; },
        R"(Get the current window manager theme.

Returns:
    String containing the window manager theme name.

Examples:
    >>> from atom.sysinfo import wm
    >>> theme = wm.get_wm_theme()
    >>> print(f"WM Theme: {theme}")
)");

    m.def(
        "get_icon_theme", []() { return getSystemInfo().icons; },
        R"(Get the current icon theme.

Returns:
    String containing the icon theme name.

Examples:
    >>> from atom.sysinfo import wm
    >>> icons = wm.get_icon_theme()
    >>> print(f"Icon Theme: {icons}")
)");

    m.def(
        "get_system_font", []() { return getSystemInfo().font; },
        R"(Get the system font information.

Returns:
    String containing the system font information.

Examples:
    >>> from atom.sysinfo import wm
    >>> font = wm.get_system_font()
    >>> print(f"System Font: {font}")
)");

    m.def(
        "get_cursor_theme", []() { return getSystemInfo().cursor; },
        R"(Get the current cursor theme.

Returns:
    String containing the cursor theme name.

Examples:
    >>> from atom.sysinfo import wm
    >>> cursor = wm.get_cursor_theme()
    >>> print(f"Cursor Theme: {cursor}")
)");

    // Helper function to get a summary dict
    m.def(
        "get_wm_summary",
        []() {
            auto info = getSystemInfo();
            py::dict summary;
            summary["desktop_environment"] = info.desktopEnvironment;
            summary["window_manager"] = info.windowManager;
            summary["wm_theme"] = info.wmTheme;
            summary["icons"] = info.icons;
            summary["font"] = info.font;
            summary["cursor"] = info.cursor;
            return summary;
        },
        R"(Get a summary dictionary of window manager information.

Returns:
    Dictionary containing all window manager and desktop environment details.

Examples:
    >>> from atom.sysinfo import wm
    >>> summary = wm.get_wm_summary()
    >>> for key, value in summary.items():
    ...     print(f"{key}: {value}")
)");
}
