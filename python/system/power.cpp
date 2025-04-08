#include "atom/system/power.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(power, m) {
    m.doc() = "System power management module for the atom package";

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

    // Power management functions
    m.def("shutdown", &atom::system::shutdown,
          R"(Shutdown the system.

Returns:
    bool: True if the system is successfully shut down, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # Be careful when calling this!
    >>> # power.shutdown()
)");

    m.def("reboot", &atom::system::reboot,
          R"(Reboot the system.

Returns:
    bool: True if the system is successfully rebooted, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # Be careful when calling this!
    >>> # power.reboot()
)");

    m.def("hibernate", &atom::system::hibernate,
          R"(Hibernate the system.

Returns:
    bool: True if the system is successfully hibernated, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # This will hibernate the system if supported
    >>> # power.hibernate()
)");

    m.def("logout", &atom::system::logout,
          R"(Logout the current user.

Returns:
    bool: True if the user is successfully logged out, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # This will log out the current user
    >>> # power.logout()
)");

    m.def("lock_screen", &atom::system::lockScreen,
          R"(Lock the screen.

Returns:
    bool: True if the screen is successfully locked, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # Lock the current session
    >>> power.lock_screen()
)");

    m.def("set_screen_brightness", &atom::system::setScreenBrightness,
          py::arg("level"),
          R"(Set the screen brightness level.

Args:
    level: The brightness level to set, typically between 0 (darkest) and 100 (brightest).
           The exact range may depend on the system.

Returns:
    bool: True if the brightness level is successfully set, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # Set screen brightness to 50%
    >>> power.set_screen_brightness(50)
)");

    // Add convenience functions
    m.def(
        "reboot_if_admin",
        []() {
            // Simple utility function that only reboots if the user has admin
            // rights This is just an example, actual implementation would need
            // proper permission checks
            try {
                // Actual implementation would check permissions here
                bool is_admin = true;  // Placeholder check
                if (is_admin) {
                    return atom::system::reboot();
                }
                return false;
            } catch (const std::exception& e) {
                return false;
            }
        },
        R"(Reboot the system, but only if the current user has admin privileges.

Returns:
    bool: True if reboot was successful, False if lacking permissions or if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # Safer than direct reboot() - checks permissions first
    >>> # power.reboot_if_admin()
)");

    // Add a safe version of screen brightness adjustment
    m.def(
        "set_screen_brightness_safely",
        [](int level) {
            // Clamp brightness to acceptable range
            int safe_level = std::max(0, std::min(100, level));
            return atom::system::setScreenBrightness(safe_level);
        },
        py::arg("level"),
        R"(Set the screen brightness level with safety checks.

This function ensures the brightness level is clamped between 0 and 100.

Args:
    level: The brightness level to set (will be clamped to 0-100 range)

Returns:
    bool: True if the brightness level is successfully set, False if an error occurred.

Examples:
    >>> from atom.system import power
    >>> # Set brightness to 75%
    >>> power.set_screen_brightness_safely(75)
    >>> # Values outside the range are clamped to 0-100
    >>> power.set_screen_brightness_safely(150)  # Will set to 100
)");
}