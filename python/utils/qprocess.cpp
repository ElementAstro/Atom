#include "atom/utils/qprocess.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(process, m) {
    m.doc() = "Process management module for the atom package";

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

    // ProcessState enum
    py::enum_<atom::utils::QProcess::ProcessState>(
        m, "ProcessState",
        "Enum representing the possible states of a QProcess.")
        .value("NOT_RUNNING", atom::utils::QProcess::ProcessState::NotRunning,
               "The process is not running")
        .value("STARTING", atom::utils::QProcess::ProcessState::Starting,
               "The process is starting but not yet running")
        .value("RUNNING", atom::utils::QProcess::ProcessState::Running,
               "The process is running")
        .export_values();

    // ProcessError enum
    py::enum_<atom::utils::QProcess::ProcessError>(
        m, "ProcessError",
        "Enum representing the possible error states of a QProcess.")
        .value("NO_ERROR", atom::utils::QProcess::ProcessError::NoError,
               "No error occurred")
        .value("FAILED_TO_START",
               atom::utils::QProcess::ProcessError::FailedToStart,
               "Process failed to start")
        .value("CRASHED", atom::utils::QProcess::ProcessError::Crashed,
               "Process crashed after starting")
        .value("TIMEDOUT", atom::utils::QProcess::ProcessError::Timedout,
               "Process operation timed out")
        .value("READ_ERROR", atom::utils::QProcess::ProcessError::ReadError,
               "Error reading from the process")
        .value("WRITE_ERROR", atom::utils::QProcess::ProcessError::WriteError,
               "Error writing to the process")
        .value("UNKNOWN_ERROR",
               atom::utils::QProcess::ProcessError::UnknownError,
               "An unknown error occurred")
        .export_values();

    // ExitStatus enum
    py::enum_<atom::utils::QProcess::ExitStatus>(
        m, "ExitStatus", "Enum representing the exit status of a QProcess.")
        .value("NORMAL_EXIT", atom::utils::QProcess::ExitStatus::NormalExit,
               "Process exited normally")
        .value("CRASH_EXIT", atom::utils::QProcess::ExitStatus::CrashExit,
               "Process crashed")
        .export_values();

    // QProcess class binding
    py::class_<atom::utils::QProcess>(
        m, "Process",
        R"(A class to manage and interact with external processes.

This class provides methods to start and control external processes. 
It allows setting working directories, managing environment variables,
and reading from or writing to the process's standard output and error streams.

Examples:
    >>> from atom.utils import Process
    >>> # Simple synchronous execution
    >>> proc = Process()
    >>> exit_code = proc.execute("ls", ["-la"])
    >>> print(f"Process exited with code {exit_code}")
    >>>
    >>> # Asynchronous execution with callbacks
    >>> proc = Process()
    >>> proc.set_ready_read_standard_output_callback(lambda data: print(f"Output: {data}"))
    >>> proc.start("ping", ["localhost", "-c", "4"])
    >>> proc.wait_for_finished()
)")
        .def(py::init<>(), "Default constructor for Process.")

        .def("set_working_directory",
             &atom::utils::QProcess::setWorkingDirectory, py::arg("dir"),
             "Sets the working directory for the process.")
        .def("working_directory", &atom::utils::QProcess::workingDirectory,
             "Gets the current working directory for the process.")

        .def("set_environment",
             &atom::utils::QProcess::setEnvironment<std::vector<std::string>>,
             py::arg("env"), "Sets the environment variables for the process.")
        .def("environment", &atom::utils::QProcess::environment,
             "Gets the current environment variables for the process.")

        .def(
            "start", &atom::utils::QProcess::start<std::vector<std::string>>,
            py::arg("program"), py::arg("args") = std::vector<std::string>(),
            R"(Starts the external process with the given program and arguments.

Args:
    program: The path to the executable program to start.
    args: A list of arguments to pass to the program.

Raises:
    RuntimeError: If process start fails or is already running.
    ValueError: If program path is invalid.
)")
        .def("start_detached",
             &atom::utils::QProcess::startDetached<std::vector<std::string>>,
             py::arg("program"), py::arg("args") = std::vector<std::string>(),
             R"(Starts the external process in detached mode.

Args:
    program: The path to the executable program to start.
    args: A list of arguments to pass to the program.

Returns:
    bool: True if the process was started successfully, False otherwise.

In detached mode, the process will run independently of the parent process 
and will not be terminated when the parent process exits.
)")
        .def(
            "wait_for_started",
            [](atom::utils::QProcess& self, int timeout_ms) {
                return self.waitForStarted(
                    std::chrono::milliseconds(timeout_ms));
            },
            py::arg("timeout_ms") = -1,
            R"(Waits for the process to start.

Args:
    timeout_ms: The maximum time to wait in milliseconds. Negative values wait indefinitely.

Returns:
    bool: True if the process has started within the specified timeout, False otherwise.
)")
        .def(
            "wait_for_finished",
            [](atom::utils::QProcess& self, int timeout_ms) {
                return self.waitForFinished(
                    std::chrono::milliseconds(timeout_ms));
            },
            py::arg("timeout_ms") = -1,
            R"(Waits for the process to finish.

Args:
    timeout_ms: The maximum time to wait in milliseconds. Negative values wait indefinitely.

Returns:
    bool: True if the process has finished within the specified timeout, False otherwise.
)")
        .def(
            "execute",
            [](atom::utils::QProcess& self, const std::string& program,
               const std::vector<std::string>& args, int timeout_sec) {
                return self.execute(program, args,
                                    std::chrono::seconds(timeout_sec));
            },
            py::arg("program"), py::arg("args") = std::vector<std::string>(),
            py::arg("timeout_sec") = -1,
            R"(Executes a program synchronously.

Args:
    program: The path to the executable program to start.
    args: A list of arguments to pass to the program.
    timeout_sec: The maximum time to wait in seconds. Negative values wait indefinitely.

Returns:
    int: The exit code of the process, or -1 if the process times out or fails to start.
)")
        .def("kill", &atom::utils::QProcess::kill,
             "Kills the process with immediate effect.")
        .def("is_running", &atom::utils::QProcess::isRunning,
             "Checks if the process is currently running.")
        .def("state", &atom::utils::QProcess::state,
             "Gets the current state of the process.")
        .def("error", &atom::utils::QProcess::error,
             "Gets the last error that occurred.")
        .def("exit_code", &atom::utils::QProcess::exitCode,
             "Gets the exit code of the process.")
        .def("exit_status", &atom::utils::QProcess::exitStatus,
             "Gets the exit status of the process.")
        .def("write", &atom::utils::QProcess::write, py::arg("data"),
             "Writes data to the process's standard input.")
        .def("close_write_channel", &atom::utils::QProcess::closeWriteChannel,
             "Closes the process's standard input.")
        .def("read_all_standard_output",
             &atom::utils::QProcess::readAllStandardOutput,
             "Reads all available data from the process's standard output.")
        .def("read_all_standard_error",
             &atom::utils::QProcess::readAllStandardError,
             "Reads all available data from the process's standard error.")
        .def("terminate", &atom::utils::QProcess::terminate,
             "Terminates the process.")

        // Callbacks
        .def("set_started_callback", &atom::utils::QProcess::setStartedCallback,
             py::arg("callback"),
             "Sets the callback function for process started events.")
        .def("set_finished_callback",
             &atom::utils::QProcess::setFinishedCallback, py::arg("callback"),
             "Sets the callback function for process finished events.")
        .def("set_error_callback", &atom::utils::QProcess::setErrorCallback,
             py::arg("callback"),
             "Sets the callback function for process error events.")
        .def("set_ready_read_standard_output_callback",
             &atom::utils::QProcess::setReadyReadStandardOutputCallback,
             py::arg("callback"),
             "Sets the callback function for standard output data.")
        .def("set_ready_read_standard_error_callback",
             &atom::utils::QProcess::setReadyReadStandardErrorCallback,
             py::arg("callback"),
             "Sets the callback function for standard error data.")

        // Context manager protocol
        .def("__enter__", [](atom::utils::QProcess& self) { return &self; })
        .def("__exit__", [](atom::utils::QProcess& self, py::object exc_type,
                            py::object exc_val, py::object exc_tb) {
            if (self.isRunning()) {
                self.terminate();
                self.waitForFinished(std::chrono::seconds(3));
            }
            return false;  // Don't suppress exceptions
        });
}