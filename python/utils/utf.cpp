#include "atom/utils/text/utf.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(utf, m) {
    m.doc() = R"pbdoc(
        UTF Encoding Utilities Module
        ------------------------------

        This module provides UTF encoding conversion utilities:
        - UTF-8, UTF-16, UTF-32 conversions
        - Wide string conversions
        - UTF-8 validation

        Examples:
            >>> from atom.utils import utf
            >>> utf8_str = utf.to_utf8("Hello")
            >>> utf16_str = utf.utf8_to_utf16("Hello")
            >>> is_valid = utf.is_valid_utf8("Hello")
    )pbdoc";

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

    // Wide string to UTF-8
    m.def("to_utf8", &atom::utils::toUTF8, py::arg("wstr"),
          R"(Convert wide string to UTF-8 encoded string.

Args:
    wstr: Wide string to convert

Returns:
    UTF-8 encoded string
)");

    // UTF-8 to wide string
    m.def("from_utf8", &atom::utils::fromUTF8, py::arg("str"),
          R"(Convert UTF-8 encoded string to wide string.

Args:
    str: UTF-8 encoded string to convert

Returns:
    Wide string
)");

    // UTF-8 to UTF-16
    m.def("utf8_to_utf16", &atom::utils::utf8toUtF16, py::arg("str"),
          R"(Convert UTF-8 encoded string to UTF-16.

Args:
    str: UTF-8 encoded string to convert

Returns:
    UTF-16 encoded string
)");

    // UTF-8 to UTF-32
    m.def("utf8_to_utf32", &atom::utils::utf8toUtF32, py::arg("str"),
          R"(Convert UTF-8 encoded string to UTF-32.

Args:
    str: UTF-8 encoded string to convert

Returns:
    UTF-32 encoded string
)");

    // UTF-16 to UTF-8
    m.def("utf16_to_utf8", &atom::utils::utf16toUtF8, py::arg("str"),
          R"(Convert UTF-16 encoded string to UTF-8.

Args:
    str: UTF-16 encoded string to convert

Returns:
    UTF-8 encoded string
)");

    // UTF-16 to UTF-32
    m.def("utf16_to_utf32", &atom::utils::utf16toUtF32, py::arg("str"),
          R"(Convert UTF-16 encoded string to UTF-32.

Args:
    str: UTF-16 encoded string to convert

Returns:
    UTF-32 encoded string
)");

    // UTF-32 to UTF-8
    m.def("utf32_to_utf8", &atom::utils::utf32toUtF8, py::arg("str"),
          R"(Convert UTF-32 encoded string to UTF-8.

Args:
    str: UTF-32 encoded string to convert

Returns:
    UTF-8 encoded string
)");

    // UTF-32 to UTF-16
    m.def("utf32_to_utf16", &atom::utils::utf32toUtF16, py::arg("str"),
          R"(Convert UTF-32 encoded string to UTF-16.

Args:
    str: UTF-32 encoded string to convert

Returns:
    UTF-16 encoded string
)");

    // UTF-8 validation
    m.def("is_valid_utf8", &atom::utils::isValidUTF8, py::arg("str"),
          R"(Validate if a string is well-formed UTF-8.

Args:
    str: String to validate

Returns:
    True if valid UTF-8, False otherwise
)");
}
