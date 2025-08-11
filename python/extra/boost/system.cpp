#include "atom/extra/boost/system.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for Result template specializations
template <typename T>
void declare_result_class(py::module& m, const std::string& name);

// Template for registering commonly used Result types
template <typename T>
void register_result_type(py::module& m, const std::string& name) {
    std::string result_name = "Result" + name;
    declare_result_class<T>(m, result_name);
}

PYBIND11_MODULE(system, m) {
    m.doc() = "Boost System wrapper module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::extra::boost::Exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Error class binding
    py::class_<atom::extra::boost::Error>(
        m, "Error",
        R"(A wrapper class for Boost.System error codes.

This class represents an error code with a value and associated category.

Examples:
    >>> from atom.extra.boost import system
    >>> error = system.Error(1, system.generic_category())
    >>> print(error.message())
    'Operation not permitted'
)")
        .def(py::init<>(),
             "Default constructor creating an error code indicating success.")
        .def(py::init<int, const ::boost::system::error_category&>(),
             py::arg("error_value"), py::arg("error_category"),
             "Constructs an Error from an error value and category.")
        .def("value", &atom::extra::boost::Error::value,
             "Gets the error value.")
        .def("category", &atom::extra::boost::Error::category,
             py::return_value_policy::reference, "Gets the error category.")
        .def("message", &atom::extra::boost::Error::message,
             "Gets the error message.")
        .def(
            "__bool__",
            [](const atom::extra::boost::Error& e) {
                return static_cast<bool>(e);
            },
            "Checks if the error code is valid (indicates an error).")
        .def("__eq__", &atom::extra::boost::Error::operator==, py::arg("other"),
             "Equality comparison.")
        .def("__ne__", &atom::extra::boost::Error::operator!=, py::arg("other"),
             "Inequality comparison.")
        .def("__str__", &atom::extra::boost::Error::message,
             "String representation showing the error message.")
        .def("__repr__", [](const atom::extra::boost::Error& e) -> std::string {
            if (e) {
                return "Error(" + std::to_string(e.value()) + ", '" +
                       e.message() + "')";
            } else {
                return std::string("Error()");
            }
        });

    // Exception class binding
    py::class_<atom::extra::boost::Exception, std::exception>(
        m, "Exception",
        R"(A custom exception class for handling errors.

This exception wraps an Error object and can be thrown and caught in both C++ and Python.

Examples:
    >>> from atom.extra.boost import system
    >>> try:
    ...     error = system.Error(1, system.generic_category())
    ...     raise system.Exception(error)
    ... except system.Exception as e:
    ...     print(e)
    Operation not permitted
)")
        .def(py::init<const atom::extra::boost::Error&>(), py::arg("error"),
             "Constructs an Exception from an Error object.")
        .def("error", &atom::extra::boost::Exception::error,
             "Gets the associated Error.")
        .def("__str__",
             [](const atom::extra::boost::Exception& e) { return e.what(); });

    // Register Result specializations for common types
    register_result_type<int>(m, "Int");
    register_result_type<double>(m, "Double");
    register_result_type<std::string>(m, "String");
    register_result_type<bool>(m, "Bool");
    register_result_type<py::object>(m, "Object");

    // Result<void> specialization
    declare_result_class<void>(m, "ResultVoid");

    // Publish default type aliases
    m.attr("Result") =
        m.attr("ResultObject");  // Default Result is ResultObject

    // Global helper functions
    m.def(
        "make_result",
        [](const py::function& func) {
            try {
                py::object result = func();
                return atom::extra::boost::Result<py::object>(result);
            } catch (const atom::extra::boost::Exception& e) {
                return atom::extra::boost::Result<py::object>(e.error());
            } catch (const std::exception& e) {
                return atom::extra::boost::Result<py::object>(
                    atom::extra::boost::Error(
                        ::boost::system::errc::invalid_argument,
                        ::boost::system::generic_category()));
            }
        },
        py::arg("func"),
        R"(Creates a Result from a function.

This function executes the provided function and wraps its return value in a Result object.
If the function throws an exception, it's caught and converted to an Error.

Args:
    func: The function to execute.

Returns:
    A Result object containing either the function's return value or an error.

Examples:
    >>> from atom.extra.boost import system
    >>> def success_func():
    ...     return "Success!"
    >>> result = system.make_result(success_func)
    >>> print(result.value())
    Success!

    >>> def error_func():
    ...     raise ValueError("Something went wrong")
    >>> result = system.make_result(error_func)
    >>> print(result.has_value())
    False
)");

    // Add error category functions - 使用引用来传递category对象
    m.def(
        "generic_category",
        []() -> const ::boost::system::error_category& {
            return ::boost::system::generic_category();
        },
        py::return_value_policy::reference,
        "Returns the generic error category.");

    m.def(
        "system_category",
        []() -> const ::boost::system::error_category& {
            return ::boost::system::system_category();
        },
        py::return_value_policy::reference,
        "Returns the system error category.");

    // Add commonly used error values as constants
    using namespace ::boost::system::errc;
    m.attr("SUCCESS") = success;
    m.attr("INVALID_ARGUMENT") = invalid_argument;
    m.attr("NO_SUCH_FILE_OR_DIRECTORY") = no_such_file_or_directory;
    m.attr("PERMISSION_DENIED") = permission_denied;
    m.attr("OPERATION_NOT_PERMITTED") = operation_not_permitted;
    m.attr("RESOURCE_UNAVAILABLE_TRY_AGAIN") = resource_unavailable_try_again;
    // Add more constants as needed...
}

