#include "atom/connection/ttybase.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

/**
 * @brief Binds the TTYResponse enum to Python.
 *
 * This function creates Python bindings for the TTYResponse enum which
 * represents different response codes for TTY operations.
 *
 * @param m The pybind11 module to bind to
 */
void bindTTYResponse(py::module_& m) {
    py::enum_<TTYBase::TTYResponse>(
        m, "TTYResponse",
        R"(Response codes for TTY operations.

This enumeration defines different response codes that can be returned
from TTY (terminal/serial) operations.

Examples:
    >>> from atom.connection.ttybase import TTYResponse
    >>> # Check operation result
    >>> if result == TTYResponse.OK:
    ...     print("Operation successful")
)")
        .value("OK", TTYBase::TTYResponse::OK, "Operation completed successfully")
        .value("ReadError", TTYBase::TTYResponse::ReadError, "Error occurred while reading from TTY")
        .value("WriteError", TTYBase::TTYResponse::WriteError, "Error occurred while writing to TTY")
        .value("SelectError", TTYBase::TTYResponse::SelectError, "Error occurred while selecting TTY device")
        .value("Timeout", TTYBase::TTYResponse::Timeout, "Operation timed out")
        .value("PortFailure", TTYBase::TTYResponse::PortFailure, "Failed to connect to TTY port")
        .value("ParamError", TTYBase::TTYResponse::ParamError, "Invalid parameter provided")
        .value("Errno", TTYBase::TTYResponse::Errno, "System error occurred (check errno)")
        .value("Overflow", TTYBase::TTYResponse::Overflow, "Buffer overflow occurred")
        .export_values();
}

