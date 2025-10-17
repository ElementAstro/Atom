#include "atom/connection/fifoserver.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

/**
 * @brief Binds the LogLevel enum to Python.
 *
 * This function creates Python bindings for the LogLevel enum which
 * represents different log levels for the FIFO server.
 *
 * @param m The pybind11 module to bind to
 */
void bindLogLevel(py::module_& m) {
    py::enum_<atom::connection::LogLevel>(m, "LogLevel",
                                          R"(Log levels for the FIFO server.

This enumeration defines different levels of logging verbosity for the server.

Examples:
    >>> from atom.connection.sync_fifoserver import LogLevel
    >>> server.set_log_level(LogLevel.Debug)
)")
        .value("Debug", atom::connection::LogLevel::Debug,
               "Debug level logging")
        .value("Info", atom::connection::LogLevel::Info,
               "Information level logging")
        .value("Warning", atom::connection::LogLevel::Warning,
               "Warning level logging")
        .value("Error", atom::connection::LogLevel::Error,
               "Error level logging")
        .value("None", atom::connection::LogLevel::None, "No logging")
        .export_values();
}

/**
 * @brief Binds the MessagePriority enum to Python.
 *
 * This function creates Python bindings for the MessagePriority enum which
 * represents different priority levels for messages.
 *
 * @param m The pybind11 module to bind to
 */
void bindMessagePriority(py::module_& m) {
    py::enum_<atom::connection::MessagePriority>(
        m, "MessagePriority",
        R"(Priority levels for FIFO messages.

This enumeration defines different priority levels that can be assigned to messages
for processing order control.

Examples:
    >>> from atom.connection.sync_fifoserver import MessagePriority
    >>> server.send_message("urgent", MessagePriority.High)
)")
        .value("Low", atom::connection::MessagePriority::Low,
               "Low priority message")
        .value("Normal", atom::connection::MessagePriority::Normal,
               "Normal priority message")
        .value("High", atom::connection::MessagePriority::High,
               "High priority message")
        .value("Critical", atom::connection::MessagePriority::Critical,
               "Critical priority message")
        .export_values();
}

/**
 * @brief Binds the ServerStats struct to Python.
 *
 * This function creates Python bindings for the ServerStats struct which
 * provides statistics about server operation.
 *
 * @param m The pybind11 module to bind to
 */
void bindServerStats(py::module_& m) {
    py::class_<atom::connection::ServerStats>(
        m, "ServerStats",
        R"(Statistics for the FIFO server operation.

This structure provides various metrics about server usage and performance.

Examples:
    >>> stats = server.get_statistics()
    >>> print(f"Messages sent: {stats.messages_sent}")
    >>> print(f"Messages failed: {stats.messages_failed}")
)")
        .def(py::init<>(), "Default constructor")
        .def_readonly("messages_sent",
                      &atom::connection::ServerStats::messages_sent,
                      "Total number of messages sent")
        .def_readonly("messages_failed",
                      &atom::connection::ServerStats::messages_failed,
                      "Total number of messages that failed to send")
        .def_readonly("messages_queued",
                      &atom::connection::ServerStats::messages_queued,
                      "Current number of messages in queue")
        .def_readonly("total_bytes_sent",
                      &atom::connection::ServerStats::total_bytes_sent,
                      "Total bytes sent")
        .def_readonly("server_uptime",
                      &atom::connection::ServerStats::server_uptime,
                      "Server uptime in milliseconds")
        .def_readonly("last_message_time",
                      &atom::connection::ServerStats::last_message_time,
                      "Time of last message sent")
        .def_readonly("reconnection_count",
                      &atom::connection::ServerStats::reconnection_count,
                      "Number of reconnections performed");
}

/**
 * @brief Binds the ServerConfig struct to Python.
 *
 * This function creates Python bindings for the ServerConfig struct which
 * provides configuration options for the FIFO server.
 *
 * @param m The pybind11 module to bind to
 */
