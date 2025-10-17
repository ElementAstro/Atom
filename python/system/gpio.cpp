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

    // Define PwmMode enum
    py::enum_<atom::system::GPIO::PwmMode>(
        m, "PwmMode", "PWM (Pulse Width Modulation) operation mode")
        .value("HARDWARE", atom::system::GPIO::PwmMode::HARDWARE,
               "Use hardware PWM if available")
        .value("SOFTWARE", atom::system::GPIO::PwmMode::SOFTWARE,
               "Use software PWM implementation")
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

        // PWM methods
        .def("set_pwm", &atom::system::GPIO::setPwm, py::arg("frequency"),
             py::arg("duty_cycle"),
             py::arg("mode") = atom::system::GPIO::PwmMode::HARDWARE,
             R"(Sets up PWM (Pulse Width Modulation) on the pin.

Args:
    frequency: The PWM frequency in hertz.
    duty_cycle: The duty cycle (0.0 to 1.0).
    mode: The PWM mode (HARDWARE or SOFTWARE). Default is HARDWARE.

Returns:
    True if PWM was successfully set up, False otherwise.

Examples:
    >>> gpio = GPIO("18", GPIO.Direction.OUTPUT)
    >>> gpio.set_pwm(1000, 0.5)  # 1kHz, 50% duty cycle
)")
        .def("update_pwm_duty_cycle", &atom::system::GPIO::updatePwmDutyCycle,
             py::arg("duty_cycle"),
             R"(Updates the PWM duty cycle.

Args:
    duty_cycle: The new duty cycle (0.0 to 1.0).

Returns:
    True if successful, False otherwise.

Examples:
    >>> gpio.update_pwm_duty_cycle(0.75)  # Change to 75% duty cycle
)")
        .def("stop_pwm", &atom::system::GPIO::stopPwm,
             R"(Stops PWM operation.

Examples:
    >>> gpio.stop_pwm()
)")

        // Button debouncing
        .def("setup_button_debounce", &atom::system::GPIO::setupButtonDebounce,
             py::arg("callback"), py::arg("debounce_time_ms") = 50,
             R"(Implements button debouncing for input pins.

Args:
    callback: The function to call when a debounced press is detected.
    debounce_time_ms: The debounce time in milliseconds. Default is 50ms.

Returns:
    True if debouncing was successfully set up, False otherwise.

Examples:
    >>> def button_pressed():
    ...     print("Button pressed!")
    >>> button_pin = GPIO("17", GPIO.Direction.INPUT)
    >>> button_pin.setup_button_debounce(button_pressed, 100)
)")

        // Interrupt counting
        .def("setup_interrupt_counter",
             &atom::system::GPIO::setupInterruptCounter,
             py::arg("edge") = atom::system::GPIO::Edge::RISING,
             R"(Sets up an interrupt counter for this pin.

Args:
    edge: The edge to count (RISING, FALLING, or BOTH). Default is RISING.

Returns:
    True if the counter was successfully set up, False otherwise.

Examples:
    >>> gpio.setup_interrupt_counter(GPIO.Edge.BOTH)
)")
        .def("get_interrupt_count", &atom::system::GPIO::getInterruptCount,
             py::arg("reset_after_reading") = false,
             R"(Gets the current interrupt count.

Args:
    reset_after_reading: Whether to reset the counter after reading. Default is False.

Returns:
    The number of interrupts counted.

Examples:
    >>> count = gpio.get_interrupt_count()
    >>> print(f"Interrupts counted: {count}")
)")
        .def("reset_interrupt_count", &atom::system::GPIO::resetInterruptCount,
             R"(Resets the interrupt counter to zero.

Examples:
    >>> gpio.reset_interrupt_count()
)")

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

    // Define ShiftRegister class
    py::class_<atom::system::GPIO::ShiftRegister>(
        m, "ShiftRegister",
        R"(A utility class for managing shift registers (e.g., 74HC595).

This class provides methods to control shift registers for expanding digital outputs.

Args:
    data_pin: The data pin (DS) number as a string.
    clock_pin: The clock pin (SH_CP) number as a string.
    latch_pin: The latch pin (ST_CP) number as a string.
    num_bits: The number of bits in the shift register chain. Default is 8.

Examples:
    >>> from atom.system import GPIO
    >>> shift_reg = GPIO.ShiftRegister("14", "15", "16", 8)
    >>> shift_reg.shift_out(0b10101010)  # Output pattern
    >>> shift_reg.set_bit(3, True)      # Set bit 3
)")
        .def(py::init<const std::string&, const std::string&,
                      const std::string&, uint8_t>(),
             py::arg("data_pin"), py::arg("clock_pin"), py::arg("latch_pin"),
             py::arg("num_bits") = 8,
             "Constructs a ShiftRegister with specified pins and bit count.")
        .def("shift_out", &atom::system::GPIO::ShiftRegister::shiftOut,
             py::arg("data"), py::arg("msb_first") = true,
             R"(Shifts out data to the register.

Args:
    data: The data to shift out.
    msb_first: True for MSB first, False for LSB first. Default is True.

Examples:
    >>> shift_reg.shift_out(0xFF, True)   # All bits high, MSB first
    >>> shift_reg.shift_out(0x55, False)  # Alternating pattern, LSB first
)")
        .def("set_bit", &atom::system::GPIO::ShiftRegister::setBit,
             py::arg("position"), py::arg("value"),
             R"(Sets a single bit in the shift register.

Args:
    position: The bit position (0-based).
    value: The value to set (True/False).

Examples:
    >>> shift_reg.set_bit(0, True)   # Set bit 0
    >>> shift_reg.set_bit(7, False)  # Clear bit 7
)")
        .def("get_state", &atom::system::GPIO::ShiftRegister::getState,
             R"(Gets the current state of the output register.

Returns:
    The current register state as an integer.

Examples:
    >>> state = shift_reg.get_state()
    >>> print(f"Current state: 0x{state:02X}")
)")
        .def("clear", &atom::system::GPIO::ShiftRegister::clear,
             R"(Clears all bits in the register (sets to 0).

Examples:
    >>> shift_reg.clear()  # All outputs low
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
