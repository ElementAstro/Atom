#include "atom/extra/uv/message_bus.hpp"
#include "atom/extra/uv/subprocess.hpp"
#include "atom/extra/uv/coro.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(uv, m) {
    m.doc() = R"(Libuv-based asynchronous operations module for the atom package.

This module provides modern C++ interfaces for asynchronous operations using libuv,
including message bus communication, subprocess management, and coroutine support.

Features:
- High-performance message bus with topic-based routing
- Asynchronous subprocess management with callbacks
- Coroutine support for modern async/await patterns
- Back-pressure handling and flow control
- Cross-platform process management
- Event-driven architecture

Examples:
    >>> from atom.extra.uv import uv
    >>> 
    >>> # Create message bus
    >>> bus = uv.MessageBus()
    >>> 
    >>> # Subscribe to messages
    >>> def handle_message(envelope):
    ...     print(f"Received: {envelope.payload}")
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
        } catch (const atom::extra::uv::MessageBusError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // MessageBusError exception
    py::register_exception<atom::extra::uv::MessageBusError>(m, "MessageBusError", PyExc_RuntimeError);

    // ProcessStatus enum
    py::enum_<atom::extra::uv::ProcessStatus>(m, "ProcessStatus",
                                              R"(Status of a subprocess.

Represents the current state of a subprocess in its lifecycle.)")
        .value("NOT_STARTED", atom::extra::uv::ProcessStatus::NOT_STARTED, "Process has not been started")
        .value("RUNNING", atom::extra::uv::ProcessStatus::RUNNING, "Process is currently running")
        .value("EXITED", atom::extra::uv::ProcessStatus::EXITED, "Process has exited normally")
        .value("KILLED", atom::extra::uv::ProcessStatus::KILLED, "Process was killed by signal")
        .value("ERROR", atom::extra::uv::ProcessStatus::ERROR, "Process encountered an error")
        .export_values();

    // BackPressureConfig struct
    py::class_<atom::extra::uv::BackPressureConfig>(m, "BackPressureConfig",
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
        .def_readwrite("max_queue_size", &atom::extra::uv::BackPressureConfig::max_queue_size,
                       "Maximum number of messages in queue before back-pressure kicks in")
        .def_readwrite("timeout", &atom::extra::uv::BackPressureConfig::timeout,
                       "Timeout for message delivery in milliseconds")
        .def_readwrite("drop_oldest", &atom::extra::uv::BackPressureConfig::drop_oldest,
                       "Whether to drop oldest messages when queue is full");

    // ProcessOptions struct
    py::class_<atom::extra::uv::ProcessOptions>(m, "ProcessOptions",
                                                R"(Options for subprocess creation.

This struct contains various options that control how subprocesses
are created and managed.

Examples:
    >>> options = uv.ProcessOptions()
    >>> options.file = "/bin/echo"
    >>> options.args = ["Hello", "World"]
    >>> options.cwd = "/tmp"
    >>> options.timeout_ms = 5000
    >>> options.capture_stdout = True
    >>> options.capture_stderr = True
)")
        .def(py::init<>(), "Create default process options")
        .def_readwrite("file", &atom::extra::uv::ProcessOptions::file,
                       "Path to the executable file")
        .def_readwrite("args", &atom::extra::uv::ProcessOptions::args,
                       "Command line arguments")
        .def_readwrite("cwd", &atom::extra::uv::ProcessOptions::cwd,
                       "Working directory for the process")
        .def_readwrite("env", &atom::extra::uv::ProcessOptions::env,
                       "Environment variables (key-value pairs)")
        .def_readwrite("timeout_ms", &atom::extra::uv::ProcessOptions::timeout_ms,
                       "Timeout in milliseconds (0 = no timeout)")
        .def_readwrite("capture_stdout", &atom::extra::uv::ProcessOptions::capture_stdout,
                       "Whether to capture stdout")
        .def_readwrite("capture_stderr", &atom::extra::uv::ProcessOptions::capture_stderr,
                       "Whether to capture stderr")
        .def_readwrite("detached", &atom::extra::uv::ProcessOptions::detached,
                       "Whether to run the process detached");

    // MessageEnvelope template for string messages (most common case)
    py::class_<atom::extra::uv::MessageEnvelope<std::string>>(m, "MessageEnvelope",
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
        .def_readwrite("topic", &atom::extra::uv::MessageEnvelope<std::string>::topic,
                       "Message topic")
        .def_readwrite("payload", &atom::extra::uv::MessageEnvelope<std::string>::payload,
                       "Message payload")
        .def_readwrite("timestamp", &atom::extra::uv::MessageEnvelope<std::string>::timestamp,
                       "Message timestamp")
        .def_readwrite("sender_id", &atom::extra::uv::MessageEnvelope<std::string>::sender_id,
                       "Sender identifier")
        .def_readwrite("message_id", &atom::extra::uv::MessageEnvelope<std::string>::message_id,
                       "Unique message identifier")
        .def_readwrite("metadata", &atom::extra::uv::MessageEnvelope<std::string>::metadata,
                       "Additional metadata key-value pairs");

    // MessageBus::QueueStats
    py::class_<atom::extra::uv::MessageBus::QueueStats>(m, "QueueStats",
                                                         R"(Statistics about message bus queue performance.

This struct provides insights into the message bus performance
and queue status.)")
        .def_readwrite("pending_messages", &atom::extra::uv::MessageBus::QueueStats::pending_messages,
                       "Number of messages waiting to be processed")
        .def_readwrite("max_queue_size", &atom::extra::uv::MessageBus::QueueStats::max_queue_size,
                       "Maximum queue size")
        .def_readwrite("total_handlers", &atom::extra::uv::MessageBus::QueueStats::total_handlers,
                       "Total number of registered handlers")
        .def_readwrite("avg_delivery_time", &atom::extra::uv::MessageBus::QueueStats::avg_delivery_time,
                       "Average message delivery time");

    // MessageBus class
    py::class_<atom::extra::uv::MessageBus>(m, "MessageBus",
                                            R"(High-performance message bus for inter-component communication.

This class provides a topic-based message routing system with support
for pattern matching, filtering, and back-pressure handling.

Examples:
    >>> bus = uv.MessageBus()
    >>> 
    >>> # Subscribe to messages
    >>> def on_user_event(envelope):
    ...     print(f"User event: {envelope.payload}")
    >>> 
    >>> subscription = bus.subscribe("user.*", on_user_event)
    >>> 
    >>> # Publish messages
    >>> bus.publish("user.login", "User john_doe logged in")
    >>> bus.publish("user.logout", "User john_doe logged out")
    >>> 
    >>> # Clean up
    >>> del subscription  # Automatically unsubscribes
)")
        .def(py::init<const atom::extra::uv::BackPressureConfig&>(),
             py::arg("config") = atom::extra::uv::BackPressureConfig{},
             "Create a message bus with optional back-pressure configuration")
        .def("subscribe", [](atom::extra::uv::MessageBus& self, const std::string& topic_pattern, py::function handler) {
            // Create a C++ handler that calls the Python function
            auto cpp_handler = [handler](const atom::extra::uv::MessageEnvelope<std::string>& envelope) {
                py::gil_scoped_acquire acquire;
                handler(envelope);
            };
            return self.subscribe<std::string>(topic_pattern, cpp_handler);
        }, py::arg("topic_pattern"), py::arg("handler"),
             R"(Subscribe to messages matching a topic pattern.

Args:
    topic_pattern: Topic pattern (supports wildcards like "user.*")
    handler: Callback function that receives MessageEnvelope objects

Returns:
    Subscription handle that automatically unsubscribes when destroyed
)")
        .def("publish", [](atom::extra::uv::MessageBus& self, const std::string& topic, const std::string& message, const std::string& sender_id) {
            return self.publish(topic, message, sender_id);
        }, py::arg("topic"), py::arg("message"), py::arg("sender_id") = "",
             R"(Publish a message to a topic.

Args:
    topic: The topic to publish to
    message: The message payload
    sender_id: Optional sender identifier

Returns:
    Result indicating success or failure
)")
        .def("get_stats", &atom::extra::uv::MessageBus::getStats,
             R"(Get message bus statistics.

Returns:
    QueueStats object with performance metrics
)")
        .def("shutdown", &atom::extra::uv::MessageBus::shutdown,
             R"(Shutdown the message bus and clean up resources.)");

    // UvProcess class
    py::class_<atom::extra::uv::UvProcess>(m, "UvProcess",
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
    >>> def on_stdout(data):
    ...     print(f"STDOUT: {data}")
    >>> 
    >>> def on_exit(exit_code, signal):
    ...     print(f"Process exited with code {exit_code}")
    >>> 
    >>> process.spawn("ls", ["-la"], stdout_callback=on_stdout, exit_callback=on_exit)
)")
        .def(py::init<>(), "Create a new process manager")
        .def("spawn", [](atom::extra::uv::UvProcess& self, const std::string& file, const std::vector<std::string>& args, const std::string& cwd, py::object exit_callback, py::object stdout_callback, py::object stderr_callback) {
            // Convert Python callbacks to C++ callbacks
            atom::extra::uv::ExitCallback exit_cb = nullptr;
            atom::extra::uv::DataCallback stdout_cb = nullptr;
            atom::extra::uv::DataCallback stderr_cb = nullptr;
            
            if (!exit_callback.is_none()) {
                exit_cb = [exit_callback](int exit_code, int signal) {
                    py::gil_scoped_acquire acquire;
                    exit_callback(exit_code, signal);
                };
            }
            
            if (!stdout_callback.is_none()) {
                stdout_cb = [stdout_callback](const std::string& data) {
                    py::gil_scoped_acquire acquire;
                    stdout_callback(data);
                };
            }
            
            if (!stderr_callback.is_none()) {
                stderr_cb = [stderr_callback](const std::string& data) {
                    py::gil_scoped_acquire acquire;
                    stderr_callback(data);
                };
            }
            
            return self.spawn(file, args, cwd, exit_cb, stdout_cb, stderr_cb);
        }, py::arg("file"), py::arg("args"), py::arg("cwd") = "", py::arg("exit_callback") = py::none(), py::arg("stdout_callback") = py::none(), py::arg("stderr_callback") = py::none(),
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
        .def("write_to_stdin", &atom::extra::uv::UvProcess::writeToStdin,
             py::arg("data"),
             R"(Write data to the process stdin.

Args:
    data: Data to write

Returns:
    True if the data was written successfully
)")
        .def("close_stdin", &atom::extra::uv::UvProcess::closeStdin,
             R"(Close the process stdin pipe.)")
        .def("kill", &atom::extra::uv::UvProcess::kill,
             py::arg("signum") = SIGTERM,
             R"(Send a signal to the process.

Args:
    signum: Signal number (default: SIGTERM)

Returns:
    True if the signal was sent successfully
)")
        .def("kill_forcefully", &atom::extra::uv::UvProcess::killForcefully,
             R"(Kill the process with SIGKILL.

Returns:
    True if the process was killed successfully
)")
        .def("is_running", &atom::extra::uv::UvProcess::isRunning,
             R"(Check if the process is currently running.

Returns:
    True if the process is running
)")
        .def("get_pid", &atom::extra::uv::UvProcess::getPid,
             R"(Get the process ID.

Returns:
    Process ID, or -1 if not running
)")
        .def("get_status", &atom::extra::uv::UvProcess::getStatus,
             R"(Get the current process status.

Returns:
    ProcessStatus enum value
)")
        .def("get_exit_code", &atom::extra::uv::UvProcess::getExitCode,
             R"(Get the process exit code.

Returns:
    Exit code, or -1 if process hasn't exited
)")
        .def("wait_for_exit", &atom::extra::uv::UvProcess::waitForExit,
             py::arg("timeout_ms") = 0,
             R"(Wait for the process to exit.

Args:
    timeout_ms: Timeout in milliseconds (0 = wait forever)

Returns:
    True if the process exited, false on timeout
)");
}
