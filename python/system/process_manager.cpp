#include "atom/system/process_manager.hpp"
#include "atom/system/process_info.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(process_manager, m) {
    m.doc() = "Process management module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::system::ProcessException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ProcessException class binding
    py::register_exception<atom::system::ProcessException>(
        m, "ProcessException", PyExc_RuntimeError);

    // ProcessManager class binding
    py::class_<atom::system::ProcessManager,
               std::shared_ptr<atom::system::ProcessManager>>(
        m, "ProcessManager",
        R"(Manages system processes with enhanced functionalities.

This class provides methods to create, monitor, and terminate processes,
as well as retrieve process information.

Args:
    max_process: The maximum number of processes to manage (default: 20).

Examples:
    >>> from atom.system import process_manager
    >>> # Create a process manager
    >>> pm = process_manager.ProcessManager(10)
    >>> # Run a command
    >>> pm.create_process("echo Hello, World!", "greeting")
    >>> # Check if process exists
    >>> pm.has_process("greeting")
    True
)")
        .def(py::init<int>(), py::arg("max_process") = 20,
             "Constructs a ProcessManager with a maximum number of processes.")
        .def_static("create_shared",
                    &atom::system::ProcessManager::createShared,
                    py::arg("max_process") = 20,
                    R"(Creates a shared pointer to a ProcessManager.

Args:
    max_process: The maximum number of processes to manage (default: 20).

Returns:
    Shared pointer to a ProcessManager.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager.create_shared(10)
)")
        .def("create_process", &atom::system::ProcessManager::createProcess,
             py::arg("command"), py::arg("identifier"),
             py::arg("is_background") = false,
             R"(Creates a new process.

Args:
    command: The command to execute.
    identifier: An identifier for the process.
    is_background: Whether to run the process in the background (default: False).

Returns:
    True if the process was created successfully, otherwise False.

Raises:
    ProcessException: If process creation fails.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> # Run ls command with identifier "list_files"
    >>> pm.create_process("ls -la", "list_files")
    >>> # Run a long process in the background
    >>> pm.create_process("sleep 10", "background_sleep", True)
)")
        .def("terminate_process",
             &atom::system::ProcessManager::terminateProcess, py::arg("pid"),
             py::arg("signal") = 15,
             R"(Terminates a process by its PID.

Args:
    pid: The process ID.
    signal: The signal to send to the process (default is 15/SIGTERM).

Returns:
    True if the process was terminated successfully, otherwise False.

Raises:
    ProcessException: If termination fails.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> # Terminate process with PID 1234
    >>> pm.terminate_process(1234)
    >>> # Forcefully kill with SIGKILL (9)
    >>> pm.terminate_process(1234, 9)
)")
        .def("terminate_process_by_name",
             &atom::system::ProcessManager::terminateProcessByName,
             py::arg("name"), py::arg("signal") = 15,
             R"(Terminates a process by its name.

Args:
    name: The process name.
    signal: The signal to send to the process (default is 15/SIGTERM).

Returns:
    True if the process was terminated successfully, otherwise False.

Raises:
    ProcessException: If termination fails.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> # Terminate all processes named "sleep"
    >>> pm.terminate_process_by_name("sleep")
)")
        .def("has_process", &atom::system::ProcessManager::hasProcess,
             py::arg("identifier"),
             R"(Checks if a process with the given identifier exists.

Args:
    identifier: The process identifier.

Returns:
    True if the process exists, otherwise False.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> pm.create_process("echo Hello", "greeting")
    >>> pm.has_process("greeting")
    True
)")
        .def("get_running_processes",
             &atom::system::ProcessManager::getRunningProcesses,
             R"(Gets a list of running processes.

Returns:
    A list of Process objects representing running processes.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> processes = pm.get_running_processes()
    >>> for proc in processes:
    ...     print(f"{proc.name} (PID: {proc.pid}) ")
)")
        .def("get_process_output",
             &atom::system::ProcessManager::getProcessOutput,
             py::arg("identifier"),
             R"(Gets the output of a process by its identifier.

Args:
    identifier: The process identifier.

Returns:
    A list of strings containing the process output.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> pm.create_process("echo 'Line 1\nLine 2'", "echo_test")
    >>> output = pm.get_process_output("echo_test")
    >>> for line in output:
    ...     print(line)
)")
        .def("wait_for_completion",
             &atom::system::ProcessManager::waitForCompletion,
             R"(Waits for all managed processes to complete.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> pm.create_process("sleep 2", "sleep1", True)
    >>> pm.create_process("sleep 3", "sleep2", True)
    >>> # Wait for both processes to finish
    >>> pm.wait_for_completion()
    >>> print("All processes completed")
)")
        .def("run_script", &atom::system::ProcessManager::runScript,
             py::arg("script"), py::arg("identifier"),
             py::arg("is_background") = false,
             R"(Runs a script as a new process.

Args:
    script: The script content to run.
    identifier: An identifier for the process.
    is_background: Whether to run the script in the background (default: False).

Returns:
    True if the script was run successfully, otherwise False.

Raises:
    ProcessException: If script execution fails.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> script = "#!/bin/bash\necho 'Running script'\nsleep 2\necho 'Done'"
    >>> pm.run_script(script, "test_script")
)")
        .def("monitor_processes",
             &atom::system::ProcessManager::monitorProcesses,
             R"(Monitors the managed processes and updates their statuses.

Returns:
    True if monitoring was successful, otherwise False.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> # Create some background processes
    >>> pm.create_process("sleep 10", "sleep1", True)
    >>> pm.create_process("sleep 15", "sleep2", True)
    >>> # Monitor their status
    >>> pm.monitor_processes()
)")
        .def("get_process_info", &atom::system::ProcessManager::getProcessInfo,
             py::arg("pid"),
             R"(Retrieves detailed information about a specific process.

Args:
    pid: The process ID.

Returns:
    A Process object with detailed information.

Raises:
    ProcessException: If retrieval fails.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> # Get info about a specific process
    >>> proc_info = pm.get_process_info(1234)
    >>> print(f"Process name: {proc_info.name}")
    >>> print(f"CPU usage: {proc_info.cpu_usage}%")
    >>> print(f"Memory usage: {proc_info.memory_usage / (1024*1024):.2f} MB")
)");

