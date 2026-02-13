#include "atom/io/core/path_utils.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>
#include <string>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(path_utils, m) {
    m.doc() = R"pbdoc(
        Path Utilities Module
        --------------------

        This module provides centralized path validation and manipulation utilities
        to ensure path security and consistency across the atom.io module.

        Features:
        - Comprehensive path validation with security checks
        - File and folder name validation
        - Permission validation
        - Path traversal detection
        - Platform-specific validation (Windows reserved names, invalid characters)

        Examples:
            >>> import atom.io.path_utils as pu
            >>> # Validate a path
            >>> pu.validate_path("/home/user/file.txt")
            True
            >>> # Check folder name validity
            >>> pu.is_folder_name_valid("my_folder")
            True
            >>> # Validate with permissions
            >>> pu.validate_permissions("/tmp/file.txt", write_access=True)
            True
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

    // Path validation functions
    m.def("validate_path", &atom::io::path_utils::validatePath, py::arg("path"),
          R"(Comprehensive path validation for security and format compliance.

Performs validation including:
- Empty path check
- Null byte detection (security vulnerability)
- Path length limits
- Path traversal pattern detection
- Windows-specific invalid characters and reserved names
- Filesystem path validation

Args:
    path: The path to validate

Returns:
    True if path is valid and safe, False otherwise

Examples:
    >>> validate_path("/home/user/file.txt")
    True
    >>> validate_path("")  # Empty path
    False
    >>> validate_path("/path/with\0null")  # Null byte
    False
)");

    m.def("is_folder_name_valid", &atom::io::path_utils::isFolderNameValid,
          py::arg("folder_name"),
          R"(Validates a folder name (not full path, just the name component).

Checks for platform-specific invalid characters and patterns.

Args:
    folder_name: The folder name to validate

Returns:
    True if valid, False otherwise

Examples:
    >>> is_folder_name_valid("my_folder")
    True
    >>> is_folder_name_valid("folder:name")  # Colon invalid on Windows
    False (on Windows)
    >>> is_folder_name_valid("")  # Empty name
    False
)");

    m.def("is_file_name_valid", &atom::io::path_utils::isFileNameValid,
          py::arg("file_name"),
          R"(Validates a file name (not full path, just the name component).

Checks for platform-specific invalid characters and patterns.

Args:
    file_name: The file name to validate

Returns:
    True if valid, False otherwise

Examples:
    >>> is_file_name_valid("document.txt")
    True
    >>> is_file_name_valid("file<name>.txt")  # < invalid on Windows
    False (on Windows)
    >>> is_file_name_valid("")  # Empty name
    False
)");

    m.def(
        "is_valid_path",
        [](const std::string& path) {
            return atom::io::path_utils::isValidPath(fs::path(path));
        },
        py::arg("path"),
        R"(Basic path validation for directory stack operations.

Args:
    path: The path to validate

Returns:
    True if valid, False otherwise

Examples:
    >>> is_valid_path("/home/user")
    True
    >>> is_valid_path("/nonexistent/../../path")
    False
)");

    m.def("validate_permissions", &atom::io::path_utils::validatePermissions,
          py::arg("path"), py::arg("write_access") = false,
          R"(Validates file permissions for read/write operations.

Args:
    path: Path to validate
    write_access: Whether write access is required (default: False)

Returns:
    True if permissions are valid, False otherwise

Examples:
    >>> validate_permissions("/tmp/file.txt")
    True
    >>> validate_permissions("/tmp/file.txt", write_access=True)
    True  # If file is writable
    >>> validate_permissions("/root/protected.txt", write_access=True)
    False  # Likely no write access
)");

    // Platform information
#ifdef _WIN32
    m.attr("PLATFORM") = "windows";
    m.attr("RESERVED_NAMES") = py::cast(std::vector<std::string>{
        "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
        "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
        "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"});
#else
    m.attr("PLATFORM") = "unix";
    m.attr("RESERVED_NAMES") = py::list();
#endif

    // Module metadata
    m.attr("__version__") = "1.0.0";
    m.attr("MAX_PATH_LENGTH") = 4096;
}
