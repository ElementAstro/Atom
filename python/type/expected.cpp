#include "atom/type/expected.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

// 异常类
class ExpectedError : public std::runtime_error {
public:
    explicit ExpectedError(const std::string& msg) : std::runtime_error(msg) {}
};

// 错误类型转换的辅助函数
template <typename T>
py::object error_to_py(const T& error) {
    return py::str(error);
}

// 实现一个可被Python访问的Expected类模板
template <typename T, typename E = std::string>
py::class_<atom::type::expected<T, E>> declare_expected(
    py::module& m, const std::string& name) {
    using ExpectedType = atom::type::expected<T, E>;
    using ErrorType = atom::type::Error<E>;
    using UnexpectedType = atom::type::unexpected<E>;

    std::string class_name = "Expected" + name;
    std::string class_doc =
        "Expected value container with " + name +
        " value type.\n\n"
        "Represents a value that may either contain a valid value of type " +
        name + " or an error.";

    return py::class_<ExpectedType>(m, class_name.c_str(), class_doc.c_str())
        .def(py::init<>(),
             "Constructs an Expected object with a default value.")
        .def(py::init<const T&>(), py::arg("value"),
             "Constructs an Expected object with the given value.")
        .def(py::init<T&&>(), py::arg("value"),
             "Constructs an Expected object with the given value (move).")
        .def(py::init<const ErrorType&>(), py::arg("error"),
             "Constructs an Expected object with the given error.")
        .def(py::init<ErrorType&&>(), py::arg("error"),
             "Constructs an Expected object with the given error (move).")
        .def(py::init<const UnexpectedType&>(), py::arg("unexpected"),
             "Constructs an Expected object with the given unexpected value.")
        .def(py::init<UnexpectedType&&>(), py::arg("unexpected"),
             "Constructs an Expected object with the given unexpected value "
             "(move).")
        .def("has_value", &ExpectedType::has_value,
             "Checks if the Expected object contains a valid value.")
        .def(
            "value",
            [](const ExpectedType& exp) {
                if (!exp.has_value()) {
                    throw ExpectedError(
                        "Attempted to access value, but it contains an error.");
                }
                return exp.value();
            },
            "Retrieves the stored value, throws if it contains an error.")
        .def(
            "error",
            [](const ExpectedType& exp) -> E {
                if (exp.has_value()) {
                    throw ExpectedError(
                        "Attempted to access error, but it contains a value.");
                }
                return exp.error().error();
            },
            "Retrieves the stored error, throws if it contains a value.")
        .def("__bool__", &ExpectedType::operator bool,
             "Returns True if the Expected contains a value, False otherwise.")
        .def(
            "map",
            [](const ExpectedType& exp, py::function func) {
                using ReturnType =
                    decltype(func(std::declval<T>()).cast<py::object>());
                if (exp.has_value()) {
                    try {
                        py::object result = func(exp.value());
                        return atom::type::make_expected(result);
                    } catch (const py::error_already_set& e) {
                        return atom::type::expected<py::object, E>(
                            atom::type::Error<E>(
                                std::string("Python exception: ") + e.what()));
                    }
                }
                return atom::type::expected<py::object, E>(exp.error());
            },
            py::arg("func"),
            "Maps the contained value with the given function if present.")
        .def(
            "and_then",
            [](const ExpectedType& exp, py::function func) {
                if (exp.has_value()) {
                    try {
                        py::object result = func(exp.value());
                        if (py::isinstance<ExpectedType>(result)) {
                            return result.cast<ExpectedType>();
                        } else {
                            throw py::type_error(
                                "and_then function must return an Expected "
                                "object");
                        }
                    } catch (const py::error_already_set& e) {
                        return atom::type::expected<T, E>(atom::type::Error<E>(
                            std::string("Python exception: ") + e.what()));
                    }
                }
                return ExpectedType(exp.error());
            },
            py::arg("func"),
            "Chains expected values with the given function if present.")
        .def(
            "transform_error",
            [](const ExpectedType& exp, py::function func) {
                if (!exp.has_value()) {
                    try {
                        py::object new_error = func(exp.error().error());
                        return atom::type::expected<T, std::string>(
                            atom::type::Error<std::string>(py::str(new_error)));
                    } catch (const py::error_already_set& e) {
                        return atom::type::expected<T, std::string>(
                            atom::type::Error<std::string>(
                                std::string("Python exception: ") + e.what()));
                    }
                }
                return atom::type::expected<T, std::string>(exp.value());
            },
            py::arg("func"),
            "Transforms the contained error with the given function if "
            "present.")
        .def(
            "__eq__",
            [](const ExpectedType& exp, const ExpectedType& other) {
                return exp == other;
            },
            py::arg("other"), "Equality comparison.")
        .def(
            "__ne__",
            [](const ExpectedType& exp, const ExpectedType& other) {
                return exp != other;
            },
            py::arg("other"), "Inequality comparison.")
        .def(
            "__repr__",
            [](const ExpectedType& exp) {
                if (exp.has_value()) {
                    return std::string("Expected(") +
                           py::str(py::cast(exp.value())).cast<std::string>() +
                           ")";
                } else {
                    return std::string("Expected(Error(") +
                           py::str(py::cast(exp.error().error()))
                               .cast<std::string>() +
                           "))";
                }
            },
            "String representation.");
}

