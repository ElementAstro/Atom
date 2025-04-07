#include "atom/algorithm/fnmatch.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(fnmatch, m) {
    m.doc() = R"pbdoc(
        Enhanced Python-Like fnmatch for C++
        -----------------------------------

        This module provides pattern matching functionality similar to Python's fnmatch,
        but with additional features and optimizations:
        
          - Case-insensitive matching
          - Path-aware matching
          - SIMD-accelerated matching (when available)
          - Support for multiple patterns
          - Parallel processing options
          
        Example:
            >>> from atom.algorithm import fnmatch
            >>> 
            >>> # Simple pattern matching
            >>> fnmatch.fnmatch("example.txt", "*.txt")
            True
            
            >>> # Case-insensitive matching
            >>> fnmatch.fnmatch("Example.TXT", "*.txt", fnmatch.CASEFOLD)
            True
            
            >>> # Filter a list of filenames
            >>> names = ["file1.txt", "file2.jpg", "file3.txt", "file4.png"]
            >>> fnmatch.filter(names, "*.txt")
            ["file1.txt", "file3.txt"]
    )pbdoc";

    // Register FnmatchException
    py::register_exception<atom::algorithm::FnmatchException>(
        m, "FnmatchException", PyExc_RuntimeError);

    // Flags as module constants
    m.attr("NOESCAPE") = py::int_(atom::algorithm::flags::NOESCAPE);
    m.attr("PATHNAME") = py::int_(atom::algorithm::flags::PATHNAME);
    m.attr("PERIOD") = py::int_(atom::algorithm::flags::PERIOD);
    m.attr("CASEFOLD") = py::int_(atom::algorithm::flags::CASEFOLD);

    // Register error enum
    py::enum_<atom::algorithm::FnmatchError>(m, "FnmatchError")
        .value("InvalidPattern", atom::algorithm::FnmatchError::InvalidPattern)
        .value("UnmatchedBracket",
               atom::algorithm::FnmatchError::UnmatchedBracket)
        .value("EscapeAtEnd", atom::algorithm::FnmatchError::EscapeAtEnd)
        .value("InternalError", atom::algorithm::FnmatchError::InternalError)
        .export_values();

    // Main fnmatch function
    m.def("fnmatch", &atom::algorithm::fnmatch<std::string, std::string>,
          py::arg("pattern"), py::arg("string"), py::arg("flags") = 0,
          R"pbdoc(
        Matches a string against a specified pattern.
        
        Args:
            pattern: The pattern to match against
            string: The string to match
            flags: Optional flags to modify matching behavior (default: 0)
                   Can be NOESCAPE, PATHNAME, PERIOD, CASEFOLD or combined with bitwise OR
            
        Returns:
            bool: True if the string matches the pattern, False otherwise
            
        Raises:
            FnmatchException: If there is an error in the pattern
    )pbdoc");

    // Non-throwing version
    m.def("fnmatch_nothrow",
          &atom::algorithm::fnmatch_nothrow<std::string, std::string>,
          py::arg("pattern"), py::arg("string"), py::arg("flags") = 0,
          R"pbdoc(
        Matches a string against a specified pattern without throwing exceptions.
        
        Args:
            pattern: The pattern to match against
            string: The string to match
            flags: Optional flags to modify matching behavior (default: 0)
            
        Returns:
            Expected object containing bool result or FnmatchError
    )pbdoc");

    // Filter with single pattern
    m.def(
        "filter",
        [](const py::list& names, const std::string& pattern, int flags) {
            std::vector<std::string> names_vec;
            for (const auto& name : names) {
                names_vec.push_back(name.cast<std::string>());
            }

            // Check if any elements match (single pattern version)
            return atom::algorithm::filter(names_vec, pattern, flags);
        },
        py::arg("names"), py::arg("pattern"), py::arg("flags") = 0,
        R"pbdoc(
        Check if any string in the list matches the pattern.
        
        Args:
            names: List of strings to filter
            pattern: Pattern to filter with
            flags: Optional flags to modify filtering behavior (default: 0)
            
        Returns:
            bool: True if any element matches the pattern
    )pbdoc");

    // Filter with multiple patterns
    m.def(
        "filter_multi",
        [](const py::list& names, const py::list& patterns, int flags,
           bool use_parallel) {
            std::vector<std::string> names_vec;
            for (const auto& name : names) {
                names_vec.push_back(name.cast<std::string>());
            }

            std::vector<std::string> patterns_vec;
            for (const auto& pattern : patterns) {
                patterns_vec.push_back(pattern.cast<std::string>());
            }

            // Filter with multiple patterns
            auto result = atom::algorithm::filter(names_vec, patterns_vec,
                                                  flags, use_parallel);

            // Convert result to Python list
            py::list py_result;
            for (const auto& item : result) {
                py_result.append(item);
            }

            return py_result;
        },
        py::arg("names"), py::arg("patterns"), py::arg("flags") = 0,
        py::arg("use_parallel") = true,
        R"pbdoc(
        Filter a list of strings with multiple patterns.
        
        Args:
            names: List of strings to filter
            patterns: List of patterns to filter with
            flags: Optional flags to modify filtering behavior (default: 0)
            use_parallel: Whether to use parallel execution (default: True)
            
        Returns:
            list: Strings from names that match any pattern in patterns
    )pbdoc");

    // Translate pattern to regex
    m.def("translate", &atom::algorithm::translate<std::string>,
          py::arg("pattern"), py::arg("flags") = 0,
          R"pbdoc(
        Translate a pattern into a regular expression string.
        
        Args:
            pattern: The pattern to translate
            flags: Optional flags to modify translation behavior (default: 0)
            
        Returns:
            Expected object containing regex string or FnmatchError
    )pbdoc");

    // CompiledPattern class for optimized matching
    py::class_<atom::algorithm::detail::CompiledPattern>(m, "CompiledPattern",
                                                         R"pbdoc(
        Pre-compiled pattern for efficient repeated matching.
        
        This class allows you to compile a pattern once and use it multiple times
        for better performance when matching the same pattern against many strings.
    )pbdoc")
        .def(py::init<std::string_view, int>(), py::arg("pattern"),
             py::arg("flags") = 0, "Compile a pattern with optional flags")
        .def("match", &atom::algorithm::detail::CompiledPattern::match,
             py::arg("string"), "Match a string against the compiled pattern");

    // Add version information
    m.attr("__version__") = "1.0.0";
}