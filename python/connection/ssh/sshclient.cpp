#include "atom/connection/ssh/sshclient.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(sshclient, m) {
    m.doc() = R"(SSH client module for the atom package.

This module provides SSH client functionality for secure remote connections,
command execution, and file operations using the libssh library.

Note: This module is only available if libssh is installed and detected during compilation.

Key Features:
- Secure SSH connections with username/password authentication
- Remote command execution (single and batch)
- File and directory operations (upload, download, create, remove)
- Directory listing and file information retrieval
- File and directory renaming
- Connection management with timeout support

Classes:
- SSHClient: Main SSH client class for remote operations

Quick Start Example:
    >>> from atom.connection.sshclient import SSHClient
    >>>
    >>> # Create SSH client
    >>> client = SSHClient("example.com", 22)
    >>>
    >>> # Connect with credentials
    >>> client.connect("username", "password", timeout=10)
    >>>
    >>> # Execute commands
    >>> output = []
    >>> client.execute_command("ls -la", output)
    >>> for line in output:
    ...     print(line)
    >>>
    >>> # File operations
    >>> client.upload_file("/local/file.txt", "/remote/file.txt")
    >>> client.download_file("/remote/data.txt", "/local/data.txt")
    >>>
    >>> # Directory operations
    >>> files = client.list_directory("/remote/path")
    >>> for file in files:
    ...     print(file)
    >>>
    >>> client.disconnect()

Advanced Features:
- Batch command execution for multiple operations
- Directory upload/download with recursive operations
- File existence checking and metadata retrieval
- Secure file transfer with SFTP protocol
- Connection state management
- Error handling with detailed exception messages
)";

