#include "atom/sysinfo/wm.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(wm, m) {
    m.doc() = "Window manager and desktop environment information module for the atom package";

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
    py::class_<SystemInfo>(m, "SystemInfo",
                          R"(Comprehensive system information including window manager and desktop environment details.

This class provides detailed information about the current desktop environment,
window manager, display server, and related system components.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get system information
    >>> sys_info = wm.get_system_info()
    >>> print(f"Desktop Environment: {sys_info.desktop_environment}")
    >>> print(f"Window Manager: {sys_info.window_manager}")
    >>> print(f"Display Server: {sys_info.display_server}")
    >>> print(f"Session Type: {sys_info.session_type}")
    >>> print(f"Theme: {sys_info.theme}")
)")
        .def(py::init<>(), "Constructs a new SystemInfo object.")
        .def_readwrite("desktop_environment", &SystemInfo::desktopEnvironment,
                       "Name of the desktop environment (e.g., 'GNOME', 'KDE', 'XFCE', 'Windows Explorer')")
        .def_readwrite("window_manager", &SystemInfo::windowManager,
                       "Name of the window manager (e.g., 'Mutter', 'KWin', 'DWM')")
        .def_readwrite("display_server", &SystemInfo::displayServer,
                       "Display server protocol (e.g., 'X11', 'Wayland', 'Windows')")
        .def_readwrite("session_type", &SystemInfo::sessionType,
                       "Session type (e.g., 'x11', 'wayland', 'tty')")
        .def_readwrite("theme", &SystemInfo::theme,
                       "Current system theme name")
        .def_readwrite("icon_theme", &SystemInfo::iconTheme,
                       "Current icon theme name")
        .def_readwrite("cursor_theme", &SystemInfo::cursorTheme,
                       "Current cursor theme name")
        .def_readwrite("font_name", &SystemInfo::fontName,
                       "System default font name")
        .def_readwrite("font_size", &SystemInfo::fontSize,
                       "System default font size")
        .def_readwrite("scaling_factor", &SystemInfo::scalingFactor,
                       "Display scaling factor (e.g., 1.0, 1.25, 2.0)")
        .def_readwrite("compositor", &SystemInfo::compositor,
                       "Compositor name if available")
        .def_readwrite("shell_version", &SystemInfo::shellVersion,
                       "Desktop shell version")
        .def_readwrite("gtk_version", &SystemInfo::gtkVersion,
                       "GTK version if available")
        .def_readwrite("qt_version", &SystemInfo::qtVersion,
                       "Qt version if available")
        .def_readwrite("accessibility_enabled", &SystemInfo::accessibilityEnabled,
                       "Whether accessibility features are enabled")
        .def_readwrite("animations_enabled", &SystemInfo::animationsEnabled,
                       "Whether desktop animations are enabled")
        .def_readwrite("transparency_enabled", &SystemInfo::transparencyEnabled,
                       "Whether window transparency is enabled")
        .def_readwrite("virtual_desktops_count", &SystemInfo::virtualDesktopsCount,
                       "Number of virtual desktops/workspaces")
        .def_readwrite("current_desktop", &SystemInfo::currentDesktop,
                       "Current active desktop/workspace number")
        .def_readwrite("screen_saver_active", &SystemInfo::screenSaverActive,
                       "Whether screen saver is currently active")
        .def_readwrite("screen_lock_enabled", &SystemInfo::screenLockEnabled,
                       "Whether automatic screen locking is enabled")
        .def_readwrite("power_management_enabled", &SystemInfo::powerManagementEnabled,
                       "Whether power management features are enabled")
        .def("__eq__", &SystemInfo::operator==, py::arg("other"),
             "Equality comparison operator")
        .def("__repr__", [](const SystemInfo& info) {
            return "<SystemInfo desktop='" + info.desktopEnvironment + "'" +
                   " wm='" + info.windowManager + "'" +
                   " display='" + info.displayServer + "'>";
        });

    // System information function
    m.def("get_system_info", &getSystemInfo,
          R"(Retrieve comprehensive system information including window manager and desktop environment details.

Returns:
    SystemInfo object containing detailed information about the current desktop environment,
    window manager, display server, themes, and various system settings.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get complete system information
    >>> sys_info = wm.get_system_info()
    >>> 
    >>> print("Desktop Environment Information:")
    >>> print(f"  Desktop Environment: {sys_info.desktop_environment}")
    >>> print(f"  Window Manager: {sys_info.window_manager}")
    >>> print(f"  Display Server: {sys_info.display_server}")
    >>> print(f"  Session Type: {sys_info.session_type}")
    >>> 
    >>> print("\\nTheme Information:")
    >>> print(f"  Theme: {sys_info.theme}")
    >>> print(f"  Icon Theme: {sys_info.icon_theme}")
    >>> print(f"  Cursor Theme: {sys_info.cursor_theme}")
    >>> 
    >>> print("\\nFont and Display:")
    >>> print(f"  Font: {sys_info.font_name} {sys_info.font_size}pt")
    >>> print(f"  Scaling Factor: {sys_info.scaling_factor}")
    >>> 
    >>> print("\\nDesktop Features:")
    >>> print(f"  Virtual Desktops: {sys_info.virtual_desktops_count}")
    >>> print(f"  Current Desktop: {sys_info.current_desktop}")
    >>> print(f"  Animations: {sys_info.animations_enabled}")
    >>> print(f"  Transparency: {sys_info.transparency_enabled}")
    >>> print(f"  Accessibility: {sys_info.accessibility_enabled}")
    >>> 
    >>> print("\\nSystem Versions:")
    >>> if sys_info.gtk_version:
    ...     print(f"  GTK Version: {sys_info.gtk_version}")
    >>> if sys_info.qt_version:
    ...     print(f"  Qt Version: {sys_info.qt_version}")
    >>> if sys_info.shell_version:
    ...     print(f"  Shell Version: {sys_info.shell_version}")
)");

    // Convenience functions for specific information
    m.def("get_desktop_environment",
          []() {
              return getSystemInfo().desktopEnvironment;
          },
          R"(Get the name of the current desktop environment.

Returns:
    String containing the desktop environment name.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get just the desktop environment
    >>> de = wm.get_desktop_environment()
    >>> print(f"Desktop Environment: {de}")
)");

    m.def("get_window_manager",
          []() {
              return getSystemInfo().windowManager;
          },
          R"(Get the name of the current window manager.

Returns:
    String containing the window manager name.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get just the window manager
    >>> wm_name = wm.get_window_manager()
    >>> print(f"Window Manager: {wm_name}")
)");

    m.def("get_display_server",
          []() {
              return getSystemInfo().displayServer;
          },
          R"(Get the display server protocol in use.

Returns:
    String containing the display server name (e.g., 'X11', 'Wayland').

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get display server
    >>> display = wm.get_display_server()
    >>> print(f"Display Server: {display}")
    >>> if display == "Wayland":
    ...     print("Using modern Wayland display server")
    >>> elif display == "X11":
    ...     print("Using traditional X11 display server")
)");

    m.def("get_session_type",
          []() {
              return getSystemInfo().sessionType;
          },
          R"(Get the current session type.

Returns:
    String containing the session type.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get session type
    >>> session = wm.get_session_type()
    >>> print(f"Session Type: {session}")
)");

    m.def("get_current_theme",
          []() {
              return getSystemInfo().theme;
          },
          R"(Get the current system theme name.

Returns:
    String containing the theme name.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get current theme
    >>> theme = wm.get_current_theme()
    >>> print(f"Current Theme: {theme}")
)");

    m.def("get_scaling_factor",
          []() {
              return getSystemInfo().scalingFactor;
          },
          R"(Get the display scaling factor.

Returns:
    Float representing the scaling factor (e.g., 1.0, 1.25, 2.0).

Examples:
    >>> from atom.sysinfo import wm
    >>> # Get scaling factor
    >>> scale = wm.get_scaling_factor()
    >>> print(f"Display Scaling: {scale}x")
    >>> if scale > 1.0:
    ...     print("High DPI scaling is enabled")
)");

    m.def("is_wayland_session",
          []() {
              auto info = getSystemInfo();
              return info.displayServer == "Wayland" || info.sessionType == "wayland";
          },
          R"(Check if the current session is using Wayland.

Returns:
    Boolean indicating whether Wayland is in use.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Check if using Wayland
    >>> if wm.is_wayland_session():
    ...     print("Running on Wayland")
    ... else:
    ...     print("Not running on Wayland (likely X11)")
)");

    m.def("has_compositor",
          []() {
              return !getSystemInfo().compositor.empty();
          },
          R"(Check if a compositor is available and active.

Returns:
    Boolean indicating whether a compositor is running.

Examples:
    >>> from atom.sysinfo import wm
    >>> # Check for compositor
    >>> if wm.has_compositor():
    ...     print("Compositor is active")
    ... else:
    ...     print("No compositor detected")
)");
}
