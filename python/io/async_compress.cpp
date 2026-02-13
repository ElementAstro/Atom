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
    py::class_<atom::io::async::BaseCompressor>(
        m, "BaseCompressor",
        R"(Base class for compression operations.

This is an abstract base class that provides common functionality
for all compressor types. Use SingleFileCompressor or DirectoryCompressor
for actual compression operations.
)")
        .def("start", &atom::io::async::BaseCompressor::start,
             "Starts the compression process.");

    // SingleFileCompressor class binding
    py::class_<atom::io::async::SingleFileCompressor,
               atom::io::async::BaseCompressor>(
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
             py::overload_cast<>(&atom::io::async::SingleFileCompressor::start),
             R"(Starts the compression process without a callback.

The compression will run asynchronously. Use the I/O context's run()
method to process the operation.

Examples:
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(
            "start",
            [](atom::io::async::SingleFileCompressor& self,
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
    py::class_<atom::io::async::DirectoryCompressor,
               atom::io::async::BaseCompressor>(
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
             py::overload_cast<>(&atom::io::async::DirectoryCompressor::start),
             R"(Starts the directory compression process without a callback.

Examples:
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(
            "start",
            [](atom::io::async::DirectoryCompressor& self,
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

    // =========================================================================
    // Decompressor Classes
    // =========================================================================

    // BaseDecompressor class (abstract base)
    py::class_<atom::io::async::BaseDecompressor>(
        m, "BaseDecompressor",
        R"(Base class for decompression operations.

This is an abstract base class that provides common functionality
for all decompressor types. Use SingleFileDecompressor or
DirectoryDecompressor for actual decompression operations.
)")
        .def("start", &atom::io::async::BaseDecompressor::start,
             "Starts the decompression process.");

    // SingleFileDecompressor class binding
    py::class_<atom::io::async::SingleFileDecompressor,
               atom::io::async::BaseDecompressor>(
        m, "SingleFileDecompressor",
        R"(Asynchronous decompressor for single files.

Decompresses a single file asynchronously using zlib decompression.

Args:
    io_context: The ASIO I/O context for asynchronous operations
    input_file: Path to the input compressed file
    output_folder: Path to the output folder for decompressed content

Examples:
    >>> import asio
    >>> from atom.io.async_compress import SingleFileDecompressor
    >>>
    >>> io_context = asio.io_context()
    >>> decompressor = SingleFileDecompressor(
    ...     io_context, "data.txt.gz", "output_dir"
    ... )
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, fs::path, fs::path>(),
             py::arg("io_context"), py::arg("input_file"),
             py::arg("output_folder"),
             "Constructs a SingleFileDecompressor for the specified file.")
        .def("start",
             py::overload_cast<>(
                 &atom::io::async::SingleFileDecompressor::start),
             R"(Starts the decompression process without a callback.

Examples:
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(
            "start",
            [](atom::io::async::SingleFileDecompressor& self,
               py::function callback) {
                self.start(
                    [callback](const std::error_code& ec, std::size_t bytes) {
                        py::gil_scoped_acquire acquire;
                        callback(ec, bytes);
                    });
            },
            py::arg("callback"),
            R"(Starts the decompression process with a completion callback.

Args:
    callback: Function to call when decompression completes.
              Signature: callback(error_code, bytes_processed)

Examples:
    >>> def on_complete(error_code, bytes_processed):
    ...     if not error_code:
    ...         print(f"Decompressed {bytes_processed} bytes")
    ...     else:
    ...         print(f"Error: {error_code.message()}")
    >>>
    >>> decompressor.start(on_complete)
    >>> io_context.run()
)");

    // DirectoryDecompressor class binding
    py::class_<atom::io::async::DirectoryDecompressor,
               atom::io::async::BaseDecompressor>(
        m, "DirectoryDecompressor",
        R"(Asynchronous decompressor for directories.

Decompresses all compressed files in a directory asynchronously.

Args:
    io_context: The ASIO I/O context for asynchronous operations
    input_dir: Path to the input directory containing compressed files
    output_folder: Path to the output folder for decompressed content

Examples:
    >>> import asio
    >>> from atom.io.async_compress import DirectoryDecompressor
    >>>
    >>> io_context = asio.io_context()
    >>> decompressor = DirectoryDecompressor(
    ...     io_context, "compressed_dir", "output_dir"
    ... )
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, const fs::path&, const fs::path&>(),
             py::arg("io_context"), py::arg("input_dir"),
             py::arg("output_folder"),
             "Constructs a DirectoryDecompressor for the specified directory.")
        .def("start",
             py::overload_cast<>(
                 &atom::io::async::DirectoryDecompressor::start),
             R"(Starts the directory decompression process without a callback.

Examples:
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(
            "start",
            [](atom::io::async::DirectoryDecompressor& self,
               py::function callback) {
                self.start(
                    [callback](const std::error_code& ec, std::size_t bytes) {
                        py::gil_scoped_acquire acquire;
                        callback(ec, bytes);
                    });
            },
            py::arg("callback"),
            R"(Starts the directory decompression with a completion callback.

Args:
    callback: Function to call when decompression completes.
              Signature: callback(error_code, bytes_processed)

Examples:
    >>> def on_complete(error_code, bytes_processed):
    ...     if not error_code:
    ...         print(f"Decompressed {bytes_processed} bytes")
    ...     else:
    ...         print(f"Error: {error_code.message()}")
    >>>
    >>> decompressor.start(on_complete)
    >>> io_context.run()
)");

    // =========================================================================
    // ZIP Operation Classes
    // =========================================================================

    // ZipOperation class binding (abstract base)
    py::class_<atom::io::async::ZipOperation>(
        m, "ZipOperation",
        R"(Base class for asynchronous ZIP operations.

This is an abstract base class for ZIP file operations.
Use the derived classes for specific ZIP operations.
)")
        .def("start", &atom::io::async::ZipOperation::start,
             "Starts the ZIP operation.");

    // ListFilesInZip
    py::class_<atom::io::async::ListFilesInZip, atom::io::async::ZipOperation>(
        m, "ListFilesInZip",
        R"(Lists files in a ZIP archive asynchronously.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Examples:
    >>> lister = ListFilesInZip(io_context, "archive.zip")
    >>> lister.start()
    >>> io_context.run()
    >>> files = lister.get_file_list()
)")
        .def(py::init<asio::io_context&, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"))
        .def("get_file_list", &atom::io::async::ListFilesInZip::getFileList,
             "Gets the list of files in the ZIP archive.");

    // FileExistsInZip
    py::class_<atom::io::async::FileExistsInZip, atom::io::async::ZipOperation>(
        m, "FileExistsInZip",
        R"(Checks if a file exists in a ZIP archive asynchronously.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to check

Examples:
    >>> checker = FileExistsInZip(io_context, "archive.zip", "doc.txt")
    >>> checker.start()
    >>> io_context.run()
    >>> print(checker.found())
)")
        .def(py::init<asio::io_context&, std::string_view, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"))
        .def("found", &atom::io::async::FileExistsInZip::found,
             "Returns True if the file was found in the archive.");

    // RemoveFileFromZip
    py::class_<atom::io::async::RemoveFileFromZip,
               atom::io::async::ZipOperation>(
        m, "RemoveFileFromZip",
        R"(Removes a file from a ZIP archive asynchronously.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to remove

Examples:
    >>> remover = RemoveFileFromZip(io_context, "archive.zip", "old.txt")
    >>> remover.start()
    >>> io_context.run()
    >>> print(remover.is_successful())
)")
        .def(py::init<asio::io_context&, std::string_view, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"))
        .def("is_successful",
             &atom::io::async::RemoveFileFromZip::isSuccessful,
             "Returns True if the file was successfully removed.");

    // GetZipFileSize
    py::class_<atom::io::async::GetZipFileSize, atom::io::async::ZipOperation>(
        m, "GetZipFileSize",
        R"(Gets the size of a ZIP file asynchronously.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Examples:
    >>> getter = GetZipFileSize(io_context, "archive.zip")
    >>> getter.start()
    >>> io_context.run()
    >>> print(getter.get_size_value())
)")
        .def(py::init<asio::io_context&, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"))
        .def("get_size_value",
             &atom::io::async::GetZipFileSize::getSizeValue,
             "Gets the size of the ZIP file in bytes.");

    // Module constants
    m.attr("__version__") = "2.0.0";
    m.attr("HAS_ASIO_SUPPORT") = true;
    m.attr("CHUNK_SIZE") = 32768;
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

    py::class_<int>(m, "SingleFileDecompressor").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));

    py::class_<int>(m, "DirectoryDecompressor").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));

    py::class_<int>(m, "ListFilesInZip").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));

    py::class_<int>(m, "FileExistsInZip").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));

    py::class_<int>(m, "RemoveFileFromZip").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));

    py::class_<int>(m, "GetZipFileSize").def(py::init([]() -> int {
        throw std::runtime_error(
            "AsyncCompress requires ASIO support which was not compiled in");
        return 0;
    }));
}
#endif  // ATOM_USE_ASIO
