#include "atom/extra/boost/charconv.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(charconv, m) {
    m.doc() = "Boost CharConv binding module for the atom package";

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

    // Bind the NumberFormat enum
    py::enum_<atom::extra::boost::NumberFormat>(
        m, "NumberFormat",
        R"(Enum class representing different number formats.

Attributes:
    GENERAL: General format (default).
    SCIENTIFIC: Scientific notation (e.g., 1.23e+4).
    FIXED: Fixed-point notation (e.g., 12300.00).
    HEX: Hexadecimal notation (e.g., 0x1F3A).
)")
        .value("GENERAL", atom::extra::boost::NumberFormat::GENERAL)
        .value("SCIENTIFIC", atom::extra::boost::NumberFormat::SCIENTIFIC)
        .value("FIXED", atom::extra::boost::NumberFormat::FIXED)
        .value("HEX", atom::extra::boost::NumberFormat::HEX)
        .export_values();

    // Bind the FormatOptions struct
    py::class_<atom::extra::boost::FormatOptions>(
        m, "FormatOptions",
        R"(Struct for specifying format options for number conversion.

Attributes:
    format: The number format (default: NumberFormat.GENERAL)
    precision: Optional precision for floating-point numbers
    uppercase: Whether to use uppercase letters (default: False)
    thousands_separator: Character to use as thousands separator (default: None)
)")
        .def(py::init<>())
        .def_readwrite("format", &atom::extra::boost::FormatOptions::format,
                       "The number format")
        .def_readwrite("precision",
                       &atom::extra::boost::FormatOptions::precision,
                       "The precision for floating-point numbers")
        .def_readwrite("uppercase",
                       &atom::extra::boost::FormatOptions::uppercase,
                       "Whether to use uppercase letters")
        .def_readwrite("thousands_separator",
                       &atom::extra::boost::FormatOptions::thousandsSeparator,
                       "The character to use as a thousands separator");

    // Bind the BoostCharConv class
    py::class_<atom::extra::boost::BoostCharConv>(
        m, "BoostCharConv",
        R"(Class for converting numbers to and from strings using Boost.CharConv.

This class provides static methods for converting between strings and numbers
with precise format control.

Examples:
    >>> from atom.extra.boost import charconv
    >>> charconv.BoostCharConv.int_to_string(12345)
    '12345'
    >>> options = charconv.FormatOptions(thousands_separator=',')
    >>> charconv.BoostCharConv.float_to_string(12345.67, options)
    '12,345.67'
)")
        .def_static("int_to_string",
                    &atom::extra::boost::BoostCharConv::intToString<int>,
                    py::arg("value"),
                    py::arg("base") = atom::extra::boost::DEFAULT_BASE,
                    py::arg("options") = atom::extra::boost::FormatOptions{},
                    R"(Converts an integer to a string.

Args:
    value: The integer value to convert.
    base: The base for the conversion (default: 10).
    options: The format options for the conversion.

Returns:
    The converted string.

Raises:
    ValueError: If the conversion fails.
)")
        .def_static("int_to_string",
                    &atom::extra::boost::BoostCharConv::intToString<long>,
                    py::arg("value"),
                    py::arg("base") = atom::extra::boost::DEFAULT_BASE,
                    py::arg("options") = atom::extra::boost::FormatOptions{})
        .def_static("int_to_string",
                    &atom::extra::boost::BoostCharConv::intToString<long long>,
                    py::arg("value"),
                    py::arg("base") = atom::extra::boost::DEFAULT_BASE,
                    py::arg("options") = atom::extra::boost::FormatOptions{})
        .def_static(
            "int_to_string",
            &atom::extra::boost::BoostCharConv::intToString<unsigned int>,
            py::arg("value"),
            py::arg("base") = atom::extra::boost::DEFAULT_BASE,
            py::arg("options") = atom::extra::boost::FormatOptions{})
        .def_static(
            "int_to_string",
            &atom::extra::boost::BoostCharConv::intToString<unsigned long>,
            py::arg("value"),
            py::arg("base") = atom::extra::boost::DEFAULT_BASE,
            py::arg("options") = atom::extra::boost::FormatOptions{})
        .def_static(
            "int_to_string",
            &atom::extra::boost::BoostCharConv::intToString<unsigned long long>,
            py::arg("value"),
            py::arg("base") = atom::extra::boost::DEFAULT_BASE,
            py::arg("options") = atom::extra::boost::FormatOptions{})

        .def_static("float_to_string",
                    &atom::extra::boost::BoostCharConv::floatToString<float>,
                    py::arg("value"),
                    py::arg("options") = atom::extra::boost::FormatOptions{},
                    R"(Converts a floating-point number to a string.

Args:
    value: The floating-point value to convert.
    options: The format options for the conversion.

Returns:
    The converted string.

Raises:
    RuntimeError: If the conversion fails.
)")
        .def_static("float_to_string",
                    &atom::extra::boost::BoostCharConv::floatToString<double>,
                    py::arg("value"),
                    py::arg("options") = atom::extra::boost::FormatOptions{})
        .def_static(
            "float_to_string",
            &atom::extra::boost::BoostCharConv::floatToString<long double>,
            py::arg("value"),
            py::arg("options") = atom::extra::boost::FormatOptions{})

        .def_static("string_to_int",
                    &atom::extra::boost::BoostCharConv::stringToInt<int>,
                    py::arg("str"),
                    py::arg("base") = atom::extra::boost::DEFAULT_BASE,
                    R"(Converts a string to an integer.

Args:
    str: The string to convert.
    base: The base for the conversion (default: 10).

Returns:
    The converted integer.

Raises:
    ValueError: If the conversion fails.
)")
        .def_static("string_to_int",
                    &atom::extra::boost::BoostCharConv::stringToInt<long>,
                    py::arg("str"),
                    py::arg("base") = atom::extra::boost::DEFAULT_BASE)
        .def_static("string_to_int",
                    &atom::extra::boost::BoostCharConv::stringToInt<long long>,
                    py::arg("str"),
                    py::arg("base") = atom::extra::boost::DEFAULT_BASE)
        .def_static(
            "string_to_int",
            &atom::extra::boost::BoostCharConv::stringToInt<unsigned int>,
            py::arg("str"), py::arg("base") = atom::extra::boost::DEFAULT_BASE)
        .def_static(
            "string_to_int",
            &atom::extra::boost::BoostCharConv::stringToInt<unsigned long>,
            py::arg("str"), py::arg("base") = atom::extra::boost::DEFAULT_BASE)
        .def_static(
            "string_to_int",
            &atom::extra::boost::BoostCharConv::stringToInt<unsigned long long>,
            py::arg("str"), py::arg("base") = atom::extra::boost::DEFAULT_BASE)

        .def_static("string_to_float",
                    &atom::extra::boost::BoostCharConv::stringToFloat<float>,
                    py::arg("str"),
                    R"(Converts a string to a floating-point number.

Args:
    str: The string to convert.

Returns:
    The converted floating-point number.

Raises:
    ValueError: If the conversion fails.
)")
        .def_static("string_to_float",
                    &atom::extra::boost::BoostCharConv::stringToFloat<double>,
                    py::arg("str"))
        .def_static(
            "string_to_float",
            &atom::extra::boost::BoostCharConv::stringToFloat<long double>,
            py::arg("str"))

        .def_static(
            "to_string",
            [](int value, const atom::extra::boost::FormatOptions& options) {
                return atom::extra::boost::BoostCharConv::toString(value,
                                                                   options);
            },
            py::arg("value"),
            py::arg("options") = atom::extra::boost::FormatOptions{},
            R"(Converts a value to a string using the appropriate conversion function.

Args:
    value: The value to convert.
    options: The format options for the conversion.

Returns:
    The converted string.
)")
        .def_static(
            "to_string",
            [](double value, const atom::extra::boost::FormatOptions& options) {
                return atom::extra::boost::BoostCharConv::toString(value,
                                                                   options);
            },
            py::arg("value"),
            py::arg("options") = atom::extra::boost::FormatOptions{})

        .def_static(
            "from_string",
            [](const std::string& str, int base) {
                return atom::extra::boost::BoostCharConv::fromString<int>(str,
                                                                          base);
            },
            py::arg("str"), py::arg("base") = atom::extra::boost::DEFAULT_BASE,
            R"(Converts a string to a value using the appropriate conversion function.

Args:
    str: The string to convert.
    base: The base for the conversion (default: 10).

Returns:
    The converted value.

Raises:
    ValueError: If the conversion fails.
)")
        .def_static(
            "from_string",
            [](const std::string& str) {
                return atom::extra::boost::BoostCharConv::fromString<double>(
                    str);
            },
            py::arg("str"))

        .def_static(
            "special_value_to_string",
            &atom::extra::boost::BoostCharConv::specialValueToString<float>,
            py::arg("value"),
            R"(Converts special floating-point values (NaN, Inf) to strings.

Args:
    value: The floating-point value to convert.

Returns:
    The converted string.
)")
        .def_static(
            "special_value_to_string",
            &atom::extra::boost::BoostCharConv::specialValueToString<double>,
            py::arg("value"))
        .def_static("special_value_to_string",
                    &atom::extra::boost::BoostCharConv::specialValueToString<
                        long double>,
                    py::arg("value"));

    // Convenience functions directly at module level
    m.def("int_to_string", &atom::extra::boost::BoostCharConv::intToString<int>,
          py::arg("value"), py::arg("base") = atom::extra::boost::DEFAULT_BASE,
          py::arg("options") = atom::extra::boost::FormatOptions{},
          "Shorthand for BoostCharConv.int_to_string");

    m.def("float_to_string",
          &atom::extra::boost::BoostCharConv::floatToString<double>,
          py::arg("value"),
          py::arg("options") = atom::extra::boost::FormatOptions{},
          "Shorthand for BoostCharConv.float_to_string");

    m.def("string_to_int", &atom::extra::boost::BoostCharConv::stringToInt<int>,
          py::arg("str"), py::arg("base") = atom::extra::boost::DEFAULT_BASE,
          "Shorthand for BoostCharConv.string_to_int");

    m.def("string_to_float",
          &atom::extra::boost::BoostCharConv::stringToFloat<double>,
          py::arg("str"), "Shorthand for BoostCharConv.string_to_float");
}