#include "atom/io/async_glob.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(glob, m) {
    m.doc() = "Asynchronous glob implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::filesystem::filesystem_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::regex_error& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::error::Exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // AsyncGlob class binding
    py::class_<atom::io::AsyncGlob>(
        m, "AsyncGlob",
        R"(Class for performing asynchronous file globbing operations.

This class provides methods for matching file patterns using glob syntax,
supporting both synchronous and asynchronous operations.

Args:
    io_context: The ASIO I/O context to use for asynchronous operations.

Examples:
    >>> import asio
    >>> from atom.io.glob import AsyncGlob
    >>>
    >>> # Create an io_context and glob object
    >>> io_context = asio.io_context()
    >>> glob = AsyncGlob(io_context)
    >>>
    >>> # Example of synchronous usage
    >>> matches = glob.glob_sync("*.txt")
    >>> print(f"Found {len(matches)} text files")
    >>>
    >>> # Example of asynchronous usage with callback
    >>> def on_files_found(files):
    ...     print(f"Found {len(files)} files")
    >>>
    >>> glob.glob("*.py", on_files_found, recursive=True)
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&>(), py::arg("io_context"),
             "Constructs an AsyncGlob object with the given ASIO I/O context.")
        .def(
            "glob",
            [](atom::io::AsyncGlob& self, const std::string& pathname,
               py::function callback, bool recursive = false,
               bool dironly = false) {
                self.glob(
                    pathname,
                    [callback](std::vector<fs::path> paths) {
                        py::gil_scoped_acquire acquire;
                        callback(paths);
                    },
                    recursive, dironly);
            },
            py::arg("pathname"), py::arg("callback"),
            py::arg("recursive") = false, py::arg("dironly") = false,
            R"(Performs a glob operation to match files.

Args:
    pathname: The pattern to match files.
    callback: A callback function that will be called with the matched files.
    recursive: Whether to search directories recursively (default: False).
    dironly: Whether to match directories only (default: False).

Examples:
    >>> def print_matches(files):
    ...     print(f"Matched {len(files)} files")
    ...     for file in files:
    ...         print(f"  - {file}")
    >>>
    >>> glob.glob("*.py", print_matches)
    >>> io_context.run()  # Run the ASIO io_context
)")
        .def("glob_sync", &atom::io::AsyncGlob::glob_sync, py::arg("pathname"),
             py::arg("recursive") = false, py::arg("dironly") = false,
             R"(Performs a glob operation synchronously.

Args:
    pathname: The pattern to match files.
    recursive: Whether to search directories recursively (default: False).
    dironly: Whether to match directories only (default: False).

Returns:
    A list of matched paths.

Examples:
    >>> matches = glob.glob_sync("*.txt")
    >>> print(f"Found {len(matches)} text files")
)")
        .def(
            "glob_async",
            [](atom::io::AsyncGlob& self, const std::string& pathname,
               bool recursive, bool dironly) {
                // Wrap the Task in a future that we can convert to a Python
                // future
                auto task = self.glob_async(pathname, recursive, dironly);
                std::future<std::vector<fs::path>> future = std::async(
                    std::launch::deferred, [task = std::move(task)]() mutable {
                        return std::move(task).get_result();
                    });
                return future;
            },
            py::arg("pathname"), py::arg("recursive") = false,
            py::arg("dironly") = false,
            R"(Performs a glob operation asynchronously.

Args:
    pathname: The pattern to match files.
    recursive: Whether to search directories recursively (default: False).
    dironly: Whether to match directories only (default: False).

Returns:
    A future that will resolve to a list of matched paths.

Examples:
    >>> future = glob.glob_async("*.py")
    >>> # Do other work...
    >>> matches = future.result()  # Wait for result
    >>> print(f"Found {len(matches)} Python files")
)");

    // Helper functions

    // Simple glob convenience function similar to Python's glob.glob
    m.def(
        "glob",
        [](const std::string& pattern, bool recursive = false,
           bool dironly = false) {
            asio::io_context io_context;
            atom::io::AsyncGlob globber(io_context);
            return globber.glob_sync(pattern, recursive, dironly);
        },
        py::arg("pattern"), py::arg("recursive") = false,
        py::arg("dironly") = false,
        R"(A simple synchronous glob function.

This is a convenience function that works like Python's built-in glob.glob().

Args:
    pattern: The pattern to match files.
    recursive: Whether to search directories recursively (default: False).
    dironly: Whether to match directories only (default: False).

Returns:
    A list of matched paths.

Examples:
    >>> from atom.io.glob import glob
    >>> matches = glob("*.txt")
    >>> print(f"Found {len(matches)} text files")
)");

    // Recursive glob convenience function similar to Python's glob.glob with
    // recursive=True
    m.def(
        "rglob",
        [](const std::string& pattern, bool dironly = false) {
            asio::io_context io_context;
            atom::io::AsyncGlob globber(io_context);
            return globber.glob_sync(pattern, true, dironly);
        },
        py::arg("pattern"), py::arg("dironly") = false,
        R"(A simple recursive glob function.

This is a convenience function that works like Python's glob.glob() with recursive=True.

Args:
    pattern: The pattern to match files.
    dironly: Whether to match directories only (default: False).

Returns:
    A list of matched paths.

Examples:
    >>> from atom.io.glob import rglob
    >>> matches = rglob("**/*.py")  # Find all Python files recursively
    >>> print(f"Found {len(matches)} Python files")
)");

    // Function to check if a pattern has glob magic characters
    m.def(
        "has_magic",
        [](const std::string& pattern) -> bool {
            const std::string magic_chars = "*?[]";
            for (char c : pattern) {
                if (magic_chars.find(c) != std::string::npos) {
                    return true;
                }
            }
            return false;
        },
        py::arg("pattern"),
        R"(Checks if a pattern contains glob magic characters.

Args:
    pattern: The pattern to check.

Returns:
    True if the pattern contains magic characters, False otherwise.

Examples:
    >>> from atom.io.glob import has_magic
    >>> has_magic("file.txt")
    False
    >>> has_magic("*.txt")
    True
)");

    // Function to escape glob magic characters
    m.def(
        "escape",
        [](const std::string& pathname) {
            std::string result;
            result.reserve(pathname.size());
            for (char c : pathname) {
                if (c == '*' || c == '?' || c == '[' || c == ']') {
                    result.push_back('\\');
                }
                result.push_back(c);
            }
            return result;
        },
        py::arg("pathname"),
        R"(Escapes glob magic characters in a pathname.

Args:
    pathname: The pathname to escape.

Returns:
    The escaped pathname.

Examples:
    >>> from atom.io.glob import escape
    >>> escape("file[1].txt")  # Escapes the brackets
    'file\\[1\\].txt'
)");
}
