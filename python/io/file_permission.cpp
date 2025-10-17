#include "atom/io/filesystem/file_permission.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>
#include <optional>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(file_permission, m) {
    m.doc() = R"pbdoc(
        File Permission Management Module
        --------------------------------

        This module provides comprehensive file permission management functionality including:
        - Permission comparison between files and current process
        - Permission string parsing and formatting
        - Cross-platform permission handling
        - Secure permission modification
        - Permission validation and analysis

        Security Note:
            Permission operations should be used carefully as they can affect
            system security. Always validate inputs and consider the principle
            of least privilege when modifying file permissions.

        Examples:
            >>> import atom.io.file_permission as perm
            >>> # Check if we can access a file
            >>> can_access = perm.compare_file_and_self_permissions("sensitive.txt")
            >>> if can_access:
            ...     print("Process has sufficient permissions")
            >>>
            >>> # Get file permissions
            >>> perms = perm.get_file_permissions("example.txt")
            >>> print(f"File permissions: {perms}")
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

    // Core permission functions
    m.def(
        "compare_file_and_self_permissions",
        [](const std::string& file_path) -> py::object {
            auto result = atom::io::compareFileAndSelfPermissions(file_path);
            if (result.has_value()) {
                return py::cast(result.value());
            } else {
                return py::none();
            }
        },
        py::arg("file_path"),
        R"(Compare file permissions with current process permissions.

Args:
    file_path: Path to the file for permission comparison

Returns:
    bool or None:
        - True: process has equal or greater permissions than file
        - False: process has lesser permissions than file
        - None: error occurred during comparison

Examples:
    >>> result = compare_file_and_self_permissions("example.txt")
    >>> if result is True:
    ...     print("Process has sufficient permissions")
    >>> elif result is False:
    ...     print("Process has insufficient permissions")
    >>> else:
    ...     print("Error comparing permissions")
)");

    m.def(
        "get_file_permissions",
        [](const std::string& file_path) -> py::object {
            auto result = atom::io::getFilePermissions(file_path);
            if (!result.empty()) {
                return py::cast(result);
            } else {
                return py::none();
            }
        },
        py::arg("file_path"),
        R"(Retrieve file permissions as a readable string.

Args:
    file_path: Path to the file

Returns:
    str or None: Permission string in format "rwxrwxrwx" or None on error

Examples:
    >>> perms = get_file_permissions("example.txt")
    >>> if perms:
    ...     print(f"File permissions: {perms}")
    >>> else:
    ...     print("Could not read file permissions")
)");

    m.def(
        "get_self_permissions",
        []() -> py::object {
            auto result = atom::io::getSelfPermissions();
            if (!result.empty()) {
                return py::cast(result);
            } else {
                return py::none();
            }
        },
        R"(Retrieve current process permissions as a readable string.

Returns:
    str or None: Permission string in format "rwxrwxrwx" or None on error

Examples:
    >>> perms = get_self_permissions()
    >>> if perms:
    ...     print(f"Process permissions: {perms}")
    >>> else:
    ...     print("Could not read process permissions")
)");

    m.def(
        "change_file_permissions",
        [](const std::string& file_path, const std::string& permissions) {
            fs::path path(file_path);
            atom::containers::String perms(permissions.c_str());
            atom::io::changeFilePermissions(path, perms);
        },
        py::arg("file_path"), py::arg("permissions"),
        R"(Modify file permissions using permission string.

Args:
    file_path: Filesystem path to the target file
    permissions: Permission string in format "rwxrwxrwx"

Raises:
    ValueError: If permission string format is invalid
    RuntimeError: If file doesn't exist or permission change fails
    OSError: If there's a filesystem error

Security Warning:
    This function modifies file permissions which can affect system security.
    Ensure proper validation of inputs and consider security implications.

Examples:
    >>> # Make file readable and writable by owner only
    >>> change_file_permissions("example.txt", "rw-------")
    >>>
    >>> # Make file executable by owner, readable by group and others
    >>> change_file_permissions("script.sh", "rwxr--r--")
)");

    // Utility functions for permission analysis
    m.def(
        "parse_permission_string",
        [](const std::string& permissions) {
            if (permissions.length() != 9) {
                throw std::invalid_argument(
                    "Permission string must be exactly 9 characters");
            }

            py::dict result;

            // Owner permissions
            result["owner_read"] = permissions[0] == 'r';
            result["owner_write"] = permissions[1] == 'w';
            result["owner_execute"] = permissions[2] == 'x';

            // Group permissions
            result["group_read"] = permissions[3] == 'r';
            result["group_write"] = permissions[4] == 'w';
            result["group_execute"] = permissions[5] == 'x';

            // Other permissions
            result["other_read"] = permissions[6] == 'r';
            result["other_write"] = permissions[7] == 'w';
            result["other_execute"] = permissions[8] == 'x';

            // Convenience flags
            result["is_readable"] = permissions[0] == 'r';
            result["is_writable"] = permissions[1] == 'w';
            result["is_executable"] = permissions[2] == 'x';
            result["is_public_readable"] = permissions[6] == 'r';
            result["is_public_writable"] = permissions[7] == 'w';

            return result;
        },
        py::arg("permissions"),
        R"(Parse a permission string into individual permission flags.

Args:
    permissions: Permission string in format "rwxrwxrwx"

Returns:
    dict: Dictionary with individual permission flags

Raises:
    ValueError: If permission string format is invalid

Examples:
    >>> perms = parse_permission_string("rwxr--r--")
    >>> print(f"Owner can read: {perms['owner_read']}")
    >>> print(f"File is executable: {perms['is_executable']}")
    >>> print(f"Public can write: {perms['is_public_writable']}")
)");

    m.def(
        "create_permission_string",
        [](bool owner_read, bool owner_write, bool owner_execute,
           bool group_read, bool group_write, bool group_execute,
           bool other_read, bool other_write, bool other_execute) {
            std::string result;
            result += owner_read ? 'r' : '-';
            result += owner_write ? 'w' : '-';
            result += owner_execute ? 'x' : '-';
            result += group_read ? 'r' : '-';
            result += group_write ? 'w' : '-';
            result += group_execute ? 'x' : '-';
            result += other_read ? 'r' : '-';
            result += other_write ? 'w' : '-';
            result += other_execute ? 'x' : '-';
            return result;
        },
        py::arg("owner_read"), py::arg("owner_write"), py::arg("owner_execute"),
        py::arg("group_read"), py::arg("group_write"), py::arg("group_execute"),
        py::arg("other_read"), py::arg("other_write"), py::arg("other_execute"),
        R"(Create a permission string from individual permission flags.

Args:
    owner_read: Owner read permission
    owner_write: Owner write permission
    owner_execute: Owner execute permission
    group_read: Group read permission
    group_write: Group write permission
    group_execute: Group execute permission
    other_read: Other read permission
    other_write: Other write permission
    other_execute: Other execute permission

Returns:
    str: Permission string in format "rwxrwxrwx"

Examples:
    >>> perms = create_permission_string(
    ...     True, True, True,    # Owner: rwx
    ...     True, False, True,   # Group: r-x
    ...     True, False, False   # Other: r--
    ... )
    >>> print(perms)  # "rwxr-xr--"
)");

    m.def(
        "is_permission_valid",
        [](const std::string& permissions) {
            if (permissions.length() != 9) {
                return false;
            }

            for (size_t i = 0; i < 9; ++i) {
                char c = permissions[i];
                if (i % 3 == 0) {  // Read position
                    if (c != 'r' && c != '-')
                        return false;
                } else if (i % 3 == 1) {  // Write position
                    if (c != 'w' && c != '-')
                        return false;
                } else {  // Execute position
                    if (c != 'x' && c != '-')
                        return false;
                }
            }
            return true;
        },
        py::arg("permissions"),
        R"(Validate a permission string format.

Args:
    permissions: Permission string to validate

Returns:
    bool: True if the permission string is valid, False otherwise

Examples:
    >>> is_permission_valid("rwxr-xr--")
    True
    >>> is_permission_valid("invalid")
    False
    >>> is_permission_valid("rwxrwxrwx")
    True
)");

    // Security-focused utility functions
    m.def(
        "is_secure_permissions",
        [](const std::string& permissions) {
            if (!atom::io::getFilePermissions(permissions).empty()) {
                // Check if file is not world-writable and not group-writable
                return permissions.length() >= 9 &&
                       permissions[4] != 'w' &&  // Group write
                       permissions[7] != 'w';    // Other write
            }
            return false;
        },
        py::arg("permissions"),
        R"(Check if permissions are considered secure (not world/group writable).

Args:
    permissions: Permission string to check

Returns:
    bool: True if permissions are secure, False otherwise

Examples:
    >>> is_secure_permissions("rw-------")  # Owner only
    True
    >>> is_secure_permissions("rw-rw-rw-")  # World writable
    False
)");

    m.def(
        "get_permission_octal",
        [](const std::string& permissions) {
            if (permissions.length() != 9) {
                throw std::invalid_argument(
                    "Permission string must be exactly 9 characters");
            }

            int octal = 0;

            // Owner permissions
            if (permissions[0] == 'r')
                octal += 400;
            if (permissions[1] == 'w')
                octal += 200;
            if (permissions[2] == 'x')
                octal += 100;

            // Group permissions
            if (permissions[3] == 'r')
                octal += 40;
            if (permissions[4] == 'w')
                octal += 20;
            if (permissions[5] == 'x')
                octal += 10;

            // Other permissions
            if (permissions[6] == 'r')
                octal += 4;
            if (permissions[7] == 'w')
                octal += 2;
            if (permissions[8] == 'x')
                octal += 1;

            return octal;
        },
        py::arg("permissions"),
        R"(Convert permission string to octal representation.

Args:
    permissions: Permission string in format "rwxrwxrwx"

Returns:
    int: Octal representation of permissions

Examples:
    >>> octal = get_permission_octal("rwxr-xr--")
    >>> print(f"Octal: {octal}")  # 754
)");
}
