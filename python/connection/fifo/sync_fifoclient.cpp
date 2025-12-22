#include "atom/connection/fifoclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(sync_fifoclient, m) {
    m.doc() = R"(Synchronous FIFO client module for the atom package.

This module provides synchronous FIFO (First In, First Out) client functionality
with modern C++20 features including expected<T, E> error handling, concepts,
and comprehensive configuration options.

Key Features:
- Synchronous FIFO read/write operations
- Configurable timeouts and buffer sizes
- Message priority support
- Automatic reconnection
- Statistics tracking
- Async operation support with futures
- Compression and encryption support (configurable)

Classes:
- FifoClient: Main FIFO client class
- ClientConfig: Configuration options
- ClientStats: Statistics tracking
- MessagePriority: Priority levels for messages

Quick Start Example:
    >>> from atom.connection.sync_fifoclient import FifoClient, ClientConfig
    >>>
    >>> # Create client with custom configuration
    >>> config = ClientConfig()
    >>> config.auto_reconnect = True
    >>> config.default_timeout = 5000
    >>> client = FifoClient("/tmp/myfifo", config)
    >>>
    >>> # Write data
    >>> result = client.write("Hello, FIFO!")
    >>> if result.has_value():
    ...     print(f"Wrote {result.value()} bytes")
    >>>
    >>> # Read data
    >>> data = client.read()
    >>> if data.has_value():
    ...     print(f"Received: {data.value()}")
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::system_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // MessagePriority enum
    py::enum_<atom::connection::MessagePriority>(
        m, "MessagePriority", "Priority levels for FIFO messages")
        .value("LOW", atom::connection::MessagePriority::Low,
               "Low priority message")
        .value("NORMAL", atom::connection::MessagePriority::Normal,
               "Normal priority message")
        .value("HIGH", atom::connection::MessagePriority::High,
               "High priority message")
        .value("CRITICAL", atom::connection::MessagePriority::Critical,
               "Critical priority message")
        .export_values();

    // ClientConfig struct
    py::class_<atom::connection::ClientConfig>(
        m, "ClientConfig",
        R"(Configuration options for the FIFO client.

This structure provides various settings to control FIFO client behavior,
including buffer sizes, timeouts, reconnection settings, and optional
compression/encryption.

Examples:
    >>> config = ClientConfig()
    >>> config.read_buffer_size = 8192
    >>> config.auto_reconnect = True
    >>> config.max_reconnect_attempts = 5
    >>> config.default_timeout = 5000
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("read_buffer_size",
                       &atom::connection::ClientConfig::read_buffer_size,
                       "Size of the read buffer in bytes")
        .def_readwrite("max_message_size",
                       &atom::connection::ClientConfig::max_message_size,
                       "Maximum message size in bytes")
        .def_readwrite("auto_reconnect",
                       &atom::connection::ClientConfig::auto_reconnect,
                       "Whether to automatically reconnect on disconnection")
        .def_readwrite("max_reconnect_attempts",
                       &atom::connection::ClientConfig::max_reconnect_attempts,
                       "Maximum number of reconnection attempts")
        .def_readwrite("reconnect_delay",
                       &atom::connection::ClientConfig::reconnect_delay,
                       "Delay between reconnection attempts")
        .def_readwrite("default_timeout",
                       &atom::connection::ClientConfig::default_timeout,
                       "Default timeout for operations")
        .def_readwrite("enable_compression",
                       &atom::connection::ClientConfig::enable_compression,
                       "Whether to enable data compression")
        .def_readwrite("compression_threshold",
                       &atom::connection::ClientConfig::compression_threshold,
                       "Minimum message size for compression")
        .def_readwrite("enable_encryption",
                       &atom::connection::ClientConfig::enable_encryption,
                       "Whether to enable data encryption");

    // ClientStats struct
    py::class_<atom::connection::ClientStats>(m, "ClientStats",
                                              R"(Statistics for the FIFO client.

This structure provides metrics about client operation including
message counts, bytes transferred, latency, and reconnection statistics.

Examples:
    >>> stats = client.get_statistics()
    >>> print(f"Messages sent: {stats.messages_sent}")
    >>> print(f"Average latency: {stats.avg_write_latency_ms} ms")
)")
        .def(py::init<>(), "Default constructor")
        .def_readonly("messages_sent",
                      &atom::connection::ClientStats::messages_sent,
                      "Total number of messages sent")
        .def_readonly("messages_failed",
                      &atom::connection::ClientStats::messages_failed,
                      "Number of failed message sends")
        .def_readonly("bytes_sent", &atom::connection::ClientStats::bytes_sent,
                      "Total bytes sent")
        .def_readonly("bytes_received",
                      &atom::connection::ClientStats::bytes_received,
                      "Total bytes received")
        .def_readonly("avg_write_latency_ms",
                      &atom::connection::ClientStats::avg_write_latency_ms,
                      "Average write latency in milliseconds")
        .def_readonly("avg_read_latency_ms",
                      &atom::connection::ClientStats::avg_read_latency_ms,
                      "Average read latency in milliseconds")
        .def_readonly("reconnect_attempts",
                      &atom::connection::ClientStats::reconnect_attempts,
                      "Number of reconnection attempts")
        .def_readonly("successful_reconnects",
                      &atom::connection::ClientStats::successful_reconnects,
                      "Number of successful reconnections")
        .def_readonly("avg_compression_ratio",
                      &atom::connection::ClientStats::avg_compression_ratio,
                      "Average compression ratio");

    // FifoClient class
    py::class_<atom::connection::FifoClient>(
        m, "FifoClient",
        R"(Synchronous FIFO client for reading and writing to named pipes.

This class provides comprehensive FIFO communication with support for
timeouts, priorities, statistics, and optional compression/encryption.

Examples:
    >>> from atom.connection.sync_fifoclient import FifoClient, ClientConfig, MessagePriority
    >>>
    >>> # Create client with default config
    >>> client = FifoClient("/tmp/myfifo")
    >>>
    >>> # Write with priority
    >>> result = client.write("Important message", MessagePriority.HIGH)
    >>>
    >>> # Read with timeout
    >>> data = client.read(4096, 1000)  # max 4096 bytes, 1 second timeout
    >>> if data.has_value():
    ...     print(f"Received: {data.value()}")
    >>>
    >>> # Get statistics
    >>> stats = client.get_statistics()
    >>> print(f"Messages sent: {stats.messages_sent}")
)")
        .def(py::init<std::string_view>(), py::arg("fifo_path"),
             "Constructs a FifoClient with the specified FIFO path and default "
             "configuration.")
        .def(
            py::init<std::string_view, const atom::connection::ClientConfig&>(),
            py::arg("fifo_path"), py::arg("config"),
            "Constructs a FifoClient with the specified FIFO path and custom "
            "configuration.")
        .def(
            "write",
            [](atom::connection::FifoClient& self, std::string_view data,
               std::optional<std::chrono::milliseconds> timeout) {
                auto result = self.write(data, timeout);
                if (!result.has_value()) {
                    throw std::system_error(result.error());
                }
                return result.value();
            },
            py::arg("data"),
            py::arg("timeout") = std::optional<std::chrono::milliseconds>(),
            R"(Writes string data to the FIFO.

Args:
    data: The string data to write
    timeout: Optional timeout in milliseconds

Returns:
    Number of bytes written

Raises:
    OSError: If the write operation fails
)")
        .def(
            "write",
            [](atom::connection::FifoClient& self, std::string_view data,
               atom::connection::MessagePriority priority,
               std::optional<std::chrono::milliseconds> timeout) {
                auto result = self.write(data, priority, timeout);
                if (!result.has_value()) {
                    throw std::system_error(result.error());
                }
                return result.value();
            },
            py::arg("data"), py::arg("priority"),
            py::arg("timeout") = std::optional<std::chrono::milliseconds>(),
            R"(Writes data to the FIFO with specified priority.

Args:
    data: The string data to write
    priority: Message priority level
    timeout: Optional timeout in milliseconds

Returns:
    Number of bytes written

Raises:
    OSError: If the write operation fails
)")
        .def(
            "read",
            [](atom::connection::FifoClient& self, std::size_t maxSize,
               std::optional<std::chrono::milliseconds> timeout) {
                auto result = self.read(maxSize, timeout);
                if (!result.has_value()) {
                    throw std::system_error(result.error());
                }
                return result.value();
            },
            py::arg("max_size") = 0,
            py::arg("timeout") = std::optional<std::chrono::milliseconds>(),
            R"(Reads data from the FIFO.

Args:
    max_size: Maximum size to read (0 for default buffer size)
    timeout: Optional timeout in milliseconds

Returns:
    The read data as a string

Raises:
    OSError: If the read operation fails
)")
        .def("is_open", &atom::connection::FifoClient::isOpen,
             R"(Checks if the FIFO is currently open.

Returns:
    True if the FIFO is open, False otherwise
)")
        .def("get_path", &atom::connection::FifoClient::getPath,
             R"(Gets the FIFO path.

Returns:
    The FIFO path as a string
)")
        .def(
            "open",
            [](atom::connection::FifoClient& self,
               std::optional<std::chrono::milliseconds> timeout) {
                auto result = self.open(timeout);
                if (!result.has_value()) {
                    throw std::system_error(result.error());
                }
            },
            py::arg("timeout") = std::optional<std::chrono::milliseconds>(),
            R"(Opens the FIFO connection.

Args:
    timeout: Optional timeout in milliseconds

Raises:
    OSError: If the open operation fails
)")
        .def("close", &atom::connection::FifoClient::close,
             R"(Closes the FIFO connection.)")
        .def("get_config", &atom::connection::FifoClient::getConfig,
             R"(Gets the current client configuration.

Returns:
    ClientConfig object with current settings
)")
        .def("update_config", &atom::connection::FifoClient::updateConfig,
             py::arg("config"),
             R"(Updates the client configuration.

Args:
    config: New configuration settings

Returns:
    True if successfully updated, False otherwise
)")
        .def("get_statistics", &atom::connection::FifoClient::getStatistics,
             R"(Gets client statistics.

Returns:
    ClientStats object with current statistics
)")
        .def("reset_statistics", &atom::connection::FifoClient::resetStatistics,
             R"(Resets client statistics to zero.)")
        .def(
            "__enter__",
            [](atom::connection::FifoClient& self)
                -> atom::connection::FifoClient& { return self; },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::connection::FifoClient& self, py::object, py::object,
               py::object) { self.close(); },
            "Ensure FIFO is closed when exiting context");

    // Factory function
    m.def(
        "create_fifo_client",
        [](const std::string& path, bool auto_reconnect) {
            atom::connection::ClientConfig config;
            config.auto_reconnect = auto_reconnect;
            return std::make_unique<atom::connection::FifoClient>(path, config);
        },
        py::arg("path"), py::arg("auto_reconnect") = true,
        R"(Creates a FIFO client with common configuration.

Args:
    path: Path to the FIFO file
    auto_reconnect: Whether to enable automatic reconnection

Returns:
    A newly created FifoClient object

Examples:
    >>> client = create_fifo_client("/tmp/myfifo", auto_reconnect=True)
)");
}
