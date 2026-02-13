#include "atom/utils/text/cstring.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(cstring, m) {
    m.doc() = R"pbdoc(
        Compile-Time String Utilities Module
        -------------------------------------

        This module provides compile-time string manipulation utilities.
        Note: While the C++ functions are constexpr and work at compile-time,
        the Python bindings execute at runtime but provide the same functionality.

        Functions include:
        - deduplicate: Remove duplicate characters
        - replace: Replace characters
        - to_lower: Convert to lowercase
        - to_upper: Convert to uppercase
        - reverse: Reverse string
        - find: Find character position
        - length: Get string length
        - equal: Compare strings
        - concat: Concatenate strings

        Examples:
            >>> from atom.utils import cstring
            >>> cstring.deduplicate("hello")
            'helo'
            >>> cstring.to_upper("hello")
            'HELLO'
            >>> cstring.reverse("hello")
            'olleh'
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

    // Helper function to convert array to string
    auto arrayToString = [](const auto& arr) -> std::string {
        std::string result;
        for (size_t i = 0; i < arr.size() && arr[i] != '\0'; ++i) {
            result += arr[i];
        }
        return result;
    };

    m.def(
        "deduplicate",
        [arrayToString](const std::string& str) {
            if (str.empty())
                return std::string();
            // Create a compile-time compatible version
            std::string result;
            for (char c : str) {
                if (result.find(c) == std::string::npos) {
                    result += c;
                }
            }
            return result;
        },
        py::arg("str"),
        R"(Remove duplicate characters from a string.

Args:
    str: Input string

Returns:
    String with duplicate characters removed

Examples:
    >>> cstring.deduplicate("hello")
    'helo'
    >>> cstring.deduplicate("aabbcc")
    'abc'
)");

    m.def(
        "replace",
        [](const std::string& str, char old_char, char new_char) {
            std::string result = str;
            std::replace(result.begin(), result.end(), old_char, new_char);
            return result;
        },
        py::arg("str"), py::arg("old_char"), py::arg("new_char"),
        R"(Replace all occurrences of a character in a string.

Args:
    str: Input string
    old_char: Character to replace
    new_char: Replacement character

Returns:
    String with characters replaced

Examples:
    >>> cstring.replace("hello", 'l', 'x')
    'hexxo'
)");

    m.def(
        "to_lower",
        [](const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return result;
        },
        py::arg("str"),
        R"(Convert all characters in a string to lowercase.

Args:
    str: Input string

Returns:
    Lowercase string

Examples:
    >>> cstring.to_lower("HELLO")
    'hello'
)");

    m.def(
        "to_upper",
        [](const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::toupper(c); });
            return result;
        },
        py::arg("str"),
        R"(Convert all characters in a string to uppercase.

Args:
    str: Input string

Returns:
    Uppercase string

Examples:
    >>> cstring.to_upper("hello")
    'HELLO'
)");

    m.def(
        "reverse",
        [](const std::string& str) {
            std::string result = str;
            std::reverse(result.begin(), result.end());
            return result;
        },
        py::arg("str"),
        R"(Reverse the characters in a string.

Args:
    str: Input string

Returns:
    Reversed string

Examples:
    >>> cstring.reverse("hello")
    'olleh'
)");

    m.def(
        "find",
        [](const std::string& str, char c) -> int {
            auto pos = str.find(c);
            return (pos != std::string::npos) ? static_cast<int>(pos) : -1;
        },
        py::arg("str"), py::arg("char"),
        R"(Find the position of a character in a string.

Args:
    str: Input string
    char: Character to find

Returns:
    Position of the character, or -1 if not found

Examples:
    >>> cstring.find("hello", 'e')
    1
    >>> cstring.find("hello", 'x')
    -1
)");

    m.def(
        "length", [](const std::string& str) { return str.length(); },
        py::arg("str"),
        R"(Get the length of a string.

Args:
    str: Input string

Returns:
    Length of the string

Examples:
    >>> cstring.length("hello")
    5
)");

    m.def(
        "equal",
        [](const std::string& str1, const std::string& str2) {
            return str1 == str2;
        },
        py::arg("str1"), py::arg("str2"),
        R"(Compare two strings for equality.

Args:
    str1: First string
    str2: Second string

Returns:
    True if strings are equal, False otherwise

Examples:
    >>> cstring.equal("hello", "hello")
    True
    >>> cstring.equal("hello", "world")
    False
)");

    m.def(
        "concat",
        [](const std::string& str1, const std::string& str2) {
            return str1 + str2;
        },
        py::arg("str1"), py::arg("str2"),
        R"(Concatenate two strings.

Args:
    str1: First string
    str2: Second string

Returns:
    Concatenated string

Examples:
    >>> cstring.concat("hello", "world")
    'helloworld'
)");

    m.def(
        "starts_with",
        [](const std::string& str, const std::string& prefix) {
            return str.size() >= prefix.size() &&
                   str.compare(0, prefix.size(), prefix) == 0;
        },
        py::arg("str"), py::arg("prefix"),
        R"(Check if a string starts with a prefix.

Args:
    str: Input string
    prefix: Prefix to check

Returns:
    True if string starts with prefix, False otherwise

Examples:
    >>> cstring.starts_with("hello", "hel")
    True
)");

    m.def(
        "ends_with",
        [](const std::string& str, const std::string& suffix) {
            return str.size() >= suffix.size() &&
                   str.compare(str.size() - suffix.size(), suffix.size(),
                               suffix) == 0;
        },
        py::arg("str"), py::arg("suffix"),
        R"(Check if a string ends with a suffix.

Args:
    str: Input string
    suffix: Suffix to check

Returns:
    True if string ends with suffix, False otherwise

Examples:
    >>> cstring.ends_with("hello", "llo")
    True
)");

    m.def(
        "contains",
        [](const std::string& str, char c) {
            return str.find(c) != std::string::npos;
        },
        py::arg("str"), py::arg("char"),
        R"(Check if a string contains a character.

Args:
    str: Input string
    char: Character to find

Returns:
    True if string contains the character, False otherwise

Examples:
    >>> cstring.contains("hello", 'e')
    True
    >>> cstring.contains("hello", 'x')
    False
)");

    m.def(
        "count",
        [](const std::string& str, char c) {
            return std::count(str.begin(), str.end(), c);
        },
        py::arg("str"), py::arg("char"),
        R"(Count occurrences of a character in a string.

Args:
    str: Input string
    char: Character to count

Returns:
    Number of occurrences

Examples:
    >>> cstring.count("hello", 'l')
    2
)");
}