// 实现Expected<void>的特化版本
py::class_<atom::type::expected<void, std::string>> declare_expected_void(
    py::module& m) {
    using ExpectedType = atom::type::expected<void, std::string>;
    using ErrorType = atom::type::Error<std::string>;
    using UnexpectedType = atom::type::unexpected<std::string>;

    std::string class_name = "ExpectedVoid";
    std::string class_doc =
        "Expected void container.\n\n"
        "Represents a success state (void) or an error.";

    return py::class_<ExpectedType>(m, class_name.c_str(), class_doc.c_str())
        .def(py::init<>(),
             "Constructs an ExpectedVoid object in success state.")
        .def(py::init<const ErrorType&>(), py::arg("error"),
             "Constructs an ExpectedVoid object with the given error.")
        .def(py::init<ErrorType&&>(), py::arg("error"),
             "Constructs an ExpectedVoid object with the given error (move).")
        .def(py::init<const UnexpectedType&>(), py::arg("unexpected"),
             "Constructs an ExpectedVoid object with the given unexpected "
             "value.")
        .def(py::init<UnexpectedType&&>(), py::arg("unexpected"),
             "Constructs an ExpectedVoid object with the given unexpected "
             "value (move).")
        .def("has_value", &ExpectedType::has_value,
             "Checks if the ExpectedVoid object is in success state.")
        .def(
            "value",
            [](const ExpectedType& exp) {
                if (!exp.has_value()) {
                    throw ExpectedError(
                        "Attempted to access value, but it contains an error.");
                }
                return py::none();
            },
            "Verifies the success state, throws if it contains an error.")
        .def(
            "error",
            [](const ExpectedType& exp) -> std::string {
                if (exp.has_value()) {
                    throw ExpectedError(
                        "Attempted to access error, but it contains a value.");
                }
                return exp.error().error();
            },
            "Retrieves the stored error, throws if it's in success state.")
        .def("__bool__", &ExpectedType::operator bool,
             "Returns True if the ExpectedVoid is in success state, False "
             "otherwise.")
        .def(
            "and_then",
            [](const ExpectedType& exp, py::function func) {
                if (exp.has_value()) {
                    try {
                        py::object result = func();
                        if (py::isinstance<py::none>(result)) {
                            return ExpectedType();
                        } else if (py::isinstance<ExpectedType>(result)) {
                            return result.cast<ExpectedType>();
                        } else {
                            throw py::type_error(
                                "and_then function must return None or an "
                                "ExpectedVoid object");
                        }
                    } catch (const py::error_already_set& e) {
                        return ExpectedType(ErrorType(
                            std::string("Python exception: ") + e.what()));
                    }
                }
                return ExpectedType(exp.error());
            },
            py::arg("func"),
            "Chains expected values with the given function if in success "
            "state.")
        .def(
            "transform_error",
            [](const ExpectedType& exp, py::function func) {
                if (!exp.has_value()) {
                    try {
                        py::object new_error = func(exp.error().error());
                        return ExpectedType(ErrorType(py::str(new_error)));
                    } catch (const py::error_already_set& e) {
                        return ExpectedType(ErrorType(
                            std::string("Python exception: ") + e.what()));
                    }
                }
                return ExpectedType();
            },
            py::arg("func"),
            "Transforms the contained error with the given function if "
            "present.")
        .def(
            "__eq__",
            [](const ExpectedType& exp, const ExpectedType& other) {
                return exp == other;
            },
            py::arg("other"), "Equality comparison.")
        .def(
            "__ne__",
            [](const ExpectedType& exp, const ExpectedType& other) {
                return exp != other;
            },
            py::arg("other"), "Inequality comparison.")
        .def(
            "__repr__",
            [](const ExpectedType& exp) {
                if (exp.has_value()) {
                    return std::string("ExpectedVoid(Success)");
                } else {
                    return std::string("ExpectedVoid(Error(") +
                           exp.error().error() + "))";
                }
            },
            "String representation.");
}

// Error类绑定
py::class_<atom::type::Error<std::string>> declare_error(py::module& m) {
    using ErrorType = atom::type::Error<std::string>;

    return py::class_<ErrorType>(
               m, "Error", "Error class that encapsulates error information.")
        .def(py::init<std::string>(), py::arg("error"),
             "Constructs an Error object with the given error message.")
        .def(py::init<const char*>(), py::arg("error"),
             "Constructs an Error object with the given error message.")
        .def("error", &ErrorType::error, "Retrieves the stored error message.")
        .def(
            "__eq__",
            [](const ErrorType& err, const ErrorType& other) {
                return err == other;
            },
            py::arg("other"), "Equality comparison.")
        .def(
            "__repr__",
            [](const ErrorType& err) {
                return std::string("Error(") + err.error() + ")";
            },
            "String representation.");
}