#if __has_include(<libssh/libssh.h>)
    // Bind the SSHClient class only if libssh is available
    py::class_<atom::connection::SSHClient>(
        m, "SSHClient",
        R"(SSH client for secure remote connections and file operations.

This class provides methods for establishing SSH connections, executing commands,
and performing file operations on remote servers using the SSH protocol.

Examples:
    >>> from atom.connection.sshclient import SSHClient
    >>>
    >>> # Create client and connect
    >>> client = SSHClient("example.com", 22)
    >>> client.connect("username", "password")
    >>>
    >>> # Execute command
    >>> output = []
    >>> client.execute_command("whoami", output)
    >>> print(output[0])  # username
    >>>
    >>> # File operations
    >>> client.upload_file("local.txt", "remote.txt")
    >>> client.download_file("remote.txt", "downloaded.txt")
    >>>
    >>> client.disconnect()
)")
        .def(py::init<const std::string&, int>(), py::arg("host"),
             py::arg("port") = atom::connection::DEFAULT_SSH_PORT,
             R"(Constructs an SSHClient for the specified host and port.

Args:
    host: The hostname or IP address of the SSH server
    port: The port number of the SSH server (default: 22)

Examples:
    >>> client = SSHClient("example.com")  # Default port 22
    >>> client = SSHClient("192.168.1.100", 2222)  # Custom port
)")
        .def("connect", &atom::connection::SSHClient::connect,
             py::arg("username"), py::arg("password"),
             py::arg("timeout") = atom::connection::DEFAULT_TIMEOUT,
             R"(Connects to the SSH server with authentication.

Args:
    username: The username for authentication
    password: The password for authentication
    timeout: The connection timeout in seconds (default: 10)

Raises:
    RuntimeError: If connection or authentication fails

Examples:
    >>> client.connect("myuser", "mypassword")
    >>> client.connect("admin", "secret", timeout=30)
)")
        .def("is_connected", &atom::connection::SSHClient::isConnected,
             R"(Checks if the SSH client is connected to the server.

Returns:
    True if connected, False otherwise

Examples:
    >>> if client.is_connected():
    ...     print("Connected to server")
)")
        .def("disconnect", &atom::connection::SSHClient::disconnect,
             R"(Disconnects from the SSH server.

Examples:
    >>> client.disconnect()
)")
        .def("execute_command", &atom::connection::SSHClient::executeCommand,
             py::arg("command"), py::arg("output"),
             R"(Executes a single command on the SSH server.

Args:
    command: The command to execute
    output: List to store the command output lines

Raises:
    RuntimeError: If command execution fails

Examples:
    >>> output = []
    >>> client.execute_command("ls -la", output)
    >>> for line in output:
    ...     print(line)
)")
        .def("execute_commands", &atom::connection::SSHClient::executeCommands,
             py::arg("commands"), py::arg("output"),
             R"(Executes multiple commands on the SSH server.

Args:
    commands: List of commands to execute
    output: List of lists to store the command outputs

Raises:
    RuntimeError: If any command execution fails

Examples:
    >>> commands = ["pwd", "ls", "whoami"]
    >>> outputs = [[], [], []]
    >>> client.execute_commands(commands, outputs)
    >>> for i, cmd_output in enumerate(outputs):
    ...     print(f"Command {commands[i]} output:")
    ...     for line in cmd_output:
    ...         print(f"  {line}")
)")
        .def("file_exists", &atom::connection::SSHClient::fileExists,
             py::arg("remote_path"),
             R"(Checks if a file exists on the remote server.

Args:
    remote_path: The path of the remote file

Returns:
    True if the file exists, False otherwise

Examples:
    >>> if client.file_exists("/etc/passwd"):
    ...     print("File exists")
)")
        .def("create_directory", &atom::connection::SSHClient::createDirectory,
             py::arg("remote_path"),
             py::arg("mode") = atom::connection::DEFAULT_MODE,
             R"(Creates a directory on the remote server.

Args:
    remote_path: The path of the remote directory
    mode: The permissions of the directory (default: S_NORMAL)

Raises:
    RuntimeError: If directory creation fails

Examples:
    >>> client.create_directory("/tmp/mydir")
    >>> client.create_directory("/tmp/mydir", 0o755)
)")
        .def("remove_file", &atom::connection::SSHClient::removeFile,
             py::arg("remote_path"),
             R"(Removes a file from the remote server.

Args:
    remote_path: The path of the remote file

Raises:
    RuntimeError: If file removal fails

Examples:
    >>> client.remove_file("/tmp/unwanted.txt")
)")
        .def("remove_directory", &atom::connection::SSHClient::removeDirectory,
             py::arg("remote_path"),
             R"(Removes a directory from the remote server.

Args:
    remote_path: The path of the remote directory

Raises:
    RuntimeError: If directory removal fails

Examples:
    >>> client.remove_directory("/tmp/olddir")
)")
        .def("list_directory", &atom::connection::SSHClient::listDirectory,
             py::arg("remote_path"),
             R"(Lists the contents of a directory on the remote server.

Args:
    remote_path: The path of the remote directory

Returns:
    List of strings containing the names of the directory contents

Raises:
    RuntimeError: If listing directory fails

Examples:
    >>> files = client.list_directory("/home/user")
    >>> for file in files:
    ...     print(file)
)")
        .def("rename", &atom::connection::SSHClient::rename,
             py::arg("old_path"), py::arg("new_path"),
             R"(Renames a file or directory on the remote server.

Args:
    old_path: The current path of the remote file or directory
    new_path: The new path of the remote file or directory

Raises:
    RuntimeError: If renaming fails

Examples:
    >>> client.rename("/tmp/old_name.txt", "/tmp/new_name.txt")
)")
        .def("download_file", &atom::connection::SSHClient::downloadFile,
             py::arg("remote_path"), py::arg("local_path"),
             R"(Downloads a file from the remote server.

Args:
    remote_path: The path of the remote file
    local_path: The path of the local destination file

Raises:
    RuntimeError: If file download fails

Examples:
    >>> client.download_file("/remote/data.txt", "/local/data.txt")
)")
        .def("upload_file", &atom::connection::SSHClient::uploadFile,
             py::arg("local_path"), py::arg("remote_path"),
             R"(Uploads a file to the remote server.

Args:
    local_path: The path of the local source file
    remote_path: The path of the remote destination file

Raises:
    RuntimeError: If file upload fails

Examples:
    >>> client.upload_file("/local/file.txt", "/remote/file.txt")
)")
        .def("upload_directory", &atom::connection::SSHClient::uploadDirectory,
             py::arg("local_path"), py::arg("remote_path"),
             R"(Uploads a directory to the remote server.

Args:
    local_path: The path of the local source directory
    remote_path: The path of the remote destination directory

Raises:
    RuntimeError: If directory upload fails

Examples:
    >>> client.upload_directory("/local/mydir", "/remote/mydir")
)")
        .def(
            "__enter__",
            [](atom::connection::SSHClient& self)
                -> atom::connection::SSHClient& { return self; },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::connection::SSHClient& self, py::object, py::object,
               py::object) {
                if (self.isConnected()) {
                    self.disconnect();
                }
            },
            "Ensure client is disconnected when exiting context");

#else
    // If libssh is not available, provide a stub implementation
    py::class_<int>(m, "SSHClient")  // Using int as a dummy type
        .def(
            py::init([]() {
                throw std::runtime_error(
                    "SSHClient is not available - libssh library not found. "
                    "Please install libssh development package and recompile.");
                return 0;  // Never reached
            }),
            "SSHClient is not available without libssh");

#endif
}