void bindServerConfig(py::module_& m) {
    py::class_<atom::connection::ServerConfig>(
        m, "ServerConfig",
        R"(Configuration options for the FIFO server.

This structure provides various settings to control server behavior,
including queue limits, compression, encryption, and reconnection settings.

Examples:
    >>> from atom.connection.sync_fifoserver import ServerConfig, LogLevel
    >>> config = ServerConfig()
    >>> config.max_queue_size = 2000
    >>> config.enable_compression = True
    >>> config.log_level = LogLevel.Debug
    >>> server = FIFOServer("/tmp/myfifo", config)
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("max_queue_size",
                       &atom::connection::ServerConfig::max_queue_size,
                       "Maximum number of messages in queue")
        .def_readwrite("max_message_size",
                       &atom::connection::ServerConfig::max_message_size,
                       "Maximum size of a single message in bytes")
        .def_readwrite("enable_compression",
                       &atom::connection::ServerConfig::enable_compression,
                       "Enable message compression")
        .def_readwrite("enable_encryption",
                       &atom::connection::ServerConfig::enable_encryption,
                       "Enable message encryption")
        .def_readwrite("auto_reconnect",
                       &atom::connection::ServerConfig::auto_reconnect,
                       "Enable automatic reconnection")
        .def_readwrite("max_reconnect_attempts",
                       &atom::connection::ServerConfig::max_reconnect_attempts,
                       "Maximum number of reconnection attempts")
        .def_readwrite("reconnect_delay",
                       &atom::connection::ServerConfig::reconnect_delay,
                       "Delay between reconnection attempts")
        .def_readwrite("log_level", &atom::connection::ServerConfig::log_level,
                       "Server logging level")
        .def_readwrite("flush_on_stop",
                       &atom::connection::ServerConfig::flush_on_stop,
                       "Flush remaining messages when stopping")
        .def_readwrite("message_ttl",
                       &atom::connection::ServerConfig::message_ttl,
                       "Message time-to-live (optional)");
}

PYBIND11_MODULE(sync_fifoserver, m) {
    m.doc() = R"(Synchronous FIFO server module for the atom package.

This module provides synchronous FIFO (First In, First Out) server functionality
for inter-process communication using named pipes with comprehensive message handling,
priority queuing, and server management features.

Key Features:
- Named pipe (FIFO) based communication
- Message priority queuing system
- Asynchronous message sending
- Comprehensive server statistics
- Configurable compression and encryption
- Automatic reconnection capabilities
- Callback-based event handling
- Resource management with RAII

Classes:
- FIFOServer: Main FIFO server class
- ServerConfig: Configuration for server behavior
- ServerStats: Server operation statistics
- LogLevel: Enumeration of logging levels
- MessagePriority: Enumeration of message priorities

Quick Start Example:
    >>> from atom.connection.sync_fifoserver import FIFOServer, ServerConfig, MessagePriority
    >>>
    >>> # Create server with custom configuration
    >>> config = ServerConfig()
    >>> config.max_queue_size = 2000
    >>> config.enable_compression = True
    >>> server = FIFOServer("/tmp/myfifo", config)
    >>>
    >>> # Set up callbacks
    >>> def on_message_sent(message, success):
    ...     if success:
    ...         print(f"Message sent: {message}")
    ...     else:
    ...         print(f"Failed to send: {message}")
    >>>
    >>> def on_status_change(running):
    ...     print(f"Server {'started' if running else 'stopped'}")
    >>>
    >>> server.register_message_callback(on_message_sent)
    >>> server.register_status_callback(on_status_change)
    >>>
    >>> # Start server and send messages
    >>> server.start()
    >>> server.send_message("Hello, FIFO!", MessagePriority.High)
    >>> server.send_message("Normal message")
    >>>
    >>> # Check statistics
    >>> stats = server.get_statistics()
    >>> print(f"Messages sent: {stats.messages_sent}")
    >>>
    >>> server.stop()
)";

    // Bind enums and data structures
    bindLogLevel(m);
    bindMessagePriority(m);
    bindServerStats(m);
    bindServerConfig(m);

    // Bind the main FIFOServer class
    py::class_<atom::connection::FIFOServer>(
        m, "FIFOServer",
        R"(Synchronous FIFO server for inter-process communication.

This class provides methods for creating and managing a FIFO (named pipe) server
with message queuing, priority handling, and comprehensive configuration options.

Examples:
    >>> from atom.connection.sync_fifoserver import FIFOServer, ServerConfig, MessagePriority
    >>>
    >>> # Create server with default configuration
    >>> server = FIFOServer("/tmp/myfifo")
    >>>
    >>> # Or with custom configuration
    >>> config = ServerConfig()
    >>> config.max_queue_size = 2000
    >>> config.enable_compression = True
    >>> server = FIFOServer("/tmp/myfifo", config)
    >>>
    >>> # Start server and send messages
    >>> server.start()
    >>> server.send_message("Hello, FIFO!")
    >>> server.send_message("Priority message", MessagePriority.High)
    >>> server.stop()
)")
        .def(py::init<std::string_view>(), py::arg("fifo_path"),
             R"(Constructs a FIFOServer with default configuration.

Args:
    fifo_path: Path to the FIFO pipe

Raises:
    ValueError: If fifo_path is empty
    RuntimeError: If FIFO creation fails

Examples:
    >>> server = FIFOServer("/tmp/myfifo")
)")
        .def(
            py::init<std::string_view, const atom::connection::ServerConfig&>(),
            py::arg("fifo_path"), py::arg("config"),
            R"(Constructs a FIFOServer with custom configuration.

Args:
    fifo_path: Path to the FIFO pipe
    config: Custom server configuration

Raises:
    ValueError: If fifo_path is empty
    RuntimeError: If FIFO creation fails

Examples:
    >>> config = ServerConfig()
    >>> config.max_queue_size = 2000
    >>> server = FIFOServer("/tmp/myfifo", config)
)")
        .def("send_message",
             static_cast<bool (atom::connection::FIFOServer::*)(std::string)>(
                 &atom::connection::FIFOServer::sendMessage),
             py::arg("message"),
             R"(Sends a message through the FIFO pipe.

Args:
    message: The message to be sent

Returns:
    True if message was queued successfully, False otherwise

Examples:
    >>> success = server.send_message("Hello, FIFO!")
    >>> if success:
    ...     print("Message queued successfully")
)")
        .def("send_message",
             static_cast<bool (atom::connection::FIFOServer::*)(
                 std::string, atom::connection::MessagePriority)>(
                 &atom::connection::FIFOServer::sendMessage),
             py::arg("message"), py::arg("priority"),
             R"(Sends a message with specified priority.

Args:
    message: The message to be sent
    priority: The priority level for the message

Returns:
    True if message was queued successfully, False otherwise

Examples:
    >>> success = server.send_message("Urgent!", MessagePriority.High)
    >>> if success:
    ...     print("Priority message queued")
)")
        .def("send_message_async",
             static_cast<std::future<bool> (atom::connection::FIFOServer::*)(
                 std::string)>(&atom::connection::FIFOServer::sendMessageAsync),
             py::arg("message"),
             R"(Sends a message asynchronously.

Args:
    message: The message to be sent

Returns:
    Future that will contain the result of the send operation

Examples:
    >>> future = server.send_message_async("Async message")
    >>> # Do other work...
    >>> success = future.get()  # Wait for completion
)")
        .def("send_message_async",
             static_cast<std::future<bool> (atom::connection::FIFOServer::*)(
                 std::string, atom::connection::MessagePriority)>(
                 &atom::connection::FIFOServer::sendMessageAsync),
             py::arg("message"), py::arg("priority"),
             R"(Sends a message asynchronously with specified priority.

Args:
    message: The message to be sent
    priority: The priority level for the message

Returns:
    Future that will contain the result of the send operation

Examples:
    >>> future = server.send_message_async("Urgent async!", MessagePriority.Critical)
    >>> success = future.get()
)")
        .def(
            "send_messages",
            [](atom::connection::FIFOServer& self,
               const std::vector<std::string>& messages) {
                return self.sendMessages(messages);
            },
            py::arg("messages"),
            R"(Sends multiple messages from a list.

Args:
    messages: List of messages to send

Returns:
    Number of messages successfully queued

Examples:
    >>> messages = ["Message 1", "Message 2", "Message 3"]
    >>> count = server.send_messages(messages)
    >>> print(f"Queued {count} messages")
)")
        .def(
            "send_messages",
            [](atom::connection::FIFOServer& self,
               const std::vector<std::string>& messages,
               atom::connection::MessagePriority priority) {
                return self.sendMessages(messages, priority);
            },
            py::arg("messages"), py::arg("priority"),
            R"(Sends multiple messages with the same priority.

Args:
    messages: List of messages to send
    priority: Priority level for all messages

Returns:
    Number of messages successfully queued

Examples:
    >>> messages = ["Urgent 1", "Urgent 2", "Urgent 3"]
    >>> count = server.send_messages(messages, MessagePriority.High)
    >>> print(f"Queued {count} high-priority messages")
)")
        .def("register_message_callback",
             &atom::connection::FIFOServer::registerMessageCallback,
             py::arg("callback"),
             R"(Registers a callback for message delivery status.

Args:
    callback: Function to call when a message delivery status changes

Returns:
    A unique identifier for the callback registration

Examples:
    >>> def on_message(msg, success):
    ...     print(f"Message '{msg}' {'sent' if success else 'failed'}")
    >>> callback_id = server.register_message_callback(on_message)
)")
        .def("unregister_message_callback",
             &atom::connection::FIFOServer::unregisterMessageCallback,
             py::arg("id"),
             R"(Unregisters a previously registered message callback.

Args:
    id: The identifier returned by register_message_callback

Returns:
    True if callback was successfully unregistered

Examples:
    >>> success = server.unregister_message_callback(callback_id)
)")
        .def("register_status_callback",
             &atom::connection::FIFOServer::registerStatusCallback,
             py::arg("callback"),
             R"(Registers a callback for server status changes.

Args:
    callback: Function to call when server status changes

Returns:
    A unique identifier for the callback registration

Examples:
    >>> def on_status(running):
    ...     print(f"Server {'started' if running else 'stopped'}")
    >>> status_id = server.register_status_callback(on_status)
)")
        .def("unregister_status_callback",
             &atom::connection::FIFOServer::unregisterStatusCallback,
             py::arg("id"),
             R"(Unregisters a previously registered status callback.

Args:
    id: The identifier returned by register_status_callback

Returns:
    True if callback was successfully unregistered

Examples:
    >>> success = server.unregister_status_callback(status_id)
)")
        .def("start", &atom::connection::FIFOServer::start,
             R"(Starts the server.

Raises:
    RuntimeError: If server fails to start

Examples:
    >>> server.start()
)")
        .def("stop", &atom::connection::FIFOServer::stop,
             py::arg("flush_queue") = true,
             R"(Stops the server.

Args:
    flush_queue: If True, processes remaining messages before stopping

Examples:
    >>> server.stop()  # Stop with flush
    >>> server.stop(False)  # Stop immediately
)")
        .def("clear_queue", &atom::connection::FIFOServer::clearQueue,
             R"(Clears all pending messages from the queue.

Returns:
    Number of messages cleared

Examples:
    >>> cleared = server.clear_queue()
    >>> print(f"Cleared {cleared} messages")
)")
        .def("is_running", &atom::connection::FIFOServer::isRunning,
             R"(Checks if the server is running.

Returns:
    True if the server is running, False otherwise

Examples:
    >>> if server.is_running():
    ...     print("Server is active")
)")
        .def("get_fifo_path", &atom::connection::FIFOServer::getFifoPath,
             R"(Gets the path of the FIFO pipe.

Returns:
    The FIFO path as a string

Examples:
    >>> path = server.get_fifo_path()
    >>> print(f"FIFO path: {path}")
)")
        .def("get_config", &atom::connection::FIFOServer::getConfig,
             R"(Gets the current configuration.

Returns:
    The current server configuration

Examples:
    >>> config = server.get_config()
    >>> print(f"Max queue size: {config.max_queue_size}")
)")
        .def("update_config", &atom::connection::FIFOServer::updateConfig,
             py::arg("config"),
             R"(Updates the server configuration.

Args:
    config: New configuration settings

Returns:
    True if configuration was updated successfully

Examples:
    >>> new_config = ServerConfig()
    >>> new_config.max_queue_size = 5000
    >>> success = server.update_config(new_config)
)")
        .def("get_statistics", &atom::connection::FIFOServer::getStatistics,
             R"(Gets current server statistics.

Returns:
    Statistics about server operation

Examples:
    >>> stats = server.get_statistics()
    >>> print(f"Messages sent: {stats.messages_sent}")
)")
        .def("reset_statistics", &atom::connection::FIFOServer::resetStatistics,
             R"(Resets server statistics.

Examples:
    >>> server.reset_statistics()
)")
        .def("set_log_level", &atom::connection::FIFOServer::setLogLevel,
             py::arg("level"),
             R"(Sets the log level for the server.

Args:
    level: New log level

Examples:
    >>> server.set_log_level(LogLevel.Debug)
)")
        .def("get_queue_size", &atom::connection::FIFOServer::getQueueSize,
             R"(Gets the current number of messages in the queue.

Returns:
    Current queue size

Examples:
    >>> size = server.get_queue_size()
    >>> print(f"Queue has {size} messages")
)")
        .def(
            "__enter__",
            [](atom::connection::FIFOServer& self)
                -> atom::connection::FIFOServer& {
                self.start();
                return self;
            },
            "Support for context manager protocol - starts server")
        .def(
            "__exit__",
            [](atom::connection::FIFOServer& self, py::object, py::object,
               py::object) {
                if (self.isRunning()) {
                    self.stop();
                }
            },
            "Ensure server is stopped when exiting context");
}
