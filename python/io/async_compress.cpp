#include "atom/io/async/async_compress.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>
#include <system_error>

namespace py = pybind11;
namespace fs = std::filesystem;

#ifdef ATOM_USE_ASIO
PYBIND11_MODULE(async_compress, m) {
    m.doc() = R"pbdoc(
        Asynchronous Compression Module
        ------------------------------

        This module provides asynchronous file compression operations using ASIO
        for non-blocking compression and decompression of files and directories.

        Features:
        - Asynchronous single file compression
        - Asynchronous directory compression
        - Non-blocking I/O operations
        - Zlib-based compression
        - Completion handlers for async operations

        Examples:
            >>> import asio
            >>> from atom.io.async_compress import SingleFileCompressor
            >>>
            >>> io_context = asio.io_context()
            >>> compressor = SingleFileCompressor(io_context, "input.txt", "output.gz")
            >>>
            >>> def on_complete(error_code, bytes_processed):
            ...     if not error_code:
            ...         print(f"Compressed {bytes_processed} bytes")
            ...     else:
            ...         print(f"Error: {error_code.message()}")
            >>>
            >>> compressor.start(on_complete)
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

    // BaseCompressor class (abstract base)
    py::class_<atom::async::io::BaseCompressor>(
        m, "BaseCompressor",
        R"(Base class for compression operations.

This is an abstract base class that provides common functionality
for all compressor types. Use SingleFileCompressor or DirectoryCompressor
for actual compression operations.
)")
        .def("start", &atom::async::io::BaseCompressor::start,
             "Starts the compression process.");

    // SingleFileCompressor class binding
    py::class_<atom::async::io::SingleFileCompressor,
               atom::async::io::BaseCompressor>(
        m, "SingleFileCompressor",
        R"(Asynchronous compressor for single files.

Compresses a single file asynchronously using zlib compression.

Args:
    io_context: The ASIO I/O context for asynchronous operations
    input_file: Path to the input file to compress
    output_file: Path to the output compressed file

Examples:
    >>> import asio
    >>> from atom.io.async_compress import SingleFileCompressor
    >>>
    >>> io_context = asio.io_context()
    >>> compressor = SingleFileCompressor(
    ...     io_context, "large_file.txt", "large_file.gz"
    ... )
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, const fs::path&, const fs::path&>(),
             py::arg("io_context"), py::arg("input_file"),
             py::arg("output_file"),
             "Constructs a SingleFileCompressor for the specified files.")
        .def("start",
             py::overload_cast<>(&atom::async::io::SingleFileCompressor::start),
             R"(Starts the compression process without a callback.

The compression will run asynchronously. Use the I/O context's run()
method to process the operation.

Examples:
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(
            "start",
            [](atom::async::io::SingleFileCompressor& self,
               py::function callback) {
                self.start(
                    [callback](const std::error_code& ec, std::size_t bytes) {
                        py::gil_scoped_acquire acquire;
                        callback(ec, bytes);
                    });
            },
            py::arg("callback"),
            R"(Starts the compression process with a completion callback.

Args:
    callback: Function to call when compression completes.
              Signature: callback(error_code, bytes_processed)

Examples:
    >>> def on_complete(error_code, bytes_processed):
    ...     if not error_code:
    ...         print(f"Successfully compressed {bytes_processed} bytes")
    ...     else:
    ...         print(f"Error: {error_code.message()}")
    >>>
    >>> compressor.start(on_complete)
    >>> io_context.run()
)");

    // DirectoryCompressor class binding
    py::class_<atom::async::io::DirectoryCompressor,
               atom::async::io::BaseCompressor>(
        m, "DirectoryCompressor",
        R"(Asynchronous compressor for directories.

Compresses an entire directory asynchronously, creating a compressed
archive of all files in the directory.

Args:
    io_context: The ASIO I/O context for asynchronous operations
    input_dir: Path to the input directory to compress
    output_file: Path to the output compressed file

Examples:
    >>> import asio
    >>> from atom.io.async_compress import DirectoryCompressor
    >>>
    >>> io_context = asio.io_context()
    >>> compressor = DirectoryCompressor(
    ...     io_context, "/path/to/directory", "archive.gz"
    ... )
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, const fs::path&, const fs::path&>(),
             py::arg("io_context"), py::arg("input_dir"),
             py::arg("output_file"),
             "Constructs a DirectoryCompressor for the specified directory.")
        .def("start",
             py::overload_cast<>(&atom::async::io::DirectoryCompressor::start),
             R"(Starts the directory compression process without a callback.

Examples:
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(
            "start",
            [](atom::async::io::DirectoryCompressor& self,
               py::function callback) {
                self.start(
                    [callback](const std::error_code& ec, std::size_t bytes) {
                        py::gil_scoped_acquire acquire;
                        callback(ec, bytes);
                    });
            },
            py::arg("callback"),
            R"(Starts the directory compression process with a completion callback.

Args:
    callback: Function to call when compression completes.
              Signature: callback(error_code, bytes_processed)

Examples:
    >>> def on_complete(error_code, bytes_processed):
    ...     if not error_code:
    ...         print(f"Directory compressed: {bytes_processed} bytes")
    ...     else:
    ...         print(f"Compression failed: {error_code.message()}")
    >>>
    >>> compressor.start(on_complete)
    >>> io_context.run()
)");

    // Module constants
    m.attr("__version__") = "1.0.0";
    m.attr("HAS_ASIO_SUPPORT") = true;
    m.attr("CHUNK_SIZE") = 32768;  // Default chunk size from C++
}
#else
// Provide a stub module when ASIO is not available
PYBIND11_MODULE(async_compress, m) {
    m.doc() = "AsyncCompress module - ASIO support not compiled";
    m.attr("HAS_ASIO_SUPPORT") = false;

    py::class_<int>(m, "SingleFileCompressor").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));

    py::class_<int>(m, "DirectoryCompressor").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));
}
#endif  // ATOM_USE_ASIO
