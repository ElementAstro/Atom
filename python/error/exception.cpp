#include "atom/error/exception.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(exception, m) {
    m.doc() = "Better Exception Library for the atom package";

    // Base Exception class
    py::class_<atom::error::Exception, std::exception>(
        m, "Exception",
        R"(Custom exception class with detailed information about the error.

This exception class captures comprehensive information including file location,
line number, function name, and stack trace where the exception occurred.

Examples:
    >>> from atom.error import Exception
    >>> try:
    ...     raise Exception("test.cpp", 42, "test_function", "Something went wrong")
    ... except Exception as e:
    ...     print(e.what())
    ...     print(e.get_file())
    ...     print(e.get_line())
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"),
             R"(Constructs an Exception object.

Args:
    file (str): The file where the exception occurred
    line (int): The line number in the file where the exception occurred
    func (str): The function where the exception occurred
    message (str): Error message
)")
        .def("what", &atom::error::Exception::what,
             R"(Returns a C-style string describing the exception.

Returns:
    str: A string describing the exception
)")
        .def("get_file", &atom::error::Exception::getFile,
             R"(Gets the file where the exception occurred.

Returns:
    str: The file where the exception occurred
)")
        .def("get_line", &atom::error::Exception::getLine,
             R"(Gets the line number where the exception occurred.

Returns:
    int: The line number where the exception occurred
)")
        .def("get_function", &atom::error::Exception::getFunction,
             R"(Gets the function where the exception occurred.

Returns:
    str: The function where the exception occurred
)")
        .def("get_message", &atom::error::Exception::getMessage,
             R"(Gets the message associated with the exception.

Returns:
    str: The message associated with the exception
)")
        .def("get_thread_id",
             [](const atom::error::Exception& ex) {
                 std::ostringstream oss;
                 oss << ex.getThreadId();
                 return oss.str();
             },
             R"(Gets the ID of the thread where the exception occurred.

Returns:
    str: String representation of the thread ID
)");

    // SystemErrorException class
    py::class_<atom::error::SystemErrorException, atom::error::Exception>(
        m, "SystemErrorException",
        R"(System error exception class.

This exception class represents system-level errors with error codes
from the operating system.

Examples:
    >>> from atom.error import SystemErrorException
    >>> try:
    ...     raise SystemErrorException("test.cpp", 42, "test_function", 2, "File not found")
    ... except SystemErrorException as e:
    ...     print(e.what())
)")
        .def(py::init<const char*, int, const char*, int, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), 
             py::arg("error_code"), py::arg("message"),
             R"(Constructs a SystemErrorException.

