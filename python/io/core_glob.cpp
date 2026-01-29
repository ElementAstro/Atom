#include "atom/io/core/glob.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(core_glob, m) {
    m.doc() = R"pbdoc(
        Core Glob Pattern Matching Module
        --------------------------------

        This module provides comprehensive glob pattern matching functionality including:
        - Shell-style pattern matching with wildcards (*, ?, [])
        - Recursive pattern matching with **
        - Path filtering and validation
        - Tilde expansion for home directory
        - Hidden file detection
        - Cross-platform path handling

        Examples:
            >>> import atom.io.core_glob as glob
            >>> # Find all Python files
            >>> files = glob.glob("*.py")
            >>> print(f"Found {len(files)} Python files")
            >>>
            >>> # Recursive search
            >>> all_files = glob.rglob("**/*.txt")
            >>> print(f"Found {len(all_files)} text files recursively")
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
        } catch (const std::filesystem::filesystem_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Core glob functions
    m.def(
        "glob",
        [](const std::string& pathname, bool recursive = false,
           bool dironly = false) {
            atom::containers::String pattern(pathname.c_str());
            auto result = atom::io::glob(pattern, recursive, dironly);
            std::vector<std::string> paths;
            for (const auto& path : result) {
                paths.push_back(path.string());
            }
            return paths;
        },
        py::arg("pathname"), py::arg("recursive") = false,
        py::arg("dironly") = false,
        R"(Find all paths matching a shell-style pattern.

Args:
    pathname: The pattern to match (supports *, ?, [], **)
    recursive: Enable recursive matching with ** patterns
    dironly: Only return directories if True

Returns:
    List of matching file paths as strings

Examples:
    >>> # Find all Python files in current directory
    >>> files = glob("*.py")
    >>>
    >>> # Find all files recursively
    >>> all_files = glob("**/*", recursive=True)
    >>>
    >>> # Find only directories
    >>> dirs = glob("*/", dironly=True)
)");

    m.def(
        "rglob",
        [](const std::string& pathname) {
            atom::containers::String pattern(pathname.c_str());
            auto result = atom::io::rglob(pattern);
            std::vector<std::string> paths;
            for (const auto& path : result) {
                paths.push_back(path.string());
            }
            return paths;
        },
        py::arg("pathname"),
        R"(Find all paths matching a shell-style pattern recursively.

Args:
    pathname: The pattern to match

Returns:
    List of matching file paths as strings

Examples:
    >>> # Find all text files recursively
    >>> files = rglob("**/*.txt")
    >>>
    >>> # Find all Python files in any subdirectory
    >>> py_files = rglob("**/*.py")
)");

    m.def(
        "glob_multiple",
        [](const std::vector<std::string>& pathnames, bool recursive = false) {
            atom::containers::Vector<atom::containers::String> patterns;
            for (const auto& pathname : pathnames) {
                patterns.emplace_back(pathname.c_str());
            }

            auto result = recursive ? atom::io::rglob(patterns)
                                    : atom::io::glob(patterns);
            std::vector<std::string> paths;
            for (const auto& path : result) {
                paths.push_back(path.string());
            }
            return paths;
        },
        py::arg("pathnames"), py::arg("recursive") = false,
        R"(Find all paths matching multiple shell-style patterns.

Args:
    pathnames: List of patterns to match
    recursive: Enable recursive matching

Returns:
    List of matching file paths as strings

Examples:
    >>> # Find Python and C++ files
    >>> patterns = ["*.py", "*.cpp", "*.hpp"]
    >>> files = glob_multiple(patterns)
    >>>
    >>> # Recursive search for multiple patterns
    >>> files = glob_multiple(["**/*.txt", "**/*.md"], recursive=True)
)");

    // Pattern matching utilities
    m.def(
        "fnmatch",
        [](const std::string& name, const std::string& pattern) {
            fs::path path(name);
            atom::containers::String pat(pattern.c_str());
            return atom::io::fnmatch(path, pat);
        },
        py::arg("name"), py::arg("pattern"),
        R"(Test whether a filename matches a shell-style pattern.

Args:
    name: The filename or path to test
    pattern: The shell pattern to match against

Returns:
    True if the name matches the pattern, False otherwise

Examples:
    >>> fnmatch("hello.py", "*.py")
    True
    >>> fnmatch("test.txt", "*.py")
    False
    >>> fnmatch("file123.dat", "file???.dat")
    True
)");

    m.def(
        "filter_paths",
        [](const std::vector<std::string>& names, const std::string& pattern) {
            atom::containers::Vector<fs::path> paths;
            for (const auto& name : names) {
                paths.emplace_back(name);
            }

            atom::containers::String pat(pattern.c_str());
            auto result = atom::io::filter(paths, pat);

            std::vector<std::string> filtered;
            for (const auto& path : result) {
                filtered.push_back(path.string());
            }
            return filtered;
        },
        py::arg("names"), py::arg("pattern"),
        R"(Filter a list of paths by a shell-style pattern.

Args:
    names: List of file paths to filter
    pattern: The shell pattern to match against

Returns:
    List of paths that match the pattern

Examples:
    >>> paths = ["file1.py", "file2.txt", "script.py"]
    >>> python_files = filter_paths(paths, "*.py")
    >>> print(python_files)  # ['file1.py', 'script.py']
)");

    // Path utilities
    m.def(
        "expand_tilde",
        [](const std::string& path) {
            fs::path p(path);
            auto expanded = atom::io::expandTilde(p);
            return expanded.string();
        },
        py::arg("path"),
        R"(Expand tilde (~) in a filesystem path to the user's home directory.

Args:
    path: The path that may contain a tilde

Returns:
    The expanded path with tilde replaced by home directory

Raises:
    ValueError: If HOME environment variable is not set

Examples:
    >>> expand_tilde("~/documents")
    '/home/user/documents'  # On Linux
    >>> expand_tilde("~/Desktop/file.txt")
    'C:\\Users\\user\\Desktop\\file.txt'  # On Windows
)");

    m.def(
        "has_magic",
        [](const std::string& pathname) {
            atom::containers::String path(pathname.c_str());
            return atom::io::hasMagic(path);
        },
        py::arg("pathname"),
        R"(Check if a pathname contains glob magic characters.

Args:
    pathname: The path string to check

Returns:
    True if the pathname contains *, ?, or [ characters

Examples:
    >>> has_magic("*.py")
    True
    >>> has_magic("file.txt")
    False
    >>> has_magic("test[123].dat")
    True
)");

    m.def(
        "is_hidden",
        [](const std::string& pathname) {
            atom::containers::String path(pathname.c_str());
            return atom::io::isHidden(path);
        },
        py::arg("pathname"),
        R"(Check if a pathname represents a hidden file or directory.

Args:
    pathname: The path string to check

Returns:
    True if the pathname is hidden (starts with dot)

Examples:
    >>> is_hidden(".hidden_file")
    True
    >>> is_hidden("visible_file")
    False
    >>> is_hidden("/path/to/.hidden")
    True
)");

    m.def(
        "is_recursive",
        [](const std::string& pattern) {
            atom::containers::String pat(pattern.c_str());
            return atom::io::isRecursive(pat);
        },
        py::arg("pattern"),
        R"(Check if a pattern is a recursive glob pattern (**).

Args:
    pattern: The pattern to check

Returns:
    True if the pattern is "**"

Examples:
    >>> is_recursive("**")
    True
    >>> is_recursive("*")
    False
)");

    // Directory listing utilities
    m.def(
        "iter_directory",
        [](const std::string& dirname, bool dironly = false) {
            fs::path dir(dirname);
            auto result = atom::io::iterDirectory(dir, dironly);
            std::vector<std::string> paths;
            for (const auto& path : result) {
                paths.push_back(path.string());
            }
            return paths;
        },
        py::arg("dirname"), py::arg("dironly") = false,
        R"(Iterate through entries in a directory.

Args:
    dirname: The directory to iterate
    dironly: If True, only return directories

Returns:
    List of filesystem paths found in the directory

Examples:
    >>> entries = iter_directory("/path/to/dir")
    >>> dirs_only = iter_directory("/path/to/dir", dironly=True)
)");

    m.def(
        "rlist_directory",
        [](const std::string& dirname, bool dironly = false) {
            fs::path dir(dirname);
            auto result = atom::io::rlistdir(dir, dironly);
            std::vector<std::string> paths;
            for (const auto& path : result) {
                paths.push_back(path.string());
            }
            return paths;
        },
        py::arg("dirname"), py::arg("dironly") = false,
        R"(Recursively list all entries in a directory tree.

Args:
    dirname: The root directory to start from
    dironly: If True, only return directories

Returns:
    List of all filesystem paths found recursively

Examples:
    >>> all_files = rlist_directory("/path/to/dir")
    >>> all_dirs = rlist_directory("/path/to/dir", dironly=True)
)");

    // Pattern compilation utilities
    m.def(
        "translate_pattern",
        [](const std::string& pattern) {
            atom::containers::String pat(pattern.c_str());
            auto translated = atom::io::translate(pat);
            return std::string(translated.c_str());
        },
        py::arg("pattern"),
        R"(Translate a shell-style pattern to a regular expression.

Args:
    pattern: The shell pattern to translate (e.g., "*.txt", "file?.py")

Returns:
    The equivalent regular expression pattern

Examples:
    >>> regex_pattern = translate_pattern("*.py")
    >>> print(regex_pattern)  # Shows the regex equivalent
)");

    // Convenience functions for common patterns
    m.def(
        "find_files",
        [](const std::string& directory, const std::string& extension,
           bool recursive = false) {
            std::string pattern =
                recursive ? "**/*" + extension : "*" + extension;
            atom::containers::String pat(pattern.c_str());

            // Change to the directory temporarily for relative search
            fs::path original_path = fs::current_path();
            fs::path search_dir(directory);

            try {
                if (fs::exists(search_dir) && fs::is_directory(search_dir)) {
                    fs::current_path(search_dir);
                }

                auto result = recursive ? atom::io::rglob(pat)
                                        : atom::io::glob(pat, false, false);
                std::vector<std::string> paths;
                for (const auto& path : result) {
                    // Convert back to absolute paths relative to original
                    // directory
                    fs::path abs_path = search_dir / path;
                    paths.push_back(abs_path.string());
                }

                fs::current_path(original_path);
                return paths;
            } catch (...) {
                fs::current_path(original_path);
                throw;
            }
        },
        py::arg("directory"), py::arg("extension"),
        py::arg("recursive") = false,
        R"(Find files with a specific extension in a directory.

Args:
    directory: The directory to search in
    extension: The file extension to search for (e.g., ".py", ".txt")
    recursive: Search recursively in subdirectories

Returns:
    List of matching file paths

Examples:
    >>> python_files = find_files("/project", ".py", recursive=True)
    >>> text_files = find_files("/docs", ".txt")
)");

    m.def(
        "find_by_name",
        [](const std::string& directory, const std::string& name_pattern,
           bool recursive = false) {
            std::string pattern =
                recursive ? "**/" + name_pattern : name_pattern;
            atom::containers::String pat(pattern.c_str());

            fs::path original_path = fs::current_path();
            fs::path search_dir(directory);

            try {
                if (fs::exists(search_dir) && fs::is_directory(search_dir)) {
                    fs::current_path(search_dir);
                }

                auto result = recursive ? atom::io::rglob(pat)
                                        : atom::io::glob(pat, false, false);
                std::vector<std::string> paths;
                for (const auto& path : result) {
                    fs::path abs_path = search_dir / path;
                    paths.push_back(abs_path.string());
                }

                fs::current_path(original_path);
                return paths;
            } catch (...) {
                fs::current_path(original_path);
                throw;
            }
        },
        py::arg("directory"), py::arg("name_pattern"),
        py::arg("recursive") = false,
        R"(Find files matching a name pattern in a directory.

Args:
    directory: The directory to search in
    name_pattern: The name pattern to match (supports wildcards)
    recursive: Search recursively in subdirectories

Returns:
    List of matching file paths

Examples:
    >>> config_files = find_by_name("/project", "config*", recursive=True)
    >>> test_files = find_by_name("/tests", "test_*.py")
)");

    // Helper function for common glob patterns
    m.def(
        "common_patterns",
        []() {
            std::map<std::string, std::string> patterns;
            patterns["python_files"] = "*.py";
            patterns["cpp_files"] = "*.cpp";
            patterns["header_files"] = "*.h";
            patterns["text_files"] = "*.txt";
            patterns["markdown_files"] = "*.md";
            patterns["config_files"] = "*.conf";
            patterns["json_files"] = "*.json";
            patterns["xml_files"] = "*.xml";
            patterns["all_files"] = "*";
            patterns["hidden_files"] = ".*";
            patterns["recursive_all"] = "**/*";
            patterns["recursive_python"] = "**/*.py";
            patterns["recursive_cpp"] = "**/*.{cpp,hpp,h}";
            return patterns;
        },
        R"(Get a dictionary of common glob patterns.

Returns:
    Dictionary mapping pattern names to glob patterns

Examples:
    >>> patterns = common_patterns()
    >>> python_files = glob(patterns["python_files"])
    >>> all_files_recursive = glob(patterns["recursive_all"], recursive=True)
)");
}
