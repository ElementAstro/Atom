#include "atom/connection/udpserver.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * @brief Registers exception translations for the UDP server module.
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
 * @brief Binds the SocketOption enum to Python.
 *
 * This function creates Python bindings for the SocketOption enum which
 * represents different socket configuration options for UDP servers.
 *
 * @param m The pybind11 module to bind to
 */
void bindSocketOption(py::module_& m) {
    py::enum_<atom::async::connection::SocketOption>(
        m, "SocketOption", "Socket options for UDP server configuration")
        .value("Broadcast", atom::async::connection::SocketOption::Broadcast,
               "Enable/disable broadcasting")
        .value("ReuseAddress",
               atom::async::connection::SocketOption::ReuseAddress,
               "Enable/disable address reuse")
        .value("ReceiveBufferSize",
               atom::async::connection::SocketOption::ReceiveBufferSize,
               "Set receive buffer size")
        .value("SendBufferSize",
               atom::async::connection::SocketOption::SendBufferSize,
               "Set send buffer size")
        .value("ReceiveTimeout",
               atom::async::connection::SocketOption::ReceiveTimeout,
               "Set receive timeout")
        .value("SendTimeout",
               atom::async::connection::SocketOption::SendTimeout,
               "Set send timeout")
        .export_values();
}

/**
 * @brief Binds the Statistics struct to Python.
 *
 * This function creates Python bindings for the Statistics struct which
 * provides metrics about UDP server activity and performance.
 *
 * @param m The pybind11 module to bind to
 */
void bindStatistics(py::module_& m) {
    py::class_<atom::async::connection::UdpSocketHub::Statistics>(
        m, "Statistics",
        R"(Statistics for monitoring UDP server activity.

This structure provides metrics about server usage, including message and byte counts.

Attributes:
    bytes_received: Total bytes received
    bytes_sent: Total bytes sent
    messages_received: Total number of messages received
    messages_sent: Total number of messages sent
    errors: Total number of errors encountered

Examples:
    >>> stats = server.get_statistics()
    >>> print(f"Received: {stats.bytes_received} bytes, {stats.messages_received} messages")
    >>> print(f"Sent: {stats.bytes_sent} bytes, {stats.messages_sent} messages")
    >>> print(f"Errors: {stats.errors}")
)")
        .def(py::init<>())
        .def_readwrite(
            "bytes_received",
            &atom::async::connection::UdpSocketHub::Statistics::bytesReceived,
            "Total bytes received by the server")
        .def_readwrite(
            "bytes_sent",
            &atom::async::connection::UdpSocketHub::Statistics::bytesSent,
            "Total bytes sent by the server")
        .def_readwrite("messages_received",
                       &atom::async::connection::UdpSocketHub::Statistics::
                           messagesReceived,
                       "Total number of messages received")
        .def_readwrite(
            "messages_sent",
            &atom::async::connection::UdpSocketHub::Statistics::messagesSent,
            "Total number of messages sent")
        .def_readwrite(
            "errors",
            &atom::async::connection::UdpSocketHub::Statistics::errors,
            "Total number of errors encountered");
}

PYBIND11_MODULE(udpserver, m) {
    m.doc() = "UDP server module for the atom package";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind core enums and data structures
    bindSocketOption(m);
    bindStatistics(m);
    bindUdpSocketHub(m);

    // Bind factory functions
    bindFactoryFunctions(m);

    // Add module documentation
    addModuleDocumentation(m);
}
