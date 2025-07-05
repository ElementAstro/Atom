#include "atom/system/gpio.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(gpio, m) {
    m.doc() =
        "GPIO (General Purpose Input/Output) management module for the atom "
        "package";

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

    // Define Direction enum
    py::enum_<atom::system::GPIO::Direction>(m, "Direction",
                                             "GPIO pin direction")
        .value("INPUT", atom::system::GPIO::Direction::INPUT, "Input mode")
        .value("OUTPUT", atom::system::GPIO::Direction::OUTPUT, "Output mode")
        .export_values();

    // Define Edge enum
    py::enum_<atom::system::GPIO::Edge>(m, "Edge",
                                        "GPIO pin edge detection mode")
        .value("NONE", atom::system::GPIO::Edge::NONE, "No edge detection")
        .value("RISING", atom::system::GPIO::Edge::RISING,
               "Rising edge detection")
        .value("FALLING", atom::system::GPIO::Edge::FALLING,
               "Falling edge detection")
        .value("BOTH", atom::system::GPIO::Edge::BOTH, "Both edges detection")
        .export_values();

    // Define PullMode enum
    py::enum_<atom::system::GPIO::PullMode>(
        m, "PullMode", "GPIO pin pull-up/down resistor mode")
        .value("NONE", atom::system::GPIO::PullMode::NONE, "No pull-up/down")
        .value("UP", atom::system::GPIO::PullMode::UP, "Pull-up resistor")
        .value("DOWN", atom::system::GPIO::PullMode::DOWN, "Pull-down resistor")
        .export_values();

    // Define GPIO class
    py::class_<atom::system::GPIO>(
        m, "GPIO",
        R"(A class to manage GPIO (General Purpose Input/Output) pins.

This class provides methods to control and monitor GPIO pins, including
setting pin direction, values, edge detection, and pull-up/down modes.

Args:
    pin: The pin number as a string.
    direction: Optional direction (INPUT or OUTPUT). Default is INPUT.
    initial_value: Optional initial value for output pins. Default is False.

Examples:
    >>> from atom.system import GPIO
    >>> led_pin = GPIO("18", GPIO.Direction.OUTPUT)
    >>> led_pin.set_value(True)  # Turn on the LED
    >>> button_pin = GPIO("17", GPIO.Direction.INPUT)
    >>> button_state = button_pin.get_value()
)")
        .def(py::init<const std::string&>(), py::arg("pin"),
             "Constructs a GPIO object for a specific pin.")
        .def(
            py::init<const std::string&, atom::system::GPIO::Direction, bool>(),
            py::arg("pin"), py::arg("direction"),
            py::arg("initial_value") = false,
            "Constructs a GPIO object with specific configuration.")

        // Value methods
        .def("set_value", &atom::system::GPIO::setValue, py::arg("value"),
             R"(Sets the value of the GPIO pin.

Args:
    value: The value to set (True for HIGH, False for LOW).
)")
        .def("get_value", &atom::system::GPIO::getValue,
             R"(Gets the current value of the GPIO pin.

Returns:
    The current value of the pin (True for HIGH, False for LOW).
)")
        .def("toggle", &atom::system::GPIO::toggle,
             R"(Toggles the value of the GPIO pin.

Returns:
    The new value of the pin after toggling.
)")
        .def("pulse", &atom::system::GPIO::pulse, py::arg("value"),
             py::arg("duration"),
             R"(Pulses the GPIO pin for a specified duration.

Args:
    value: The value to pulse (True for HIGH, False for LOW).
    duration: The duration of the pulse in milliseconds.
)")

        // Direction methods
        .def("set_direction", &atom::system::GPIO::setDirection,
             py::arg("direction"),
             R"(Sets the direction of the GPIO pin.

Args:
    direction: The direction to set (INPUT or OUTPUT).
)")
        .def("get_direction", &atom::system::GPIO::getDirection,
             R"(Gets the current direction of the GPIO pin.

Returns:
    The current direction of the pin.
)")

        // Edge methods
        .def("set_edge", &atom::system::GPIO::setEdge, py::arg("edge"),
             R"(Sets the edge detection mode of the GPIO pin.

Args:
    edge: The edge detection mode to set (NONE, RISING, FALLING, or BOTH).
)")
        .def("get_edge", &atom::system::GPIO::getEdge,
             R"(Gets the current edge detection mode of the GPIO pin.

Returns:
    The current edge detection mode of the pin.
)")

        // Pull mode methods
        .def("set_pull_mode", &atom::system::GPIO::setPullMode, py::arg("mode"),
             R"(Sets the pull-up/down resistor mode of the GPIO pin.

Args:
    mode: The pull-up/down mode to set (NONE, UP, or DOWN).
)")
        .def("get_pull_mode", &atom::system::GPIO::getPullMode,
             R"(Gets the pull-up/down resistor mode of the GPIO pin.

Returns:
    The current pull-up/down mode of the pin.
)")

        // Pin info
        .def("get_pin", &atom::system::GPIO::getPin,
             R"(Gets the pin number.

Returns:
    The pin number as a string.
)")

        // Callback methods
        .def("on_value_change", &atom::system::GPIO::onValueChange,
             py::arg("callback"),
             R"(Sets up a callback for pin value changes.

Args:
    callback: The callback function to call when the pin value changes.
              The callback should take a boolean parameter.

Returns:
    True if the callback was successfully set up, False otherwise.

Examples:
    >>> def value_changed(value):
    ...     print(f"Value changed to: {value}")
    >>> gpio.on_value_change(value_changed)
)")
        .def("on_edge_change", &atom::system::GPIO::onEdgeChange,
             py::arg("edge"), py::arg("callback"),
             R"(Sets up a callback for specific edge changes.

Args:
    edge: The edge detection mode (RISING, FALLING, or BOTH).
    callback: The callback function to call when the specified edge is detected.
              The callback should take a boolean parameter.

Returns:
    True if the callback was successfully set up, False otherwise.
)")
        .def("stop_callbacks", &atom::system::GPIO::stopCallbacks,
             R"(Stops all callbacks on this pin.)")

        // Static methods
        .def_static(
            "notify_on_change", &atom::system::GPIO::notifyOnChange,
            py::arg("pin"), py::arg("callback"),
            R"(Sets up a notification callback for changes on the GPIO pin.

Args:
    pin: The pin number as a string.
    callback: The callback function to call when the pin value changes.
              The callback should take a boolean parameter.

Note:
    This is a static method. It's recommended to use the instance method
    on_value_change() instead.
)");

    // Define GPIOGroup class
    py::class_<atom::system::GPIO::GPIOGroup>(
        m, "GPIOGroup",
        R"(A utility class for managing multiple GPIO pins as a group.

This class provides methods to control multiple GPIO pins simultaneously,
making it easier to work with related pins.

Args:
    pins: A list of pin numbers as strings.

Examples:
    >>> from atom.system import GPIO
    >>> led_group = GPIO.GPIOGroup(["17", "18", "19", "20"])
    >>> led_group.set_direction(GPIO.Direction.OUTPUT)
    >>> led_group.set_values([True, False, True, False])
)")
        .def(py::init<const std::vector<std::string>&>(), py::arg("pins"),
             "Constructs a GPIOGroup with specified pins.")
        .def("set_values", &atom::system::GPIO::GPIOGroup::setValues,
             py::arg("values"),
             R"(Sets values for all pins in the group.

Args:
    values: List of boolean values for each pin.
)")
        .def("get_values", &atom::system::GPIO::GPIOGroup::getValues,
             R"(Gets values from all pins in the group.

Returns:
    List of boolean values from each pin.
)")
        .def("set_direction", &atom::system::GPIO::GPIOGroup::setDirection,
             py::arg("direction"),
             R"(Sets the same direction for all pins in the group.

Args:
    direction: The direction to set for all pins.
)");

    // Helper functions
    m.def("string_to_direction", &atom::system::stringToDirection,
          py::arg("direction"),
          R"(Converts string to GPIO::Direction enumeration.

Args:
    direction: The direction as a string ("in" or "out").

Returns:
    The corresponding Direction enumeration.
)");

    m.def("direction_to_string", &atom::system::directionToString,
          py::arg("direction"),
          R"(Converts GPIO::Direction enumeration to string.

Args:
    direction: The Direction enumeration.

Returns:
    The corresponding direction as a string.
)");

    m.def("string_to_edge", &atom::system::stringToEdge, py::arg("edge"),
          R"(Converts string to GPIO::Edge enumeration.

Args:
    edge: The edge as a string ("none", "rising", "falling", or "both").

Returns:
    The corresponding Edge enumeration.
)");

    m.def("edge_to_string", &atom::system::edgeToString, py::arg("edge"),
          R"(Converts GPIO::Edge enumeration to string.

Args:
    edge: The Edge enumeration.

Returns:
    The corresponding edge as a string.
)");
}
