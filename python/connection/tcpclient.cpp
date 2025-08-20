#include "atom/connection/async_tcpclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

/**
 * @brief Registers exception translations for the TCP client module.
 *
 * This function sets up proper exception handling to translate C++ exceptions
 * to appropriate Python exceptions for better error reporting.
 *
 * @param m The pybind11 module to register exceptions for
 */
void registerExceptionTranslations(py::module_& m) {
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
}

/**
 * @brief Binds the ConnectionState enum to Python.
 *
 * This function creates Python bindings for the ConnectionState enum which
 * represents different states of the TCP client connection.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionState(py::module_& m) {
    py::enum_<atom::async::connection::ConnectionState>(
        m, "ConnectionState", "States of the TCP client connection")
        .value("Disconnected",
               atom::async::connection::ConnectionState::Disconnected,
               "Client is disconnected from the server")
        .value("Connecting",
               atom::async::connection::ConnectionState::Connecting,
               "Client is attempting to connect to the server")
        .value("Connected", atom::async::connection::ConnectionState::Connected,
               "Client is successfully connected to the server")
        .value("Reconnecting",
               atom::async::connection::ConnectionState::Reconnecting,
               "Client is attempting to reconnect after disconnection")
        .value("Failed", atom::async::connection::ConnectionState::Failed,
               "Connection attempt has failed")
        .export_values();
}

/**
 * @brief Binds the ConnectionConfig struct to Python.
 *
 * This function creates Python bindings for the ConnectionConfig struct which
 * provides various settings to control TCP connection behavior.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionConfig(py::module_& m) {
    py::class_<atom::async::connection::ConnectionConfig>(
        m, "ConnectionConfig",
        R"(Configuration for TCP client connections.

This structure provides various settings to control connection behavior,
including timeouts, SSL settings, and reconnection parameters.

Examples:
    >>> from atom.connection.tcpclient import ConnectionConfig
    >>> config = ConnectionConfig()
    >>> config.use_ssl = True
    >>> config.connect_timeout = 10000  # 10 seconds
    >>> config.reconnect_attempts = 5
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("use_ssl",
                       &atom::async::connection::ConnectionConfig::use_ssl,
                       "Whether to use SSL/TLS encryption")
        .def_readwrite("verify_ssl",
                       &atom::async::connection::ConnectionConfig::verify_ssl,
                       "Whether to verify SSL certificates")
        .def_readwrite(
            "connect_timeout",
            &atom::async::connection::ConnectionConfig::connect_timeout,
            "Timeout for connection attempts in milliseconds")
        .def_readwrite("read_timeout",
                       &atom::async::connection::ConnectionConfig::read_timeout,
                       "Timeout for read operations in milliseconds")
        .def_readwrite(
            "write_timeout",
            &atom::async::connection::ConnectionConfig::write_timeout,
            "Timeout for write operations in milliseconds")
        .def_readwrite("keep_alive",
                       &atom::async::connection::ConnectionConfig::keep_alive,
                       "Whether to use TCP keep-alive")
        .def_readwrite(
            "reconnect_attempts",
            &atom::async::connection::ConnectionConfig::reconnect_attempts,
            "Number of reconnection attempts")
        .def_readwrite(
            "reconnect_delay",
            &atom::async::connection::ConnectionConfig::reconnect_delay,
            "Delay between reconnection attempts in milliseconds")
        .def_readwrite(
            "heartbeat_interval",
            &atom::async::connection::ConnectionConfig::heartbeat_interval,
            "Interval between heartbeat messages in milliseconds")
        .def_readwrite(
            "receive_buffer_size",
            &atom::async::connection::ConnectionConfig::receive_buffer_size,
            "Size of the receive buffer in bytes")
        .def_readwrite(
            "auto_reconnect",
            &atom::async::connection::ConnectionConfig::auto_reconnect,
            "Whether to automatically reconnect on disconnection")
        .def_readwrite(
            "ssl_certificate_path",
            &atom::async::connection::ConnectionConfig::ssl_certificate_path,
            "Path to the SSL certificate file")
        .def_readwrite(
            "ssl_private_key_path",
            &atom::async::connection::ConnectionConfig::ssl_private_key_path,
            "Path to the SSL private key file")
        .def_readwrite(
            "ca_certificate_path",
            &atom::async::connection::ConnectionConfig::ca_certificate_path,
            "Path to the Certificate Authority certificate file");
}

/**
 * @brief Binds the ProxyConfig struct to Python.
 *
 * This function creates Python bindings for the ProxyConfig struct which
 * provides settings for connecting through a proxy server.
 *
 * @param m The pybind11 module to bind to
 */
