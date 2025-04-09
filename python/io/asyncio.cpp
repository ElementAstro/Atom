#include "atom/io/async_io.hpp"

#include <pybind11/chrono.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <filesystem>
#include <fstream>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(asyncio, m) {
    m.doc() = "Asynchronous I/O implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const fs::filesystem_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define AsyncResult template specializations
    py::class_<atom::async::io::AsyncResult<std::string>>(m,
                                                          "AsyncResultString")
        .def(py::init<>())
        .def_readwrite("success",
                       &atom::async::io::AsyncResult<std::string>::success)
        .def_readwrite(
            "error_message",
            &atom::async::io::AsyncResult<std::string>::error_message)
        .def_readwrite("value",
                       &atom::async::io::AsyncResult<std::string>::value)
        .def(
            "__bool__",
            [](const atom::async::io::AsyncResult<std::string>& result) {
                return result.success;
            },
            "Check if the operation was successful")
        .def("__str__",
             [](const atom::async::io::AsyncResult<std::string>& result) {
                 if (result.success) {
                     return "AsyncResult(success=True, value='" +
                            result.value.substr(0, 30) +
                            (result.value.length() > 30 ? "..." : "") + "')";
                 } else {
                     return "AsyncResult(success=False, error='" +
                            result.error_message + "')";
                 }
             });

    py::class_<atom::async::io::AsyncResult<void>>(m, "AsyncResultVoid")
        .def(py::init<>())
        .def_readwrite("success", &atom::async::io::AsyncResult<void>::success)
        .def_readwrite("error_message",
                       &atom::async::io::AsyncResult<void>::error_message)
        .def(
            "__bool__",
            [](const atom::async::io::AsyncResult<void>& result) {
                return result.success;
            },
            "Check if the operation was successful")
        .def("__str__",
             [](const atom::async::io::AsyncResult<void>& result)
                 -> std::string {
                 if (result.success) {
                     return "AsyncResult(success=True)";
                 } else {
                     return "AsyncResult(success=False, error='" +
                            result.error_message + "')";
                 }
             });

    py::class_<atom::async::io::AsyncResult<bool>>(m, "AsyncResultBool")
        .def(py::init<>())
        .def_readwrite("success", &atom::async::io::AsyncResult<bool>::success)
        .def_readwrite("error_message",
                       &atom::async::io::AsyncResult<bool>::error_message)
        .def_readwrite("value", &atom::async::io::AsyncResult<bool>::value)
        .def(
            "__bool__",
            [](const atom::async::io::AsyncResult<bool>& result) {
                return result.success;
            },
            "Check if the operation was successful")
        .def("__str__", [](const atom::async::io::AsyncResult<bool>& result) {
            if (result.success) {
                return "AsyncResult(success=True, value=" +
                       std::string(result.value ? "True" : "False") + ")";
            } else {
                return "AsyncResult(success=False, error='" +
                       result.error_message + "')";
            }
        });

    py::class_<atom::async::io::AsyncResult<std::vector<std::string>>>(
        m, "AsyncResultStringList")
        .def(py::init<>())
        .def_readwrite(
            "success",
            &atom::async::io::AsyncResult<std::vector<std::string>>::success)
        .def_readwrite("error_message",
                       &atom::async::io::AsyncResult<
                           std::vector<std::string>>::error_message)
        .def_readwrite(
            "value",
            &atom::async::io::AsyncResult<std::vector<std::string>>::value)
        .def(
            "__bool__",
            [](const atom::async::io::AsyncResult<std::vector<std::string>>&
                   result) { return result.success; },
            "Check if the operation was successful")
        .def(
            "__len__",
            [](const atom::async::io::AsyncResult<std::vector<std::string>>&
                   result) { return result.success ? result.value.size() : 0; },
            "Get the number of items if successful")
        .def("__str__",
             [](const atom::async::io::AsyncResult<std::vector<std::string>>&
                    result) {
                 if (result.success) {
                     return "AsyncResult(success=True, items=" +
                            std::to_string(result.value.size()) + ")";
                 } else {
                     return "AsyncResult(success=False, error='" +
                            result.error_message + "')";
                 }
             });

    py::class_<atom::async::io::AsyncResult<std::vector<fs::path>>>(
        m, "AsyncResultPathList")
        .def(py::init<>())
        .def_readwrite(
            "success",
            &atom::async::io::AsyncResult<std::vector<fs::path>>::success)
        .def_readwrite(
            "error_message",
            &atom::async::io::AsyncResult<std::vector<fs::path>>::error_message)
        .def_readwrite(
            "value",
            &atom::async::io::AsyncResult<std::vector<fs::path>>::value)
        .def(
            "__bool__",
            [](const atom::async::io::AsyncResult<std::vector<fs::path>>&
                   result) { return result.success; },
            "Check if the operation was successful")
        .def(
            "__len__",
            [](const atom::async::io::AsyncResult<std::vector<fs::path>>&
                   result) { return result.success ? result.value.size() : 0; },
            "Get the number of items if successful")
        .def("__str__",
             [](const atom::async::io::AsyncResult<std::vector<fs::path>>&
                    result) {
                 if (result.success) {
                     return "AsyncResult(success=True, items=" +
                            std::to_string(result.value.size()) + ")";
                 } else {
                     return "AsyncResult(success=False, error='" +
                            result.error_message + "')";
                 }
             });

    py::class_<atom::async::io::AsyncResult<fs::file_status>>(
        m, "AsyncResultFileStatus")
        .def(py::init<>())
        .def_readwrite("success",
                       &atom::async::io::AsyncResult<fs::file_status>::success)
        .def_readwrite(
            "error_message",
            &atom::async::io::AsyncResult<fs::file_status>::error_message)
        .def_property(
            "value",
            [](const atom::async::io::AsyncResult<fs::file_status>& result) {
                return result.value;
            },
            [](atom::async::io::AsyncResult<fs::file_status>& result,
               const fs::file_status& status) { result.value = status; })
        .def(
            "__bool__",
            [](const atom::async::io::AsyncResult<fs::file_status>& result) {
                return result.success;
            },
            "Check if the operation was successful")
        .def(
            "is_directory",
            [](const atom::async::io::AsyncResult<fs::file_status>& result) {
                return result.success && fs::is_directory(result.value);
            },
            "Check if the file is a directory")
        .def(
            "is_regular_file",
            [](const atom::async::io::AsyncResult<fs::file_status>& result) {
                return result.success && fs::is_regular_file(result.value);
            },
            "Check if the file is a regular file")
        .def(
            "is_symlink",
            [](const atom::async::io::AsyncResult<fs::file_status>& result) {
                return result.success && fs::is_symlink(result.value);
            },
            "Check if the file is a symbolic link")
        .def("__str__",
             [](const atom::async::io::AsyncResult<fs::file_status>& result) {
                 if (result.success) {
                     std::string type;
                     if (fs::is_directory(result.value))
                         type = "directory";
                     else if (fs::is_regular_file(result.value))
                         type = "regular file";
                     else if (fs::is_symlink(result.value))
                         type = "symlink";
                     else
                         type = "other";

                     return "AsyncResult(success=True, file_type='" + type +
                            "')";
                 } else {
                     return "AsyncResult(success=False, error='" +
                            result.error_message + "')";
                 }
             });

    // Define AsyncFile class
    py::class_<atom::async::io::AsyncFile>(
        m, "AsyncFile",
        R"(Class for performing asynchronous file operations.

This class provides methods for reading, writing, and manipulating files asynchronously.

Args:
    io_context: The ASIO I/O context to use for asynchronous operations.

Examples:
    >>> import asio
    >>> from atom.io.asyncio import AsyncFile
    >>> 
    >>> io_context = asio.io_context()
    >>> async_file = AsyncFile(io_context)
    >>> 
    >>> def on_read(result):
    ...     if result.success:
    ...         print(f"Read {len(result.value)} bytes")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_read("example.txt", on_read)
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&>(), py::arg("io_context"),
             "Constructs an AsyncFile object with the given ASIO I/O context.")
        .def(
            "async_read",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               py::function callback) {
                self.asyncRead(
                    filename,
                    [callback](
                        atom::async::io::AsyncResult<std::string> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("filename"), py::arg("callback"),
            R"(Asynchronously reads the content of a file.

Args:
    filename: The name of the file to read.
    callback: Function to call with the read result.

Examples:
    >>> def on_read(result):
    ...     if result.success:
    ...         print(f"Content: {result.value[:50]}...")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_read("example.txt", on_read)
)")
        .def(
            "async_write",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               const std::string& content, py::function callback) {
                std::span<const char> content_span(content.data(),
                                                   content.size());
                self.asyncWrite(
                    filename, content_span,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("filename"), py::arg("content"), py::arg("callback"),
            R"(Asynchronously writes content to a file.

Args:
    filename: The name of the file to write to.
    content: The content to write to the file.
    callback: Function to call with the write result.

Examples:
    >>> def on_write(result):
    ...     if result.success:
    ...         print("Write successful")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_write("example.txt", "Hello, World!", on_write)
)")
        .def(
            "async_delete",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               py::function callback) {
                self.asyncDelete(
                    filename,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("filename"), py::arg("callback"),
            R"(Asynchronously deletes a file.

Args:
    filename: The name of the file to delete.
    callback: Function to call with the delete result.

Examples:
    >>> def on_delete(result):
    ...     if result.success:
    ...         print("Delete successful")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_delete("temporary.txt", on_delete)
)")
        .def(
            "async_copy",
            [](atom::async::io::AsyncFile& self, const std::string& src,
               const std::string& dest, py::function callback) {
                self.asyncCopy(
                    src, dest,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("src"), py::arg("dest"), py::arg("callback"),
            R"(Asynchronously copies a file.

Args:
    src: The source file path.
    dest: The destination file path.
    callback: Function to call with the copy result.

Examples:
    >>> def on_copy(result):
    ...     if result.success:
    ...         print("Copy successful")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_copy("original.txt", "backup.txt", on_copy)
)")
        .def(
            "async_read_with_timeout",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               int timeout_ms, py::function callback) {
                self.asyncReadWithTimeout(
                    filename, std::chrono::milliseconds(timeout_ms),
                    [callback](
                        atom::async::io::AsyncResult<std::string> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("filename"), py::arg("timeout_ms"), py::arg("callback"),
            R"(Asynchronously reads the content of a file with a timeout.

Args:
    filename: The name of the file to read.
    timeout_ms: The timeout in milliseconds.
    callback: Function to call with the read result.

Examples:
    >>> def on_read(result):
    ...     if result.success:
    ...         print(f"Read successful: {len(result.value }) bytes")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_read_with_timeout("example.txt", 1000, on_read)  # 1 second timeout
)")
        .def(
            "async_batch_read",
            [](atom::async::io::AsyncFile& self,
               const std::vector<std::string>& files, py::function callback) {
                self.asyncBatchRead(
                    files,
                    [callback](
                        atom::async::io::AsyncResult<std::vector<std::string>>
                            result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("files"), py::arg("callback"),
            R"(Asynchronously reads the content of multiple files.

Args:
    files: List of file paths to read.
    callback: Function to call with the read results.

Examples:
    >>> def on_batch_read(result):
    ...     if result.success:
    ...         print(f"Read {len(result.value)} files")
    ...         for i, content in enumerate(result.value):
    ...             print(f"File {i+1}: {len(content)} bytes")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_batch_read(["file1.txt", "file2.txt"], on_batch_read)
)")
        .def(
            "async_stat",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               py::function callback) {
                self.asyncStat(filename,
                               [callback](atom::async::io::AsyncResult<
                                          std::filesystem::file_status>
                                              result) {
                                   py::gil_scoped_acquire acquire;
                                   callback(result);
                               });
            },
            py::arg("filename"), py::arg("callback"),
            R"(Asynchronously retrieves the status of a file.

Args:
    filename: The name of the file.
    callback: Function to call with the file status.

Examples:
    >>> def on_stat(result):
    ...     if result.success:
    ...         if result.is_directory():
    ...             print("It's a directory")
    ...         elif result.is_regular_file():
    ...             print("It's a regular file")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_stat("example.txt", on_stat)
)")
        .def(
            "async_move",
            [](atom::async::io::AsyncFile& self, const std::string& src,
               const std::string& dest, py::function callback) {
                self.asyncMove(
                    src, dest,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("src"), py::arg("dest"), py::arg("callback"),
            R"(Asynchronously moves a file.

Args:
    src: The source file path.
    dest: The destination file path.
    callback: Function to call with the move result.

Examples:
    >>> def on_move(result):
    ...     if result.success:
    ...         print("Move successful")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_move("old_path.txt", "new_path.txt", on_move)
)")
        .def(
            "async_change_permissions",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               fs::perms perms, py::function callback) {
                self.asyncChangePermissions(
                    filename, perms,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("filename"), py::arg("perms"), py::arg("callback"),
            R"(Asynchronously changes the permissions of a file.

Args:
    filename: The name of the file.
    perms: The new permissions.
    callback: Function to call with the result.

Examples:
    >>> import stat
    >>> from pathlib import Path
    >>> perms = stat.S_IRUSR | stat.S_IWUSR  # Read & write for owner only
    >>> 
    >>> def on_chmod(result):
    ...     if result.success:
    ...         print("Changed permissions successfully")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_change_permissions("example.txt", perms, on_chmod)
)")
        .def(
            "async_create_directory",
            [](atom::async::io::AsyncFile& self, const std::string& path,
               py::function callback) {
                self.asyncCreateDirectory(
                    path,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("path"), py::arg("callback"),
            R"(Asynchronously creates a directory.

Args:
    path: The path of the directory to create.
    callback: Function to call with the result.

Examples:
    >>> def on_create_dir(result):
    ...     if result.success:
    ...         print("Directory created successfully")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_create_directory("new_directory", on_create_dir)
)")
        .def(
            "async_exists",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               py::function callback) {
                self.asyncExists(
                    filename,
                    [callback](atom::async::io::AsyncResult<bool> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("filename"), py::arg("callback"),
            R"(Asynchronously checks if a file exists.

Args:
    filename: The name of the file.
    callback: Function to call with the result.

Examples:
    >>> def on_exists(result):
    ...     if result.success:
    ...         if result.value:
    ...             print("File exists")
    ...         else:
    ...             print("File does not exist")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_file.async_exists("example.txt", on_exists)
)")
        .def(
            "read_file",
            [](atom::async::io::AsyncFile& self, const std::string& filename) {
                return self.readFile(filename);
            },
            py::arg("filename"),
            R"(Coroutine-based asynchronous file read.

Args:
    filename: The name of the file to read.

Returns:
    A Task that will complete with the file content.

Examples:
    >>> task = async_file.read_file("example.txt")
    >>> # Do other work...
    >>> result = task.get()  # Wait for completion
    >>> if result.success:
    ...     print(f"Read {len(result.value)} bytes")
)")
        .def(
            "write_file",
            [](atom::async::io::AsyncFile& self, const std::string& filename,
               const std::string& content) {
                std::span<const char> content_span(content.data(),
                                                   content.size());
                return self.writeFile(filename, content_span);
            },
            py::arg("filename"), py::arg("content"),
            R"(Coroutine-based asynchronous file write.

Args:
    filename: The name of the file to write to.
    content: The content to write.

Returns:
    A Task that will complete when the operation is done.

Examples:
    >>> task = async_file.write_file("example.txt", "Hello, World!")
    >>> # Do other work...
    >>> result = task.get()  # Wait for completion
    >>> if result.success:
    ...     print("Write successful")
)");

    // Define Task<AsyncResult<std::string>>
    py::class_<
        atom::async::io::Task<atom::async::io::AsyncResult<std::string>>>(
        m, "TaskString")
        .def("get",
             &atom::async::io::Task<
                 atom::async::io::AsyncResult<std::string>>::get,
             "Wait for the task to complete and return the result")
        .def("is_ready",
             &atom::async::io::Task<
                 atom::async::io::AsyncResult<std::string>>::is_ready,
             "Check if the task is ready");

    // Define Task<AsyncResult<void>>
    py::class_<atom::async::io::Task<atom::async::io::AsyncResult<void>>>(
        m, "TaskVoid")
        .def("get",
             &atom::async::io::Task<atom::async::io::AsyncResult<void>>::get,
             "Wait for the task to complete and return the result")
        .def("is_ready",
             &atom::async::io::Task<
                 atom::async::io::AsyncResult<void>>::is_ready,
             "Check if the task is ready");

    // Define Task<AsyncResult<std::vector<std::filesystem::path>>>
    py::class_<atom::async::io::Task<
        atom::async::io::AsyncResult<std::vector<std::filesystem::path>>>>(
        m, "TaskPathList")
        .def("get",
             &atom::async::io::Task<atom::async::io::AsyncResult<
                 std::vector<std::filesystem::path>>>::get,
             "Wait for the task to complete and return the result")
        .def("is_ready",
             &atom::async::io::Task<atom::async::io::AsyncResult<
                 std::vector<std::filesystem::path>>>::is_ready,
             "Check if the task is ready");

    // Define AsyncDirectory class
    py::class_<atom::async::io::AsyncDirectory>(
        m, "AsyncDirectory",
        R"(Class for performing asynchronous directory operations.

This class provides methods for creating, removing, and listing directories asynchronously.

Args:
    io_context: The ASIO I/O context to use for asynchronous operations.

Examples:
    >>> import asio
    >>> from atom.io.asyncio import AsyncDirectory
    >>> 
    >>> io_context = asio.io_context()
    >>> async_dir = AsyncDirectory(io_context)
    >>> 
    >>> def on_list(result):
    ...     if result.success:
    ...         print(f"Found {len(result.value)} entries:")
    ...         for path in result.value:
    ...             print(f"  - {path}")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_dir.async_list_contents(".", on_list)
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&>(), py::arg("io_context"),
             "Constructs an AsyncDirectory object with the given ASIO I/O "
             "context.")
        .def(
            "async_create",
            [](atom::async::io::AsyncDirectory& self, const std::string& path,
               py::function callback) {
                self.asyncCreate(
                    path,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("path"), py::arg("callback"),
            R"(Asynchronously creates a directory.

Args:
    path: The path of the directory to create.
    callback: Function to call with the result of the operation.

Examples:
    >>> def on_create(result):
    ...     if result.success:
    ...         print("Directory created successfully")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_dir.async_create("new_directory", on_create)
)")
        .def(
            "async_remove",
            [](atom::async::io::AsyncDirectory& self, const std::string& path,
               py::function callback) {
                self.asyncRemove(
                    path,
                    [callback](atom::async::io::AsyncResult<void> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("path"), py::arg("callback"),
            R"(Asynchronously removes a directory.

Args:
    path: The path of the directory to remove.
    callback: Function to call with the result of the operation.

Examples:
    >>> def on_remove(result):
    ...     if result.success:
    ...         print("Directory removed successfully")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_dir.async_remove("old_directory", on_remove)
)")
        .def(
            "async_list_contents",
            [](atom::async::io::AsyncDirectory& self, const std::string& path,
               py::function callback) {
                self.asyncListContents(
                    path, [callback](atom::async::io::AsyncResult<
                                     std::vector<std::filesystem::path>>
                                         result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("path"), py::arg("callback"),
            R"(Asynchronously lists the contents of a directory.

Args:
    path: The path of the directory.
    callback: Function to call with the list of contents.

Examples:
    >>> def on_list(result):
    ...     if result.success:
    ...         print(f"Found {len(result.value)} entries:")
    ...         for path in result.value:
    ...             print(f"  - {path}")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_dir.async_list_contents(".", on_list)
)")
        .def(
            "async_exists",
            [](atom::async::io::AsyncDirectory& self, const std::string& path,
               py::function callback) {
                self.asyncExists(
                    path,
                    [callback](atom::async::io::AsyncResult<bool> result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
            },
            py::arg("path"), py::arg("callback"),
            R"(Asynchronously checks if a directory exists.

Args:
    path: The path of the directory.
    callback: Function to call with the result of the check.

Examples:
    >>> def on_exists(result):
    ...     if result.success:
    ...         if result.value:
    ...             print("Directory exists")
    ...         else:
    ...             print("Directory does not exist")
    ...     else:
    ...         print(f"Error: {result.error_message}")
    >>> 
    >>> async_dir.async_exists("my_directory", on_exists)
)")
        .def(
            "list_contents",
            [](atom::async::io::AsyncDirectory& self, const std::string& path) {
                return self.listContents(path);
            },
            py::arg("path"),
            R"(Coroutine-based asynchronous directory listing.

Args:
    path: The path of the directory to list.

Returns:
    A Task that will complete with the directory contents.

Examples:
    >>> task = async_dir.list_contents(".")
    >>> # Do other work...
    >>> result = task.get()  # Wait for completion
    >>> if result.success:
    ...     print(f"Found {len(result.value)} entries")
)");

    // Utility functions
    m.def(
        "read_file_sync",
        [](const std::string& filename) {
            try {
                std::ifstream file(filename, std::ios::binary | std::ios::ate);
                if (!file) {
                    throw std::runtime_error("Could not open file: " +
                                             filename);
                }

                auto size = file.tellg();
                std::string buffer(size, '\0');

                file.seekg(0);
                if (!file.read(buffer.data(), size)) {
                    throw std::runtime_error("Error reading file: " + filename);
                }

                atom::async::io::AsyncResult<std::string> result;
                result.success = true;
                result.value = std::move(buffer);
                return result;
            } catch (const std::exception& e) {
                atom::async::io::AsyncResult<std::string> result;
                result.success = false;
                result.error_message = e.what();
                return result;
            }
        },
        py::arg("filename"),
        R"(Synchronously reads the content of a file.

Args:
    filename: The name of the file to read.

Returns:
    An AsyncResult containing the file content or error information.

Examples:
    >>> from atom.io.asyncio import read_file_sync
    >>> result = read_file_sync("example.txt")
    >>> if result.success:
    ...     print(f"Read {len(result.value)} bytes")
    ... else:
    ...     print(f"Error: {result.error_message}")
)");

    m.def(
        "write_file_sync",
        [](const std::string& filename, const std::string& content) {
            try {
                std::ofstream file(filename, std::ios::binary);
                if (!file) {
                    throw std::runtime_error(
                        "Could not open file for writing: " + filename);
                }

                if (!file.write(content.data(), content.size())) {
                    throw std::runtime_error("Error writing to file: " +
                                             filename);
                }

                atom::async::io::AsyncResult<void> result;
                result.success = true;
                return result;
            } catch (const std::exception& e) {
                atom::async::io::AsyncResult<void> result;
                result.success = false;
                result.error_message = e.what();
                return result;
            }
        },
        py::arg("filename"), py::arg("content"),
        R"(Synchronously writes content to a file.

Args:
    filename: The name of the file to write to.
    content: The content to write to the file.

Returns:
    An AsyncResult indicating success or containing error information.

Examples:
    >>> from atom.io.asyncio import write_file_sync
    >>> result = write_file_sync("example.txt", "Hello, World!")
    >>> if result.success:
    ...     print("Write successful")
    ... else:
    ...     print(f"Error: {result.error_message}")
)");

    m.def(
        "file_exists_sync",
        [](const std::string& filename) {
            try {
                atom::async::io::AsyncResult<bool> result;
                result.success = true;
                result.value = fs::exists(filename);
                return result;
            } catch (const std::exception& e) {
                atom::async::io::AsyncResult<bool> result;
                result.success = false;
                result.error_message = e.what();
                return result;
            }
        },
        py::arg("filename"),
        R"(Synchronously checks if a file exists.

Args:
    filename: The name of the file to check.

Returns:
    An AsyncResult containing the existence status or error information.

Examples:
    >>> from atom.io.asyncio import file_exists_sync
    >>> result = file_exists_sync("example.txt")
    >>> if result.success:
    ...     if result.value:
    ...         print("File exists")
    ...     else:
    ...         print("File does not exist")
    ... else:
    ...     print(f"Error: {result.error_message}")
)");
}