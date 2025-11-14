#include "atom/io/core/io.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>
#include <span>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(core_io, m) {
    m.doc() = R"pbdoc(
        Core I/O Operations Module
        -------------------------

        This module provides comprehensive file and directory operations including:
        - Directory creation, removal, and management
        - File operations (copy, move, rename, delete)
        - Symbolic link operations
        - File size and type checking
        - Path utilities and validation
        - File splitting and merging operations

        Examples:
            >>> import atom.io.core_io as io
            >>> # Create a directory
            >>> io.create_directory("test_dir")
            True
            >>> # Copy a file
            >>> io.copy_file("source.txt", "destination.txt")
            True
            >>> # Get file size
            >>> size = io.file_size("myfile.txt")
            >>> print(f"File size: {size} bytes")
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

    // PathType enum
    py::enum_<atom::io::PathType>(
        m, "PathType",
        R"(Enumeration representing different types of filesystem paths.

        Values:
            NOT_EXISTS: Path does not exist
            REGULAR_FILE: Regular file
            DIRECTORY: Directory
            SYMLINK: Symbolic link
            OTHER: Other type (device, pipe, etc.)
        )")
        .value("NOT_EXISTS", atom::io::PathType::NOT_EXISTS,
               "Path does not exist")
        .value("REGULAR_FILE", atom::io::PathType::REGULAR_FILE, "Regular file")
        .value("DIRECTORY", atom::io::PathType::DIRECTORY, "Directory")
        .value("SYMLINK", atom::io::PathType::SYMLINK, "Symbolic link")
        .value("OTHER", atom::io::PathType::OTHER,
               "Other type (device, pipe, etc.)");

    // CreateDirectoriesOptions struct
    py::class_<atom::io::CreateDirectoriesOptions>(
        m, "CreateDirectoriesOptions",
        R"(Options for directory creation operations.

        Attributes:
            verbose: Enable verbose output
            dry_run: Perform dry run without actual operations
            delay: Delay between operations in milliseconds
        )")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("verbose", &atom::io::CreateDirectoriesOptions::verbose,
                       "Enable verbose output during operations")
        .def_readwrite("dry_run", &atom::io::CreateDirectoriesOptions::dryRun,
                       "Perform dry run without actual filesystem operations")
        .def_readwrite("delay", &atom::io::CreateDirectoriesOptions::delay,
                       "Delay between operations in milliseconds");

    // Directory operations
    m.def("create_directory", &atom::io::createDirectory<std::string>,
          py::arg("path"),
          R"(Creates a directory at the specified path.

Args:
    path: The path of the directory to create

Returns:
    True if the operation was successful, False otherwise

Raises:
    OSError: If there's a filesystem error

Examples:
    >>> create_directory("new_folder")
    True
    >>> create_directory("/path/to/new/directory")
    True
)");

    m.def("remove_directory", &atom::io::removeDirectory<std::string>,
          py::arg("path"),
          R"(Removes an empty directory at the specified path.

Args:
    path: The path of the directory to remove

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> remove_directory("empty_folder")
    True
)");

    m.def("rename_directory",
          &atom::io::renameDirectory<std::string, std::string>,
          py::arg("old_path"), py::arg("new_path"),
          R"(Renames a directory from old_path to new_path.

Args:
    old_path: The current path of the directory
    new_path: The new path for the directory

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> rename_directory("old_name", "new_name")
    True
)");

    m.def("move_directory", &atom::io::moveDirectory<std::string, std::string>,
          py::arg("old_path"), py::arg("new_path"),
          R"(Moves a directory from old_path to new_path.

Args:
    old_path: The source path of the directory
    new_path: The destination path for the directory

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> move_directory("source/dir", "destination/dir")
    True
)");

    // File operations
    m.def("copy_file", &atom::io::copyFile<std::string, std::string>,
          py::arg("src_path"), py::arg("dst_path"),
          R"(Copies a file from source path to destination path.

Args:
    src_path: The source file path
    dst_path: The destination file path

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> copy_file("source.txt", "destination.txt")
    True
)");

    m.def("move_file", &atom::io::moveFile<std::string, std::string>,
          py::arg("src_path"), py::arg("dst_path"),
          R"(Moves a file from source path to destination path.

Args:
    src_path: The source file path
    dst_path: The destination file path

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> move_file("old_location.txt", "new_location.txt")
    True
)");

    m.def("rename_file", &atom::io::renameFile<std::string, std::string>,
          py::arg("old_path"), py::arg("new_path"),
          R"(Renames a file from old_path to new_path.

Args:
    old_path: The current file path
    new_path: The new file path

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> rename_file("oldname.txt", "newname.txt")
    True
)");

    m.def("remove_file", &atom::io::removeFile<std::string>, py::arg("path"),
          R"(Removes a file at the specified path.

Args:
    path: The path of the file to remove

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> remove_file("unwanted_file.txt")
    True
)");

    // Symbolic link operations
    m.def("create_symlink", &atom::io::createSymlink<std::string, std::string>,
          py::arg("target_path"), py::arg("symlink_path"),
          R"(Creates a symbolic link pointing to target_path.

Args:
    target_path: The path that the symlink will point to
    symlink_path: The path where the symlink will be created

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> create_symlink("target_file.txt", "link_to_file.txt")
    True
)");

    m.def("remove_symlink", &atom::io::removeSymlink<std::string>,
          py::arg("path"),
          R"(Removes a symbolic link at the specified path.

Args:
    path: The path of the symlink to remove

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> remove_symlink("link_to_file.txt")
    True
)");

    // File information and utilities
    m.def("file_size", &atom::io::fileSize<std::string>, py::arg("path"),
          R"(Returns the size of a file in bytes.

Args:
    path: The path of the file

Returns:
    The size of the file in bytes, or 0 if the file doesn't exist

Examples:
    >>> size = file_size("myfile.txt")
    >>> print(f"File size: {size} bytes")
)");

    m.def("get_file_size", &atom::io::getFileSize<std::string>,
          py::arg("file_path"),
          R"(Alternative function to get file size.

Args:
    file_path: The path of the file

Returns:
    The size of the file in bytes

Examples:
    >>> size = get_file_size("myfile.txt")
    >>> print(f"File size: {size} bytes")
)");

    m.def("truncate_file", &atom::io::truncateFile<std::string>,
          py::arg("path"), py::arg("size"),
          R"(Truncates a file to the specified size.

Args:
    path: The path of the file to truncate
    size: The size to truncate the file to

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> truncate_file("myfile.txt", 1024)  # Truncate to 1KB
    True
)");

    // Path checking and validation
    m.def("is_folder_exists", &atom::io::isFolderExists<std::string>,
          py::arg("folder_path"),
          R"(Checks if a folder exists at the specified path.

Args:
    folder_path: The path to check

Returns:
    True if the folder exists, False otherwise

Examples:
    >>> is_folder_exists("/path/to/folder")
    True
)");

    m.def("is_file_exists", &atom::io::isFileExists<std::string>,
          py::arg("file_path"),
          R"(Checks if a file exists at the specified path.

Args:
    file_path: The path to check

Returns:
    True if the file exists, False otherwise

Examples:
    >>> is_file_exists("myfile.txt")
    True
)");

    m.def("is_folder_empty", &atom::io::isFolderEmpty<std::string>,
          py::arg("folder_path"),
          R"(Checks if a folder is empty.

Args:
    folder_path: The path of the folder to check

Returns:
    True if the folder is empty, False otherwise

Examples:
    >>> is_folder_empty("empty_folder")
    True
)");

    m.def("is_absolute_path", &atom::io::isAbsolutePath<std::string>,
          py::arg("path"),
          R"(Checks if a path is absolute.

Args:
    path: The path to check

Returns:
    True if the path is absolute, False otherwise

Examples:
    >>> is_absolute_path("/absolute/path")
    True
    >>> is_absolute_path("relative/path")
    False
)");

    m.def("check_path_type", &atom::io::checkPathType<std::string>,
          py::arg("path"),
          R"(Determines the type of a filesystem path.

Args:
    path: The path to check

Returns:
    PathType enum value indicating the type of path

Examples:
    >>> path_type = check_path_type("myfile.txt")
    >>> if path_type == PathType.REGULAR_FILE:
    ...     print("It's a regular file")
)");

    m.def("is_executable_file",
          &atom::io::isExecutableFile<std::string, std::string>,
          py::arg("file_name"), py::arg("file_ext") = "",
          R"(Checks if a file is executable.

Args:
    file_name: The name of the file to check
    file_ext: Optional file extension to check

Returns:
    True if the file is executable, False otherwise

Examples:
    >>> is_executable_file("program.exe")
    True
    >>> is_executable_file("script", ".sh")
    True
)");

    // Directory and file walking
    m.def("jwalk", &atom::io::jwalk<std::string>, py::arg("root"),
          R"(Recursively walks through a directory and returns JSON information.

Args:
    root: The root directory to walk

Returns:
    JSON string containing file information

Examples:
    >>> json_info = jwalk("/path/to/directory")
    >>> print(json_info)
)");

    m.def(
        "fwalk", &atom::io::fwalk<std::string>, py::arg("root"),
        py::arg("callback"),
        R"(Recursively walks through a directory, calling callback for each file.

Args:
    root: The root directory to walk
    callback: Function to call for each file path

Examples:
    >>> def print_file(path):
    ...     print(f"Found file: {path}")
    >>> fwalk("/path/to/directory", print_file)
)");

    // Working directory operations
    m.def("change_working_directory",
          &atom::io::changeWorkingDirectory<std::string>,
          py::arg("directory_path"),
          R"(Changes the current working directory.

Args:
    directory_path: The path to change to

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> change_working_directory("/new/working/directory")
    True
)");

    // File time operations
    m.def("get_file_times", &atom::io::getFileTimes<std::string>,
          py::arg("file_path"),
          R"(Gets the creation and modification times of a file.

Args:
    file_path: The path of the file

Returns:
    Tuple of (creation_time, modification_time) as strings

Examples:
    >>> creation, modification = get_file_times("myfile.txt")
    >>> print(f"Created: {creation}, Modified: {modification}")
)");

    // Line counting
    m.def("count_lines_in_file", &atom::io::countLinesInFile<std::string>,
          py::arg("file_path"),
          R"(Counts the number of lines in a text file.

Args:
    file_path: The path of the file to count lines in

Returns:
    Number of lines in the file, or None if file couldn't be opened

Examples:
    >>> lines = count_lines_in_file("textfile.txt")
    >>> if lines is not None:
    ...     print(f"File has {lines} lines")
)");

    // File splitting and merging
    m.def("split_file", &atom::io::splitFile<std::string, std::string>,
          py::arg("file_path"), py::arg("chunk_size"),
          py::arg("output_pattern") = "",
          R"(Splits a file into multiple chunks.

Args:
    file_path: The path of the file to split
    chunk_size: The size of each chunk in bytes
    output_pattern: Pattern for output file names (optional)

Examples:
    >>> split_file("largefile.txt", 1024*1024)  # Split into 1MB chunks
)");

    m.def(
        "merge_files",
        [](const std::string& output_file_path,
           const std::vector<std::string>& part_files) {
            atom::io::mergeFiles(output_file_path,
                                 std::span<const std::string>(
                                     part_files.data(), part_files.size()));
        },
        py::arg("output_file_path"), py::arg("part_files"),
        R"(Merges multiple file parts into a single file.

Args:
    output_file_path: The path for the merged output file
    part_files: List of part file paths to merge

Examples:
    >>> part_files = ["file.part1", "file.part2", "file.part3"]
    >>> merge_files("merged_file.txt", part_files)
)");

    m.def("quick_split", &atom::io::quickSplit<std::string, std::string>,
          py::arg("file_path"), py::arg("num_chunks"),
          py::arg("output_pattern") = "",
          R"(Quickly splits a file into a specified number of chunks.

Args:
    file_path: The path of the file to split
    num_chunks: The number of chunks to create
    output_pattern: Pattern for output file names (optional)

Examples:
    >>> quick_split("largefile.txt", 4)  # Split into 4 equal parts
)");

    // Utility functions
    m.def("calculate_chunk_size", &atom::io::calculateChunkSize,
          py::arg("file_size"), py::arg("num_chunks"),
          R"(Calculates the chunk size for splitting a file.

Args:
    file_size: The total size of the file
    num_chunks: The number of chunks to create

Returns:
    The calculated chunk size

Examples:
    >>> chunk_size = calculate_chunk_size(1024*1024, 4)  # 1MB file, 4 chunks
    >>> print(f"Each chunk will be {chunk_size} bytes")
)");

    // Search functions
    m.def(
        "search_executable_files",
        &atom::io::searchExecutableFiles<std::string>, py::arg("dir"),
        py::arg("search_str"),
        R"(Searches for executable files in a directory containing a search string.

Args:
    dir: Directory to search in
    search_str: String to search for in executable names

Returns:
    List of paths to matching executable files

Examples:
    >>> executables = search_executable_files("/usr/bin", "python")
    >>> for exe in executables:
    ...     print(f"Found: {exe}")
)");

    // Additional utility functions
    m.def(
        "get_executable_name",
        [](const std::string& path) -> std::string {
            return std::filesystem::path(path).filename().string();
        },
        py::arg("path"),
        R"(Gets the executable name from a path.

Args:
    path: The path to extract the executable name from

Returns:
    The executable name as a string

Examples:
    >>> name = get_executable_name("/usr/bin/python3")
    >>> print(f"Executable name: {name}")  # Output: python3
)");

    // Path conversion utilities
    m.def("convert_to_linux_path", &atom::io::convertToLinuxPath,
          py::arg("windows_path"),
          R"(Converts a Windows path to a Linux path.

Replaces backslashes with forward slashes.

Args:
    windows_path: The Windows path to convert

Returns:
    The converted Linux path as a string

Examples:
    >>> linux_path = convert_to_linux_path("C:\\Users\\Name\\file.txt")
    >>> print(linux_path)  # C:/Users/Name/file.txt
)");

    m.def("convert_to_windows_path", &atom::io::convertToWindowsPath,
          py::arg("linux_path"),
          R"(Converts a Linux path to a Windows path.

Replaces forward slashes with backslashes.

Args:
    linux_path: The Linux path to convert

Returns:
    The converted Windows path as a string

Examples:
    >>> windows_path = convert_to_windows_path("/home/user/file.txt")
    >>> print(windows_path)  # \home\user\file.txt
)");

    m.def("norm_path", &atom::io::normPath, py::arg("raw_path"),
          R"(Normalizes a path according to platform conventions.

Args:
    raw_path: The path to normalize

Returns:
    Normalized path string

Examples:
    >>> normalized = norm_path("./some/../path/./to/file.txt")
    >>> print(normalized)
)");

    m.def("is_folder_name_valid", &atom::io::isFolderNameValid,
          py::arg("folder_name"),
          R"(Checks if a folder name is valid.

Validates folder name according to platform-specific rules.

Args:
    folder_name: The folder name to validate

Returns:
    True if valid, False otherwise

Examples:
    >>> is_folder_name_valid("my_folder")
    True
    >>> is_folder_name_valid("folder:name")  # Colon invalid on Windows
    False (on Windows)
)");

    m.def("is_file_name_valid", &atom::io::isFileNameValid,
          py::arg("file_name"),
          R"(Checks if a file name is valid.

Validates file name according to platform-specific rules.

Args:
    file_name: The file name to validate

Returns:
    True if valid, False otherwise

Examples:
    >>> is_file_name_valid("document.txt")
    True
    >>> is_file_name_valid("file<name>.txt")  # < invalid on Windows
    False (on Windows)
)");

    m.def("classify_files", &atom::io::classifyFiles<std::string>,
          py::arg("directory"),
          R"(Classifies files in a directory by extension.

Args:
    directory: Directory to classify files in

Returns:
    Dictionary mapping extensions to lists of file paths

Examples:
    >>> files_by_ext = classify_files("/path/to/directory")
    >>> for ext, files in files_by_ext.items():
    ...     print(f"{ext}: {len(files)} files")
)");

    m.def(
        "check_file_type_in_folder",
        [](const std::string& folder_path,
           const std::vector<std::string>& file_types,
           atom::io::FileOption file_option) {
            return atom::io::checkFileTypeInFolder(
                folder_path,
                std::span<const std::string>(file_types.data(),
                                             file_types.size()),
                file_option);
        },
        py::arg("folder_path"), py::arg("file_types"), py::arg("file_option"),
        R"(Checks for files of specific types in a folder.

Args:
    folder_path: The folder to search in
    file_types: List of file extensions to look for (e.g., ['.txt', '.py'])
    file_option: FileOption.PATH or FileOption.NAME

Returns:
    List of file paths or names matching the specified types

Examples:
    >>> files = check_file_type_in_folder(
    ...     "/path/to/folder",
    ...     [".py", ".txt"],
    ...     FileOption.PATH
    ... )
    >>> for f in files:
    ...     print(f)
)");

    // FileOption enum
    py::enum_<atom::io::FileOption>(m, "FileOption",
                                    R"(Option for file type checking results.

Values:
    PATH: Return full file paths
    NAME: Return only file names
)")
        .value("PATH", atom::io::FileOption::PATH, "Return full file paths")
        .value("NAME", atom::io::FileOption::NAME, "Return only file names");

    m.def("quick_merge", &atom::io::quickMerge<std::string, std::string>,
          py::arg("output_file_path"), py::arg("part_pattern"),
          py::arg("num_chunks"),
          R"(Quickly merges file parts created by quick_split.

Args:
    output_file_path: The path for the merged output file
    part_pattern: The pattern used for part file names
    num_chunks: The number of chunks to merge

Examples:
    >>> quick_merge("merged_file.txt", "file.txt", 4)
)");
}
