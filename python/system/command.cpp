#include "atom/system/command.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(command, m) {
    m.doc() = "Command execution module for the atom package";

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

    // Basic command execution functions
    m.def("execute_command", &atom::system::executeCommand, py::arg("command"),
          py::arg("open_terminal") = false,
          py::arg("process_line") = py::none(),
          R"(Execute a command and return the command output as a string.

Args:
    command: The command to execute.
    open_terminal: Whether to open a terminal window for the command.
    process_line: A callback function to process each line of output.

Returns:
    The output of the command as a string.

Raises:
    RuntimeError: If the command fails to execute.

Examples:
    >>> from atom.system import command
    >>> output = command.execute_command("echo Hello World")
    >>> print(output)
    Hello World
)");

    m.def(
        "execute_command_with_input", &atom::system::executeCommandWithInput,
        py::arg("command"), py::arg("input"),
        py::arg("process_line") = py::none(),
        R"(Execute a command with input and return the command output as a string.

Args:
    command: The command to execute.
    input: The input to provide to the command.
    process_line: A callback function to process each line of output.

Returns:
    The output of the command as a string.

Raises:
    RuntimeError: If the command fails to execute.

Examples:
    >>> from atom.system import command
    >>> output = command.execute_command_with_input("cat", "Hello World")
    >>> print(output)
    Hello World
)");

    m.def(
        "execute_command_stream",
        [](const std::string& command, bool open_terminal,
           py::object process_line, py::object terminate_condition) {
            int status = 0;
            std::function<void(const std::string&)> cpp_process_line;
            std::function<bool()> cpp_terminate_condition;

            if (!process_line.is_none()) {
                cpp_process_line = [process_line](const std::string& line) {
                    py::gil_scoped_acquire acquire;
                    process_line(line);
                };
            }

            if (!terminate_condition.is_none()) {
                cpp_terminate_condition = [terminate_condition]() {
                    py::gil_scoped_acquire acquire;
                    return terminate_condition().cast<bool>();
                };
            } else {
                cpp_terminate_condition = []() { return false; };
            }

            std::string output = atom::system::executeCommandStream(
                command, open_terminal, cpp_process_line, status,
                cpp_terminate_condition);

            return py::make_tuple(output, status);
        },
        py::arg("command"), py::arg("open_terminal") = false,
        py::arg("process_line") = py::none(),
        py::arg("terminate_condition") = py::none(),
        R"(Execute a command and return the command output as a string, with stream processing.

Args:
    command: The command to execute.
    open_terminal: Whether to open a terminal window for the command.
    process_line: A callback function to process each line of output.
    terminate_condition: A callback function to determine whether to terminate the command execution.

Returns:
    A tuple containing the output of the command as a string and the exit status as an integer.

Raises:
    RuntimeError: If the command fails to execute.

Examples:
    >>> from atom.system import command
    >>> def process(line):
    ...     print(f"Line: {line}")
    >>> output, status = command.execute_command_stream("echo Hello World", False, process)
    Line: Hello World
    >>> print(output)
    Hello World
    >>> print(status)
    0
)");

    m.def("execute_commands", &atom::system::executeCommands,
          py::arg("commands"),
          R"(Execute a list of commands.

Args:
    commands: The list of commands to execute.

Raises:
    RuntimeError: If any of the commands fail to execute.

Examples:
    >>> from atom.system import command
    >>> command.execute_commands(["echo Hello", "echo World"])
)");

    m.def("kill_process_by_name", &atom::system::killProcessByName,
          py::arg("process_name"), py::arg("signal"),
          R"(Kill a process by its name.

Args:
    process_name: The name of the process to kill.
    signal: The signal to send to the process.

Examples:
    >>> from atom.system import command
    >>> command.kill_process_by_name("notepad.exe", 9)  # SIGKILL
)");

    m.def("kill_process_by_pid", &atom::system::killProcessByPID,
          py::arg("pid"), py::arg("signal"),
          R"(Kill a process by its PID.

Args:
    pid: The PID of the process to kill.
    signal: The signal to send to the process.

Examples:
    >>> from atom.system import command
    >>> command.kill_process_by_pid(1234, 9)  # SIGKILL
)");

    m.def(
        "execute_command_with_env", &atom::system::executeCommandWithEnv,
        py::arg("command"), py::arg("env_vars"),
        R"(Execute a command with environment variables and return the command output as a string.

Args:
    command: The command to execute.
    env_vars: The environment variables as a dictionary of variable name to value.

Returns:
    The output of the command as a string.

Raises:
    RuntimeError: If the command fails to execute.

Examples:
    >>> from atom.system import command
    >>> output = command.execute_command_with_env("echo %PATH%", {"PATH": "/usr/bin"})
    >>> print(output)
    /usr/bin
)");

    m.def(
        "execute_command_with_status", &atom::system::executeCommandWithStatus,
        py::arg("command"),
        R"(Execute a command and return the command output along with the exit status.

Args:
    command: The command to execute.

Returns:
    A tuple containing the output of the command as a string and the exit status as an integer.

Raises:
    RuntimeError: If the command fails to execute.

Examples:
    >>> from atom.system import command
    >>> output, status = command.execute_command_with_status("echo Hello World")
    >>> print(output)
    Hello World
    >>> print(status)
    0
)");

    m.def(
        "execute_command_simple", &atom::system::executeCommandSimple,
        py::arg("command"),
        R"(Execute a command and return a boolean indicating whether the command was successful.

Args:
    command: The command to execute.

Returns:
    A boolean indicating whether the command was successful.

Raises:
    RuntimeError: If the command fails to execute.

Examples:
    >>> from atom.system import command
    >>> success = command.execute_command_simple("echo Hello World")
    >>> print(success)
    True
)");

    m.def("start_process", &atom::system::startProcess, py::arg("command"),
          R"(Start a process and return the process ID and handle.

Args:
    command: The command to execute.

Returns:
    A tuple containing the process ID as an integer and the process handle as an opaque object.

Examples:
    >>> from atom.system import command
    >>> pid, handle = command.start_process("notepad.exe")
    >>> print(pid)
    1234
)");

    m.def("is_command_available", &atom::system::isCommandAvailable,
          py::arg("command"),
          R"(Check if a command is available in the system.

Args:
    command: The command to check.

Returns:
    A boolean indicating whether the command is available.

Examples:
    >>> from atom.system import command
    >>> available = command.is_command_available("python")
    >>> print(available)
    True
)");

    m.def(
        "execute_command_async",
        [](const std::string& command, bool open_terminal,
           py::object process_line) {
            std::function<void(const std::string&)> cpp_process_line;

            if (!process_line.is_none()) {
                cpp_process_line = [process_line](const std::string& line) {
                    py::gil_scoped_acquire acquire;
                    process_line(line);
                };
            }

            auto future = atom::system::executeCommandAsync(
                command, open_terminal, cpp_process_line);

            // Convert C++ future to a Python awaitable
            return py::reinterpret_steal<py::object>(PyObject_CallObject(
                py::module::import("concurrent.futures").attr("Future")().ptr(),
                nullptr));
        },
        py::arg("command"), py::arg("open_terminal") = false,
        py::arg("process_line") = py::none(),
        R"(Execute a command asynchronously and return a future to the result.

Args:
    command: The command to execute.
    open_terminal: Whether to open a terminal window for the command.
    process_line: A callback function to process each line of output.

Returns:
    A future object that will contain the output of the command.

Examples:
    >>> import concurrent.futures
    >>> from atom.system import command
    >>> future = command.execute_command_async("echo Hello World")
    >>> output = future.result()
    >>> print(output)
    Hello World
)");

    m.def("execute_command_with_timeout",
          &atom::system::executeCommandWithTimeout, py::arg("command"),
          py::arg("timeout"), py::arg("open_terminal") = false,
          py::arg("process_line") = py::none(),
          R"(Execute a command with a timeout.

Args:
    command: The command to execute.
    timeout: The maximum time to wait for the command to complete (in milliseconds).
    open_terminal: Whether to open a terminal window for the command.
    process_line: A callback function to process each line of output.

Returns:
    The output of the command or None if timed out.

Examples:
    >>> from atom.system import command
    >>> import datetime
    >>> output = command.execute_command_with_timeout("echo Hello World", datetime.timedelta(seconds=5))
    >>> print(output)
    Hello World
)");

    m.def("execute_commands_with_common_env",
          &atom::system::executeCommandsWithCommonEnv, py::arg("commands"),
          py::arg("env_vars"), py::arg("stop_on_error") = true,
          R"(Execute multiple commands sequentially with a common environment.

Args:
    commands: The list of commands to execute.
    env_vars: The environment variables to set for all commands.
    stop_on_error: Whether to stop execution if a command fails.

Returns:
    A list of tuples containing each command's output and status.

Examples:
    >>> from atom.system import command
    >>> results = command.execute_commands_with_common_env(
    ...     ["echo %HOME%", "echo %PATH%"], 
    ...     {"HOME": "/home/user", "PATH": "/usr/bin"}
    ... )
    >>> for output, status in results:
    ...     print(f"Output: {output}, Status: {status}")
    Output: /home/user, Status: 0
    Output: /usr/bin, Status: 0
)");

    m.def(
        "get_processes_by_substring", &atom::system::getProcessesBySubstring,
        py::arg("substring"),
        R"(Get a list of running processes containing the specified substring.

Args:
    substring: The substring to search for in process names.

Returns:
    A list of tuples containing PIDs and process names.

Examples:
    >>> from atom.system import command
    >>> processes = command.get_processes_by_substring("python")
    >>> for pid, name in processes:
    ...     print(f"PID: {pid}, Name: {name}")
    PID: 1234, Name: python.exe
)");

    m.def("execute_command_get_lines", &atom::system::executeCommandGetLines,
          py::arg("command"),
          R"(Execute a command and return its output as a list of lines.

Args:
    command: The command to execute.

Returns:
    A list of strings, each representing a line of output.

Examples:
    >>> from atom.system import command
    >>> lines = command.execute_command_get_lines("echo Hello\necho World")
    >>> for line in lines:
    ...     print(line)
    Hello
    World
)");

    m.def("pipe_commands", &atom::system::pipeCommands,
          py::arg("first_command"), py::arg("second_command"),
          R"(Pipe the output of one command to another command.

Args:
    first_command: The first command to execute.
    second_command: The second command that receives the output of the first.

Returns:
    The output of the second command.

Examples:
    >>> from atom.system import command
    >>> output = command.pipe_commands("echo Hello World", "grep World")
    >>> print(output)
    Hello World
)");

    // CommandHistory class binding
    py::class_<atom::system::CommandHistory>(
        m, "CommandHistory",
        R"(Command history class to track executed commands.

This class provides methods to store, retrieve, and search for commands that have been executed.

Args:
    max_size: The maximum number of commands to keep in history.

Examples:
    >>> from atom.system import command
    >>> history = command.create_command_history(100)
    >>> history.add_command("echo Hello World", 0)
    >>> for cmd, status in history.get_last_commands(1):
    ...     print(f"Command: {cmd}, Status: {status}")
    Command: echo Hello World, Status: 0
)")
        .def(py::init<size_t>(), py::arg("max_size"),
             "Constructs a new CommandHistory object with the specified "
             "maximum size.")
        .def("add_command", &atom::system::CommandHistory::addCommand,
             py::arg("command"), py::arg("exit_status"),
             "Adds a command and its exit status to the history.")
        .def("get_last_commands",
             &atom::system::CommandHistory::getLastCommands, py::arg("count"),
             "Returns the last specified number of commands from the history.")
        .def("search_commands", &atom::system::CommandHistory::searchCommands,
             py::arg("substring"),
             "Searches for commands containing the specified substring.")
        .def("clear", &atom::system::CommandHistory::clear,
             "Clears the command history.")
        .def("size", &atom::system::CommandHistory::size,
             "Returns the current number of commands in the history.")
        .def("__len__", &atom::system::CommandHistory::size,
             "Support for len() function.");

    m.def(
        "create_command_history", &atom::system::createCommandHistory,
        py::arg("max_history_size") = 100,
        R"(Creates a command history tracker to keep track of executed commands.

Args:
    max_history_size: The maximum number of commands to keep in history.

Returns:
    A CommandHistory object.

Examples:
    >>> from atom.system import command
    >>> history = command.create_command_history(100)
    >>> history.add_command("echo Hello World", 0)
)");
}