Args:
    file (str): The file where the exception occurred
    line (int): The line number
    func (str): The function name
    error_code (int): System error code
    message (str): Error message
)");

    // RuntimeError class
    py::class_<atom::error::RuntimeError, atom::error::Exception>(
        m, "RuntimeError",
        R"(Runtime error exception.

General runtime error that occurs during program execution.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // LogicError class
    py::class_<atom::error::LogicError, atom::error::Exception>(
        m, "LogicError",
        R"(Logic error exception.

Represents errors in program logic that could be detected before runtime.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // UnlawfulOperation class
    py::class_<atom::error::UnlawfulOperation, atom::error::Exception>(
        m, "UnlawfulOperation",
        R"(Unlawful operation exception.

Represents an operation that is not allowed in the current state.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // OutOfRange class
    py::class_<atom::error::OutOfRange, atom::error::Exception>(
        m, "OutOfRange",
        R"(Out of range exception.

Represents an attempt to access an element outside valid range.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // OverflowException class
    py::class_<atom::error::OverflowException, atom::error::Exception>(
        m, "OverflowException",
        R"(Overflow exception.

Represents arithmetic overflow.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // UnderflowException class
    py::class_<atom::error::UnderflowException, atom::error::Exception>(
        m, "UnderflowException",
        R"(Underflow exception.

Represents arithmetic underflow.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // LengthException class
    py::class_<atom::error::LengthException, atom::error::Exception>(
        m, "LengthException",
        R"(Length exception.

Represents an error related to length constraints.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // Unkown class
    py::class_<atom::error::Unkown, atom::error::Exception>(
        m, "Unknown",
        R"(Unknown exception.

Represents an unknown or unclassified error.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // Object-related exceptions
    py::class_<atom::error::ObjectAlreadyExist, atom::error::Exception>(
        m, "ObjectAlreadyExist",
        R"(Object already exists exception.

Thrown when attempting to create an object that already exists.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::ObjectAlreadyInitialized, atom::error::Exception>(
        m, "ObjectAlreadyInitialized",
        R"(Object already initialized exception.

Thrown when attempting to initialize an already initialized object.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::ObjectNotExist, atom::error::Exception>(
        m, "ObjectNotExist",
        R"(Object does not exist exception.

Thrown when attempting to access a non-existent object.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::ObjectUninitialized, atom::error::Exception>(
        m, "ObjectUninitialized",
        R"(Object uninitialized exception.

Thrown when attempting to use an uninitialized object.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::SystemCollapse, atom::error::Exception>(
        m, "SystemCollapse",
        R"(System collapse exception.

Represents a critical system failure.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::NullPointer, atom::error::Exception>(
        m, "NullPointer",
        R"(Null pointer exception.

Thrown when a null pointer is dereferenced.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::NotFound, atom::error::Exception>(
        m, "NotFound",
        R"(Not found exception.

Thrown when a requested resource is not found.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // Argument-related exceptions
    py::class_<atom::error::WrongArgument, atom::error::Exception>(
        m, "WrongArgument",
        R"(Wrong argument exception.

Thrown when an argument has an incorrect value.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::InvalidArgument, atom::error::Exception>(
        m, "InvalidArgument",
        R"(Invalid argument exception.

Thrown when an argument is invalid.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::MissingArgument, atom::error::Exception>(
        m, "MissingArgument",
        R"(Missing argument exception.

Thrown when a required argument is missing.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // File-related exceptions
    py::class_<atom::error::FileNotFound, atom::error::Exception>(
        m, "FileNotFound",
        R"(File not found exception.

Thrown when a file cannot be found.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FileNotReadable, atom::error::Exception>(
        m, "FileNotReadable",
        R"(File not readable exception.

Thrown when a file cannot be read.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FileNotWritable, atom::error::Exception>(
        m, "FileNotWritable",
        R"(File not writable exception.

Thrown when a file cannot be written to.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToOpenFile, atom::error::Exception>(
        m, "FailToOpenFile",
        R"(Failed to open file exception.

Thrown when a file cannot be opened.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToCloseFile, atom::error::Exception>(
        m, "FailToCloseFile",
        R"(Failed to close file exception.

Thrown when a file cannot be closed.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToCreateFile, atom::error::Exception>(
        m, "FailToCreateFile",
        R"(Failed to create file exception.

Thrown when a file cannot be created.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToDeleteFile, atom::error::Exception>(
        m, "FailToDeleteFile",
        R"(Failed to delete file exception.

Thrown when a file cannot be deleted.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToCopyFile, atom::error::Exception>(
        m, "FailToCopyFile",
        R"(Failed to copy file exception.

Thrown when a file cannot be copied.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToMoveFile, atom::error::Exception>(
        m, "FailToMoveFile",
        R"(Failed to move file exception.

Thrown when a file cannot be moved.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToReadFile, atom::error::Exception>(
        m, "FailToReadFile",
        R"(Failed to read file exception.

Thrown when a file cannot be read.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToWriteFile, atom::error::Exception>(
        m, "FailToWriteFile",
        R"(Failed to write file exception.

Thrown when a file cannot be written to.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // Dynamic library exceptions
    py::class_<atom::error::FailToLoadDll, atom::error::Exception>(
        m, "FailToLoadDll",
        R"(Failed to load DLL exception.

Thrown when a dynamic library cannot be loaded.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToUnloadDll, atom::error::Exception>(
        m, "FailToUnloadDll",
        R"(Failed to unload DLL exception.

Thrown when a dynamic library cannot be unloaded.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToLoadSymbol, atom::error::Exception>(
        m, "FailToLoadSymbol",
        R"(Failed to load symbol exception.

Thrown when a symbol cannot be loaded from a dynamic library.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // Process exceptions
    py::class_<atom::error::FailToCreateProcess, atom::error::Exception>(
        m, "FailToCreateProcess",
        R"(Failed to create process exception.

Thrown when a process cannot be created.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::FailToTerminateProcess, atom::error::Exception>(
        m, "FailToTerminateProcess",
        R"(Failed to terminate process exception.

Thrown when a process cannot be terminated.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // JSON exceptions
    py::class_<atom::error::JsonParseError, atom::error::Exception>(
        m, "JsonParseError",
        R"(JSON parse error exception.

Thrown when JSON parsing fails.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::JsonValueError, atom::error::Exception>(
        m, "JsonValueError",
        R"(JSON value error exception.

Thrown when a JSON value is invalid or unexpected.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    // Network exceptions
    py::class_<atom::error::CurlInitializationError, atom::error::Exception>(
        m, "CurlInitializationError",
        R"(CURL initialization error exception.

Thrown when CURL library initialization fails.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));

    py::class_<atom::error::CurlRuntimeError, atom::error::Exception>(
        m, "CurlRuntimeError",
        R"(CURL runtime error exception.

Thrown when a CURL operation fails at runtime.
)")
        .def(py::init<const char*, int, const char*, std::string>(),
             py::arg("file"), py::arg("line"), py::arg("func"), py::arg("message"));
}

