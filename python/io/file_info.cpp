#include "atom/io/filesystem/file_info.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(file_info, m) {
    m.doc() = R"pbdoc(
        File Information Module
        ----------------------

        This module provides comprehensive file information retrieval functionality including:
        - Detailed file metadata (size, timestamps, permissions)
        - Cross-platform file attributes
        - File type detection
        - Owner and group information (Unix/Linux)
        - Symbolic link target resolution
        - Hidden file detection

        Examples:
            >>> import atom.io.file_info as file_info
            >>> # Get detailed information about a file
            >>> info = file_info.get_file_info("example.txt")
            >>> print(f"File size: {info.file_size} bytes")
            >>> print(f"Last modified: {info.last_modified_time}")
            >>> print(f"Permissions: {info.permissions}")
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

    // FileInfo structure binding
    py::class_<atom::io::FileInfo>(m, "FileInfo",
        R"(Structure containing detailed file information.
        
        This class provides comprehensive metadata about a file including
        timestamps, permissions, ownership, and type information.
        
        Attributes:
            file_path: Absolute path of the file
            file_name: Name of the file
            extension: File extension
            file_size: Size of the file in bytes
            file_type: Type of the file (e.g., Regular file, Directory)
            creation_time: Creation timestamp
            last_modified_time: Last modification timestamp
            last_access_time: Last access timestamp
            permissions: File permissions (e.g., rwxr-xr-x)
            is_hidden: Whether the file is hidden
            owner: Owner of the file
            group: Group of the file (Unix/Linux only)
            symlink_target: Target of symbolic link (Unix/Linux only)
        )")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("file_path", &atom::io::FileInfo::filePath,
                      "Absolute path of the file")
        .def_readwrite("file_name", &atom::io::FileInfo::fileName,
                      "Name of the file")
        .def_readwrite("extension", &atom::io::FileInfo::extension,
                      "File extension")
        .def_readwrite("file_size", &atom::io::FileInfo::fileSize,
                      "Size of the file in bytes")
        .def_readwrite("file_type", &atom::io::FileInfo::fileType,
                      "Type of the file (e.g., Regular file, Directory)")
        .def_readwrite("creation_time", &atom::io::FileInfo::creationTime,
                      "Creation timestamp")
        .def_readwrite("last_modified_time", &atom::io::FileInfo::lastModifiedTime,
                      "Last modification timestamp")
        .def_readwrite("last_access_time", &atom::io::FileInfo::lastAccessTime,
                      "Last access timestamp")
        .def_readwrite("permissions", &atom::io::FileInfo::permissions,
                      "File permissions (e.g., rwxr-xr-x)")
        .def_readwrite("is_hidden", &atom::io::FileInfo::isHidden,
                      "Whether the file is hidden")
        .def_readwrite("owner", &atom::io::FileInfo::owner,
                      "Owner of the file")
#ifndef _WIN32
        .def_readwrite("group", &atom::io::FileInfo::group,
                      "Group of the file (Unix/Linux only)")
        .def_readwrite("symlink_target", &atom::io::FileInfo::symlinkTarget,
                      "Target of symbolic link (Unix/Linux only)")
#endif
        .def("__repr__", [](const atom::io::FileInfo& info) {
            return "<FileInfo(path='" + std::string(info.filePath.c_str()) + 
                   "', size=" + std::to_string(info.fileSize) + 
                   ", type='" + std::string(info.fileType.c_str()) + "')>";
        })
        .def("__str__", [](const atom::io::FileInfo& info) {
            std::string result = "File Information:\n";
            result += "  Path: " + std::string(info.filePath.c_str()) + "\n";
            result += "  Name: " + std::string(info.fileName.c_str()) + "\n";
            result += "  Extension: " + std::string(info.extension.c_str()) + "\n";
            result += "  Size: " + std::to_string(info.fileSize) + " bytes\n";
            result += "  Type: " + std::string(info.fileType.c_str()) + "\n";
            result += "  Modified: " + std::string(info.lastModifiedTime.c_str()) + "\n";
            result += "  Permissions: " + std::string(info.permissions.c_str()) + "\n";
            result += "  Hidden: " + (info.isHidden ? "Yes" : "No") + "\n";
            result += "  Owner: " + std::string(info.owner.c_str());
#ifndef _WIN32
            result += "\n  Group: " + std::string(info.group.c_str());
            if (!info.symlinkTarget.empty()) {
                result += "\n  Symlink Target: " + std::string(info.symlinkTarget.c_str());
            }
#endif
            return result;
        });

    // Main functions
    m.def("get_file_info", 
          [](const std::string& file_path) {
              fs::path path(file_path);
              return atom::io::getFileInfo(path);
          },
          py::arg("file_path"),
          R"(Retrieves detailed information about a file.

Args:
    file_path: The path to the file

Returns:
    FileInfo object containing the file's detailed information

Raises:
    RuntimeError: If the file does not exist or cannot be accessed
    OSError: If there's a filesystem error

Examples:
    >>> info = get_file_info("example.txt")
    >>> print(f"File size: {info.file_size} bytes")
    >>> print(f"Last modified: {info.last_modified_time}")
    >>> print(f"Permissions: {info.permissions}")
    >>> print(f"Is hidden: {info.is_hidden}")
)");

    m.def("print_file_info", &atom::io::printFileInfo,
          py::arg("info"),
          R"(Prints file information to the console.

Args:
    info: FileInfo object containing file details

Examples:
    >>> info = get_file_info("example.txt")
    >>> print_file_info(info)
)");

    // Convenience functions
    m.def("get_file_size", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return info.fileSize;
          },
          py::arg("file_path"),
          R"(Gets the size of a file in bytes.

