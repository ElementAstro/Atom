#include "atom/io/async/async_glob.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>

namespace py = pybind11;
namespace fs = std::filesystem;

#ifdef ATOM_USE_ASIO
PYBIND11_MODULE(async_glob, m) {
    m.doc() = R"pbdoc(
        Asynchronous Glob Pattern Matching Module
        ----------------------------------------

        This module provides asynchronous file globbing operations using ASIO
        for non-blocking pattern matching and file searching.

        Features:
        - Asynchronous glob operations with callbacks
        - Coroutine-based async glob support
        - Recursive pattern matching
        - Directory-only filtering
        - Pattern filtering utilities

        Examples:
            >>> import asio
            >>> from atom.io.async_glob import AsyncGlob
            >>>
            >>> io_context = asio.io_context()
            >>> glob = AsyncGlob(io_context)
            >>>
            >>> # Asynchronous glob with callback
            >>> def on_glob(paths):
            ...     print(f"Found {len(paths)} files")
            ...     for path in paths:
            ...         print(path)
            >>>
            >>> glob.glob("*.py", on_glob)
            >>> io_context.run()
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

    // AsyncGlob Task wrapper (not directly exposed, used internally)
    py::class_<atom::io::AsyncGlob::Task<std::vector<fs::path>>>(
        m, "TaskPaths", "Internal task type for async glob operations")
        .def("get_result",
             &atom::io::AsyncGlob::Task<std::vector<fs::path>>::get_result);

    // AsyncGlob class binding
    py::class_<atom::io::AsyncGlob>(
        m, "AsyncGlob",
        R"(Asynchronous file globbing operations using ASIO.

This class provides non-blocking glob operations for pattern matching
and file searching, supporting both callback-based and coroutine-based
async patterns.

Args:
    io_context: The ASIO I/O context for asynchronous operations

Examples:
    >>> import asio
    >>> from atom.io.async_glob import AsyncGlob
    >>>
    >>> io_context = asio.io_context()
    >>> glob = AsyncGlob(io_context)
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
                    [callback](const std::vector<fs::path>& paths) {
                        py::gil_scoped_acquire acquire;
                        std::vector<std::string> str_paths;
                        for (const auto& path : paths) {
                            str_paths.push_back(path.string());
                        }
                        callback(str_paths);
                    },
                    recursive, dironly);
            },
            py::arg("pathname"), py::arg("callback"),
            py::arg("recursive") = false, py::arg("dironly") = false,
            R"(Performs asynchronous glob operation with callback.

Args:
    pathname: The pattern to match files (supports *, ?, [], **)
    callback: Function to call with matched file paths
    recursive: Enable recursive matching with ** patterns
    dironly: Only match directories if True

Examples:
    >>> def on_files(paths):
    ...     for path in paths:
    ...         print(f"Found: {path}")
    >>>
    >>> glob.glob("**/*.txt", on_files, recursive=True)
    >>> io_context.run()
)")
        .def(
            "glob_sync",
            [](atom::io::AsyncGlob& self, const std::string& pathname,
               bool recursive = false, bool dironly = false) {
                auto paths = self.glob_sync(pathname, recursive, dironly);
                std::vector<std::string> str_paths;
                for (const auto& path : paths) {
                    str_paths.push_back(path.string());
                }
                return str_paths;
            },
            py::arg("pathname"), py::arg("recursive") = false,
            py::arg("dironly") = false,
            R"(Performs synchronous glob operation.

Args:
    pathname: The pattern to match files
    recursive: Enable recursive matching
    dironly: Only match directories if True

Returns:
    List of matching file paths as strings

Examples:
    >>> paths = glob.glob_sync("*.py")
    >>> print(f"Found {len(paths)} Python files")
)")
        .def(
            "filter",
            [](const atom::io::AsyncGlob& self,
               const std::vector<std::string>& names,
               const std::string& pattern) {
                std::vector<fs::path> paths;
                for (const auto& name : names) {
                    paths.emplace_back(name);
                }
                auto filtered = self.filter(paths, pattern);
                std::vector<std::string> result;
                for (const auto& path : filtered) {
                    result.push_back(path.string());
                }
                return result;
            },
            py::arg("names"), py::arg("pattern"),
            R"(Filters a list of file names against a glob pattern.

Args:
    names: List of file names to filter
    pattern: Glob pattern to match against

Returns:
    Filtered list of file names that match the pattern

Examples:
    >>> files = ["test.py", "main.cpp", "utils.py", "data.txt"]
    >>> python_files = glob.filter(files, "*.py")
    >>> print(python_files)  # ['test.py', 'utils.py']
)");

    // Module metadata
    m.attr("__version__") = "1.0.0";
    m.attr("HAS_ASIO_SUPPORT") = true;
}
#else
// Provide a stub module when ASIO is not available
PYBIND11_MODULE(async_glob, m) {
    m.doc() = "AsyncGlob module - ASIO support not compiled";
    m.attr("HAS_ASIO_SUPPORT") = false;

    py::class_<int>(m, "AsyncGlob").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncGlob requires ASIO support which was not compiled in");
        return 0;
    }));
}
#endif  // ATOM_USE_ASIO
