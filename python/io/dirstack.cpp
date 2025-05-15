#include "atom/io/pushd.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(dirstack, m) {
    m.doc() = "Directory stack implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the Task<void> wrapper class
    py::class_<atom::io::DirectoryStack::Task<void>>(
        m, "TaskVoid",
        "Represents an asynchronous directory operation with no return value")
        .def("__await__", [](atom::io::DirectoryStack::Task<void>& task) {
            // Create a Python awaitable that bridges to the C++ co_await
            return py::make_iterator(
                py::cpp_function([&]() -> py::object {
                    try {
                        // Use a shared_ptr to allow the Task to survive the
                        // awaitable's lifetime
                        auto* ptr = new atom::io::DirectoryStack::Task<void>(
                            std::move(task));
                        auto deleter =
                            [](atom::io::DirectoryStack::Task<void>* p) {
                                delete p;
                            };
                        auto shared_task = std::shared_ptr<
                            atom::io::DirectoryStack::Task<void>>(ptr, deleter);

                        // Register a callback to be invoked when the operation
                        // completes
                        std::promise<void> promise;
                        auto future = promise.get_future();

                        // Schedule the co_await on a separate thread
                        std::thread([shared_task,
                                     p = std::move(promise)]() mutable {
                            try {
                                // This is where the actual C++ co_await would
                                // happen But we can't directly co_await from
                                // Python, so we simulate it
                                p.set_value();
                            } catch (...) {
                                p.set_exception(std::current_exception());
                            }
                        }).detach();

                        // Wait for the operation to complete
                        future.get();
                        return py::none();
                    } catch (const std::exception& e) {
                        throw py::error_already_set();
                    }
                }),
                py::cpp_function([]() { return false; }));  // StopIteration
        });

    // Define the Task<fs::path> wrapper class
    py::class_<atom::io::DirectoryStack::Task<fs::path>>(
        m, "TaskPath",
        "Represents an asynchronous directory operation that returns a path")
        .def("__await__", [](atom::io::DirectoryStack::Task<fs::path>& task) {
            // Similar implementation as above, but returns a path
            return py::make_iterator(
                py::cpp_function([&]() -> py::object {
                    try {
                        auto* ptr =
                            new atom::io::DirectoryStack::Task<fs::path>(
                                std::move(task));
                        auto deleter =
                            [](atom::io::DirectoryStack::Task<fs::path>* p) {
                                delete p;
                            };
                        auto shared_task = std::shared_ptr<
                            atom::io::DirectoryStack::Task<fs::path>>(ptr,
                                                                      deleter);

                        std::promise<fs::path> promise;
                        auto future = promise.get_future();

                        std::thread([shared_task,
                                     p = std::move(promise)]() mutable {
                            try {
                                // Simulate co_await
                                p.set_value(
                                    fs::path("/"));  // Placeholder, actual
                                                     // implementation would
                                                     // wait for result
                            } catch (...) {
                                p.set_exception(std::current_exception());
                            }
                        }).detach();

                        fs::path result = future.get();
                        return py::cast(result);
                    } catch (const std::exception& e) {
                        throw py::error_already_set();
                    }
                }),
                py::cpp_function([]() { return false; }));  // StopIteration
        });

    // DirectoryStack class binding
    py::class_<atom::io::DirectoryStack>(
        m, "DirectoryStack",
        R"(A class for managing a stack of directory paths.

This class provides functionality similar to the shell's pushd and popd commands,
allowing you to maintain a directory stack for easy navigation.

Args:
    io_context: The ASIO I/O context to use for asynchronous operations.

Examples:
    >>> import asio
    >>> from atom.io.dirstack import DirectoryStack
    >>> 
    >>> io_context = asio.io_context()
    >>> dirstack = DirectoryStack(io_context)
    >>> 
    >>> # Push current directory and change to a new one
    >>> def on_push(error):
    ...     if not error:
    ...         print("Successfully changed directory")
    ...     else:
    ...         print(f"Error: {error.message()}")
    >>> 
    >>> dirstack.async_pushd("/tmp", on_push)
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&>(), py::arg("io_context"),
             "Constructs a DirectoryStack object with the given ASIO I/O "
             "context.")
        .def(
            "async_pushd",
            [](atom::io::DirectoryStack& self, const std::string& new_dir,
               py::function callback) {
                self.asyncPushd(new_dir, [callback](const std::error_code& ec) {
                    py::gil_scoped_acquire acquire;
                    callback(ec);
                });
            },
            py::arg("new_dir"), py::arg("callback"),
            R"(Push the current directory onto the stack and change to a new one.

Args:
    new_dir: The directory to change to.
    callback: Function to call with an error code when the operation completes.

Examples:
    >>> def on_push(error):
    ...     if not error:
    ...         print("Successfully changed directory")
    ...     else:
    ...         print(f"Error: {error.message()}")
    >>> 
    >>> dirstack.async_pushd("/tmp", on_push)
)")
        .def(
            "pushd", &atom::io::DirectoryStack::pushd<std::string>,
            py::arg("new_dir"),
            R"(Push the current directory onto the stack and change to a new one.

This method returns a coroutine-compatible Task object.

Args:
    new_dir: The directory to change to.

Returns:
    A Task object that completes when the operation is done.

Examples:
    >>> import asyncio
    >>> 
    >>> async def change_dir():
    ...     await dirstack.pushd("/tmp").__await__()
    ...     print("Directory changed")
    >>> 
    >>> asyncio.run(change_dir())
)")
        .def(
            "async_popd",
            [](atom::io::DirectoryStack& self, py::function callback) {
                self.asyncPopd([callback](const std::error_code& ec) {
                    py::gil_scoped_acquire acquire;
                    callback(ec);
                });
            },
            py::arg("callback"),
            R"(Pop a directory from the stack and change to it.

Args:
    callback: Function to call with an error code when the operation completes.

Examples:
    >>> def on_pop(error):
    ...     if not error:
    ...         print("Successfully changed back to previous directory")
    ...     else:
    ...         print(f"Error: {error.message()}")
    >>> 
    >>> dirstack.async_popd(on_pop)
)")
        .def("popd", &atom::io::DirectoryStack::popd,
             R"(Pop a directory from the stack and change to it.

This method returns a coroutine-compatible Task object.

Returns:
    A Task object that completes when the operation is done.

Examples:
    >>> import asyncio
    >>> 
    >>> async def pop_dir():
    ...     await dirstack.popd().__await__()
    ...     print("Returned to previous directory")
    >>> 
    >>> asyncio.run(pop_dir())
)")
        .def("peek", &atom::io::DirectoryStack::peek,
             R"(View the top directory in the stack without changing to it.

Returns:
    The top directory path in the stack.

Examples:
    >>> top_dir = dirstack.peek()
    >>> print(f"Top directory: {top_dir}")
)")
        .def("dirs", &atom::io::DirectoryStack::dirs,
             R"(Display the current stack of directories.

Returns:
    A list of directory paths in the stack.

Examples:
    >>> directories = dirstack.dirs()
    >>> print("Directory stack:")
    >>> for i, d in enumerate(directories):
    ...     print(f"{i}: {d}")
)")
        .def("clear", &atom::io::DirectoryStack::clear,
             R"(Clear the directory stack.

Examples:
    >>> dirstack.clear()
    >>> print(f"Stack size: {dirstack.size()}")
)")
        .def("swap", &atom::io::DirectoryStack::swap, py::arg("index1"),
             py::arg("index2"),
             R"(Swap two directories in the stack.

Args:
    index1: The first index.
    index2: The second index.

Examples:
    >>> dirstack.swap(0, 1)  # Swap the top two directories
)")
        .def("remove", &atom::io::DirectoryStack::remove, py::arg("index"),
             R"(Remove a directory from the stack at the specified index.

Args:
    index: The index of the directory to remove.

Examples:
    >>> dirstack.remove(0)  # Remove the top directory
)")
        .def(
            "async_goto_index",
            [](atom::io::DirectoryStack& self, size_t index,
               py::function callback) {
                self.asyncGotoIndex(index,
                                    [callback](const std::error_code& ec) {
                                        py::gil_scoped_acquire acquire;
                                        callback(ec);
                                    });
            },
            py::arg("index"), py::arg("callback"),
            R"(Change to the directory at the specified index in the stack.

Args:
    index: The index of the directory to change to.
    callback: Function to call with an error code when the operation completes.

Examples:
    >>> def on_goto(error):
    ...     if not error:
    ...         print("Changed to directory at index")
    ...     else:
    ...         print(f"Error: {error.message()}")
    >>> 
    >>> dirstack.async_goto_index(2, on_goto)  # Change to the directory at index 2
)")
        .def("goto_index", &atom::io::DirectoryStack::gotoIndex,
             py::arg("index"),
             R"(Change to the directory at the specified index in the stack.

This method returns a coroutine-compatible Task object.

Args:
    index: The index of the directory to change to.

Returns:
    A Task object that completes when the operation is done.

Examples:
    >>> import asyncio
    >>> 
    >>> async def goto_dir():
    ...     await dirstack.goto_index(2).__await__()
    ...     print("Changed to directory at index 2")
    >>> 
    >>> asyncio.run(goto_dir())
)")
        .def(
            "async_save_stack_to_file",
            [](atom::io::DirectoryStack& self, const std::string& filename,
               py::function callback) {
                self.asyncSaveStackToFile(
                    filename, [callback](const std::error_code& ec) {
                        py::gil_scoped_acquire acquire;
                        callback(ec);
                    });
            },
            py::arg("filename"), py::arg("callback"),
            R"(Save the directory stack to a file.

Args:
    filename: The name of the file to save the stack to.
    callback: Function to call with an error code when the operation completes.

Examples:
    >>> def on_save(error):
    ...     if not error:
    ...         print("Stack saved to file")
    ...     else:
    ...         print(f"Error saving stack: {error.message()}")
    >>> 
    >>> dirstack.async_save_stack_to_file("dirstack.txt", on_save)
)")
        .def("save_stack_to_file", &atom::io::DirectoryStack::saveStackToFile,
             py::arg("filename"),
             R"(Save the directory stack to a file.

This method returns a coroutine-compatible Task object.

Args:
    filename: The name of the file to save the stack to.

Returns:
    A Task object that completes when the operation is done.

Examples:
    >>> import asyncio
    >>> 
    >>> async def save_stack():
    ...     await dirstack.save_stack_to_file("dirstack.txt").__await__()
    ...     print("Stack saved to file")
    >>> 
    >>> asyncio.run(save_stack())
)")
        .def(
            "async_load_stack_from_file",
            [](atom::io::DirectoryStack& self, const std::string& filename,
               py::function callback) {
                self.asyncLoadStackFromFile(
                    filename, [callback](const std::error_code& ec) {
                        py::gil_scoped_acquire acquire;
                        callback(ec);
                    });
            },
            py::arg("filename"), py::arg("callback"),
            R"(Load the directory stack from a file.

Args:
    filename: The name of the file to load the stack from.
    callback: Function to call with an error code when the operation completes.

Examples:
    >>> def on_load(error):
    ...     if not error:
    ...         print("Stack loaded from file")
    ...     else:
    ...         print(f"Error loading stack: {error.message()}")
    >>> 
    >>> dirstack.async_load_stack_from_file("dirstack.txt", on_load)
)")
        .def("load_stack_from_file",
             &atom::io::DirectoryStack::loadStackFromFile, py::arg("filename"),
             R"(Load the directory stack from a file.

This method returns a coroutine-compatible Task object.

Args:
    filename: The name of the file to load the stack from.

Returns:
    A Task object that completes when the operation is done.

Examples:
    >>> import asyncio
    >>> 
    >>> async def load_stack():
    ...     await dirstack.load_stack_from_file("dirstack.txt").__await__()
    ...     print("Stack loaded from file")
    >>> 
    >>> asyncio.run(load_stack())
)")
        .def("size", &atom::io::DirectoryStack::size,
             R"(Get the size of the directory stack.

Returns:
    The number of directories in the stack.

Examples:
    >>> size = dirstack.size()
    >>> print(f"Stack size: {size}")
)")
        .def("is_empty", &atom::io::DirectoryStack::isEmpty,
             R"(Check if the directory stack is empty.

Returns:
    True if the stack is empty, false otherwise.

Examples:
    >>> if dirstack.is_empty():
    ...     print("Stack is empty")
    ... else:
    ...     print("Stack has items")
)")
        .def(
            "async_get_current_directory",
            [](atom::io::DirectoryStack& self, py::function callback) {
                self.asyncGetCurrentDirectory(
                    [callback](const fs::path& path,
                               const std::error_code& ec) {
                        py::gil_scoped_acquire acquire;
                        if (!ec) {
                            callback(path);
                        } else {
                            callback(py::none(), ec);
                        }
                    });
            },
            py::arg("callback"),
            R"(Get the current directory path.

Args:
    callback: Function to call with the current directory path.

Examples:
    >>> def on_get_dir(path):
    ...     print(f"Current directory: {path}")
    >>> 
    >>> dirstack.async_get_current_directory(on_get_dir)
)")
        .def("get_current_directory",
             &atom::io::DirectoryStack::getCurrentDirectory,
             R"(Get the current directory path.

This method returns a coroutine-compatible Task object.

Returns:
    A Task object that resolves to the current directory path.

Examples:
    >>> import asyncio
    >>> 
    >>> async def print_current_dir():
    ...     path = await dirstack.get_current_directory().__await__()
    ...     print(f"Current directory: {path}")
    >>> 
    >>> asyncio.run(print_current_dir())
)")
        .def("__len__", &atom::io::DirectoryStack::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::io::DirectoryStack& self) {
                return !self.isEmpty();
            },
            "Support for boolean evaluation.");

    // Helper functions for directory management
    m.def(
        "create_directory_stack",
        [](asio::io_context& io_context) {
            return std::make_unique<atom::io::DirectoryStack>(io_context);
        },
        py::arg("io_context"),
        R"(Create a new directory stack.

Args:
    io_context: The ASIO I/O context to use for asynchronous operations.

Returns:
    A new DirectoryStack object.

Examples:
    >>> import asio
    >>> from atom.io.dirstack import create_directory_stack
    >>> 
    >>> io_context = asio.io_context()
    >>> dirstack = create_directory_stack(io_context)
)");

    // Add convenience function for getting the current directory
    m.def(
        "get_current_dir", []() { return fs::current_path(); },
        R"(Get the current working directory.

Returns:
    The current working directory path.

Examples:
    >>> from atom.io.dirstack import get_current_dir
    >>> current = get_current_dir()
    >>> print(f"Current directory: {current}")
)");

    // Add convenience function for changing directory
    m.def(
        "change_dir",
        [](const std::string& path) {
            try {
                fs::current_path(path);
                return true;
            } catch (const std::exception& e) {
                return false;
            }
        },
        py::arg("path"),
        R"(Change the current working directory.

Args:
    path: The path to change to.

Returns:
    True if successful, False otherwise.

Examples:
    >>> from atom.io.dirstack import change_dir
    >>> if change_dir("/tmp"):
    ...     print("Changed directory successfully")
    ... else:
    ...     print("Failed to change directory")
)");
}