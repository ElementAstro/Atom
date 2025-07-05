#include "atom/extra/boost/locale.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/regex.hpp>

namespace py = pybind11;

PYBIND11_MODULE(locale, m) {
    m.doc() = "Boost Locale wrapper module for the atom package";

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

    // Enum for normalization types
    py::enum_<boost::locale::norm_type>(m, "NormType",
                                        R"(Unicode normalization form types.

These constants specify different Unicode normalization forms used for string normalization.)")
        .value("NFC", boost::locale::norm_nfc,
               "Canonical decomposition followed by canonical composition")
        .value("NFD", boost::locale::norm_nfd, "Canonical decomposition")
        .value("NFKC", boost::locale::norm_nfkc,
               "Compatibility decomposition followed by canonical composition")
        .value("NFKD", boost::locale::norm_nfkd, "Compatibility decomposition")
        .value("DEFAULT", boost::locale::norm_default,
               "Default normalization form (NFC)")
        .export_values();

    // LocaleWrapper class binding
    py::class_<atom::extra::boost::LocaleWrapper>(
        m, "LocaleWrapper",
        R"(A wrapper class for Boost.Locale functionalities.

This class provides various utilities for string conversion, Unicode normalization,
tokenization, translation, case conversion, collation, date and time formatting,
number formatting, currency formatting, and regex replacement using Boost.Locale.

Args:
    locale_name: The name of the locale to use. If empty, the global locale is used.

Examples:
    >>> from atom.extra.boost import locale
    >>> wrapper = locale.LocaleWrapper("en_US.UTF-8")
    >>> wrapper.to_upper("hello")
    'HELLO'
)")
        .def(py::init<const std::string&>(), py::arg("locale_name") = "",
             "Constructs a LocaleWrapper object with the specified locale.")
        .def_static("to_utf8", &atom::extra::boost::LocaleWrapper::toUtf8,
                    py::arg("str"), py::arg("from_charset"),
                    R"(Converts a string to UTF-8 encoding.

Args:
    str: The string to convert.
    from_charset: The original character set of the string.

Returns:
    The UTF-8 encoded string.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.to_utf8("Hello", "ISO-8859-1")
)")
        .def_static(
            "from_utf8", &atom::extra::boost::LocaleWrapper::fromUtf8,
            py::arg("str"), py::arg("to_charset"),
            R"(Converts a UTF-8 encoded string to another character set.

Args:
    str: The UTF-8 encoded string to convert.
    to_charset: The target character set.

Returns:
    The converted string.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.from_utf8("Hello", "ISO-8859-1")
)")
        .def_static("normalize", &atom::extra::boost::LocaleWrapper::normalize,
                    py::arg("str"),
                    py::arg("norm") = boost::locale::norm_default,
                    R"(Normalizes a Unicode string.

Args:
    str: The string to normalize.
    norm: The normalization form to use (default is NFC).

Returns:
    The normalized string.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.normalize("café", locale.NormType.NFC)
)")
        .def_static("tokenize", &atom::extra::boost::LocaleWrapper::tokenize,
                    py::arg("str"), py::arg("locale_name") = "",
                    R"(Tokenizes a string into words.

Args:
    str: The string to tokenize.
    locale_name: The name of the locale to use for tokenization.

Returns:
    A list of tokens.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.tokenize("Hello world!")
    ['Hello', ' ', 'world', '!']
)")
        .def_static("translate", &atom::extra::boost::LocaleWrapper::translate,
                    py::arg("str"), py::arg("domain"),
                    py::arg("locale_name") = "",
                    R"(Translates a string to the specified locale.

Args:
    str: The string to translate.
    domain: The domain for the translation.
    locale_name: The name of the locale to use for translation.

Returns:
    The translated string.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.translate("Hello", "messages", "fr_FR.UTF-8")
)")
        .def("to_upper", &atom::extra::boost::LocaleWrapper::toUpper,
             py::arg("str"),
             R"(Converts a string to uppercase.

Args:
    str: The string to convert.

Returns:
    The uppercase string.

Examples:
    >>> from atom.extra.boost import locale
    >>> wrapper = locale.LocaleWrapper("en_US.UTF-8")
    >>> wrapper.to_upper("hello")
    'HELLO'
)")
        .def("to_lower", &atom::extra::boost::LocaleWrapper::toLower,
             py::arg("str"),
             R"(Converts a string to lowercase.

Args:
    str: The string to convert.

Returns:
    The lowercase string.

Examples:
    >>> from atom.extra.boost import locale
    >>> wrapper = locale.LocaleWrapper("en_US.UTF-8")
    >>> wrapper.to_lower("HELLO")
    'hello'
)")
        .def("to_title", &atom::extra::boost::LocaleWrapper::toTitle,
             py::arg("str"),
             R"(Converts a string to title case.

Args:
    str: The string to convert.

Returns:
    The title case string.

Examples:
    >>> from atom.extra.boost import locale
    >>> wrapper = locale.LocaleWrapper("en_US.UTF-8")
    >>> wrapper.to_title("hello world")
    'Hello World'
)")
        .def("compare", &atom::extra::boost::LocaleWrapper::compare,
             py::arg("str1"), py::arg("str2"),
             R"(Compares two strings using locale-specific collation rules.

Args:
    str1: The first string to compare.
    str2: The second string to compare.

Returns:
    An integer less than, equal to, or greater than zero if str1 is found,
    respectively, to be less than, to match, or be greater than str2.

Examples:
    >>> from atom.extra.boost import locale
    >>> wrapper = locale.LocaleWrapper("en_US.UTF-8")
    >>> wrapper.compare("a", "b")
    -1
)")
        .def_static(
            "format_date", &atom::extra::boost::LocaleWrapper::formatDate,
            py::arg("date_time"), py::arg("format"),
            R"(Formats a date and time according to the specified format.

Args:
    date_time: The date and time to format (as a boost.posix_time.ptime object).
    format: The format string.

Returns:
    The formatted date and time string.

Examples:
    >>> from atom.extra.boost import locale
    >>> import datetime
    >>> locale.LocaleWrapper.format_date(datetime.datetime.now(), "%Y-%m-%d")
)")
        .def_static("format_number",
                    &atom::extra::boost::LocaleWrapper::formatNumber,
                    py::arg("number"), py::arg("precision") = 2,
                    R"(Formats a number with the specified precision.

Args:
    number: The number to format.
    precision: The number of decimal places.

Returns:
    The formatted number string.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.format_number(1234.567, 2)
    '1234.57'
)")
        .def_static("format_currency",
                    &atom::extra::boost::LocaleWrapper::formatCurrency,
                    py::arg("amount"), py::arg("currency"),
                    R"(Formats a currency amount.

Args:
    amount: The amount to format.
    currency: The currency code.

Returns:
    The formatted currency string.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.format_currency(1234.56, "USD")
)")
        .def_static(
            "regex_replace",
            [](const std::string& str, const std::string& pattern,
               const std::string& format) {
                boost::regex regex(pattern);
                return atom::extra::boost::LocaleWrapper::regexReplace(
                    str, regex, format);
            },
            py::arg("str"), py::arg("pattern"), py::arg("format"),
            R"(Replaces occurrences of a regex pattern in a string with a format string.

Args:
    str: The string to search.
    pattern: The regex pattern to search for.
    format: The format string to replace with.

Returns:
    The resulting string after replacements.

Examples:
    >>> from atom.extra.boost import locale
    >>> locale.LocaleWrapper.regex_replace("Hello world", "world", "Python")
    'Hello Python'
)")
        .def(
            "format",
            [](const atom::extra::boost::LocaleWrapper& self,
               const std::string& format_string, py::args args) {
                std::string result = format_string;

                // Simple placeholder replacement (just for demonstration)
                // In a real implementation, you would use boost::locale::format
                // properly
                for (size_t i = 0; i < args.size(); ++i) {
                    std::string placeholder = "{" + std::to_string(i) + "}";
                    size_t pos = result.find(placeholder);
                    if (pos != std::string::npos) {
                        std::string arg_str = py::str(args[i]);
                        result.replace(pos, placeholder.length(), arg_str);
                    }
                }

                return result;
            },
            py::arg("format_string"),
            R"(Formats a string with named arguments.

Args:
    format_string: The format string.
    *args: The arguments to format.

Returns:
    The formatted string.

Examples:
    >>> from atom.extra.boost import locale
    >>> wrapper = locale.LocaleWrapper("en_US.UTF-8")
    >>> wrapper.format("Hello, {0}!", "world")
    'Hello, world!'
)")
        .def("__str__",
             [](const atom::extra::boost::LocaleWrapper& self) {
                 return "LocaleWrapper()";
             })
        .def("__repr__", [](const atom::extra::boost::LocaleWrapper& self) {
            return "LocaleWrapper()";
        });

    // Convenience functions directly at module level
    m.def("to_utf8", &atom::extra::boost::LocaleWrapper::toUtf8, py::arg("str"),
          py::arg("from_charset"), "Shorthand for LocaleWrapper.to_utf8");

    m.def("from_utf8", &atom::extra::boost::LocaleWrapper::fromUtf8,
          py::arg("str"), py::arg("to_charset"),
          "Shorthand for LocaleWrapper.from_utf8");

    m.def("normalize", &atom::extra::boost::LocaleWrapper::normalize,
          py::arg("str"), py::arg("norm") = boost::locale::norm_default,
          "Shorthand for LocaleWrapper.normalize");

    m.def("tokenize", &atom::extra::boost::LocaleWrapper::tokenize,
          py::arg("str"), py::arg("locale_name") = "",
          "Shorthand for LocaleWrapper.tokenize");

    m.def("translate", &atom::extra::boost::LocaleWrapper::translate,
          py::arg("str"), py::arg("domain"), py::arg("locale_name") = "",
          "Shorthand for LocaleWrapper.translate");

    m.def("format_date", &atom::extra::boost::LocaleWrapper::formatDate,
          py::arg("date_time"), py::arg("format"),
          "Shorthand for LocaleWrapper.format_date");

    m.def("format_number", &atom::extra::boost::LocaleWrapper::formatNumber,
          py::arg("number"), py::arg("precision") = 2,
          "Shorthand for LocaleWrapper.format_number");

    m.def("format_currency", &atom::extra::boost::LocaleWrapper::formatCurrency,
          py::arg("amount"), py::arg("currency"),
          "Shorthand for LocaleWrapper.format_currency");

    m.def(
        "regex_replace",
        [](const std::string& str, const std::string& pattern,
           const std::string& format) {
            boost::regex regex(pattern);
            return atom::extra::boost::LocaleWrapper::regexReplace(str, regex,
                                                                   format);
        },
        py::arg("str"), py::arg("pattern"), py::arg("format"),
        "Shorthand for LocaleWrapper.regex_replace");

    // Create a default instance
    m.attr("default_wrapper") = py::cast(atom::extra::boost::LocaleWrapper());
}
