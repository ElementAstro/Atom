#include "atom/system/shortcut/detector.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(shortcut, m) {
    m.doc() = R"pbdoc(
        Keyboard Shortcut Detection Module
        ----------------------------------

        This module provides functionality for detecting whether keyboard shortcuts
        are captured by the system or other applications, and for monitoring
        keyboard hook installations.

        Examples:
            >>> from atom.system import shortcut
            >>>
            >>> # Create a shortcut detector
            >>> detector = shortcut.ShortcutDetector()
            >>>
            >>> # Create a shortcut (Ctrl+Alt+F1)
            >>> my_shortcut = shortcut.Shortcut(0x70, True, True, False, False)  # F1 key
            >>> print(f"Shortcut: {my_shortcut.to_string()}")
            >>>
            >>> # Check if shortcut is captured
            >>> result = detector.is_shortcut_captured(my_shortcut)
            >>> if result.status == shortcut.ShortcutStatus.Available:
            ...     print("Shortcut is available for use")
            >>> elif result.status == shortcut.ShortcutStatus.CapturedByApp:
            ...     print(f"Shortcut captured by: {result.capturing_application}")
            >>>
            >>> # Check for keyboard hooks
            >>> if detector.has_keyboard_hook_installed():
            ...     processes = detector.get_processes_with_keyboard_hooks()
            ...     print(f"Processes with keyboard hooks: {processes}")
    )pbdoc";

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

    // ShortcutStatus enum
    py::enum_<shortcut_detector::ShortcutStatus>(
        m, "ShortcutStatus",
        R"(Possible states for a keyboard shortcut.

This enum represents the different states a keyboard shortcut can be in,
indicating whether it's available for use or captured by something else.

Values:
    Available: Shortcut is available for registration.
    CapturedByApp: Shortcut is captured by an application.
    CapturedBySystem: Shortcut is captured by system component.
    Reserved: Shortcut is reserved by the operating system.

Examples:
    >>> if result.status == shortcut.ShortcutStatus.Available:
    ...     print("Can register this shortcut")
    >>> elif result.status == shortcut.ShortcutStatus.CapturedByApp:
    ...     print("Another app is using this shortcut")
)")
        .value("Available", shortcut_detector::ShortcutStatus::Available,
               "Shortcut is available for registration")
        .value("CapturedByApp",
               shortcut_detector::ShortcutStatus::CapturedByApp,
               "Shortcut is captured by an application")
        .value("CapturedBySystem",
               shortcut_detector::ShortcutStatus::CapturedBySystem,
               "Shortcut is captured by system component")
        .value("Reserved", shortcut_detector::ShortcutStatus::Reserved,
               "Shortcut is reserved by the operating system")
        .export_values();

    // ShortcutCheckResult structure
    py::class_<shortcut_detector::ShortcutCheckResult>(
        m, "ShortcutCheckResult",
        R"(Result of a shortcut check operation.

This class contains the result of checking whether a keyboard shortcut
is available or captured, including details about what captured it.

Attributes:
    status: Status of the shortcut (ShortcutStatus enum).
    capturing_application: Name of capturing application (if applicable).
    details: Additional details about the shortcut status.

Examples:
    >>> result = detector.is_shortcut_captured(my_shortcut)
    >>> print(f"Status: {result.status}")
    >>> if result.capturing_application:
    ...     print(f"Captured by: {result.capturing_application}")
    >>> if result.details:
    ...     print(f"Details: {result.details}")
)")
        .def(py::init<>(), "Constructs an empty ShortcutCheckResult.")
        .def_readwrite("status",
                       &shortcut_detector::ShortcutCheckResult::status,
                       "Status of the shortcut")
        .def_readwrite(
            "capturing_application",
            &shortcut_detector::ShortcutCheckResult::capturingApplication,
            "Name of capturing application (if applicable)")
        .def_readwrite("details",
                       &shortcut_detector::ShortcutCheckResult::details,
                       "Additional details about the shortcut status")
        .def("__repr__",
             [](const shortcut_detector::ShortcutCheckResult& self) {
                 std::string status_str;
                 switch (self.status) {
                     case shortcut_detector::ShortcutStatus::Available:
                         status_str = "Available";
                         break;
                     case shortcut_detector::ShortcutStatus::CapturedByApp:
                         status_str = "CapturedByApp";
                         break;
                     case shortcut_detector::ShortcutStatus::CapturedBySystem:
                         status_str = "CapturedBySystem";
                         break;
                     case shortcut_detector::ShortcutStatus::Reserved:
                         status_str = "Reserved";
                         break;
                 }
                 return "<ShortcutCheckResult(status=" + status_str +
                        ", app='" + self.capturingApplication + "')>";
             });

    // Shortcut structure
    py::class_<shortcut_detector::Shortcut>(m, "Shortcut",
                                            R"(Represents a keyboard shortcut.

This class defines a keyboard shortcut consisting of a virtual key code
and modifier keys (Ctrl, Alt, Shift, Windows key).

Attributes:
    vk_code: Virtual key code.
    ctrl: Control key required.
    alt: Alt key required.
    shift: Shift key required.
    win: Windows key required.

Examples:
    >>> # Create Ctrl+Alt+F1 shortcut
    >>> shortcut = shortcut.Shortcut(0x70, True, True, False, False)
    >>> print(shortcut.to_string())  # "Ctrl+Alt+F1"
    >>>
    >>> # Create simple F5 shortcut
    >>> f5_shortcut = shortcut.Shortcut(0x74)  # F5 key only
    >>> print(f5_shortcut.to_string())  # "F5"
)")
        .def(py::init<uint32_t, bool, bool, bool, bool>(), py::arg("key"),
             py::arg("with_ctrl") = false, py::arg("with_alt") = false,
             py::arg("with_shift") = false, py::arg("with_win") = false,
             R"(Construct a new Shortcut.

Args:
    key: Virtual key code.
    with_ctrl: Control key flag. Default is False.
    with_alt: Alt key flag. Default is False.
    with_shift: Shift key flag. Default is False.
    with_win: Windows key flag. Default is False.

Examples:
    >>> # Ctrl+C
    >>> ctrl_c = shortcut.Shortcut(0x43, True)  # 'C' key with Ctrl
    >>>
    >>> # Alt+Tab
    >>> alt_tab = shortcut.Shortcut(0x09, False, True)  # Tab with Alt
    >>>
    >>> # Ctrl+Shift+Esc
    >>> task_mgr = shortcut.Shortcut(0x1B, True, False, True)  # Esc with Ctrl+Shift
)")
        .def_readwrite("vk_code", &shortcut_detector::Shortcut::vkCode,
                       "Virtual key code")
        .def_readwrite("ctrl", &shortcut_detector::Shortcut::ctrl,
                       "Control key required")
        .def_readwrite("alt", &shortcut_detector::Shortcut::alt,
                       "Alt key required")
        .def_readwrite("shift", &shortcut_detector::Shortcut::shift,
                       "Shift key required")
        .def_readwrite("win", &shortcut_detector::Shortcut::win,
                       "Windows key required")
        .def("to_string", &shortcut_detector::Shortcut::toString,
             R"(Convert to human-readable string.

Returns:
    String representation of the shortcut (e.g., "Ctrl+Alt+F1").

Examples:
    >>> shortcut = shortcut.Shortcut(0x70, True, True)  # Ctrl+Alt+F1
    >>> print(shortcut.to_string())  # "Ctrl+Alt+F1"
)")
        .def("hash", &shortcut_detector::Shortcut::hash,
             R"(Get hash value for the shortcut.

Returns:
    Hash value that can be used for storing shortcuts in hash tables.

Examples:
    >>> hash_val = shortcut.hash()
    >>> print(f"Shortcut hash: {hash_val}")
)")
        .def("__eq__", &shortcut_detector::Shortcut::operator==,
             R"(Equality operator.

Args:
    other: Other shortcut to compare with.

Returns:
    True if shortcuts are equal, False otherwise.

Examples:
    >>> shortcut1 = shortcut.Shortcut(0x70, True, True)
    >>> shortcut2 = shortcut.Shortcut(0x70, True, True)
    >>> print(shortcut1 == shortcut2)  # True
)")
        .def("__hash__", &shortcut_detector::Shortcut::hash)
        .def("__repr__",
             [](const shortcut_detector::Shortcut& self) {
                 return "<Shortcut('" + self.toString() + "')>";
             })
        .def("__str__", &shortcut_detector::Shortcut::toString);

    // ShortcutDetector class
    py::class_<shortcut_detector::ShortcutDetector>(
        m, "ShortcutDetector",
        R"(Main class for detecting if keyboard shortcuts are captured.

This class provides methods to check if keyboard shortcuts are available
for use or if they're already captured by the system or other applications.
It can also detect keyboard hook installations.

Examples:
    >>> detector = shortcut.ShortcutDetector()
    >>>
    >>> # Check a specific shortcut
    >>> my_shortcut = shortcut.Shortcut(0x70, True, True)  # Ctrl+Alt+F1
    >>> result = detector.is_shortcut_captured(my_shortcut)
    >>>
    >>> # Check for keyboard hooks
    >>> if detector.has_keyboard_hook_installed():
    ...     print("Keyboard hooks detected")
)")
        .def(py::init<>(), "Construct a new Shortcut Detector.")
        .def(
            "is_shortcut_captured",
            &shortcut_detector::ShortcutDetector::isShortcutCaptured,
            py::arg("shortcut"),
            R"(Check if a shortcut is captured by the system or another application.

Args:
    shortcut: The shortcut to check.

Returns:
    ShortcutCheckResult containing status and details.

Examples:
    >>> shortcut = shortcut.Shortcut(0x70, True, True)  # Ctrl+Alt+F1
    >>> result = detector.is_shortcut_captured(shortcut)
    >>>
    >>> if result.status == shortcut.ShortcutStatus.Available:
    ...     print("Shortcut is available")
    >>> elif result.status == shortcut.ShortcutStatus.CapturedByApp:
    ...     print(f"Captured by: {result.capturing_application}")
    >>> elif result.status == shortcut.ShortcutStatus.CapturedBySystem:
    ...     print("Captured by system")
    >>> elif result.status == shortcut.ShortcutStatus.Reserved:
    ...     print("Reserved by operating system")
)")
        .def("has_keyboard_hook_installed",
             &shortcut_detector::ShortcutDetector::hasKeyboardHookInstalled,
             R"(Check if a keyboard hook is currently installed.

Returns:
    True if a keyboard hook is detected, False otherwise.

Examples:
    >>> if detector.has_keyboard_hook_installed():
    ...     print("Keyboard hook detected")
    ...     processes = detector.get_processes_with_keyboard_hooks()
    ...     print(f"Processes with hooks: {processes}")
    ... else:
    ...     print("No keyboard hooks detected")

Note:
    Keyboard hooks can be installed by various applications for legitimate
    purposes (e.g., global hotkey managers, accessibility software) or
    potentially malicious purposes (e.g., keyloggers).
)")
        .def(
            "get_processes_with_keyboard_hooks",
            &shortcut_detector::ShortcutDetector::getProcessesWithKeyboardHooks,
            R"(Get a list of processes with keyboard hooks.

Returns:
    List of process names that have keyboard hooks installed.

Examples:
    >>> processes = detector.get_processes_with_keyboard_hooks()
    >>> if processes:
    ...     print("Processes with keyboard hooks:")
    ...     for process in processes:
    ...         print(f"  - {process}")
    ... else:
    ...     print("No processes with keyboard hooks found")

Note:
    This function may require elevated privileges on some systems to
    enumerate all processes and their hook status.
)");

    // Utility functions for common virtual key codes
    m.attr("VK_F1") = 0x70;
    m.attr("VK_F2") = 0x71;
    m.attr("VK_F3") = 0x72;
    m.attr("VK_F4") = 0x73;
    m.attr("VK_F5") = 0x74;
    m.attr("VK_F6") = 0x75;
    m.attr("VK_F7") = 0x76;
    m.attr("VK_F8") = 0x77;
    m.attr("VK_F9") = 0x78;
    m.attr("VK_F10") = 0x79;
    m.attr("VK_F11") = 0x7A;
    m.attr("VK_F12") = 0x7B;
    m.attr("VK_ESCAPE") = 0x1B;
    m.attr("VK_TAB") = 0x09;
    m.attr("VK_SPACE") = 0x20;
    m.attr("VK_RETURN") = 0x0D;
    m.attr("VK_BACK") = 0x08;
    m.attr("VK_DELETE") = 0x2E;
    m.attr("VK_INSERT") = 0x2D;
    m.attr("VK_HOME") = 0x24;
    m.attr("VK_END") = 0x23;
    m.attr("VK_PRIOR") = 0x21;  // Page Up
    m.attr("VK_NEXT") = 0x22;   // Page Down
    m.attr("VK_UP") = 0x26;
    m.attr("VK_DOWN") = 0x28;
    m.attr("VK_LEFT") = 0x25;
    m.attr("VK_RIGHT") = 0x27;
}
