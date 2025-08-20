#include "atom/web/utils.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

/**
 * @brief Registers exception translations for the web utilities module.
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
 * @brief Binds system initialization functions to Python.
 *
 * This function creates Python bindings for system-level initialization
 * functions, particularly Windows-specific networking setup.
 *
 * @param m The pybind11 module to bind to
 */
void bindSystemInitialization(py::module_& m) {
    m.def("initialize_windows_socket_api",
          &atom::web::initializeWindowsSocketAPI,
          R"(Initialize networking subsystem (Windows-specific).

This function initializes the Windows Socket API, which is necessary for network operations on Windows.
On other platforms, this function does nothing and returns True.

Returns:
    bool: True if initialization succeeded, False otherwise.

Raises:
    RuntimeError: If initialization fails with a specific error message.

Examples:
    >>> from atom.web.utils import initialize_windows_socket_api
    >>> initialize_windows_socket_api()
    True
)");
}

/**
 * @brief Binds port utility functions to Python.
 *
 * This function creates Python bindings for port-related utility functions
 * including port checking, process management, and async operations.
 *
 * @param m The pybind11 module to bind to
 */
void bindPortUtilities(py::module_& m) {
    // Basic port checking
    m.def(
        "is_port_in_use", [](int port) { return atom::web::isPortInUse(port); },
        py::arg("port"),
        R"(Check if a port is in use.

This function checks if a port is in use by attempting to bind a socket to the port.
If the socket can be bound, the port is not in use.

Args:
    port: The port number to check (0-65535).

Returns:
    bool: True if the port is in use, False otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If socket operations fail.

Examples:
    >>> from atom.web.utils import is_port_in_use
    >>> is_port_in_use(8080)
    False  # Port is available
)");

    // Process management on ports
    m.def(
        "check_and_kill_program_on_port",
        [](int port) { return atom::web::checkAndKillProgramOnPort(port); },
        py::arg("port"),
        R"(Check if there is any program running on the specified port and kill it if found.

This function checks if there is any program running on the specified port by querying the system.
If a program is found, it will be terminated.

Args:
    port: The port number to check (0-65535).

Returns:
    bool: True if a program was found and terminated, False otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If socket operations fail.
    OSError: If process termination fails.

Examples:
    >>> from atom.web.utils import check_and_kill_program_on_port
    >>> check_and_kill_program_on_port(8080)
    True  # Program found and killed
)");

    m.def(
        "get_process_id_on_port",
        [](int port) { return atom::web::getProcessIDOnPort(port); },
        py::arg("port"),
        R"(Get the process ID of the program running on a specific port.

Args:
    port: The port number to check (0-65535).

Returns:
    Optional[int]: The process ID if found, None otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If command execution fails.

Examples:
    >>> from atom.web.utils import get_process_id_on_port
    >>> pid = get_process_id_on_port(8080)
    >>> if pid is not None:
    ...     print(f"Process with ID {pid} is using port 8080")
    ... else:
    ...     print("No process is using port 8080")
)");

    // Async port checking
    m.def(
        "is_port_in_use_async",
        [](int port) {
            auto future = atom::web::isPortInUseAsync(port);
            return future.get();  // Wait for the result and return it
        },
        py::arg("port"),
        R"(Asynchronously check if a port is in use.

This function checks if a port is in use in a separate thread and returns the result.

Args:
    port: The port number to check (0-65535).

Returns:
    bool: True if the port is in use, False otherwise.

Raises:
    ValueError: If port is outside valid range.
    RuntimeError: If socket operations fail.

Examples:
    >>> from atom.web.utils import is_port_in_use_async
    >>> is_port_in_use_async(8080)
    False  # Port is available
)");
}

PYBIND11_MODULE(utils, m) {
    m.doc() = "Network utilities module for the atom package";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind different categories of network utilities
    bindSystemInitialization(m);
    bindPortUtilities(m);
    bindPortScanning(m);
    bindDnsAndIpUtilities(m);
    bindAddressInfoUtilities(m);
    bindConvenienceUtilities(m);

    // Add module documentation
    addModuleDocumentation(m);
}