Args:
    file_path: The path to the file

Returns:
    File size in bytes

Examples:
    >>> size = get_file_size("example.txt")
    >>> print(f"File is {size} bytes")
)");

    m.def("get_file_type", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return std::string(info.fileType.c_str());
          },
          py::arg("file_path"),
          R"(Gets the type of a file.

Args:
    file_path: The path to the file

Returns:
    File type as a string (e.g., "Regular file", "Directory")

Examples:
    >>> file_type = get_file_type("example.txt")
    >>> print(f"File type: {file_type}")
)");

    m.def("get_file_permissions", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return std::string(info.permissions.c_str());
          },
          py::arg("file_path"),
          R"(Gets the permissions of a file.

Args:
    file_path: The path to the file

Returns:
    File permissions as a string (e.g., "rwxr-xr-x")

Examples:
    >>> perms = get_file_permissions("example.txt")
    >>> print(f"Permissions: {perms}")
)");

    m.def("is_hidden_file", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return info.isHidden;
          },
          py::arg("file_path"),
          R"(Checks if a file is hidden.

Args:
    file_path: The path to the file

Returns:
    True if the file is hidden, False otherwise

Examples:
    >>> hidden = is_hidden_file(".hidden_file")
    >>> print(f"Is hidden: {hidden}")
)");

    m.def("get_file_owner", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return std::string(info.owner.c_str());
          },
          py::arg("file_path"),
          R"(Gets the owner of a file.

Args:
    file_path: The path to the file

Returns:
    File owner as a string

Examples:
    >>> owner = get_file_owner("example.txt")
    >>> print(f"Owner: {owner}")
)");

#ifndef _WIN32
    m.def("get_file_group", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return std::string(info.group.c_str());
          },
          py::arg("file_path"),
          R"(Gets the group of a file (Unix/Linux only).

Args:
    file_path: The path to the file

Returns:
    File group as a string

Examples:
    >>> group = get_file_group("example.txt")
    >>> print(f"Group: {group}")
)");

    m.def("get_symlink_target", 
          [](const std::string& file_path) {
              auto info = atom::io::getFileInfo(fs::path(file_path));
              return std::string(info.symlinkTarget.c_str());
          },
          py::arg("file_path"),
          R"(Gets the target of a symbolic link (Unix/Linux only).

Args:
    file_path: The path to the symbolic link

Returns:
    Target path as a string, empty if not a symbolic link

Examples:
    >>> target = get_symlink_target("link_to_file")
    >>> if target:
    ...     print(f"Link points to: {target}")
)");
#endif

    // Utility functions for file information analysis
    m.def("compare_file_times", 
          [](const std::string& file1, const std::string& file2) {
              auto info1 = atom::io::getFileInfo(fs::path(file1));
              auto info2 = atom::io::getFileInfo(fs::path(file2));
              
              py::dict result;
              result["file1_newer"] = info1.lastModifiedTime > info2.lastModifiedTime;
              result["file2_newer"] = info2.lastModifiedTime > info1.lastModifiedTime;
              result["same_time"] = info1.lastModifiedTime == info2.lastModifiedTime;
              result["file1_time"] = std::string(info1.lastModifiedTime.c_str());
              result["file2_time"] = std::string(info2.lastModifiedTime.c_str());
              
              return result;
          },
          py::arg("file1"), py::arg("file2"),
          R"(Compares the modification times of two files.

Args:
    file1: Path to the first file
    file2: Path to the second file

Returns:
    Dictionary with comparison results

Examples:
    >>> result = compare_file_times("file1.txt", "file2.txt")
    >>> if result["file1_newer"]:
    ...     print("file1.txt is newer")
)");
}
