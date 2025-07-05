#include "atom/system/stat.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(stat, m) {
    m.doc() = "File statistics module for the atom package";

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

    // FilePermission enum
    py::enum_<atom::system::FilePermission>(m, "FilePermission",
                                            "File permission flags")
        .value("READ", atom::system::FilePermission::Read, "Read permission")
        .value("WRITE", atom::system::FilePermission::Write, "Write permission")
        .value("EXECUTE", atom::system::FilePermission::Execute,
               "Execute permission")
        .export_values();

    // File type enum (from std::filesystem)
    py::enum_<fs::file_type>(m, "FileType", "File type classification")
        .value("NONE", fs::file_type::none, "No file type or an error occurred")
        .value("NOT_FOUND", fs::file_type::not_found, "File not found")
        .value("REGULAR", fs::file_type::regular, "Regular file")
        .value("DIRECTORY", fs::file_type::directory, "Directory")
        .value("SYMLINK", fs::file_type::symlink, "Symbolic link")
        .value("BLOCK", fs::file_type::block, "Block special file")
        .value("CHARACTER", fs::file_type::character, "Character special file")
        .value("FIFO", fs::file_type::fifo, "FIFO or pipe")
        .value("SOCKET", fs::file_type::socket, "Socket")
        .value("UNKNOWN", fs::file_type::unknown, "Unknown file type")
        .export_values();

    // Stat class
    py::class_<atom::system::Stat>(m, "Stat",
                                   R"(Class representing file statistics.

This class provides methods to retrieve various attributes of a file,
such as its type, size, access time, modification time, and so on.
It caches file information for better performance.

Args:
    path: The path to the file whose statistics are to be retrieved.
    follow_symlinks: Whether to follow symlinks (default: True)

Raises:
    FileNotFoundError: If the file does not exist
    PermissionError: If the file cannot be accessed due to permission issues
    OSError: For other file access errors

Examples:
    >>> from atom.system import stat
    >>> # Get file statistics
    >>> s = stat.Stat("example.txt")
    >>> print(f"File size: {s.size()} bytes")
    >>> print(f"Last modified: {stat.Stat.format_time(s.mtime())}")
)")
        .def(py::init<const fs::path&, bool>(), py::arg("path"),
             py::arg("follow_symlinks") = true,
             "Constructs a Stat object for the specified file path.")
        .def("update", &atom::system::Stat::update,
             R"(Updates the file statistics.

This method refreshes the statistics for the file specified in the
constructor.

Raises:
    FileNotFoundError: If the file does not exist
    PermissionError: If the file cannot be accessed due to permission issues
    OSError: For other file access errors

Examples:
    >>> s = stat.Stat("example.txt")
    >>> # After file has been modified
    >>> s.update()
    >>> print(f"New size: {s.size()} bytes")
)")
        .def("exists", &atom::system::Stat::exists,
             R"(Checks if the file exists.

Returns:
    bool: True if the file exists, False otherwise.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> if s.exists():
    ...     print("File exists")
    ... else:
    ...     print("File does not exist")
)")
        .def("type", &atom::system::Stat::type,
             R"(Gets the type of the file.

Returns:
    FileType: The type of the file as an enum value.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> file_type = s.type()
    >>> if file_type == stat.FileType.REGULAR:
    ...     print("Regular file")
    >>> elif file_type == stat.FileType.DIRECTORY:
    ...     print("Directory")
)")
        .def("size", &atom::system::Stat::size,
             R"(Gets the size of the file.

Returns:
    int: The size of the file in bytes.

Raises:
    OSError: If the file size cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> size = s.size()
    >>> print(f"File size: {size} bytes")
)")
        .def("atime", &atom::system::Stat::atime,
             R"(Gets the last access time of the file.

Returns:
    int: The last access time of the file as a Unix timestamp.

Raises:
    OSError: If the access time cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> access_time = s.atime()
    >>> print(f"Last accessed: {stat.Stat.format_time(access_time)}")
)")
        .def("mtime", &atom::system::Stat::mtime,
             R"(Gets the last modification time of the file.

Returns:
    int: The last modification time of the file as a Unix timestamp.

Raises:
    OSError: If the modification time cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> mod_time = s.mtime()
    >>> print(f"Last modified: {stat.Stat.format_time(mod_time)}")
)")
        .def("ctime", &atom::system::Stat::ctime,
             R"(Gets the creation time of the file.

Returns:
    int: The creation time of the file as a Unix timestamp.

Raises:
    OSError: If the creation time cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> creation_time = s.ctime()
    >>> print(f"Created: {stat.Stat.format_time(creation_time)}")
)")
        .def("mode", &atom::system::Stat::mode,
             R"(Gets the file mode/permissions.

Returns:
    int: The file mode/permissions as an integer value.

Raises:
    OSError: If the file permissions cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> mode = s.mode()
    >>> print(f"File mode: {mode:o}")  # Print in octal format
)")
        .def("uid", &atom::system::Stat::uid,
             R"(Gets the user ID of the file owner.

Returns:
    int: The user ID of the file owner.

Raises:
    OSError: If the user ID cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Owner UID: {s.uid()}")
)")
        .def("gid", &atom::system::Stat::gid,
             R"(Gets the group ID of the file owner.

Returns:
    int: The group ID of the file owner.

Raises:
    OSError: If the group ID cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Group GID: {s.gid()}")
)")
        .def("path", &atom::system::Stat::path,
             R"(Gets the path of the file.

Returns:
    str: The path of the file.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"File path: {s.path()}")
)")
        .def("hard_link_count", &atom::system::Stat::hardLinkCount,
             R"(Gets the number of hard links to the file.

Returns:
    int: The number of hard links to the file.

Raises:
    OSError: If the hard link count cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Hard links: {s.hard_link_count()}")
)")
        .def("device_id", &atom::system::Stat::deviceId,
             R"(Gets the device ID of the file.

Returns:
    int: The device ID of the file.

Raises:
    OSError: If the device ID cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Device ID: {s.device_id()}")
)")
        .def("inode_number", &atom::system::Stat::inodeNumber,
             R"(Gets the inode number of the file.

Returns:
    int: The inode number of the file.

Raises:
    OSError: If the inode number cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Inode number: {s.inode_number()}")
)")
        .def("block_size", &atom::system::Stat::blockSize,
             R"(Gets the block size for the file system.

Returns:
    int: The block size for the file system.

Raises:
    OSError: If the block size cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Block size: {s.block_size()} bytes")
)")
        .def("owner_name", &atom::system::Stat::ownerName,
             R"(Gets the username of the file owner.

Returns:
    str: The username of the file owner.

Raises:
    OSError: If the username cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Owner: {s.owner_name()}")
)")
        .def("group_name", &atom::system::Stat::groupName,
             R"(Gets the group name of the file.

Returns:
    str: The group name of the file.

Raises:
    OSError: If the group name cannot be determined

Examples:
    >>> s = stat.Stat("example.txt")
    >>> print(f"Group: {s.group_name()}")
)")
        .def("is_symlink", &atom::system::Stat::isSymlink,
             R"(Checks if the file is a symbolic link.

Returns:
    bool: True if the file is a symbolic link, False otherwise.

Examples:
    >>> s = stat.Stat("example.link")
    >>> if s.is_symlink():
    ...     print("This is a symbolic link")
)")
        .def("is_directory", &atom::system::Stat::isDirectory,
             R"(Checks if the file is a directory.

Returns:
    bool: True if the file is a directory, False otherwise.

Examples:
    >>> s = stat.Stat("example_dir")
    >>> if s.is_directory():
    ...     print("This is a directory")
)")
        .def("is_regular_file", &atom::system::Stat::isRegularFile,
             R"(Checks if the file is a regular file.

Returns:
    bool: True if the file is a regular file, False otherwise.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> if s.is_regular_file():
    ...     print("This is a regular file")
)")
        .def("is_readable", &atom::system::Stat::isReadable,
             R"(Checks if the file is readable by the current user.

Returns:
    bool: True if the file is readable, False otherwise.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> if s.is_readable():
    ...     print("File is readable")
)")
        .def("is_writable", &atom::system::Stat::isWritable,
             R"(Checks if the file is writable by the current user.

Returns:
    bool: True if the file is writable, False otherwise.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> if s.is_writable():
    ...     print("File is writable")
)")
        .def("is_executable", &atom::system::Stat::isExecutable,
             R"(Checks if the file is executable by the current user.

Returns:
    bool: True if the file is executable, False otherwise.

Examples:
    >>> s = stat.Stat("example.sh")
    >>> if s.is_executable():
    ...     print("File is executable")
)")
        .def("has_permission", &atom::system::Stat::hasPermission,
             py::arg("user"), py::arg("group"), py::arg("others"),
             py::arg("permission"),
             R"(Checks if the file has specific permission.

Args:
    user: Check for user permissions
    group: Check for group permissions
    others: Check for others permissions
    permission: The permission to check (READ, WRITE, or EXECUTE)

Returns:
    bool: True if the permission is granted, False otherwise.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> # Check if file has read permission for owner
    >>> if s.has_permission(True, False, False, stat.FilePermission.READ):
    ...     print("File is readable by owner")
)")
        .def("symlink_target", &atom::system::Stat::symlinkTarget,
             R"(Gets the target path if the file is a symbolic link.

Returns:
    str: The target path of the symbolic link. Empty if not a symlink.

Examples:
    >>> s = stat.Stat("example.link")
    >>> if s.is_symlink():
    ...     print(f"Link target: {s.symlink_target()}")
)")
        .def_static("format_time", &atom::system::Stat::formatTime,
                    py::arg("time"), py::arg("format") = "%Y-%m-%d %H:%M:%S",
                    R"(Formats the file time (atime, mtime, ctime) as a string.

Args:
    time: The time to format.
    format: The format string (default: "%Y-%m-%d %H:%M:%S").

Returns:
    str: The formatted time string.

Examples:
    >>> s = stat.Stat("example.txt")
    >>> # Format the modification time
    >>> formatted_time = stat.Stat.format_time(s.mtime())
    >>> print(f"Last modified: {formatted_time}")
    >>>
    >>> # Custom time format
    >>> custom_format = stat.Stat.format_time(s.mtime(), "%H:%M:%S %d-%m-%Y")
    >>> print(f"Last modified: {custom_format}")
)");

    // Utility functions
    m.def(
        "get_file_info",
        [](const std::string& path) {
            py::dict result;
            try {
                atom::system::Stat s(path);

                if (!s.exists()) {
                    return result;  // Return empty dict for non-existent files
                }

                result["exists"] = true;
                result["path"] = s.path().string();
                result["type"] = s.type();
                result["size"] = s.size();
                result["atime"] = s.atime();
                result["mtime"] = s.mtime();
                result["ctime"] = s.ctime();
                result["mode"] = s.mode();
                result["is_symlink"] = s.isSymlink();
                result["is_directory"] = s.isDirectory();
                result["is_regular_file"] = s.isRegularFile();
                result["is_readable"] = s.isReadable();
                result["is_writable"] = s.isWritable();
                result["is_executable"] = s.isExecutable();

                try {
                    result["owner"] = s.ownerName();
                    result["group"] = s.groupName();
                    result["uid"] = s.uid();
                    result["gid"] = s.gid();
                    result["inode"] = s.inodeNumber();
                    result["device_id"] = s.deviceId();
                    result["block_size"] = s.blockSize();
                    result["hard_links"] = s.hardLinkCount();

                    if (s.isSymlink()) {
                        result["target"] = s.symlinkTarget().string();
                    }
                } catch (const std::exception& e) {
                    // Some attributes might not be available on all platforms
                    // Just ignore those exceptions
                }

                // Format timestamps
                result["atime_str"] = atom::system::Stat::formatTime(s.atime());
                result["mtime_str"] = atom::system::Stat::formatTime(s.mtime());
                result["ctime_str"] = atom::system::Stat::formatTime(s.ctime());
            } catch (const std::exception& e) {
                result["exists"] = false;
                result["error"] = e.what();
            }

            return result;
        },
        py::arg("path"),
        R"(Get comprehensive file information as a dictionary.

This is a convenience function that collects all available file information
in a single dictionary.

Args:
    path: Path to the file to examine

Returns:
    dict: Dictionary containing all available file information

Examples:
    >>> from atom.system import stat
    >>> info = stat.get_file_info("example.txt")
    >>> for key, value in info.items():
    ...     print(f"{key}: {value}")
)");

    m.def(
        "file_type_to_string",
        [](fs::file_type type) {
            switch (type) {
                case fs::file_type::none:
                    return "none";
                case fs::file_type::not_found:
                    return "not_found";
                case fs::file_type::regular:
                    return "regular";
                case fs::file_type::directory:
                    return "directory";
                case fs::file_type::symlink:
                    return "symlink";
                case fs::file_type::block:
                    return "block";
                case fs::file_type::character:
                    return "character";
                case fs::file_type::fifo:
                    return "fifo";
                case fs::file_type::socket:
                    return "socket";
                default:
                    return "unknown";
            }
        },
        py::arg("type"),
        R"(Convert a FileType enum value to a string.

Args:
    type: The FileType enum value

Returns:
    str: String representation of the file type

Examples:
    >>> from atom.system import stat
    >>> s = stat.Stat("example.txt")
    >>> file_type = s.type()
    >>> type_str = stat.file_type_to_string(file_type)
    >>> print(f"File type: {type_str}")
)");

    m.def(
        "format_file_mode",
        [](int mode) {
            std::string result;

            if (S_ISREG(mode))
                result += "-";
            else if (S_ISDIR(mode))
                result += "d";
#ifdef S_ISLNK
            else if (S_ISLNK(mode))
                result += "l";
#endif
            else if (S_ISCHR(mode))
                result += "c";
            else if (S_ISBLK(mode))
                result += "b";
            else if (S_ISFIFO(mode))
                result += "p";
#ifdef S_ISSOCK
            else if (S_ISSOCK(mode))
                result += "s";
#endif
            else
                result += "?";

            // User permissions
            result += (mode & S_IRUSR) ? "r" : "-";
            result += (mode & S_IWUSR) ? "w" : "-";
            result += (mode & S_IXUSR) ? "x" : "-";

            // Group permissions
            result += (mode & S_IRGRP) ? "r" : "-";
            result += (mode & S_IWGRP) ? "w" : "-";
            result += (mode & S_IXGRP) ? "x" : "-";

            // Others permissions
            result += (mode & S_IROTH) ? "r" : "-";
            result += (mode & S_IWOTH) ? "w" : "-";
            result += (mode & S_IXOTH) ? "x" : "-";

            return result;
        },
        py::arg("mode"),
        R"(Format a file mode/permissions integer as a string (e.g., "drwxr-xr-x").

Args:
    mode: File mode/permissions as an integer

Returns:
    str: String representation of file permissions (like in 'ls -l')

Examples:
    >>> from atom.system import stat
    >>> s = stat.Stat("example.txt")
    >>> mode_str = stat.format_file_mode(s.mode())
    >>> print(f"File permissions: {mode_str}")
)");

    m.def(
        "format_file_size",
        [](std::uintmax_t size) {
            const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
            int unit_index = 0;
            double size_d = static_cast<double>(size);

            while (size_d >= 1024.0 && unit_index < 6) {
                size_d /= 1024.0;
                unit_index++;
            }

            char buffer[64];
            if (unit_index == 0) {
                snprintf(buffer, sizeof(buffer), "%lld %s", (long long)size,
                         units[unit_index]);
            } else {
                snprintf(buffer, sizeof(buffer), "%.2f %s", size_d,
                         units[unit_index]);
            }

            return std::string(buffer);
        },
        py::arg("size"),
        R"(Format a file size in human-readable format (e.g., "1.23 MB").

Args:
    size: File size in bytes

Returns:
    str: Human-readable file size string

Examples:
    >>> from atom.system import stat
    >>> s = stat.Stat("example.txt")
    >>> size_str = stat.format_file_size(s.size())
    >>> print(f"File size: {size_str}")
)");

    // Create a dictionary with common file extensions and their descriptions
    m.attr("COMMON_FILE_TYPES") = py::dict(
        py::arg("txt") = "Text document", py::arg("pdf") = "PDF document",
        py::arg("doc") = "Microsoft Word document",
        py::arg("docx") = "Microsoft Word document",
        py::arg("xls") = "Microsoft Excel spreadsheet",
        py::arg("xlsx") = "Microsoft Excel spreadsheet",
        py::arg("ppt") = "Microsoft PowerPoint presentation",
        py::arg("pptx") = "Microsoft PowerPoint presentation",
        py::arg("jpg") = "JPEG image", py::arg("jpeg") = "JPEG image",
        py::arg("png") = "PNG image", py::arg("gif") = "GIF image",
        py::arg("mp3") = "MP3 audio", py::arg("mp4") = "MP4 video",
        py::arg("zip") = "ZIP archive", py::arg("tar") = "TAR archive",
        py::arg("gz") = "Gzip compressed file",
        py::arg("html") = "HTML document", py::arg("htm") = "HTML document",
        py::arg("css") = "CSS stylesheet", py::arg("js") = "JavaScript file",
        py::arg("py") = "Python script", py::arg("cpp") = "C++ source file",
        py::arg("h") = "C/C++ header file",
        py::arg("java") = "Java source file",
        py::arg("class") = "Java class file", py::arg("sh") = "Shell script",
        py::arg("bat") = "Windows batch file",
        py::arg("exe") = "Windows executable");

    // Class for stat caching
    // Class for stat caching
    py::class_<py::object>(m, "StatCache")
        .def(py::init([]() {
                 py::dict cache;
                 return py::object(cache);
             }),
             "Create a new stat cache")
        .def("__getitem__",
             [m](py::object& self, const std::string& path) -> py::object {
                 if (!py::hasattr(self, "cache")) {
                     self.attr("cache") = py::dict();
                 }

                 py::dict cache = self.attr("cache");

                 if (path.empty()) {
                     return py::none();
                 }

                 // Check if in cache and not expired
                 if (cache.contains(py::str(path))) {
                     py::object cached = cache[py::str(path)];
                     py::tuple cached_tuple = py::cast<py::tuple>(cached);
                     std::time_t cache_time =
                         py::cast<std::time_t>(cached_tuple[0]);
                     std::time_t now = std::time(nullptr);

                     // Cache valid for 1 second
                     if (now - cache_time < 1) {
                         return cached_tuple[1];
                     }
                 }

                 // Get fresh info
                 py::object info = m.attr("get_file_info")(path);
                 std::time_t now = std::time(nullptr);

                 // Create tuple and store in cache
                 py::tuple data = py::make_tuple(now, info);
                 cache[py::str(path)] = data;

                 return info;
             })
        .def(
            "clear",
            [](py::object& self) {
                if (py::hasattr(self, "cache")) {
                    self.attr("cache").attr("clear")();
                }
            },
            "Clear the stat cache")
        .def("__len__", [](py::object& self) -> size_t {
            if (py::hasattr(self, "cache")) {
                return py::len(self.attr("cache"));
            }
            return 0;
        });

    // Create and export a global StatCache
    m.attr("cache") = m.attr("StatCache")();

    // Context manager for working with Stat objects
    py::class_<py::object>(m, "StatContext")
        .def(py::init([](const std::string& path, bool follow_symlinks) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("path"), py::arg("follow_symlinks") = true,
             "Create a context manager for file statistics")
        .def("__enter__",
             [](py::object& self, const std::string& path,
                bool follow_symlinks) {
                 auto stat_obj = std::make_unique<atom::system::Stat>(
                     path, follow_symlinks);
                 self.attr("_stat") =
                     py::cast(std::move(stat_obj),
                              py::return_value_policy::take_ownership);
                 return self.attr("_stat");
             })
        .def("__exit__", [](py::object& self, py::object exc_type,
                            py::object exc_val, py::object exc_tb) {
            return false;  // Don't suppress exceptions
        });

    // Function to use the context manager
    m.def(
        "open_stat",
        [&m](const std::string& path, bool follow_symlinks) {
            return m.attr("StatContext")(path, follow_symlinks);
        },
        py::arg("path"), py::arg("follow_symlinks") = true,
        R"(Create a context manager for file statistics.

This function creates a context manager that can be used with a 'with' statement
to work with Stat objects.

Args:
    path: The file path to examine
    follow_symlinks: Whether to follow symlinks (default: True)

Returns:
    A context manager that yields a Stat object

Examples:
    >>> from atom.system import stat
    >>> with stat.open_stat("example.txt") as s:
    ...     print(f"File size: {s.size()} bytes")
    ...     print(f"Last modified: {stat.Stat.format_time(s.mtime())}")
)");
}
