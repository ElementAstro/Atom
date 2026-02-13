#include "atom/extra/uv/coro.hpp"
#include "atom/extra/uv/message_bus.hpp"
#include "atom/extra/uv/subprocess.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(uv, m) {
    m.doc() =
        R"(Libuv-based asynchronous operations module for the atom package.

This module provides modern C++ interfaces for asynchronous operations using libuv,
including message bus communication, subprocess management, and coroutine support.

Features:
- High-performance message bus with topic-based routing
- Asynchronous subprocess management with callbacks
- Coroutine support for modern async/await patterns
- Back-pressure handling and flow control
- Cross-platform process management
- Event-driven architecture
- TCP/UDP networking with coroutines
- Asynchronous file I/O
- HTTP client support

Examples:
    >>> from atom.extra import uv
    >>>
    >>> # Create message bus
    >>> bus = uv.MessageBus()
    >>>
    >>> # Subscribe to messages
    >>> def handle_message(payload):
    ...     print(f"Received: {payload}")
    >>>
    >>> subscription = bus.subscribe("events.*", handle_message)
    >>>
    >>> # Publish messages
    >>> bus.publish("events.user", "User logged in")
    >>>
    >>> # Subprocess management
    >>> process = uv.UvProcess()
    >>> process.spawn("echo", ["Hello, World!"])
    >>> process.wait_for_exit()
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const uv_coro::UvError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // UvError exception
    py::register_exception<uv_coro::UvError>(m, "UvError", PyExc_RuntimeError);

    // ===== UV Coro Module Bindings =====

    // Scheduler class
    py::class_<uv_coro::Scheduler>(
        m, "Scheduler",
        R"(Controls the libuv event loop for coroutines.

The Scheduler manages the libuv event loop and allows scheduling
of coroutine tasks.

Examples:
    >>> scheduler = uv.Scheduler()
    >>> scheduler.run()  # Run until all tasks complete
    >>> scheduler.stop()  # Stop the event loop
)")
        .def(py::init<>(), "Create a scheduler with the default event loop")
        .def("run", &uv_coro::Scheduler::run,
             "Run the event loop until there are no more active handles")
        .def("run_once", &uv_coro::Scheduler::run_once,
             "Run the event loop once")
        .def("stop", &uv_coro::Scheduler::stop, "Stop the event loop");

    // TcpClient class
    py::class_<uv_coro::TcpClient>(
        m, "TcpClient",
        R"(High-level TCP client with coroutine-based interface.

Provides asynchronous TCP client operations using coroutines.

Examples:
    >>> client = uv.TcpClient()
    >>> # Note: Actual usage requires C++ coroutine support
)")
        .def(py::init<uv_loop_t*>(), py::arg("loop") = nullptr,
             "Create a TCP client with optional custom event loop")
        .def("close", &uv_coro::TcpClient::close, "Close the TCP connection")
        .def("get_handle", &uv_coro::TcpClient::get_handle,
             py::return_value_policy::reference,
             "Get the underlying libuv TCP handle");

    // FileSystem class
    py::class_<uv_coro::FileSystem>(
        m, "FileSystem",
        R"(High-level file system operations with coroutine-based interface.

Provides asynchronous file I/O operations using coroutines.

Examples:
    >>> fs = uv.FileSystem()
    >>> # Note: Actual usage requires C++ coroutine support
)")
        .def(py::init<uv_loop_t*>(), py::arg("loop") = nullptr,
             "Create a FileSystem instance with optional custom event loop");

    // HttpResponse struct
    py::class_<uv_coro::HttpClient::HttpResponse>(
        m, "HttpResponse",
        R"(HTTP response containing status, headers, and body.

Represents the response from an HTTP request.)")
        .def(py::init<>(), "Create an empty HTTP response")
        .def_readwrite("status_code",
                       &uv_coro::HttpClient::HttpResponse::status_code,
                       "HTTP status code")
        .def_readwrite("headers", &uv_coro::HttpClient::HttpResponse::headers,
                       "Response headers as key-value pairs")
        .def_readwrite("body", &uv_coro::HttpClient::HttpResponse::body,
                       "Response body as string");

    // HttpClient class
    py::class_<uv_coro::HttpClient>(m, "HttpClient",
                                    R"(Simple HTTP client built using TcpClient.

Provides basic HTTP GET functionality using coroutines.

Examples:
    >>> client = uv.HttpClient()
    >>> # Note: Actual usage requires C++ coroutine support
)")
        .def(py::init<uv_loop_t*>(), py::arg("loop") = nullptr,
             "Create an HTTP client with optional custom event loop");

    // ProcessOptions struct for coroutines
    py::class_<uv_coro::ProcessOptions>(
        m, "ProcessOptions",
        R"(Options for spawning processes in coroutines.

Configuration for process spawning operations.)")
        .def(py::init<>(), "Create default process options")
        .def_readwrite("file", &uv_coro::ProcessOptions::file,
                       "Path to the executable file")
        .def_readwrite("args", &uv_coro::ProcessOptions::args,
                       "Command line arguments");

    // ProcessResult struct
    py::class_<uv_coro::ProcessResult>(m, "ProcessResult",
                                       R"(Result of a process execution.

Contains exit code and captured output from a process.)")
        .def(py::init<>(), "Create an empty process result")
        .def_readwrite("exit_code", &uv_coro::ProcessResult::exit_code,
                       "Process exit code")
        .def_readwrite("stdout_data", &uv_coro::ProcessResult::stdout_data,
                       "Captured stdout data")
        .def_readwrite("stderr_data", &uv_coro::ProcessResult::stderr_data,
                       "Captured stderr data");

    // UdpSendResult struct
    py::class_<uv_coro::UdpSendResult>(m, "UdpSendResult",
                                       R"(Result of a UDP send operation.)")
        .def(py::init<>(), "Create an empty UDP send result")
        .def_readwrite("bytes_sent", &uv_coro::UdpSendResult::bytes_sent,
                       "Number of bytes sent");

    // UdpReceiveResult struct
    py::class_<uv_coro::UdpReceiveResult>(
        m, "UdpReceiveResult", R"(Result of a UDP receive operation.)")
        .def(py::init<>(), "Create an empty UDP receive result")
        .def_readwrite("data", &uv_coro::UdpReceiveResult::data,
                       "Received data")
        .def_readwrite("from_ip", &uv_coro::UdpReceiveResult::from_ip,
                       "Source IP address")
        .def_readwrite("from_port", &uv_coro::UdpReceiveResult::from_port,
                       "Source port number");

    // Convenience functions
    m.def("get_scheduler", &uv_coro::get_scheduler,
          py::return_value_policy::reference,
          "Get the global scheduler instance");

    m.def("make_tcp_client", &uv_coro::make_tcp_client,
          "Create a TCP client using the global scheduler");

    m.def("make_http_client", &uv_coro::make_http_client,
          "Create an HTTP client using the global scheduler");

    m.def("make_file_system", &uv_coro::make_file_system,
          "Create a FileSystem instance using the global scheduler");

    // ===== Message Bus Module Bindings =====

    // MessageBusError enum
    py::enum_<msgbus::MessageBusError>(
        m, "MessageBusError",
        R"(Error types for message bus operations.

Represents various error conditions that can occur in the message bus.)")
        .value("InvalidTopic", msgbus::MessageBusError::InvalidTopic,
               "The topic is invalid")
        .value("HandlerNotFound", msgbus::MessageBusError::HandlerNotFound,
               "No handler found for the topic")
        .value("QueueFull", msgbus::MessageBusError::QueueFull,
               "Message queue is full")
        .value("SerializationError",
               msgbus::MessageBusError::SerializationError,
               "Error serializing or deserializing message")
        .value("NetworkError", msgbus::MessageBusError::NetworkError,
               "Network error occurred")
        .value("ShutdownInProgress",
               msgbus::MessageBusError::ShutdownInProgress,
               "Message bus is shutting down")
        .export_values();

    // BackPressureConfig struct
    py::class_<msgbus::BackPressureConfig>(
        m, "BackPressureConfig",
        R"(Configuration for message bus back-pressure handling.

This struct defines how the message bus handles situations where
message production exceeds consumption capacity.

Examples:
    >>> config = uv.BackPressureConfig()
    >>> config.max_queue_size = 5000
    >>> config.timeout = 500  # milliseconds
    >>> config.drop_oldest = False
    >>> bus = uv.MessageBus(config)
)")
        .def(py::init<>(), "Create default back-pressure configuration")
        .def_readwrite(
            "max_queue_size", &msgbus::BackPressureConfig::max_queue_size,
            "Maximum number of messages in queue before back-pressure kicks in")
        .def_readwrite("timeout", &msgbus::BackPressureConfig::timeout,
                       "Timeout for message delivery in milliseconds")
        .def_readwrite("drop_oldest", &msgbus::BackPressureConfig::drop_oldest,
                       "Whether to drop oldest messages when queue is full");

    // MessageEnvelope template for string messages (most common case)
    py::class_<msgbus::MessageEnvelope<std::string>>(
        m, "MessageEnvelope",
        R"(Envelope containing a message and metadata.

This class wraps messages with additional metadata such as topic,
timestamp, sender ID, and custom metadata.

Examples:
    >>> # Usually created automatically by MessageBus
    >>> def handler(envelope):
    ...     print(f"Topic: {envelope.topic}")
    ...     print(f"Payload: {envelope.payload}")
    ...     print(f"Sender: {envelope.sender_id}")
    ...     print(f"Time: {envelope.timestamp}")
)")
        .def(py::init<std::string, std::string, std::string>(),
             py::arg("topic"), py::arg("payload"), py::arg("sender_id") = "",
             "Create a message envelope")
        .def_readwrite("topic", &msgbus::MessageEnvelope<std::string>::topic,
                       "Message topic")
        .def_readwrite("payload",
                       &msgbus::MessageEnvelope<std::string>::payload,
                       "Message payload")
        .def_readwrite("timestamp",
                       &msgbus::MessageEnvelope<std::string>::timestamp,
                       "Message timestamp")
        .def_readwrite("sender_id",
                       &msgbus::MessageEnvelope<std::string>::sender_id,
                       "Sender identifier")
        .def_readwrite("message_id",
                       &msgbus::MessageEnvelope<std::string>::message_id,
                       "Unique message identifier")
        .def_readwrite("metadata",
                       &msgbus::MessageEnvelope<std::string>::metadata,
                       "Additional metadata key-value pairs");

    // MessageBus::QueueStats
    py::class_<msgbus::MessageBus::QueueStats>(
        m, "QueueStats",
        R"(Statistics about message bus queue performance.

This struct provides insights into the message bus performance
and queue status.)")
        .def_readwrite("pending_messages",
                       &msgbus::MessageBus::QueueStats::pending_messages,
                       "Number of messages waiting to be processed")
        .def_readwrite("max_queue_size",
                       &msgbus::MessageBus::QueueStats::max_queue_size,
                       "Maximum queue size")
        .def_readwrite("total_handlers",
                       &msgbus::MessageBus::QueueStats::total_handlers,
                       "Total number of registered handlers")
        .def_readwrite("avg_delivery_time",
                       &msgbus::MessageBus::QueueStats::avg_delivery_time,
                       "Average message delivery time");

    // HandlerRegistration (opaque handle)
    py::class_<msgbus::HandlerRegistration>(
        m, "HandlerRegistration",
        R"(Registration handle for a message handler.

This object represents a subscription to a topic. When destroyed,
the handler is automatically unsubscribed.)")
        .def_readonly("id", &msgbus::HandlerRegistration::id,
                      "Unique handler ID")
        .def_readonly("topic_pattern",
                      &msgbus::HandlerRegistration::topic_pattern,
                      "Topic pattern this handler is subscribed to");

    // MessageBus class
    py::class_<msgbus::MessageBus>(
        m, "MessageBus",
        R"(High-performance message bus for inter-component communication.

This class provides a topic-based message routing system with support
for pattern matching, filtering, and back-pressure handling.

Examples:
    >>> bus = uv.MessageBus()
    >>>
    >>> # Subscribe to messages
    >>> def on_user_event(payload):
    ...     print(f"User event: {payload}")
    >>>
    >>> subscription = bus.subscribe("user.*", on_user_event)
    >>>
    >>> # Publish messages
    >>> bus.publish("user.login", "User john_doe logged in")
    >>> bus.publish("user.logout", "User john_doe logged out")
    >>>
    >>> # Process messages
    >>> bus.process_messages()
    >>>
    >>> # Clean up
    >>> del subscription  # Automatically unsubscribes
)")
        .def(py::init<const msgbus::BackPressureConfig&>(),
             py::arg("config") = msgbus::BackPressureConfig{},
             "Create a message bus with optional back-pressure configuration")
        .def(
            "subscribe",
            [](msgbus::MessageBus& self, const std::string& topic_pattern,
               py::function handler) {
                // Create a C++ handler that calls the Python function
                auto cpp_handler = [handler](const std::string& payload) {
                    py::gil_scoped_acquire acquire;
                    handler(payload);
                };
                return self.subscribe<std::string>(topic_pattern, cpp_handler);
            },
            py::arg("topic_pattern"), py::arg("handler"),
            py::keep_alive<0, 2>(),  // Keep handler alive as long as
                                     // subscription exists
            R"(Subscribe to messages matching a topic pattern.

Args:
    topic_pattern: Topic pattern (supports wildcards like "user.*")
    handler: Callback function that receives message payloads

Returns:
    Subscription handle that automatically unsubscribes when destroyed
)")
        .def(
            "publish",
            [](msgbus::MessageBus& self, const std::string& topic,
               const std::string& message, const std::string& sender_id) {
                auto result =
                    self.publish(topic, std::string(message), sender_id);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to publish message");
                }
            },
            py::arg("topic"), py::arg("message"), py::arg("sender_id") = "",
            R"(Publish a message to a topic.

