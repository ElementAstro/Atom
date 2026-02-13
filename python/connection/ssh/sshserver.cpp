#include "atom/connection/ssh/sshserver.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(sshserver, m) {
    m.doc() = R"(SSH server module for the atom package.

This module provides SSH server functionality for hosting secure shell connections,
managing client sessions, and handling authentication using the libssh library.

Key Features:
- Secure SSH server with comprehensive configuration
- Multiple authentication methods (password, public key)
- Connection management and statistics
- Subsystem support (SFTP, etc.)
- IP filtering and access control
- Logging and monitoring
- Encryption configuration (ciphers, MACs, key exchange)

Classes:
- SshServer: Main SSH server class
- SshConnection: Information about active connections
- LogLevel: Logging levels for server operation

Quick Start Example:
    >>> from atom.connection.sshserver import SshServer, LogLevel
    >>>
    >>> # Create and configure server
    >>> server = SshServer("/etc/ssh/sshd_config")
    >>> server.set_port(2222)
    >>> server.set_host_key("/etc/ssh/ssh_host_rsa_key")
    >>> server.set_password_authentication(True)
    >>>
    >>> # Set up callbacks
    >>> def on_new_connection(conn):
    ...     print(f"New connection from {conn.ip_address}")
    >>>
    >>> server.on_new_connection(on_new_connection)
    >>>
    >>> # Start server
    >>> if server.start():
    ...     print("SSH server started")
    >>>
    >>> # ... server is running ...
    >>> server.stop()
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
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // LogLevel enum
    py::enum_<atom::connection::LogLevel>(m, "LogLevel",
                                          "Log levels for SSH server")
        .value("QUIET", atom::connection::LogLevel::QUIET, "No logging")
        .value("FATAL", atom::connection::LogLevel::FATAL,
               "Log only fatal errors")
        .value("ERROR", atom::connection::LogLevel::ERROR, "Log errors")
        .value("INFO", atom::connection::LogLevel::INFO,
               "Log general information")
        .value("VERBOSE", atom::connection::LogLevel::VERBOSE,
               "Log detailed information")
        .value("DEBUG", atom::connection::LogLevel::DEBUG,
               "Log debug information")
        .value("DEBUG1", atom::connection::LogLevel::DEBUG1,
               "More detailed debug (level 1)")
        .value("DEBUG2", atom::connection::LogLevel::DEBUG2,
               "More detailed debug (level 2)")
        .value("DEBUG3", atom::connection::LogLevel::DEBUG3,
               "Most detailed debug (level 3)")
        .export_values();

    // SshConnection struct
    py::class_<atom::connection::SshConnection>(
        m, "SshConnection",
        R"(Information about an active SSH connection.

This structure contains details about a client's SSH session.

Examples:
    >>> connections = server.get_active_connections()
    >>> for conn in connections:
    ...     print(f"User {conn.username} from {conn.ip_address}")
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("username", &atom::connection::SshConnection::username,
                       "Username used for the connection")
        .def_readwrite("ip_address",
                       &atom::connection::SshConnection::ipAddress,
                       "Remote IP address")
        .def_readwrite("port", &atom::connection::SshConnection::port,
                       "Remote port")
        .def_readwrite("connected_time",
                       &atom::connection::SshConnection::connectedTime,
                       "Time when connection was established")
        .def_readwrite("session_id",
                       &atom::connection::SshConnection::sessionId,
                       "Unique session identifier");

    // SshServer class
    py::class_<atom::connection::SshServer>(
        m, "SshServer",
        R"(SSH server for handling secure shell connections.

This class provides comprehensive SSH server functionality including
authentication, connection management, and configuration.

Examples:
    >>> server = SshServer("/etc/ssh/sshd_config")
    >>> server.set_port(2222)
    >>> server.set_password_authentication(True)
    >>> if server.start():
    ...     print("Server started")
)")
        .def(py::init<const std::filesystem::path&>(), py::arg("config_file"),
             "Constructs an SSH server with the specified configuration file.")
        .def("start", &atom::connection::SshServer::start,
             R"(Starts the SSH server.

Returns:
    True if server started successfully, False otherwise
)")
        .def("stop", &atom::connection::SshServer::stop,
             py::arg("force") = false,
             R"(Stops the SSH server.

Args:
    force: If True, forcefully terminate even with active connections

Returns:
    True if server stopped successfully, False otherwise
)")
        .def("restart", &atom::connection::SshServer::restart,
             R"(Restarts the SSH server.

Returns:
    True if server restarted successfully, False otherwise
)")
        .def("is_running", &atom::connection::SshServer::isRunning,
             R"(Checks if the server is currently running.

Returns:
    True if running, False otherwise
)")
        .def("set_port", &atom::connection::SshServer::setPort, py::arg("port"),
             R"(Sets the port on which the server listens.

Args:
    port: The port number to listen on
)")
        .def("get_port", &atom::connection::SshServer::getPort,
             R"(Gets the port on which the server is listening.

Returns:
    The current listening port
)")
        .def("set_listen_address",
             &atom::connection::SshServer::setListenAddress, py::arg("address"),
             R"(Sets the address on which the server listens.

Args:
    address: The IP address or hostname for listening
)")
        .def("get_listen_address",
             &atom::connection::SshServer::getListenAddress,
             R"(Gets the address on which the server is listening.

Returns:
    The current listening address
)")
        .def("set_host_key", &atom::connection::SshServer::setHostKey,
             py::arg("key_file"),
             R"(Sets the host key file used for SSH connections.

Args:
    key_file: Path to the host key file
)")
        .def("get_host_key", &atom::connection::SshServer::getHostKey,
             R"(Gets the path to the host key file.

Returns:
    The current host key file path
)")
        .def("set_authorized_keys",
             &atom::connection::SshServer::setAuthorizedKeys,
             py::arg("key_files"),
             R"(Sets the list of authorized public key files.

Args:
    key_files: List of paths to public key files
)")
        .def("get_authorized_keys",
             &atom::connection::SshServer::getAuthorizedKeys,
             R"(Gets the list of authorized public key files.

Returns:
    List of paths to authorized public key files
)")
        .def("allow_root_login", &atom::connection::SshServer::allowRootLogin,
             py::arg("allow"),
             R"(Enables or disables root login.

Args:
    allow: True to permit root login, False to deny
)")
        .def("is_root_login_allowed",
             &atom::connection::SshServer::isRootLoginAllowed,
             R"(Checks if root login is allowed.

Returns:
    True if root login is permitted, False otherwise
)")
        .def("set_password_authentication",
             &atom::connection::SshServer::setPasswordAuthentication,
             py::arg("enable"),
             R"(Enables or disables password authentication.

Args:
    enable: True to enable password authentication
)")
        .def("is_password_authentication_enabled",
             &atom::connection::SshServer::isPasswordAuthenticationEnabled,
             R"(Checks if password authentication is enabled.

Returns:
    True if password authentication is enabled
)")
        .def("set_subsystem", &atom::connection::SshServer::setSubsystem,
             py::arg("name"), py::arg("command"),
             R"(Sets a subsystem for handling a specific command.

Args:
    name: The name of the subsystem
    command: The command that the subsystem will execute
)")
        .def("remove_subsystem", &atom::connection::SshServer::removeSubsystem,
             py::arg("name"),
             R"(Removes a previously set subsystem.

Args:
    name: The name of the subsystem to remove
)")
        .def("get_subsystem", &atom::connection::SshServer::getSubsystem,
             py::arg("name"),
             R"(Gets the command associated with a subsystem.

Args:
    name: The name of the subsystem

Returns:
    The command associated with the subsystem
)")
        .def("get_active_connections",
             &atom::connection::SshServer::getActiveConnections,
             R"(Gets all active connections to the server.

Returns:
    List of SshConnection objects representing active connections
)")
        .def("disconnect_client",
             &atom::connection::SshServer::disconnectClient,
             py::arg("session_id"),
             R"(Disconnects a specific client session.

Args:
    session_id: The unique session ID to disconnect

Returns:
    True if client was disconnected, False if session not found
)")
        .def("set_max_auth_attempts",
             &atom::connection::SshServer::setMaxAuthAttempts,
             py::arg("max_attempts"),
             R"(Sets the maximum number of authentication attempts allowed.

Args:
    max_attempts: Maximum number of attempts before blocking
)")
        .def("get_max_auth_attempts",
             &atom::connection::SshServer::getMaxAuthAttempts,
             R"(Gets the maximum number of authentication attempts allowed.

Returns:
    The current maximum authentication attempts setting
)")
        .def("set_max_connections",
             &atom::connection::SshServer::setMaxConnections,
             py::arg("max_connections"),
             R"(Sets the maximum number of concurrent connections allowed.

Args:
    max_connections: Maximum number of simultaneous connections
)")
        .def("get_max_connections",
             &atom::connection::SshServer::getMaxConnections,
             R"(Gets the maximum number of concurrent connections allowed.

Returns:
    The current maximum connections setting
)")
        .def("set_login_grace_time",
             &atom::connection::SshServer::setLoginGraceTime,
             py::arg("seconds"),
             R"(Sets the login grace time in seconds.

Args:
    seconds: Number of seconds to wait for authentication
)")
        .def("get_login_grace_time",
             &atom::connection::SshServer::getLoginGraceTime,
             R"(Gets the current login grace time.

Returns:
    The login grace time in seconds
)")
        .def("set_idle_timeout", &atom::connection::SshServer::setIdleTimeout,
             py::arg("seconds"),
             R"(Sets the server's idle timeout.

Args:
    seconds: Number of seconds after which idle connections are terminated
)")
        .def("get_idle_timeout", &atom::connection::SshServer::getIdleTimeout,
             R"(Gets the current idle timeout.

Returns:
    The idle timeout in seconds
)")
        .def("allow_ip_address", &atom::connection::SshServer::allowIpAddress,
             py::arg("ip_address"),
             R"(Adds an IP address to the allowed list.

Args:
    ip_address: The IP address to allow
)")
        .def("deny_ip_address", &atom::connection::SshServer::denyIpAddress,
             py::arg("ip_address"),
             R"(Adds an IP address to the denied list.

Args:
    ip_address: The IP address to deny
)")
        .def("is_ip_address_allowed",
             &atom::connection::SshServer::isIpAddressAllowed,
             py::arg("ip_address"),
             R"(Checks if an IP address is allowed to connect.

Args:
    ip_address: The IP address to check

Returns:
    True if the IP is allowed, False otherwise
)")
        .def("allow_agent_forwarding",
             &atom::connection::SshServer::allowAgentForwarding,
             py::arg("allow"),
             R"(Sets whether to allow agent forwarding.

Args:
    allow: True to allow agent forwarding
)")
        .def("is_agent_forwarding_allowed",
             &atom::connection::SshServer::isAgentForwardingAllowed,
             R"(Checks if agent forwarding is allowed.

Returns:
    True if agent forwarding is allowed
)")
        .def("allow_tcp_forwarding",
             &atom::connection::SshServer::allowTcpForwarding, py::arg("allow"),
             R"(Sets whether to allow TCP forwarding.

Args:
    allow: True to allow TCP forwarding
)")
        .def("is_tcp_forwarding_allowed",
             &atom::connection::SshServer::isTcpForwardingAllowed,
             R"(Checks if TCP forwarding is allowed.

Returns:
    True if TCP forwarding is allowed
)")
        .def("set_log_level", &atom::connection::SshServer::setLogLevel,
             py::arg("level"),
             R"(Sets the server's log level.

Args:
    level: The desired log level
)")
        .def("get_log_level", &atom::connection::SshServer::getLogLevel,
             R"(Gets the server's current log level.

Returns:
    The current log level
)")
        .def("set_log_file", &atom::connection::SshServer::setLogFile,
             py::arg("log_file"),
             R"(Sets the log file path.

Args:
    log_file: Path where logs should be written
)")
        .def("get_log_file", &atom::connection::SshServer::getLogFile,
             R"(Gets the current log file path.

Returns:
    The log file path
)")
        .def("generate_host_key", &atom::connection::SshServer::generateHostKey,
             py::arg("key_type"), py::arg("key_size"), py::arg("output_path"),
             R"(Generates a new host key.

Args:
    key_type: The type of key to generate (e.g., "rsa", "ed25519")
    key_size: The size of the key in bits
    output_path: Where to save the generated key

Returns:
    True if key was generated successfully, False otherwise
)")
        .def("verify_configuration",
             &atom::connection::SshServer::verifyConfiguration,
             R"(Verifies the current server configuration.

Returns:
    True if the configuration is valid, False if there are issues
)")
        .def("get_configuration_issues",
             &atom::connection::SshServer::getConfigurationIssues,
             R"(Gets detailed information about configuration problems.

Returns:
    List of strings describing configuration issues
)")
        .def("set_ciphers", &atom::connection::SshServer::setCiphers,
             py::arg("ciphers"),
             R"(Sets allowed ciphers for encryption.

Args:
    ciphers: Comma-separated list of cipher algorithms
)")
        .def("get_ciphers", &atom::connection::SshServer::getCiphers,
             R"(Gets the currently allowed ciphers.

Returns:
    String containing the allowed ciphers
)")
        .def("set_macs", &atom::connection::SshServer::setMACs, py::arg("macs"),
             R"(Sets allowed MACs (Message Authentication Codes).

Args:
    macs: Comma-separated list of MAC algorithms
)")
        .def("get_macs", &atom::connection::SshServer::getMACs,
             R"(Gets the currently allowed MACs.

Returns:
    String containing the allowed MACs
)")
        .def("set_kex_algorithms",
             &atom::connection::SshServer::setKexAlgorithms,
             py::arg("kex_algorithms"),
             R"(Sets allowed key exchange algorithms.

Args:
    kex_algorithms: Comma-separated list of key exchange algorithms
)")
        .def("get_kex_algorithms",
             &atom::connection::SshServer::getKexAlgorithms,
             R"(Gets the currently allowed key exchange algorithms.

Returns:
    String containing the allowed key exchange algorithms
)")
        .def(
            "on_new_connection",
            [](atom::connection::SshServer& self, py::function callback) {
                self.onNewConnection(
                    [callback = std::move(callback)](
                        const atom::connection::SshConnection& conn) {
                        py::gil_scoped_acquire gil;
                        try {
                            callback(conn);
                        } catch (const py::error_already_set& e) {
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Registers a callback for new connection events.

Args:
    callback: Function that takes an SshConnection parameter
)")
        .def(
            "on_connection_closed",
            [](atom::connection::SshServer& self, py::function callback) {
                self.onConnectionClosed(
                    [callback = std::move(callback)](
                        const atom::connection::SshConnection& conn) {
                        py::gil_scoped_acquire gil;
                        try {
                            callback(conn);
                        } catch (const py::error_already_set& e) {
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Registers a callback for connection closed events.

Args:
    callback: Function that takes an SshConnection parameter
)")
        .def(
            "on_authentication_failure",
            [](atom::connection::SshServer& self, py::function callback) {
                self.onAuthenticationFailure(
                    [callback = std::move(callback)](
                        const std::string& username, const std::string& ip) {
                        py::gil_scoped_acquire gil;
                        try {
                            callback(username, ip);
                        } catch (const py::error_already_set& e) {
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Registers a callback for authentication failure events.

Args:
    callback: Function that takes (username, ip_address) parameters
)")
        .def("get_statistics", &atom::connection::SshServer::getStatistics,
             R"(Gets server statistics.

Returns:
    Dictionary of statistic name to value
)")
        .def("get_server_version",
             &atom::connection::SshServer::getServerVersion,
             R"(Gets the server version.

Returns:
    The server version string
)")
        .def("set_server_version",
             &atom::connection::SshServer::setServerVersion, py::arg("version"),
             R"(Sets the server version string.

Args:
    version: The server version string to use
)")
        .def(
            "__enter__",
            [](atom::connection::SshServer& self)
                -> atom::connection::SshServer& {
                self.start();
                return self;
            },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::connection::SshServer& self, py::object, py::object,
               py::object) { self.stop(); },
            "Ensure server is stopped when exiting context");
}