#ifdef _WIN32
    m.def(
        "get_process_handle",
        [](const atom::system::ProcessManager& pm, int pid) {
            void* handle = pm.getProcessHandle(pid);
            return reinterpret_cast<intptr_t>(handle);
        },
        py::arg("pm"), py::arg("pid"),
        R"(Gets the handle of a process by its PID (Windows only).

Args:
    pm: ProcessManager instance.
    pid: The process ID.

Returns:
    The handle of the process as an integer.

Raises:
    ProcessException: If retrieval fails.

Examples:
    >>> from atom.system import process_manager
    >>> pm = process_manager.ProcessManager()
    >>> handle = process_manager.get_process_handle(pm, 1234)
    >>> print(f"Process handle: {handle}")
)");
#else
    m.def("get_proc_file_path", &atom::system::ProcessManager::getProcFilePath,
          py::arg("pid"), py::arg("file"),
          R"(Gets the file path of a process by its PID (Linux/Unix).

Args:
    pid: The process ID.
    file: The file name.

Returns:
    The file path of the process.

Raises:
    ProcessException: If retrieval fails.

Examples:
    >>> from atom.system import process_manager
    >>> # Get the cmdline file for process 1234
    >>> path = process_manager.get_proc_file_path(1234, "cmdline")
    >>> print(f"Process cmdline file: {path}")
)");
#endif

    // Utility functions
    m.def(
        "create_and_manage_process",
        [](const std::string& command, const std::string& identifier,
           bool is_background) {
            auto pm = atom::system::ProcessManager::createShared();
            if (!pm->createProcess(command, identifier, is_background)) {
                throw py::value_error("Failed to create process: " +
                                      identifier);
            }
            return pm;
        },
        py::arg("command"), py::arg("identifier"),
        py::arg("is_background") = false,
        R"(Creates a new ProcessManager and runs a command.

This is a convenience function that creates a ProcessManager, runs a command, and returns the manager.

Args:
    command: The command to execute.
    identifier: An identifier for the process.
    is_background: Whether to run the process in the background (default: False).

Returns:
    A ProcessManager with the process running.

Raises:
    ValueError: If process creation fails.

Examples:
    >>> from atom.system import process_manager
    >>> # Create a manager and run a command in one step
    >>> pm = process_manager.create_and_manage_process("echo 'Hello'", "greeting")
    >>> output = pm.get_process_output("greeting")
    >>> print(output)
)",
        py::return_value_policy::take_ownership);

    m.def(
        "run_and_get_output",
        [](const std::string& command) {
            auto pm = atom::system::ProcessManager::createShared();
            std::string identifier = "temp_" + std::to_string(std::rand());

            if (!pm->createProcess(command, identifier, false)) {
                throw py::value_error("Failed to run command: " + command);
            }

            pm->waitForCompletion();
            auto output = pm->getProcessOutput(identifier);
            return output;
        },
        py::arg("command"),
        R"(Runs a command and returns its output.

This is a convenience function that runs a command and returns its output as a list of strings.

Args:
    command: The command to execute.

Returns:
    A list of strings containing the command output.

Raises:
    ValueError: If the command fails to run.

Examples:
    >>> from atom.system import process_manager
    >>> # Run a command and get its output directly
    >>> output = process_manager.run_and_get_output("ls -la")
    >>> for line in output:
    ...     print(line)
)");

    // Process monitoring context manager
    py::class_<py::object>(m, "ProcessContext")
        .def(py::init([](const std::string& command,
                         const std::string& identifier,
                         bool wait_for_completion) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("command"), py::arg("identifier"),
             py::arg("wait_for_completion") = true,
             "Create a context manager for process execution")
        .def("__enter__",
             [](py::object& self, const std::string& command,
                const std::string& identifier, bool wait_for_completion) {
                 auto pm = atom::system::ProcessManager::createShared();
                 if (!pm->createProcess(command, identifier, false)) {
                     throw py::value_error("Failed to create process: " +
                                           identifier);
                 }
                 self.attr("pm") =
                     py::cast(pm, py::return_value_policy::take_ownership);
                 self.attr("identifier") = py::str(identifier);
                 self.attr("wait") = py::bool_(wait_for_completion);
                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object exc_type, py::object exc_val,
                py::object exc_tb) {
                 auto pm =
                     py::cast<std::shared_ptr<atom::system::ProcessManager>>(
                         self.attr("pm"));
                 std::string identifier =
                     py::cast<std::string>(self.attr("identifier"));
                 bool wait = py::cast<bool>(self.attr("wait"));

                 if (wait) {
                     pm->waitForCompletion();
                 }

                 return false;  // Don't suppress exceptions
             })
        .def(
            "get_output",
            [](py::object& self) {
                auto pm =
                    py::cast<std::shared_ptr<atom::system::ProcessManager>>(
                        self.attr("pm"));
                std::string identifier =
                    py::cast<std::string>(self.attr("identifier"));
                return pm->getProcessOutput(identifier);
            },
            "Get the process output");

    // Module-level method to use the context manager
    m.def(
        "with_process",
        [&m](const std::string& command, const std::string& identifier,
             bool wait_for_completion) {
            return m.attr("ProcessContext")(command, identifier,
                                            wait_for_completion);
        },
        py::arg("command"), py::arg("identifier"),
        py::arg("wait_for_completion") = true,
        R"(Creates a context manager for process execution.

This function returns a context manager that automatically handles process creation and cleanup.

Args:
    command: The command to execute.
    identifier: An identifier for the process.
    wait_for_completion: Whether to wait for process completion when exiting the context (default: True).

Returns:
    A context manager for process execution.

Examples:
    >>> from atom.system import process_manager
    >>> # Use as a context manager
    >>> with process_manager.with_process("echo 'Hello, World!'", "greeting") as context:
    ...     # Process is running here
    ...     pass
    ... # Process is completed here
    >>> # Get output after the context
    >>> output = context.get_output()
    >>> print(output)
)");

    // Add ProcessContext properties and methods
    m.attr("ProcessContext").attr("__getitem__") =
        py::cpp_function([](py::object& self, const std::string& name) {
            if (name == "output") {
                auto pm =
                    py::cast<std::shared_ptr<atom::system::ProcessManager>>(
                        self.attr("pm"));
                std::string identifier =
                    py::cast<std::string>(self.attr("identifier"));
                return py::cast(pm->getProcessOutput(identifier));
            }
            throw py::key_error("Invalid key: " + name);
        });
}