Args:
    topic: The topic to publish to
    message: The message payload
    sender_id: Optional sender identifier

Raises:
    RuntimeError: If publishing fails
)")
        .def("get_stats", &msgbus::MessageBus::get_stats,
             R"(Get message bus statistics.

Returns:
    QueueStats object with performance metrics
)")
        .def("shutdown", &msgbus::MessageBus::shutdown,
             R"(Shutdown the message bus and clean up resources.)")
        .def("process_messages", &msgbus::MessageBus::process_messages,
             R"(Process pending messages synchronously.

This method processes all messages currently in the queue.)")
        .def_static("get_instance", &msgbus::MessageBus::get_instance,
                    py::return_value_policy::reference,
                    R"(Get the singleton message bus instance.

Returns:
    The global MessageBus instance
)");

    // ===== Subprocess Module Bindings =====

    // ProcessStatus enum
    py::enum_<UvProcess::ProcessStatus>(m, "ProcessStatus",
                                        R"(Status of a subprocess.

Represents the current state of a subprocess in its lifecycle.)")
        .value("IDLE", UvProcess::ProcessStatus::IDLE,
               "Process has not been started")
        .value("RUNNING", UvProcess::ProcessStatus::RUNNING,
               "Process is currently running")
        .value("EXITED", UvProcess::ProcessStatus::EXITED,
               "Process has exited normally")
        .value("TERMINATED", UvProcess::ProcessStatus::TERMINATED,
               "Process was terminated by signal")
        .value("TIMED_OUT", UvProcess::ProcessStatus::TIMED_OUT,
               "Process timed out")
        .value("ERROR", UvProcess::ProcessStatus::ERROR,
               "Process encountered an error")
        .export_values();

    // UvProcess::ProcessOptions struct
    py::class_<UvProcess::ProcessOptions>(m, "UvProcessOptions",
                                          R"(Options for subprocess creation.

This struct contains various options that control how subprocesses
are created and managed.

Examples:
    >>> options = uv.UvProcessOptions()
    >>> options.file = "/bin/echo"
    >>> options.args = ["Hello", "World"]
    >>> options.cwd = "/tmp"
    >>> options.timeout = 5000  # milliseconds
    >>> options.detached = False
)")
        .def(py::init<>(), "Create default process options")
        .def_readwrite("file", &UvProcess::ProcessOptions::file,
                       "Path to the executable file")
        .def_readwrite("args", &UvProcess::ProcessOptions::args,
                       "Command line arguments")
        .def_readwrite("cwd", &UvProcess::ProcessOptions::cwd,
                       "Working directory for the process")
        .def_readwrite("env", &UvProcess::ProcessOptions::env,
                       "Environment variables (key-value pairs)")
        .def_readwrite("detached", &UvProcess::ProcessOptions::detached,
                       "Whether to run the process detached")
        .def_readwrite("timeout", &UvProcess::ProcessOptions::timeout,
                       "Timeout in milliseconds (0 = no timeout)")
        .def_readwrite("redirect_stderr_to_stdout",
                       &UvProcess::ProcessOptions::redirect_stderr_to_stdout,
                       "Whether to redirect stderr to stdout")
        .def_readwrite("inherit_parent_env",
                       &UvProcess::ProcessOptions::inherit_parent_env,
                       "Whether to inherit parent environment variables")
        .def_readwrite("stdio_count", &UvProcess::ProcessOptions::stdio_count,
                       "Number of stdio file descriptors");

    // UvProcess class
    py::class_<UvProcess>(m, "UvProcess",
                          R"(Asynchronous subprocess management using libuv.

This class provides a modern interface for creating and managing
subprocesses with asynchronous I/O and callback support.

Examples:
    >>> process = uv.UvProcess()
    >>>
    >>> # Simple spawn
    >>> success = process.spawn("echo", ["Hello, World!"])
    >>> if success:
    ...     process.wait_for_exit()
    ...     print(f"Exit code: {process.get_exit_code()}")
    >>>
    >>> # Spawn with callbacks
    >>> def on_stdout(data, size):
    ...     print(f"STDOUT: {data}")
    >>>
    >>> def on_exit(exit_code, signal):
    ...     print(f"Process exited with code {exit_code}")
    >>>
    >>> process.spawn("ls", ["-la"], stdout_callback=on_stdout, exit_callback=on_exit)
)")
        .def(py::init<>(),
             "Create a new process manager with default event loop")
        .def(py::init<uv_loop_t*>(), py::arg("loop"),
             "Create a new process manager with custom event loop")
        .def(
            "spawn",
            [](UvProcess& self, const std::string& file,
               const std::vector<std::string>& args, const std::string& cwd,
               py::object exit_callback, py::object stdout_callback,
               py::object stderr_callback) {
                // Convert Python callbacks to C++ callbacks
                UvProcess::ExitCallback exit_cb = nullptr;
                UvProcess::DataCallback stdout_cb = nullptr;
                UvProcess::DataCallback stderr_cb = nullptr;

                if (!exit_callback.is_none()) {
                    exit_cb = [exit_callback](int64_t exit_code, int signal) {
                        py::gil_scoped_acquire acquire;
                        exit_callback(exit_code, signal);
                    };
                }

                if (!stdout_callback.is_none()) {
                    stdout_cb = [stdout_callback](const char* data,
                                                  ssize_t size) {
                        py::gil_scoped_acquire acquire;
                        stdout_callback(py::bytes(data, size), size);
                    };
                }

                if (!stderr_callback.is_none()) {
                    stderr_cb = [stderr_callback](const char* data,
                                                  ssize_t size) {
                        py::gil_scoped_acquire acquire;
                        stderr_callback(py::bytes(data, size), size);
                    };
                }

                return self.spawn(file, args, cwd, exit_cb, stdout_cb,
                                  stderr_cb);
            },
            py::arg("file"), py::arg("args"), py::arg("cwd") = "",
            py::arg("exit_callback") = py::none(),
            py::arg("stdout_callback") = py::none(),
            py::arg("stderr_callback") = py::none(),
            R"(Spawn a subprocess with basic options.

Args:
    file: Path to the executable
    args: Command line arguments
    cwd: Working directory (optional)
    exit_callback: Callback for process exit (optional)
    stdout_callback: Callback for stdout data (optional)
    stderr_callback: Callback for stderr data (optional)

Returns:
    True if the process was spawned successfully
)")
        .def(
            "spawn_with_options",
            [](UvProcess& self, const UvProcess::ProcessOptions& options,
               py::object exit_callback, py::object stdout_callback,
               py::object stderr_callback, py::object timeout_callback,
               py::object error_callback) {
                // Convert Python callbacks to C++ callbacks
                UvProcess::ExitCallback exit_cb = nullptr;
                UvProcess::DataCallback stdout_cb = nullptr;
                UvProcess::DataCallback stderr_cb = nullptr;
                UvProcess::TimeoutCallback timeout_cb = nullptr;
                UvProcess::ErrorCallback error_cb = nullptr;

                if (!exit_callback.is_none()) {
                    exit_cb = [exit_callback](int64_t exit_code, int signal) {
                        py::gil_scoped_acquire acquire;
                        exit_callback(exit_code, signal);
                    };
                }

                if (!stdout_callback.is_none()) {
                    stdout_cb = [stdout_callback](const char* data,
                                                  ssize_t size) {
                        py::gil_scoped_acquire acquire;
                        stdout_callback(py::bytes(data, size), size);
                    };
                }

                if (!stderr_callback.is_none()) {
                    stderr_cb = [stderr_callback](const char* data,
                                                  ssize_t size) {
                        py::gil_scoped_acquire acquire;
                        stderr_callback(py::bytes(data, size), size);
                    };
                }

                if (!timeout_callback.is_none()) {
                    timeout_cb = [timeout_callback]() {
                        py::gil_scoped_acquire acquire;
                        timeout_callback();
                    };
                }

                if (!error_callback.is_none()) {
                    error_cb = [error_callback](const std::string& error_msg) {
                        py::gil_scoped_acquire acquire;
                        error_callback(error_msg);
                    };
                }

                return self.spawnWithOptions(options, exit_cb, stdout_cb,
                                             stderr_cb, timeout_cb, error_cb);
            },
            py::arg("options"), py::arg("exit_callback") = py::none(),
            py::arg("stdout_callback") = py::none(),
            py::arg("stderr_callback") = py::none(),
            py::arg("timeout_callback") = py::none(),
            py::arg("error_callback") = py::none(),
            R"(Spawn a subprocess with advanced options.

Args:
    options: ProcessOptions object with configuration
    exit_callback: Callback for process exit (optional)
    stdout_callback: Callback for stdout data (optional)
    stderr_callback: Callback for stderr data (optional)
    timeout_callback: Callback for timeout (optional)
    error_callback: Callback for errors (optional)

Returns:
    True if the process was spawned successfully
)")
        .def("write_to_stdin", &UvProcess::writeToStdin, py::arg("data"),
             R"(Write data to the process stdin.

Args:
    data: Data to write

Returns:
    True if the data was written successfully
)")
        .def("close_stdin", &UvProcess::closeStdin,
             R"(Close the process stdin pipe.)")
        .def("kill", &UvProcess::kill, py::arg("signum") = SIGTERM,
             R"(Send a signal to the process.

Args:
    signum: Signal number (default: SIGTERM)

Returns:
    True if the signal was sent successfully
)")
        .def("kill_forcefully", &UvProcess::killForcefully,
             R"(Kill the process with SIGKILL.

Returns:
    True if the process was killed successfully
)")
        .def("is_running", &UvProcess::isRunning,
             R"(Check if the process is currently running.

Returns:
    True if the process is running
)")
        .def("get_pid", &UvProcess::getPid,
             R"(Get the process ID.

Returns:
    Process ID, or -1 if not running
)")
        .def("get_status", &UvProcess::getStatus,
             R"(Get the current process status.

Returns:
    ProcessStatus enum value
)")
        .def("get_exit_code", &UvProcess::getExitCode,
             R"(Get the process exit code.

Returns:
    Exit code, or -1 if process hasn't exited
)")
        .def("wait_for_exit", &UvProcess::waitForExit,
             py::arg("timeout_ms") = 0,
             R"(Wait for the process to exit.

Args:
    timeout_ms: Timeout in milliseconds (0 = wait forever)

Returns:
    True if the process exited, false on timeout
)")
        .def("reset", &UvProcess::reset,
             R"(Reset the process object to allow reuse.)")
        .def(
            "set_error_callback",
            [](UvProcess& self, py::object error_callback) {
                if (!error_callback.is_none()) {
                    auto error_cb =
                        [error_callback](const std::string& error_msg) {
                            py::gil_scoped_acquire acquire;
                            error_callback(error_msg);
                        };
                    self.setErrorCallback(error_cb);
                }
            },
            py::arg("error_callback"),
            R"(Set custom error handler.

Args:
    error_callback: Callback function for errors
)");
}