// Unexpected类绑定
py::class_<atom::type::unexpected<std::string>> declare_unexpected(
    py::module& m) {
    using UnexpectedType = atom::type::unexpected<std::string>;

    return py::class_<UnexpectedType>(
               m, "Unexpected",
               "Unexpected class that represents an error state.")
        .def(py::init<std::string>(), py::arg("error"),
             "Constructs an Unexpected object with the given error message.")
        .def(py::init<const char*>(), py::arg("error"),
             "Constructs an Unexpected object with the given error message.")
        .def("error", &UnexpectedType::error,
             "Retrieves the stored error message.")
        .def(
            "__eq__",
            [](const UnexpectedType& unex, const UnexpectedType& other) {
                return unex == other;
            },
            py::arg("other"), "Equality comparison.")
        .def(
            "__repr__",
            [](const UnexpectedType& unex) {
                return std::string("Unexpected(") + unex.error() + ")";
            },
            "String representation.");
}

PYBIND11_MODULE(expected, m) {
    m.doc() = "Expected type implementation module for the atom package";

    // 注册异常转换
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const ExpectedError& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::logic_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // 注册主要类型
    declare_error(m);
    declare_unexpected(m);

    // 注册不同类型的Expected
    declare_expected<int>(m, "Int");
    declare_expected<float>(m, "Float");
    declare_expected<double>(m, "Double");
    declare_expected<bool>(m, "Bool");
    declare_expected<std::string>(m, "String");
    declare_expected<py::object>(m, "Object");
    declare_expected_void(m);

    // 工厂函数
    m.def(
        "make_expected",
        [](py::object value) {
            return atom::type::expected<py::object>(value);
        },
        py::arg("value"),
        R"(
        Create an Expected object containing a value.

        Args:
            value: The value to be stored in the Expected.

        Returns:
            An ExpectedObject containing the value.

        Examples:
            >>> from atom.type import expected
            >>> exp = expected.make_expected(42)
            >>> exp.has_value()
            True
            >>> exp.value()
            42
    )");

    m.def(
        "make_void_expected", []() { return atom::type::expected<void>(); },
        R"(
        Create an ExpectedVoid object in success state.

        Returns:
            An ExpectedVoid in success state.

        Examples:
            >>> from atom.type import expected
            >>> exp = expected.make_void_expected()
            >>> exp.has_value()
            True
    )");

    m.def(
        "make_unexpected",
        [](py::object error) {
            std::string error_str = py::str(error);
            return atom::type::unexpected<std::string>(error_str);
        },
        py::arg("error"),
        R"(
        Create an Unexpected object containing an error.

        Args:
            error: The error message to be stored in the Unexpected.

        Returns:
            An Unexpected object containing the error.

        Examples:
            >>> from atom.type import expected
            >>> unex = expected.make_unexpected("Something went wrong")
            >>> unex.error()
            'Something went wrong'
    )");

    m.def(
        "make_error",
        [](py::object error) {
            std::string error_str = py::str(error);
            return atom::type::Error<std::string>(error_str);
        },
        py::arg("error"),
        R"(
        Create an Error object containing an error message.

        Args:
            error: The error message to be stored.

        Returns:
            An Error object containing the error message.

        Examples:
            >>> from atom.type import expected
            >>> err = expected.make_error("Something went wrong")
            >>> err.error()
            'Something went wrong'
    )");

    m.def(
        "make_error_expected",
        [](py::object error) {
            std::string error_str = py::str(error);
            return atom::type::expected<py::object>(
                atom::type::Error<std::string>(error_str));
        },
        py::arg("error"),
        R"(
        Create an Expected object containing an error.

        Args:
            error: The error message to be stored.

        Returns:
            An ExpectedObject containing the error.

        Examples:
            >>> from atom.type import expected
            >>> exp = expected.make_error_expected("Something went wrong")
            >>> exp.has_value()
            False
            >>> exp.error()
            'Something went wrong'
    )");

    m.def(
        "make_void_error",
        [](py::object error) {
            std::string error_str = py::str(error);
            return atom::type::expected<void, std::string>(
                atom::type::Error<std::string>(error_str));
        },
        py::arg("error"),
        R"(
        Create an ExpectedVoid object containing an error.

        Args:
            error: The error message to be stored.

        Returns:
            An ExpectedVoid containing the error.

        Examples:
            >>> from atom.type import expected
            >>> exp = expected.make_void_error("Something went wrong")
            >>> exp.has_value()
            False
            >>> exp.error()
            'Something went wrong'
    )");
}