// Template implementation for Result class binding
template <typename T>
void declare_result_class(py::module& m, const std::string& name) {
    using ResultT = atom::extra::boost::Result<T>;

    // Handle void specialization differently
    if constexpr (std::is_same_v<T, void>) {
        py::class_<ResultT>(
            m, name.c_str(),
            R"(A class for handling results with potential errors for void functions.

This specialization is used for functions that don't return a value but might fail.

Examples:
    >>> from atom.extra.boost import system
    >>> # Creating a successful void result
    >>> result = system.ResultVoid()
    >>> print(result.has_value())
    True

    >>> # Creating a failed void result
    >>> error_result = system.ResultVoid(system.Error(1, system.generic_category()))
    >>> print(error_result.has_value())
    False
)")
            .def(py::init<>(), "Constructs a successful Result<void>.")
            .def(py::init<atom::extra::boost::Error>(), py::arg("error"),
                 "Constructs a Result<void> with an Error.")
            .def("has_value", &ResultT::hasValue,
                 "Checks if the Result has a value (is successful).")
            .def("error",
                 static_cast<const atom::extra::boost::Error& (ResultT::*)()
                                 const&>(&ResultT::error),
                 py::return_value_policy::reference,
                 "Gets the associated Error.")
            .def("__bool__", &ResultT::operator bool,
                 "Checks if the Result has a value (is successful).");
    } else {
        py::class_<ResultT>(
            m, name.c_str(),
            R"(A class template for handling results with potential errors.

This class either contains a value of the specified type or an error.

Examples:
    >>> from atom.extra.boost import system
    >>> # Creating a successful result
    >>> result = system.ResultInt(42)
    >>> print(result.value())
    42

    >>> # Creating a failed result
    >>> error_result = system.ResultInt(system.Error(1, system.generic_category()))
    >>> print(error_result.has_value())
    False
)")
            .def(py::init<T>(), py::arg("value"),
                 "Constructs a Result with a value.")
            .def(py::init<atom::extra::boost::Error>(), py::arg("error"),
                 "Constructs a Result with an Error.")
            .def("has_value", &ResultT::hasValue,
                 "Checks if the Result has a value.")
            .def("value",
                 static_cast<const T& (ResultT::*)() const&>(&ResultT::value),
                 "Gets the result value or throws an exception if there is an "
                 "error.")
            .def("error",
                 static_cast<const atom::extra::boost::Error& (ResultT::*)()
                                 const&>(&ResultT::error),
                 py::return_value_policy::reference,
                 "Gets the associated Error.")
            .def(
                "value_or",
                [](const ResultT& r, T default_value) {
                    return r.valueOr(default_value);
                },
                py::arg("default_value"),
                "Gets the result value or a default value.")
            .def(
                "map",
                [](const ResultT& r, py::function func)
                    -> atom::extra::boost::Result<py::object> {
                    if (r.hasValue()) {
                        try {
                            py::object result = func(r.value());
                            return atom::extra::boost::Result<py::object>(
                                result);
                        } catch (const std::exception& e) {
                            return atom::extra::boost::Result<py::object>(
                                atom::extra::boost::Error(
                                    ::boost::system::errc::invalid_argument,
                                    ::boost::system::generic_category()));
                        }
                    }
                    return atom::extra::boost::Result<py::object>(r.error());
                },
                py::arg("func"),
                "Applies a function to the result value if it exists.")
            .def(
                "and_then",
                [](const ResultT& r, py::function func) -> py::object {
                    if (r.hasValue()) {
                        try {
                            py::object result = func(r.value());
                            // Check if the result is already a Result
                            if (py::isinstance<
                                    atom::extra::boost::Result<py::object>>(
                                    result)) {
                                return result;
                            }
                            return py::cast(
                                atom::extra::boost::Result<py::object>(result));
                        } catch (const std::exception& e) {
                            return py::cast(
                                atom::extra::boost::Result<py::object>(
                                    atom::extra::boost::Error(
                                        ::boost::system::errc::invalid_argument,
                                        ::boost::system::generic_category())));
                        }
                    }
                    return py::cast(
                        atom::extra::boost::Result<py::object>(r.error()));
                },
                py::arg("func"),
                "Applies a function to the result value if it exists.")
            .def("__bool__", &ResultT::operator bool,
                 "Checks if the Result has a value.");
        /*
        TODO: Fix the __str__ method to return a string representation of the Result object.
        .def("__str__", [](const ResultT& r) {
        if (r.hasValue()) {
            // Cast the value to py::object first
            py::object value_obj = py::cast(r.value());
            // Then convert the py::object to its Python string representation
    (py::str) py::str value_str_obj = py::str(value_obj);
            // Finally, cast the py::str to std::string
            std::string value_cpp_str = value_str_obj.cast<std::string>();
            return "Result(value=" + value_cpp_str + ")";
        } else {
            return "Result(error=" + r.error().message() + ")";
        }
    });
        */
    }
}
