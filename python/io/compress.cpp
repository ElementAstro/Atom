#include "atom/io/async_compress.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(compress, m) {
    m.doc() =
        "Asynchronous compression and decompression module for the atom "
        "package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const fs::filesystem_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // BaseCompressor class binding (abstract class, not directly instantiable)
    py::class_<atom::async::io::BaseCompressor>(
        m, "BaseCompressor", "Base class for compression operations")
        .def("start", &atom::async::io::BaseCompressor::start,
             "Starts the compression process");

    // SingleFileCompressor class binding
    py::class_<atom::async::io::SingleFileCompressor,
               atom::async::io::BaseCompressor>(m, "SingleFileCompressor",
                                                R"(Compressor for single files.

This class compresses a single file using zlib compression.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the input file to compress
    output_file: Path to the output compressed file

Examples:
    >>> import asio
    >>> from atom.io.compress import SingleFileCompressor
    >>> io_context = asio.io_context()
    >>> compressor = SingleFileCompressor(io_context, "data.txt", "data.txt.gz")
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, const fs::path&, const fs::path&>(),
             py::arg("io_context"), py::arg("input_file"),
             py::arg("output_file"), "Constructs a SingleFileCompressor.");

    // DirectoryCompressor class binding
    py::class_<atom::async::io::DirectoryCompressor,
               atom::async::io::BaseCompressor>(m, "DirectoryCompressor",
                                                R"(Compressor for directories.

This class compresses an entire directory into a single compressed file.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the input directory to compress
    output_file: Path to the output compressed file

Examples:
    >>> import asio
    >>> from atom.io.compress import DirectoryCompressor
    >>> io_context = asio.io_context()
    >>> compressor = DirectoryCompressor(io_context, "data_dir", "data_dir.gz")
    >>> compressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, fs::path, const fs::path&>(),
             py::arg("io_context"), py::arg("input_dir"),
             py::arg("output_file"), "Constructs a DirectoryCompressor.");

    // BaseDecompressor class binding (abstract class, not directly
    // instantiable)
    py::class_<atom::async::io::BaseDecompressor>(
        m, "BaseDecompressor", "Base class for decompression operations")
        .def("start", &atom::async::io::BaseDecompressor::start,
             "Starts the decompression process");

    // SingleFileDecompressor class binding
    py::class_<atom::async::io::SingleFileDecompressor,
               atom::async::io::BaseDecompressor>(
        m, "SingleFileDecompressor",
        R"(Decompressor for single files.

This class decompresses a single compressed file.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the input compressed file
    output_folder: Path to the output folder for decompressed content

Examples:
    >>> import asio
    >>> from atom.io.compress import SingleFileDecompressor
    >>> io_context = asio.io_context()
    >>> decompressor = SingleFileDecompressor(io_context, "data.txt.gz", "output_dir")
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, fs::path, fs::path>(),
             py::arg("io_context"), py::arg("input_file"),
             py::arg("output_folder"), "Constructs a SingleFileDecompressor.");

    // DirectoryDecompressor class binding
    py::class_<atom::async::io::DirectoryDecompressor,
               atom::async::io::BaseDecompressor>(
        m, "DirectoryDecompressor",
        R"(Decompressor for directories.

This class decompresses multiple compressed files in a directory.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the input directory containing compressed files
    output_folder: Path to the output folder for decompressed content

Examples:
    >>> import asio
    >>> from atom.io.compress import DirectoryDecompressor
    >>> io_context = asio.io_context()
    >>> decompressor = DirectoryDecompressor(io_context, "compressed_dir", "output_dir")
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, const fs::path&, const fs::path&>(),
             py::arg("io_context"), py::arg("input_dir"),
             py::arg("output_folder"), "Constructs a DirectoryDecompressor.");

    // ZipOperation class binding (abstract class, not directly instantiable)
    py::class_<atom::async::io::ZipOperation>(m, "ZipOperation",
                                              "Base class for ZIP operations")
        .def("start", &atom::async::io::ZipOperation::start,
             "Starts the ZIP operation");

    // ListFilesInZip class binding
    py::class_<atom::async::io::ListFilesInZip, atom::async::io::ZipOperation>(
        m, "ListFilesInZip",
        R"(Lists files in a ZIP archive.

This class lists all the files contained within a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Examples:
    >>> import asio
    >>> from atom.io.compress import ListFilesInZip
    >>> io_context = asio.io_context()
    >>> lister = ListFilesInZip(io_context, "archive.zip")
    >>> lister.start()
    >>> io_context.run()
    >>> files = lister.get_file_list()
    >>> print(f"Files in archive: {files}")
)")
        .def(py::init<asio::io_context&, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"),
             "Constructs a ListFilesInZip object.")
        .def("get_file_list", &atom::async::io::ListFilesInZip::getFileList,
             R"(Gets the list of files in the ZIP archive.

Returns:
    A list of filenames contained in the ZIP archive
)");

    // FileExistsInZip class binding
    py::class_<atom::async::io::FileExistsInZip, atom::async::io::ZipOperation>(
        m, "FileExistsInZip",
        R"(Checks if a file exists in a ZIP archive.

This class checks whether a specific file exists within a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to check for

Examples:
    >>> import asio
    >>> from atom.io.compress import FileExistsInZip
    >>> io_context = asio.io_context()
    >>> checker = FileExistsInZip(io_context, "archive.zip", "document.txt")
    >>> checker.start()
    >>> io_context.run()
    >>> if checker.found():
    ...     print("File exists in the archive")
    ... else:
    ...     print("File not found in the archive")
)")
        .def(py::init<asio::io_context&, std::string_view, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"),
             "Constructs a FileExistsInZip object.")
        .def("found", &atom::async::io::FileExistsInZip::found,
             R"(Checks if the file was found in the ZIP archive.

Returns:
    True if the file exists in the archive, False otherwise
)");

    // RemoveFileFromZip class binding
    py::class_<atom::async::io::RemoveFileFromZip,
               atom::async::io::ZipOperation>(
        m, "RemoveFileFromZip",
        R"(Removes a file from a ZIP archive.

This class removes a specific file from a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to remove

Examples:
    >>> import asio
    >>> from atom.io.compress import RemoveFileFromZip
    >>> io_context = asio.io_context()
    >>> remover = RemoveFileFromZip(io_context, "archive.zip", "document.txt")
    >>> remover.start()
    >>> io_context.run()
    >>> if remover.is_successful():
    ...     print("File was successfully removed")
    ... else:
    ...     print("Failed to remove file")
)")
        .def(py::init<asio::io_context&, std::string_view, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"),
             "Constructs a RemoveFileFromZip object.")
        .def("is_successful", &atom::async::io::RemoveFileFromZip::isSuccessful,
             R"(Checks if the file removal was successful.

Returns:
    True if the file was successfully removed, False otherwise
)");

    // GetZipFileSize class binding
    py::class_<atom::async::io::GetZipFileSize, atom::async::io::ZipOperation>(
        m, "GetZipFileSize",
        R"(Gets the size of a ZIP file.

This class calculates the total size of a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Examples:
    >>> import asio
    >>> from atom.io.compress import GetZipFileSize
    >>> io_context = asio.io_context()
    >>> size_getter = GetZipFileSize(io_context, "archive.zip")
    >>> size_getter.start()
    >>> io_context.run()
    >>> size = size_getter.get_size_value()
    >>> print(f"Archive size: {size} bytes")
)")
        .def(py::init<asio::io_context&, std::string_view>(),
             py::arg("io_context"), py::arg("zip_file"),
             "Constructs a GetZipFileSize object.")
        .def("get_size_value", &atom::async::io::GetZipFileSize::getSizeValue,
             R"(Gets the size of the ZIP file.

Returns:
    The size of the ZIP file in bytes
)");

    // Helper functions

    // Compress file helper
    m.def(
        "compress_file",
        [](asio::io_context& io_context, const std::string& input_file,
           const std::string& output_file) {
            auto compressor =
                std::make_unique<atom::async::io::SingleFileCompressor>(
                    io_context, input_file, output_file);
            compressor->start();
            return compressor;
        },
        py::arg("io_context"), py::arg("input_file"), py::arg("output_file"),
        R"(Convenience function to compress a single file.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the input file to compress
    output_file: Path to the output compressed file

Returns:
    A SingleFileCompressor object that has been started

Examples:
    >>> import asio
    >>> from atom.io.compress import compress_file
    >>> io_context = asio.io_context()
    >>> compressor = compress_file(io_context, "data.txt", "data.txt.gz")
    >>> io_context.run()
)");

    // Compress directory helper
    m.def(
        "compress_directory",
        [](asio::io_context& io_context, const std::string& input_dir,
           const std::string& output_file) {
            auto compressor =
                std::make_unique<atom::async::io::DirectoryCompressor>(
                    io_context, input_dir, output_file);
            compressor->start();
            return compressor;
        },
        py::arg("io_context"), py::arg("input_dir"), py::arg("output_file"),
        R"(Convenience function to compress a directory.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the input directory to compress
    output_file: Path to the output compressed file

Returns:
    A DirectoryCompressor object that has been started

Examples:
    >>> import asio
    >>> from atom.io.compress import compress_directory
    >>> io_context = asio.io_context()
    >>> compressor = compress_directory(io_context, "data_dir", "data_dir.gz")
    >>> io_context.run()
)");

    // Decompress file helper
    m.def(
        "decompress_file",
        [](asio::io_context& io_context, const std::string& input_file,
           const std::string& output_folder) {
            auto decompressor =
                std::make_unique<atom::async::io::SingleFileDecompressor>(
                    io_context, input_file, output_folder);
            decompressor->start();
            return decompressor;
        },
        py::arg("io_context"), py::arg("input_file"), py::arg("output_folder"),
        R"(Convenience function to decompress a single file.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the input compressed file
    output_folder: Path to the output folder for decompressed content

Returns:
    A SingleFileDecompressor object that has been started

Examples:
    >>> import asio
    >>> from atom.io.compress import decompress_file
    >>> io_context = asio.io_context()
    >>> decompressor = decompress_file(io_context, "data.txt.gz", "output_dir")
    >>> io_context.run()
)");

    // Decompress directory helper
    m.def(
        "decompress_directory",
        [](asio::io_context& io_context, const std::string& input_dir,
           const std::string& output_folder) {
            auto decompressor =
                std::make_unique<atom::async::io::DirectoryDecompressor>(
                    io_context, input_dir, output_folder);
            decompressor->start();
            return decompressor;
        },
        py::arg("io_context"), py::arg("input_dir"), py::arg("output_folder"),
        R"(Convenience function to decompress multiple files in a directory.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the input directory containing compressed files
    output_folder: Path to the output folder for decompressed content

Returns:
    A DirectoryDecompressor object that has been started

Examples:
    >>> import asio
    >>> from atom.io.compress import decompress_directory
    >>> io_context = asio.io_context()
    >>> decompressor = decompress_directory(io_context, "compressed_dir", "output_dir")
    >>> io_context.run()
)");

    // List files in ZIP helper
    m.def(
        "list_files_in_zip",
        [](asio::io_context& io_context, const std::string& zip_file) {
            auto lister = std::make_unique<atom::async::io::ListFilesInZip>(
                io_context, zip_file);
            lister->start();
            auto result = lister->getFileList();
            return result;
        },
        py::arg("io_context"), py::arg("zip_file"),
        R"(Convenience function to list files in a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Returns:
    A list of filenames contained in the ZIP archive

Examples:
    >>> import asio
    >>> from atom.io.compress import list_files_in_zip
    >>> io_context = asio.io_context()
    >>> files = list_files_in_zip(io_context, "archive.zip")
    >>> io_context.run()
    >>> print(f"Files in archive: {files}")
)");

    // Check if file exists in ZIP helper
    m.def(
        "file_exists_in_zip",
        [](asio::io_context& io_context, const std::string& zip_file,
           const std::string& file_name) {
            auto checker = std::make_unique<atom::async::io::FileExistsInZip>(
                io_context, zip_file, file_name);
            checker->start();
            return checker->found();
        },
        py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"),
        R"(Convenience function to check if a file exists in a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to check for

Returns:
    True if the file exists in the archive, False otherwise

Examples:
    >>> import asio
    >>> from atom.io.compress import file_exists_in_zip
    >>> io_context = asio.io_context()
    >>> exists = file_exists_in_zip(io_context, "archive.zip", "document.txt")
    >>> io_context.run()
    >>> print(f"File exists: {exists}")
)");

    // Get ZIP file size helper
    m.def(
        "get_zip_file_size",
        [](asio::io_context& io_context, const std::string& zip_file) {
            auto size_getter =
                std::make_unique<atom::async::io::GetZipFileSize>(io_context,
                                                                  zip_file);
            size_getter->start();
            return size_getter->getSizeValue();
        },
        py::arg("io_context"), py::arg("zip_file"),
        R"(Convenience function to get the size of a ZIP file.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Returns:
    The size of the ZIP file in bytes

Examples:
    >>> import asio
    >>> from atom.io.compress import get_zip_file_size
    >>> io_context = asio.io_context()
    >>> size = get_zip_file_size(io_context, "archive.zip")
    >>> io_context.run()
    >>> print(f"Archive size: {size} bytes")
)");

    // Remove file from ZIP helper
    m.def(
        "remove_file_from_zip",
        [](asio::io_context& io_context, const std::string& zip_file,
           const std::string& file_name) {
            auto remover = std::make_unique<atom::async::io::RemoveFileFromZip>(
                io_context, zip_file, file_name);
            remover->start();
            return remover->isSuccessful();
        },
        py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"),
        R"(Convenience function to remove a file from a ZIP archive.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to remove

Returns:
    True if the file was successfully removed, False otherwise

Examples:
    >>> import asio
    >>> from atom.io.compress import remove_file_from_zip
    >>> io_context = asio.io_context()
    >>> success = remove_file_from_zip(io_context, "archive.zip", "document.txt")
    >>> io_context.run()
    >>> print(f"File removed successfully: {success}")
)");
}