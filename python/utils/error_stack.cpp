#include "atom/utils/error_stack.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <ctime>
#include <sstream>

namespace py = pybind11;

// Format timestamp as human readable string
std::string format_timestamp(time_t timestamp) {
    char buffer[64];
    std::tm* timeinfo = std::localtime(&timestamp);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

PYBIND11_MODULE(error_stack, m) {
    m.doc() = "Error tracking and management module for the atom package";

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

    // Bind ErrorInfo struct
    py::class_<atom::error::ErrorInfo>(
        m, "ErrorInfo",
        R"(Error information structure containing details about an error.

This class holds comprehensive information about an error, including the error message,
the module and function where it occurred, line number, file name, and timestamp.

Examples:
    >>> from atom.utils import error_stack
    >>> # ErrorInfo objects are typically created by the ErrorStack class
    >>> stack = error_stack.ErrorStack()
    >>> stack.insert_error("File not found", "IO", "readFile", 42, "file_io.cpp")
    >>> error = stack.get_latest_error()
    >>> print(error.error_message)
    'File not found'
)")
        .def(py::init<>())
        .def_readwrite("error_message", &atom::error::ErrorInfo::errorMessage,
                      "The error message")
        .def_readwrite("module_name", &atom::error::ErrorInfo::moduleName,
                      "Module name where the error occurred")
        .def_readwrite("function_name", &atom::error::ErrorInfo::functionName,
                      "Function name where the error occurred")
        .def_readwrite("line", &atom::error::ErrorInfo::line,
                      "Line number where the error occurred")
        .def_readwrite("file_name", &atom::error::ErrorInfo::fileName,
                      "File name where the error occurred")
        .def_property_readonly("timestamp", 
                             [](const atom::error::ErrorInfo& self) {
        return self.timestamp;
                             },
                             "Timestamp when the error occurred (seconds since epoch)")
        .def_property_readonly("formatted_time", 
                             [](const atom::error::ErrorInfo& self) {
        return format_timestamp(self.timestamp);
                             },
                             "Human-readable formatted timestamp")
        .def_readwrite("uuid", &atom::error::ErrorInfo::uuid,
                      "UUID of the error")
        .def("__eq__", [](const atom::error::ErrorInfo& self, 
                          const atom::error::ErrorInfo& other) {
    // filepath: d:\msys64\home\qwdma\Atom\python\utils\error_stack.cpp
#include "atom/utils/error_stack.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <ctime>
#include <sstream>
        namespace py = pybind11;

        // Format timestamp as human readable string
        std::string format_timestamp(time_t timestamp) {
            char buffer[64];
            std::tm* timeinfo = std::localtime(&timestamp);
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                          timeinfo);
            return std::string(buffer);
        }

        PYBIND11_MODULE(error_stack, m) {
            m.doc() =
                "Error tracking and management module for the atom package";

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

            // Bind ErrorInfo struct
    py::class_<atom::error::ErrorInfo>(
        m, "ErrorInfo",
        R"(Error information structure containing details about an error.

This class holds comprehensive information about an error, including the error message,
the module and function where it occurred, line number, file name, and timestamp.

Examples:
    >>> from atom.utils import error_stack
    >>> # ErrorInfo objects are typically created by the ErrorStack class
    >>> stack = error_stack.ErrorStack()
    >>> stack.insert_error("File not found", "IO", "readFile", 42, "file_io.cpp")
    >>> error = stack.get_latest_error()
    >>> print(error.error_message)
    'File not found'
)")
        .def(py::init<>())
        .def_readwrite("error_message", &atom::error::ErrorInfo::errorMessage,
                      "The error message")
        .def_readwrite("module_name", &atom::error::ErrorInfo::moduleName,
                      "Module name where the error occurred")
        .def_readwrite("function_name", &atom::error::ErrorInfo::functionName,
                      "Function name where the error occurred")
        .def_readwrite("line", &atom::error::ErrorInfo::line,
                      "Line number where the error occurred")
        .def_readwrite("file_name", &atom::error::ErrorInfo::fileName,
                      "File name where the error occurred")
        .def_property_readonly("timestamp", 
                             [](const atom::error::ErrorInfo& self) {
                return self.timestamp;
                             },
                             "Timestamp when the error occurred (seconds since epoch)")
        .def_property_readonly("formatted_time", 
                             [](const atom::error::ErrorInfo& self) {
                return format_timestamp(self.timestamp);
                             },
                             "Human-readable formatted timestamp")
        .def_readwrite("uuid", &atom::error::ErrorInfo::uuid,
                      "UUID of the error")
        .def("__eq__", [](const atom::error::ErrorInfo& self, 
                          const atom::error::ErrorInfo& other) {