PYBIND11_MODULE(ttybase, m) {
    m.doc() = R"(TTY Base module for the atom package.

This module provides TTY (teletypewriter) base functionality for handling
terminal and serial communication with various devices.

Key Features:
- Synchronous and asynchronous read/write operations
- Configurable connection parameters (baud rate, parity, stop bits)
- Buffer safety with std::span usage
- Timeout support for operations
- Error handling with detailed response codes
- Debug mode for troubleshooting
- Connection state management

Classes:
- TTYBase: Main TTY communication class
- TTYResponse: Enumeration of operation response codes

Quick Start Example:
    >>> from atom.connection.ttybase import TTYBase, TTYResponse
    >>> 
    >>> # Create TTY instance
    >>> tty = TTYBase("MyDevice")
    >>> 
    >>> # Connect to serial device
    >>> result = tty.connect("/dev/ttyUSB0", 9600, 8, 0, 1)
    >>> if result == TTYResponse.OK:
    ...     print("Connected successfully")
    ...     
    ...     # Write data
    ...     data = b"Hello, device!"
    ...     bytes_written = tty.write_string("Hello, device!")
    ...     if bytes_written[0] == TTYResponse.OK:
    ...         print(f"Wrote {bytes_written[1]} bytes")
    ...         
    ...         # Read response
    ...         buffer = bytearray(256)
    ...         read_result = tty.read(buffer, timeout=5)
    ...         if read_result[0] == TTYResponse.OK:
    ...             print(f"Read {read_result[1]} bytes: {buffer[:read_result[1]]}")
    >>> 
    >>> tty.disconnect()

Advanced Features:
- Asynchronous operations with futures
- Section reading with stop bytes
- Buffer overflow protection
- Connection parameter validation
- Debug logging capabilities
- File descriptor access for advanced usage
)";

    // Bind enums
    bindTTYResponse(m);

    // Bind the main TTYBase class
    py::class_<TTYBase>(
        m, "TTYBase",
        R"(Base class for TTY (terminal/serial) communication.

This class provides methods for connecting to and communicating with
TTY devices such as serial ports, terminals, and other character devices.

Examples:
    >>> from atom.connection.ttybase import TTYBase, TTYResponse
    >>> 
    >>> # Create TTY instance
    >>> tty = TTYBase("SerialDevice")
    >>> 
    >>> # Connect to device
    >>> result = tty.connect("/dev/ttyUSB0", 9600, 8, 0, 1)
    >>> if result == TTYResponse.OK:
    ...     print("Connected to serial device")
    ...     
    ...     # Send command
    ...     response = tty.write_string("AT\r\n")
    ...     if response[0] == TTYResponse.OK:
    ...         print(f"Sent {response[1]} bytes")
    >>> 
    >>> tty.disconnect()
)")
        .def(py::init<std::string_view>(),
             py::arg("driver_name"),
             R"(Constructs a TTYBase instance with the specified driver name.

Args:
    driver_name: The name of the TTY driver to use

Examples:
    >>> tty = TTYBase("MySerialDriver")
)")
        .def("read",
             [](TTYBase& self, py::buffer buffer, uint8_t timeout) {
                 py::buffer_info buf_info = buffer.request();
                 if (buf_info.format != py::format_descriptor<uint8_t>::format()) {
                     throw std::runtime_error("Buffer must be of type uint8_t");
                 }
                 
                 std::span<uint8_t> span(static_cast<uint8_t*>(buf_info.ptr), buf_info.size);
                 uint32_t bytes_read = 0;
                 TTYBase::TTYResponse result = self.read(span, timeout, bytes_read);
                 return std::make_pair(result, bytes_read);
             },
             py::arg("buffer"), py::arg("timeout"),
             R"(Safely reads data from the TTY device.

Args:
    buffer: Buffer to store the read data (must be writable bytes-like object)
    timeout: Timeout for the read operation in seconds

Returns:
    Tuple of (TTYResponse, bytes_read)

Raises:
    RuntimeError: If buffer format is invalid
    SystemError: If a critical system error occurs

Examples:
    >>> buffer = bytearray(256)
    >>> result, bytes_read = tty.read(buffer, timeout=5)
    >>> if result == TTYResponse.OK:
    ...     print(f"Read {bytes_read} bytes: {buffer[:bytes_read]}")
)")
        .def("read_section",
             [](TTYBase& self, py::buffer buffer, uint8_t stop_byte, uint8_t timeout) {
                 py::buffer_info buf_info = buffer.request();
                 if (buf_info.format != py::format_descriptor<uint8_t>::format()) {
                     throw std::runtime_error("Buffer must be of type uint8_t");
                 }
                 
                 std::span<uint8_t> span(static_cast<uint8_t*>(buf_info.ptr), buf_info.size);
                 uint32_t bytes_read = 0;
                 TTYBase::TTYResponse result = self.readSection(span, stop_byte, timeout, bytes_read);
                 return std::make_pair(result, bytes_read);
             },
             py::arg("buffer"), py::arg("stop_byte"), py::arg("timeout"),
             R"(Reads data from the TTY until a stop byte is encountered.

Args:
    buffer: Buffer to store the read data (must be writable bytes-like object)
    stop_byte: The byte value at which to stop reading
    timeout: Timeout for the read operation in seconds

Returns:
    Tuple of (TTYResponse, bytes_read)

Raises:
    RuntimeError: If buffer format is invalid
    SystemError: If a critical system error occurs

Examples:
    >>> buffer = bytearray(256)
    >>> result, bytes_read = tty.read_section(buffer, ord('\n'), timeout=5)
    >>> if result == TTYResponse.OK:
    ...     line = buffer[:bytes_read].decode('utf-8')
    ...     print(f"Read line: {line}")
)")
        .def("write",
             [](TTYBase& self, py::buffer buffer) {
                 py::buffer_info buf_info = buffer.request();
                 if (buf_info.format != py::format_descriptor<uint8_t>::format()) {
                     throw std::runtime_error("Buffer must be of type uint8_t");
                 }
                 
                 std::span<const uint8_t> span(static_cast<const uint8_t*>(buf_info.ptr), buf_info.size);
                 uint32_t bytes_written = 0;
                 TTYBase::TTYResponse result = self.write(span, bytes_written);
                 return std::make_pair(result, bytes_written);
             },
             py::arg("buffer"),
             R"(Safely writes data to the TTY device.

Args:
    buffer: The data to write (bytes-like object)

Returns:
    Tuple of (TTYResponse, bytes_written)

Raises:
    RuntimeError: If buffer format is invalid
    SystemError: If a critical system error occurs

Examples:
    >>> data = b"Hello, device!"
    >>> result, bytes_written = tty.write(data)
    >>> if result == TTYResponse.OK:
    ...     print(f"Wrote {bytes_written} bytes")
)")
        .def("write_string",
             [](TTYBase& self, std::string_view string) {
                 uint32_t bytes_written = 0;
                 TTYBase::TTYResponse result = self.writeString(string, bytes_written);
                 return std::make_pair(result, bytes_written);
             },
             py::arg("string"),
             R"(Writes a string to the TTY device.

Args:
    string: The string to write to the TTY

Returns:
    Tuple of (TTYResponse, bytes_written)

Examples:
    >>> result, bytes_written = tty.write_string("AT\r\n")
    >>> if result == TTYResponse.OK:
    ...     print(f"Sent command, wrote {bytes_written} bytes")
)")
        .def("connect", &TTYBase::connect,
             py::arg("device"), py::arg("bit_rate"), py::arg("word_size"),
             py::arg("parity"), py::arg("stop_bits"),
             R"(Connects to the specified TTY device.

Args:
    device: The device name or path to connect to (e.g., "/dev/ttyUSB0")
    bit_rate: The baud rate for the connection (e.g., 9600, 115200)
    word_size: The data size in bits per character (typically 7 or 8)
    parity: The parity mode (0=none, 1=odd, 2=even)
    stop_bits: The number of stop bits (1 or 2)

Returns:
    TTYResponse indicating the result of the connection attempt

Raises:
    ValueError: For invalid parameters
    SystemError: For system-level errors

Examples:
    >>> # Connect to USB serial device at 9600 baud, 8N1
    >>> result = tty.connect("/dev/ttyUSB0", 9600, 8, 0, 1)
    >>> if result == TTYResponse.OK:
    ...     print("Connected successfully")
    >>>
    >>> # Connect to Bluetooth serial at 115200 baud
    >>> result = tty.connect("/dev/rfcomm0", 115200, 8, 0, 1)
)")
        .def("disconnect", &TTYBase::disconnect,
             R"(Disconnects from the TTY device.

Returns:
    TTYResponse indicating the result of the disconnection

Examples:
    >>> result = tty.disconnect()
    >>> if result == TTYResponse.OK:
    ...     print("Disconnected successfully")
)")
        .def("set_debug", &TTYBase::setDebug,
             py::arg("enabled"),
             R"(Enables or disables debugging information.

Args:
    enabled: True to enable debugging, False to disable

Examples:
    >>> tty.set_debug(True)  # Enable debug output
    >>> tty.set_debug(False)  # Disable debug output
)")
        .def("get_error_message", &TTYBase::getErrorMessage,
             py::arg("code"),
             R"(Gets the error message corresponding to a TTYResponse code.

Args:
    code: The TTYResponse code for which to retrieve the error message

Returns:
    String containing the error message

Examples:
    >>> error_msg = tty.get_error_message(TTYResponse.Timeout)
    >>> print(f"Error: {error_msg}")
)")
        .def("get_port_fd", &TTYBase::getPortFD,
             R"(Gets the file descriptor of the TTY port.

Returns:
    The integer file descriptor of the TTY port

Examples:
    >>> fd = tty.get_port_fd()
    >>> print(f"TTY file descriptor: {fd}")
)")
        .def("is_connected", &TTYBase::isConnected,
             R"(Checks if the TTY port is connected.

Returns:
    True if connected, False otherwise

Examples:
    >>> if tty.is_connected():
    ...     print("TTY is connected")
    ... else:
    ...     print("TTY is not connected")
)")
        .def(
            "__enter__",
            [](TTYBase& self) -> TTYBase& {
                return self;
            },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](TTYBase& self, py::object, py::object, py::object) {
                if (self.isConnected()) {
                    self.disconnect();
                }
            },
            "Ensure TTY is disconnected when exiting context");

    // Add utility functions
    m.def("response_to_string",
          [](TTYBase::TTYResponse response) {
              switch (response) {
                  case TTYBase::TTYResponse::OK: return std::string("OK");
                  case TTYBase::TTYResponse::ReadError: return std::string("ReadError");
                  case TTYBase::TTYResponse::WriteError: return std::string("WriteError");
                  case TTYBase::TTYResponse::SelectError: return std::string("SelectError");
                  case TTYBase::TTYResponse::Timeout: return std::string("Timeout");
                  case TTYBase::TTYResponse::PortFailure: return std::string("PortFailure");
                  case TTYBase::TTYResponse::ParamError: return std::string("ParamError");
                  case TTYBase::TTYResponse::Errno: return std::string("Errno");
                  case TTYBase::TTYResponse::Overflow: return std::string("Overflow");
                  default: return std::string("Unknown");
              }
          },
          py::arg("response"),
          "Converts a TTYResponse enum value to a string");
}
