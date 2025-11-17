#include "atom/extra/curl/multipart.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(multipart, m) {
    m.doc() = R"(Multipart form data module for the atom package.

This module provides a class for building multipart/form-data requests,
which are commonly used for uploading files and submitting forms with
various data types.

Examples:
    >>> from atom.extra.curl import multipart
    >>>
    >>> # Create a multipart form
    >>> form = multipart.MultipartForm()
    >>>
    >>> # Add a file
    >>> form.add_file("document", "/path/to/file.pdf", "application/pdf")
    >>>
    >>> # Add form fields
    >>> form.add_field("title", "My Document")
    >>> form.add_field("description", "This is a test document")
    >>>
    >>> # Add a buffer as a file
    >>> data = b"Hello, World!"
    >>> form.add_buffer("greeting", data, len(data), "greeting.txt", "text/plain")
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

    // MultipartForm class binding
    py::class_<atom::extra::curl::MultipartForm>(
        m, "MultipartForm",
        R"(A class for building multipart/form-data requests.

This class simplifies the creation of multipart/form-data requests,
which are commonly used for uploading files and submitting forms
with various data types. It uses the libcurl's mime API to construct
the form data.

Examples:
    >>> form = MultipartForm()
    >>>
    >>> # Add various types of data
    >>> form.add_file("avatar", "profile.jpg", "image/jpeg")
    >>> form.add_field("username", "john_doe")
    >>> form.add_field("bio", "Software developer")
    >>>
    >>> # Use with a request
    >>> # (Note: Integration with Request class would be done in C++)
)")
        .def(py::init<>(), "Create a new multipart form")
        .def("add_file", &atom::extra::curl::MultipartForm::add_file,
             py::arg("name"), py::arg("filepath"), py::arg("content_type") = "",
             R"(Add a file to the multipart form.

Args:
    name: The name of the form field.
    filepath: The path to the file to be added.
    content_type: The content type of the file (optional). If not specified,
                  libcurl will attempt to determine the content type automatically.

Raises:
    RuntimeError: If the file cannot be added to the form.

Examples:
    >>> form.add_file("document", "/path/to/file.pdf", "application/pdf")
    >>> form.add_file("image", "/path/to/photo.jpg")  # Auto-detect content type
)")
        .def(
            "add_buffer",
            [](atom::extra::curl::MultipartForm& self, std::string_view name,
               py::bytes data, std::string_view filename,
               std::string_view content_type) {
                std::string data_str = data;
                self.add_buffer(name, data_str.data(), data_str.size(),
                                filename, content_type);
            },
            py::arg("name"), py::arg("data"), py::arg("filename"),
            py::arg("content_type") = "",
            R"(Add a buffer of data as a file to the multipart form.

Args:
    name: The name of the form field.
    data: The data buffer as bytes.
    filename: The filename to be associated with the data.
    content_type: The content type of the data (optional).

Raises:
    RuntimeError: If the buffer cannot be added to the form.

Examples:
    >>> data = b"Hello, World!"
    >>> form.add_buffer("greeting", data, "greeting.txt", "text/plain")
    >>>
    >>> # Upload binary data
    >>> binary_data = bytes([0x89, 0x50, 0x4E, 0x47])  # PNG header
    >>> form.add_buffer("icon", binary_data, "icon.png", "image/png")
)")
        .def("add_field", &atom::extra::curl::MultipartForm::add_field,
             py::arg("name"), py::arg("content"),
             R"(Add a form field to the multipart form.

Args:
    name: The name of the form field.
    content: The content of the form field.

Examples:
    >>> form.add_field("username", "john_doe")
    >>> form.add_field("email", "john@example.com")
)")
        .def("add_field_with_type",
             &atom::extra::curl::MultipartForm::add_field_with_type,
             py::arg("name"), py::arg("content"), py::arg("content_type"),
             R"(Add a form field with a specified content type.

Args:
    name: The name of the form field.
    content: The content of the form field.
    content_type: The content type of the form field.

Examples:
    >>> form.add_field_with_type("data", '{"key": "value"}', "application/json")
    >>> form.add_field_with_type("xml", "<root><item>value</item></root>", "application/xml")
)");
}
