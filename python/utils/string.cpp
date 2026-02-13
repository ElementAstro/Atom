#include "atom/utils/text/string.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(string, m) {
    m.doc() = R"pbdoc(
        String Utilities Module
        -----------------------

        This module provides comprehensive string manipulation utilities including:
        - Case conversion (camelCase, snake_case, upper, lower)
        - URL encoding/decoding
        - String splitting and joining
        - String replacement and trimming
        - Wide string conversions
        - String parsing to numeric types

        Examples:
            >>> from atom.utils import string
            >>> string.to_camel_case("hello_world")
            'helloWorld'
            >>> string.url_encode("hello world")
            'hello%20world'
            >>> string.split_string("a,b,c", ',')
            ['a', 'b', 'c']
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_OverflowError, e.what());
        } catch (const std::range_error& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Case checking and conversion
    m.def("has_uppercase", &atom::utils::hasUppercase, py::arg("str"),
          "Check if string contains any uppercase characters");

    m.def("to_underscore", &atom::utils::toUnderscore, py::arg("str"),
          "Convert string to snake_case format");

    m.def("to_camel_case", &atom::utils::toCamelCase, py::arg("str"),
          "Convert string to camelCase format");

    m.def("to_lower", &atom::utils::toLower, py::arg("str"),
          "Convert string to lowercase");

    m.def("to_upper", &atom::utils::toUpper, py::arg("str"),
          "Convert string to uppercase");

    // URL encoding/decoding
    m.def("url_encode", &atom::utils::urlEncode, py::arg("str"),
          "Encode string using URL encoding");

    m.def("url_decode", &atom::utils::urlDecode, py::arg("str"),
          "Decode URL encoded string");

    // String checking
    m.def("starts_with", &atom::utils::startsWith, py::arg("str"),
          py::arg("prefix"), "Check if string starts with prefix");

    m.def("ends_with", &atom::utils::endsWith, py::arg("str"),
          py::arg("suffix"), "Check if string ends with suffix");

    // String splitting and joining
    m.def("split_string", &atom::utils::splitString, py::arg("str"),
          py::arg("delimiter"), "Split string by delimiter character");

    m.def(
        "join_strings",
        [](const std::vector<std::string>& strings,
           const std::string& delimiter) {
            std::vector<std::string_view> views;
            views.reserve(strings.size());
            for (const auto& s : strings) {
                views.emplace_back(s);
            }
            return atom::utils::joinStrings(views, delimiter);
        },
        py::arg("strings"), py::arg("delimiter"),
        "Join strings with delimiter");

    m.def("explode", &atom::utils::explode, py::arg("text"), py::arg("symbol"),
          "Explode string into vector by symbol");

    // String replacement
    m.def("replace_string", &atom::utils::replaceString, py::arg("text"),
          py::arg("old_str"), py::arg("new_str"),
          "Replace all occurrences of substring");

    m.def(
        "replace_strings",
        [](const std::string& text,
           const std::vector<std::pair<std::string, std::string>>&
               replacements) {
            std::vector<std::pair<std::string_view, std::string_view>> views;
            views.reserve(replacements.size());
            for (const auto& [old_str, new_str] : replacements) {
                views.emplace_back(old_str, new_str);
            }
            return atom::utils::replaceStrings(text, views);
        },
        py::arg("text"), py::arg("replacements"),
        "Replace multiple substrings with their replacements");

    m.def("parallel_replace_string", &atom::utils::parallelReplaceString,
          py::arg("text"), py::arg("old_str"), py::arg("new_str"),
          py::arg("threshold") = 10000,
          "Replace string in parallel for large texts");

    // String trimming
    m.def("trim", &atom::utils::trim, py::arg("line"),
          py::arg("symbols") = " \n\r\t", "Trim characters from string");

    // Wide string conversions
    m.def("string_to_wstring", &atom::utils::stringToWString, py::arg("str"),
          "Convert string to wide string");

    m.def("wstring_to_string", &atom::utils::wstringToString, py::arg("wstr"),
          "Convert wide string to string");

    // String to numeric conversions
    m.def("stod", &atom::utils::stod, py::arg("str"), py::arg("idx") = nullptr,
          "Convert string to double");

    m.def("stof", &atom::utils::stof, py::arg("str"), py::arg("idx") = nullptr,
          "Convert string to float");

    m.def("stoi", &atom::utils::stoi, py::arg("str"), py::arg("idx") = nullptr,
          py::arg("base") = 10, "Convert string to integer");

    m.def("stol", &atom::utils::stol, py::arg("str"), py::arg("idx") = nullptr,
          py::arg("base") = 10, "Convert string to long integer");
}
