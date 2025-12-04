#include "atom/secret/result.hpp"
#include "atom/secret/encryption.hpp"
#include "atom/secret/password_entry.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Helper function to bind Result<T> template
template <typename T>
void bind_result(py::module& m, const std::string& suffix) {
    using ResultType = atom::secret::Result<T>;
    std::string class_name = "Result" + suffix;

    py::class_<ResultType>(m, class_name.c_str(),
                           R"pbdoc(
        Template for operation results, alternative to exceptions.

        Provides a type-safe way to return either a success value or an error
        message, avoiding the overhead and complexity of exceptions.
        )pbdoc")
        .def(py::init<const T&>(), py::arg("value"),
             "Constructs a Result with a success value")
        .def(py::init<const atom::secret::Error&>(), py::arg("error"),
             "Constructs a Result with an error")
        .def_static("success", &ResultType::success, py::arg("value"),
                    "Creates a successful Result")
        .def_static("error", &ResultType::error, py::arg("error"),
                    "Creates an error Result")
        .def("is_success", &ResultType::isSuccess,
             "Checks if the result represents success")
        .def("is_error", &ResultType::isError,
             "Checks if the result represents an error")
        .def(
            "value",
            [](const ResultType& self) -> T {
                if (self.isError()) {
                    throw std::runtime_error(
                        "Attempted to access value of an error Result: " +
                        self.error());
                }
                return self.value();
            },
            R"pbdoc(
            Gets the success value.

            Returns:
                The success value.

            Raises:
                RuntimeError: If the result is an error.
            )pbdoc")
        .def(
            "error",
            [](const ResultType& self) -> std::string {
                if (self.isSuccess()) {
                    throw std::runtime_error(
                        "Attempted to access error of a success Result.");
                }
                return self.error();
            },
            R"pbdoc(
            Gets the error message.

            Returns:
                The error message string.

            Raises:
                RuntimeError: If the result is successful.
            )pbdoc")
        .def("__bool__", &ResultType::isSuccess,
             "Returns True if successful, False if error")
        .def("__repr__", [](const ResultType& self) {
            if (self.isSuccess()) {
                return "<Result: Success>";
            } else {
                return "<Result: Error - " + self.error() + ">";
            }
        });
}

void bind_result_types(py::module& m) {
    // Error wrapper
    py::class_<atom::secret::Error>(m, "Error",
                                    R"pbdoc(
        Error wrapper to distinguish from success values.

        Attributes:
            message (str): The error message
        )pbdoc")
        .def(py::init<const std::string&>(), py::arg("message"),
             "Constructs an Error with a message")
        .def_readwrite("message", &atom::secret::Error::message,
                       "The error message");

    // Bind Result<T> for common types
    bind_result<std::string>(m, "String");
    bind_result<bool>(m, "Bool");
    bind_result<int>(m, "Int");
    bind_result<atom::secret::PasswordEntry>(m, "PasswordEntry");
    bind_result<atom::secret::EncryptedData>(m, "EncryptedData");
    bind_result<std::vector<uint8_t>>(m, "Bytes");
}