void bindProxyConfig(py::module_& m) {
    py::class_<atom::async::connection::ProxyConfig>(
        m, "ProxyConfig",
        R"(Configuration for connection proxy.

This structure provides settings for connecting through a proxy server.

Examples:
    >>> from atom.connection.tcpclient import ProxyConfig
    >>> proxy = ProxyConfig()
    >>> proxy.host = "proxy.example.com"
    >>> proxy.port = 8080
    >>> proxy.enabled = True
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("host", &atom::async::connection::ProxyConfig::host,
                       "Proxy server hostname or IP address")
        .def_readwrite("port", &atom::async::connection::ProxyConfig::port,
                       "Proxy server port")
        .def_readwrite("username",
                       &atom::async::connection::ProxyConfig::username,
                       "Username for proxy authentication")
        .def_readwrite("password",
                       &atom::async::connection::ProxyConfig::password,
                       "Password for proxy authentication")
        .def_readwrite("enabled",
                       &atom::async::connection::ProxyConfig::enabled,
                       "Whether to use the proxy");
}

/**
 * @brief Binds the ConnectionStats struct to Python.
 *
 * This function creates Python bindings for the ConnectionStats struct which
 * provides various metrics about connection usage and performance.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionStats(py::module_& m) {
    py::class_<atom::async::connection::ConnectionStats>(
        m, "ConnectionStats",
        R"(Statistics for the TCP client connection.

This structure provides various metrics about connection usage and performance.

Examples:
    >>> stats = client.get_stats()
    >>> print(f"Bytes received: {stats.total_bytes_received}")
    >>> print(f"Average latency: {stats.average_latency} ms")
)")
        .def(py::init<>(), "Default constructor")
        .def_readonly(
            "total_bytes_sent",
            &atom::async::connection::ConnectionStats::total_bytes_sent,
            "Total bytes sent over this connection")
        .def_readonly(
            "total_bytes_received",
            &atom::async::connection::ConnectionStats::total_bytes_received,
            "Total bytes received over this connection")
        .def_readonly(
            "connection_attempts",
            &atom::async::connection::ConnectionStats::connection_attempts,
            "Number of connection attempts made")
        .def_readonly(
            "successful_connections",
            &atom::async::connection::ConnectionStats::successful_connections,
            "Number of successful connections")
        .def_readonly(
            "failed_connections",
            &atom::async::connection::ConnectionStats::failed_connections,
            "Number of failed connection attempts")
        .def_readonly(
            "last_connected_time",
            &atom::async::connection::ConnectionStats::last_connected_time,
            "Time of last successful connection")
        .def_readonly(
            "last_activity_time",
            &atom::async::connection::ConnectionStats::last_activity_time,
            "Time of last send or receive activity")
        .def_readonly(
            "average_latency",
            &atom::async::connection::ConnectionStats::average_latency,
            "Average connection latency in milliseconds");
}

PYBIND11_MODULE(tcpclient, m) {
    m.doc() = "TCP client module for the atom package";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind core enums and data structures
    bindConnectionState(m);
    bindConnectionConfig(m);
    bindProxyConfig(m);
    bindConnectionStats(m);
    bindTcpClient(m);

    // Bind utility functions
    bindUtilityFunctions(m);

    // Add module documentation
    addModuleDocumentation(m);
}