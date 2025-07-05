#include "atom/extra/boost/regex.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(regex, m) {
    m.doc() = "Boost Regex wrapper module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const ::boost::regex_error& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // RegexWrapper class binding
    py::class_<atom::extra::boost::RegexWrapper>(
        m, "RegexWrapper",
        R"(A wrapper class for Boost.Regex providing various regex operations.

This class provides pattern matching, searching, replacing, and other regex operations
using the Boost.Regex library.

Args:
    pattern: The regex pattern to use.
    flags: Optional regex syntax option flags (default is normal).

Examples:
    >>> from atom.extra.boost import regex
    >>> r = regex.RegexWrapper(r"\d+")
    >>> r.match("123")
    True
    >>> r.match("abc")
    False
    >>> r.search("abc123def")
    '123'
)")
        .def(py::init<std::string_view,
                      ::boost::regex_constants::syntax_option_type>(),
             py::arg("pattern"),
             py::arg("flags") = ::boost::regex_constants::normal,
             "Constructs a RegexWrapper with the given pattern and optional "
             "flags.")
        .def("match", &atom::extra::boost::RegexWrapper::match<std::string>,
             py::arg("str"),
             R"(Matches the given string against the regex pattern.

Args:
    str: The input string to match.

Returns:
    True if the string matches the pattern, false otherwise.

Examples:
    >>> r = regex.RegexWrapper(r"^hello")
    >>> r.match("hello world")
    True
    >>> r.match("world hello")
    False
)")
        .def(
            "search", &atom::extra::boost::RegexWrapper::search<std::string>,
            py::arg("str"),
            R"(Searches the given string for the first match of the regex pattern.

Args:
    str: The input string to search.

Returns:
    The first match if found, None otherwise.

Examples:
    >>> r = regex.RegexWrapper(r"\d+")
    >>> r.search("abc123def")
    '123'
    >>> r.search("abcdef") is None
    True
)")
        .def("search_all",
             &atom::extra::boost::RegexWrapper::searchAll<std::string>,
             py::arg("str"),
             R"(Searches the given string for all matches of the regex pattern.

Args:
    str: The input string to search.

Returns:
    A list containing all matches found.

Examples:
    >>> r = regex.RegexWrapper(r"\d+")
    >>> r.search_all("abc123def456ghi")
    ['123', '456']
)")
        .def(
            "replace",
            &atom::extra::boost::RegexWrapper::replace<std::string,
                                                       std::string>,
            py::arg("str"), py::arg("replacement"),
            R"(Replaces all matches of the regex pattern in the given string with the replacement string.

Args:
    str: The input string.
    replacement: The replacement string.

Returns:
    A new string with all matches replaced.

Examples:
    >>> r = regex.RegexWrapper(r"\d+")
    >>> r.replace("abc123def456", "X")
    'abcXdefX'
)")
        .def("split", &atom::extra::boost::RegexWrapper::split<std::string>,
             py::arg("str"),
             R"(Splits the given string by the regex pattern.

Args:
    str: The input string to split.

Returns:
    A list containing the split parts of the string.

Examples:
    >>> r = regex.RegexWrapper(r"[,;]")
    >>> r.split("a,b;c,d")
    ['a', 'b', 'c', 'd']
)")
        .def("match_groups",
             &atom::extra::boost::RegexWrapper::matchGroups<std::string>,
             py::arg("str"),
             R"(Matches the given string and returns the groups of each match.

Args:
    str: The input string to match.

Returns:
    A list of tuples, each containing the full match and a list of groups.

Examples:
    >>> r = regex.RegexWrapper(r"(\w+):(\d+) ")
    >>> r.match_groups("name:123 age:45")
    [('name:123', ['name', '123']), ('age:45', ['age', '45'])]
)")
        .def(
            "for_each_match",
            [](atom::extra::boost::RegexWrapper& self, const std::string& str,
               py::function func) {
                self.forEachMatch(str, [&func](const ::boost::smatch& match) {
                    func(match.str());
                });
            },
            py::arg("str"), py::arg("func"),
            R"(Applies a function to each match of the regex pattern in the given string.

Args:
    str: The input string.
    func: The function to apply to each match.

Examples:
    >>> r = regex.RegexWrapper(r"\d+")
    >>> matches = []
    >>> r.for_each_match("a1b2c3", lambda m: matches.append(int(m)))
    >>> matches
    [1, 2, 3]
)")
        .def("get_pattern", &atom::extra::boost::RegexWrapper::getPattern,
             "Gets the regex pattern as a string.")
        .def("set_pattern", &atom::extra::boost::RegexWrapper::setPattern,
             py::arg("pattern"),
             py::arg("flags") = ::boost::regex_constants::normal,
             R"(Sets a new regex pattern with optional flags.

Args:
    pattern: The new regex pattern.
    flags: The regex syntax option flags (default is normal).
)")
        .def("named_captures",
             &atom::extra::boost::RegexWrapper::namedCaptures<std::string>,
             py::arg("str"),
             R"(Matches the given string and returns the named captures.

Args:
    str: The input string to match.

Returns:
    A dictionary of named captures.

Examples:
    >>> r = regex.RegexWrapper(r"(\w+):(\d+) ")
    >>> r.named_captures("name:123")
    {'1': 'name', '2': '123'}
)")
        .def(
            "is_valid", &atom::extra::boost::RegexWrapper::isValid<std::string>,
            py::arg("str"),
            R"(Checks if the given string is a valid match for the regex pattern.

Args:
    str: The input string to check.

Returns:
    True if the string is a valid match, false otherwise.
)")
        .def(
            "replace_callback",
            [](atom::extra::boost::RegexWrapper& self, const std::string& str,
               py::function callback) {
                return self.replaceCallback(
                    str, [&callback](const ::boost::smatch& match) {
                        return callback(match.str()).cast<std::string>();
                    });
            },
            py::arg("str"), py::arg("callback"),
            R"(Replaces all matches of the regex pattern in the given string using a callback function.

Args:
    str: The input string.
    callback: The callback function to generate replacements.

Returns:
    A new string with all matches replaced by the callback results.

Examples:
    >>> r = regex.RegexWrapper(r"\d+")
    >>> r.replace_callback("a1b23c", lambda m: str(int(m) * 2))
    'a2b46c'
)")
        .def(
            "benchmark_match",
            &atom::extra::boost::RegexWrapper::benchmarkMatch<std::string>,
            py::arg("str"), py::arg("iterations") = 1000,
            R"(Benchmarks the match operation for the given string over a number of iterations.

Args:
    str: The input string to match.
    iterations: The number of iterations to run the benchmark (default is 1000).

Returns:
    The average time per match operation in nanoseconds.

Examples:
    >>> r = regex.RegexWrapper(r"^hello\s+world$")
    >>> time_ns = r.benchmark_match("hello world", 10000)
    >>> # time_ns contains the average time in nanoseconds
)")
        .def(
            "count_matches",
            &atom::extra::boost::RegexWrapper::countMatches<std::string>,
            py::arg("str"),
            R"(Counts the number of matches of the regex pattern in the given string.

Args:
    str: The input string to search.

Returns:
    The number of matches found.

Examples:
    >>> r = regex.RegexWrapper(r"\d+")
    >>> r.count_matches("a1b23c456")
    3
)")
        // Static methods
        .def_static(
            "escape_string", &atom::extra::boost::RegexWrapper::escapeString,
            py::arg("str"),
            R"(Escapes special characters in the given string for use in a regex pattern.

Args:
    str: The input string to escape.

Returns:
    The escaped string.

Examples:
    >>> regex.RegexWrapper.escape_string("a.b*c+d")
    'a\\.b\\*c\\+d'
)")
        .def_static("is_valid_regex",
                    &atom::extra::boost::RegexWrapper::isValidRegex,
                    py::arg("pattern"),
                    R"(Checks if the given regex pattern is valid.

Args:
    pattern: The regex pattern to check.

Returns:
    True if the pattern is valid, false otherwise.

Examples:
    >>> regex.RegexWrapper.is_valid_regex("a+b*c")
    True
    >>> regex.RegexWrapper.is_valid_regex("a(b")
    False
)")
        .def_static("validate_and_compile",
                    &atom::extra::boost::RegexWrapper::validateAndCompile,
                    py::arg("pattern"),
                    R"(Validates and compiles a regex pattern.

Args:
    pattern: The regex pattern to validate and compile.

Returns:
    True if the pattern is valid and compiled successfully, false otherwise.

Examples:
    >>> regex.RegexWrapper.validate_and_compile("a+b*c")
    True
)");

    // Python-friendly aliases and shortcuts
    m.def(
        "match",
        [](const std::string& pattern, const std::string& text) {
            atom::extra::boost::RegexWrapper regex(pattern);
            return regex.match(text);
        },
        py::arg("pattern"), py::arg("text"),
        R"(Matches the given text against the regex pattern.

Args:
    pattern: The regex pattern.
    text: The input text to match.

Returns:
    True if the text matches the pattern, false otherwise.

Examples:
    >>> from atom.extra.boost.regex import match
    >>> match(r"\d+", "123")
    True
)");

    m.def(
        "search",
        [](const std::string& pattern, const std::string& text) {
            atom::extra::boost::RegexWrapper regex(pattern);
            return regex.search(text);
        },
        py::arg("pattern"), py::arg("text"),
        R"(Searches the given text for the first match of the regex pattern.

Args:
    pattern: The regex pattern.
    text: The input text to search.

Returns:
    The first match if found, None otherwise.

Examples:
    >>> from atom.extra.boost.regex import search
    >>> search(r"\d+", "abc123def")
    '123'
)");

    m.def(
        "replace",
        [](const std::string& pattern, const std::string& text,
           const std::string& replacement) {
            atom::extra::boost::RegexWrapper regex(pattern);
            return regex.replace(text, replacement);
        },
        py::arg("pattern"), py::arg("text"), py::arg("replacement"),
        R"(Replaces all matches of the regex pattern in the given text with the replacement string.

Args:
    pattern: The regex pattern.
    text: The input text.
    replacement: The replacement string.

Returns:
    A new string with all matches replaced.

Examples:
    >>> from atom.extra.boost.regex import replace
    >>> replace(r"\d+", "abc123def456", "X")
    'abcXdefX'
)");

    m.def(
        "split",
        [](const std::string& pattern, const std::string& text) {
            atom::extra::boost::RegexWrapper regex(pattern);
            return regex.split(text);
        },
        py::arg("pattern"), py::arg("text"),
        R"(Splits the given text by the regex pattern.

Args:
    pattern: The regex pattern.
    text: The input text to split.

Returns:
    A list containing the split parts of the text.

Examples:
    >>> from atom.extra.boost.regex import split
    >>> split(r"[,;]", "a,b;c,d")
    ['a', 'b', 'c', 'd']
)");

    // Add regex constants from boost::regex_constants
    m.attr("SYNTAX_NORMAL") =
        static_cast<unsigned int>(::boost::regex_constants::normal);
    m.attr("SYNTAX_ICASE") =
        static_cast<unsigned int>(::boost::regex_constants::icase);
    m.attr("SYNTAX_NOSUBS") =
        static_cast<unsigned int>(::boost::regex_constants::nosubs);
    m.attr("SYNTAX_OPTIMIZE") =
        static_cast<unsigned int>(::boost::regex_constants::optimize);
    m.attr("SYNTAX_COLLATE") =
        static_cast<unsigned int>(::boost::regex_constants::collate);
    m.attr("SYNTAX_ECMASCRIPT") =
        static_cast<unsigned int>(::boost::regex_constants::ECMAScript);
    m.attr("SYNTAX_BASIC") =
        static_cast<unsigned int>(::boost::regex_constants::basic);
    m.attr("SYNTAX_EXTENDED") =
        static_cast<unsigned int>(::boost::regex_constants::extended);
    m.attr("SYNTAX_PERL") =
        static_cast<unsigned int>(::boost::regex_constants::perl);

    py::enum_<::boost::regex_constants::match_flag_type>(m, "MatchFlag",
                                                         py::arithmetic())
        .value("match_default", ::boost::regex_constants::match_default,
               "Default matching behavior")
        .value("match_not_bol", ::boost::regex_constants::match_not_bol,
               "Beginning of line is not special")
        .value("match_not_eol", ::boost::regex_constants::match_not_eol,
               "End of line is not special")
        .value("match_not_bow", ::boost::regex_constants::match_not_bow,
               "Beginning of word is not special")
        .value("match_not_eow", ::boost::regex_constants::match_not_eow,
               "End of word is not special")
        .value("match_any", ::boost::regex_constants::match_any,
               "Match any pattern")
        .value("match_not_null", ::boost::regex_constants::match_not_null,
               "Do not match empty strings")
        .value("match_continuous", ::boost::regex_constants::match_continuous,
               "Match must start at search location")
        .value("match_prev_avail", ::boost::regex_constants::match_prev_avail,
               "Previous character is available")
        .export_values();

    // Convenience functions
    m.def(
        "is_email",
        [](const std::string& text) {
            static const atom::extra::boost::RegexWrapper email_regex(
                R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
            return email_regex.match(text);
        },
        py::arg("text"),
        R"(Checks if the given text is a valid email address.

Args:
    text: The text to check.

Returns:
    True if the text is a valid email address, false otherwise.

Examples:
    >>> from atom.extra.boost.regex import is_email
    >>> is_email("user@example.com")
    True
    >>> is_email("invalid-email")
    False
)");

    m.def(
        "is_url",
        [](const std::string& text) {
            static const atom::extra::boost::RegexWrapper url_regex(
                R"(^(https?|ftp)://[^\s/$.?#].[^\s]*$)");
            return url_regex.match(text);
        },
        py::arg("text"),
        R"(Checks if the given text is a valid URL.

Args:
    text: The text to check.

Returns:
    True if the text is a valid URL, false otherwise.

Examples:
    >>> from atom.extra.boost.regex import is_url
    >>> is_url("https://example.com")
    True
    >>> is_url("invalid-url")
    False
)");

    m.def(
        "extract_numbers",
        [](const std::string& text) {
            static const atom::extra::boost::RegexWrapper number_regex(
                R"(\d+(?:\.\d+)?)");
            auto matches = number_regex.searchAll(text);
            std::vector<double> numbers;
            numbers.reserve(matches.size());
            for (const auto& match : matches) {
                try {
                    numbers.push_back(std::stod(match));
                } catch (const std::exception&) {
                    // Ignore invalid conversions
                }
            }
            return numbers;
        },
        py::arg("text"),
        R"(Extracts all numbers (integer and floating-point) from the given text.

Args:
    text: The text to extract numbers from.

Returns:
    A list of numbers found in the text.

Examples:
    >>> from atom.extra.boost.regex import extract_numbers
    >>> extract_numbers("Temperature is 25.5 degrees and humidity is 60%")
    [25.5, 60.0]
)");

    // Add version info
    m.attr("__version__") = "1.0.0";
}
