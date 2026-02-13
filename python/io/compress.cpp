#include "atom/io/async/async_compress.hpp"
#include "atom/io/compression/compress.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
namespace fs = std::filesystem;

PYBIND11_MODULE(compress, m) {
    m.doc() = R"pbdoc(
        Compression and Decompression Module
        ------------------------------------

        This module provides comprehensive compression and decompression
        functionality including synchronous and asynchronous operations.

        Synchronous Operations:
        - GZ file compression/decompression (compressFile, decompressFile)
        - ZIP archive operations (compressFolder, extractZip, createZip, etc.)
        - Slice-based compression (compressFileInSlices, mergeCompressedSlices)
        - In-memory data compression/decompression
        - Backup and restore with optional compression

        Asynchronous Operations (requires ASIO):
        - SingleFileCompressor / SingleFileDecompressor
        - DirectoryCompressor / DirectoryDecompressor
        - ZIP operations (ListFilesInZip, FileExistsInZip, etc.)

        Types:
        - CompressionResult: Result of compression operations
        - CompressionOptions: Options for compression
        - DecompressionOptions: Options for decompression
        - ZipFileInfo: Information about a file in a ZIP archive

        Examples:
            >>> from atom.io.compress import compress_gz, decompress_gz
            >>> compress_gz("data.txt", "data.txt.gz")
            >>> decompress_gz("data.txt.gz", "data_restored.txt")
            >>>
            >>> from atom.io.compress import compress_folder, extract_zip
            >>> compress_folder("my_folder", "archive.zip")
            >>> extract_zip("archive.zip", "output_dir")
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

    // =========================================================================
    // Types and Enums
    // =========================================================================

    // CompressionResult struct
    py::class_<atom::io::CompressionResult>(
        m, "CompressionResult",
        R"(Result of a compression or decompression operation.

Attributes:
    success: Whether the operation succeeded
    original_size: Original data size in bytes
    compressed_size: Compressed data size in bytes
    compression_ratio: Achieved compression ratio
    error_message: Error message if operation failed
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("success", &atom::io::CompressionResult::success,
                       "Whether the operation succeeded")
        .def_readwrite("original_size",
                       &atom::io::CompressionResult::original_size,
                       "Original data size in bytes")
        .def_readwrite("compressed_size",
                       &atom::io::CompressionResult::compressed_size,
                       "Compressed data size in bytes")
        .def_readwrite("compression_ratio",
                       &atom::io::CompressionResult::compression_ratio,
                       "Compression ratio achieved")
        .def_property(
            "error_message",
            [](const atom::io::CompressionResult& self) {
                return std::string(self.error_message.c_str());
            },
            [](atom::io::CompressionResult& self, const std::string& msg) {
                self.error_message = msg.c_str();
            },
            "Error message if operation failed")
        .def(
            "__bool__",
            [](const atom::io::CompressionResult& self) {
                return self.success;
            },
            "Check if the operation was successful")
        .def("__repr__", [](const atom::io::CompressionResult& self) {
            if (self.success) {
                return "CompressionResult(success=True, original=" +
                       std::to_string(self.original_size) +
                       ", compressed=" +
                       std::to_string(self.compressed_size) +
                       ", ratio=" +
                       std::to_string(self.compression_ratio) + ")";
            } else {
                return "CompressionResult(success=False, error='" +
                       std::string(self.error_message.c_str()) + "')";
            }
        });

    // CompressionOptions struct
    py::class_<atom::io::CompressionOptions>(
        m, "CompressionOptions",
        R"(Options for compression operations.

Attributes:
    level: Compression level (-1=default, 0-9)
    window_bits: Window bits for compression context
    chunk_size: Size of chunks for processing
    use_parallel: Whether to use parallel processing
    num_threads: Number of parallel threads
    create_backup: Whether to create a backup before compression
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("level", &atom::io::CompressionOptions::level,
                       "Compression level (-1=default, 0-9)")
        .def_readwrite("window_bits",
                       &atom::io::CompressionOptions::window_bits,
                       "Window bits for compression context")
        .def_readwrite("chunk_size", &atom::io::CompressionOptions::chunk_size,
                       "Size of chunks for processing")
        .def_readwrite("use_parallel",
                       &atom::io::CompressionOptions::use_parallel,
                       "Whether to use parallel processing")
        .def_readwrite("num_threads",
                       &atom::io::CompressionOptions::num_threads,
                       "Number of parallel threads")
        .def_readwrite("create_backup",
                       &atom::io::CompressionOptions::create_backup,
                       "Whether to create a backup");

    // DecompressionOptions struct
    py::class_<atom::io::DecompressionOptions>(
        m, "DecompressionOptions",
        R"(Options for decompression operations.

Attributes:
    chunk_size: Size of chunks for processing
    use_parallel: Whether to use parallel processing
    num_threads: Number of parallel threads
    verify_checksum: Whether to verify checksum
    window_bits: Window bits for decompression context
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("chunk_size",
                       &atom::io::DecompressionOptions::chunk_size,
                       "Size of chunks for processing")
        .def_readwrite("use_parallel",
                       &atom::io::DecompressionOptions::use_parallel,
                       "Whether to use parallel processing")
        .def_readwrite("num_threads",
                       &atom::io::DecompressionOptions::num_threads,
                       "Number of parallel threads")
        .def_readwrite("verify_checksum",
                       &atom::io::DecompressionOptions::verify_checksum,
                       "Whether to verify checksum")
        .def_readwrite("window_bits",
                       &atom::io::DecompressionOptions::window_bits,
                       "Window bits for decompression context");

    // ZipFileInfo struct
    py::class_<atom::io::ZipFileInfo>(
        m, "ZipFileInfo",
        R"(Information about a file in a ZIP archive.

Attributes:
    name: Name of the file in the archive
    size: Uncompressed size in bytes
    compressed_size: Compressed size in bytes
    datetime: Date and time information
    is_directory: Whether the entry is a directory
    is_encrypted: Whether the entry is encrypted
    crc: CRC checksum
)")
        .def(py::init<>(), "Default constructor")
        .def_property(
            "name",
            [](const atom::io::ZipFileInfo& self) {
                return std::string(self.name.c_str());
            },
            [](atom::io::ZipFileInfo& self, const std::string& n) {
                self.name = n.c_str();
            },
            "Name of the file in the archive")
        .def_readwrite("size", &atom::io::ZipFileInfo::size,
                       "Uncompressed size in bytes")
        .def_readwrite("compressed_size",
                       &atom::io::ZipFileInfo::compressed_size,
                       "Compressed size in bytes")
        .def_property(
            "datetime",
            [](const atom::io::ZipFileInfo& self) {
                return std::string(self.datetime.c_str());
            },
            [](atom::io::ZipFileInfo& self, const std::string& dt) {
                self.datetime = dt.c_str();
            },
            "Date and time information")
        .def_readwrite("is_directory", &atom::io::ZipFileInfo::is_directory,
                       "Whether the entry is a directory")
        .def_readwrite("is_encrypted", &atom::io::ZipFileInfo::is_encrypted,
                       "Whether the entry is encrypted")
        .def_readwrite("crc", &atom::io::ZipFileInfo::crc, "CRC checksum")
        .def("__repr__", [](const atom::io::ZipFileInfo& self) {
            return "ZipFileInfo(name='" +
                   std::string(self.name.c_str()) +
                   "', size=" + std::to_string(self.size) +
                   ", compressed=" +
                   std::to_string(self.compressed_size) +
                   ", is_dir=" + (self.is_directory ? "True" : "False") +
                   ")";
        });

    // =========================================================================
    // Synchronous GZ Compression / Decompression
    // =========================================================================

    m.def(
        "compress_gz",
        [](const std::string& input_file, const std::string& output_folder) {
            return atom::io::compressFile(input_file, output_folder);
        },
        py::arg("input_file"), py::arg("output_folder"),
        R"(Compresses a file using GZ (zlib) compression.

Args:
    input_file: Path to the input file
    output_folder: Path to the output folder for compressed file

Returns:
    CompressionResult with success status and size information

Examples:
    >>> result = compress_gz("data.txt", "compressed/")
    >>> if result:
    ...     print(f"Compressed {result.original_size} -> {result.compressed_size}")
)");

    m.def(
        "decompress_gz",
        [](const std::string& input_file, const std::string& output_folder) {
            return atom::io::decompressFile(input_file, output_folder);
        },
        py::arg("input_file"), py::arg("output_folder"),
        R"(Decompresses a GZ compressed file.

Args:
    input_file: Path to the compressed input file
    output_folder: Path to the output folder for decompressed file

Returns:
    CompressionResult with success status and size information

Examples:
    >>> result = decompress_gz("data.txt.gz", "output/")
    >>> if result:
    ...     print("Decompression succeeded")
)");

    // =========================================================================
    // Synchronous ZIP Operations
    // =========================================================================

    m.def(
        "compress_folder",
        [](const std::string& folder_path, const std::string& output_path) {
            return atom::io::compressFolder(folder_path, output_path);
        },
        py::arg("folder_path"), py::arg("output_path"),
        R"(Compresses a folder into a ZIP archive.

Args:
    folder_path: Path to the folder to compress
    output_path: Path to the output ZIP file

Returns:
    CompressionResult with success status and size information

Examples:
    >>> result = compress_folder("my_project", "my_project.zip")
    >>> if result:
    ...     print(f"Compressed to {result.compressed_size} bytes")
)");

    m.def(
        "extract_zip",
        [](const std::string& zip_path, const std::string& output_folder) {
            return atom::io::extractZip(zip_path, output_folder);
        },
        py::arg("zip_path"), py::arg("output_folder"),
        R"(Extracts a ZIP archive to a directory.

Args:
    zip_path: Path to the ZIP file
    output_folder: Path to the output directory

Returns:
    CompressionResult with success status

Examples:
    >>> result = extract_zip("archive.zip", "output_dir")
    >>> if result:
    ...     print("Extraction succeeded")
)");

    m.def(
        "create_zip",
        [](const std::string& source_path, const std::string& zip_path) {
            return atom::io::createZip(source_path, zip_path);
        },
        py::arg("source_path"), py::arg("zip_path"),
        R"(Creates a ZIP archive from a source folder or file.

Args:
    source_path: Source folder or file path
    zip_path: Target ZIP file path

Returns:
    CompressionResult with success status

Examples:
    >>> result = create_zip("my_folder", "archive.zip")
    >>> if result:
    ...     print("ZIP created successfully")
)");

    m.def(
        "list_zip_contents",
        [](const std::string& zip_path) {
            auto contents = atom::io::listZipContents(zip_path);
            std::vector<atom::io::ZipFileInfo> result(contents.begin(),
                                                      contents.end());
            return result;
        },
        py::arg("zip_path"),
        R"(Lists the contents of a ZIP archive.

Args:
    zip_path: Path to the ZIP file

Returns:
    List of ZipFileInfo objects describing files in the archive

Examples:
    >>> contents = list_zip_contents("archive.zip")
    >>> for item in contents:
    ...     print(f"{item.name}: {item.size} bytes")
)");

    m.def(
        "file_exists_in_zip_sync",
        [](const std::string& zip_path, const std::string& file_path) {
            return atom::io::fileExistsInZip(zip_path, file_path);
        },
        py::arg("zip_path"), py::arg("file_path"),
        R"(Checks if a file exists in a ZIP archive (synchronous).

Args:
    zip_path: Path to the ZIP file
    file_path: Path of the file to check for within the archive

Returns:
    True if the file exists in the archive, False otherwise

Examples:
    >>> file_exists_in_zip_sync("archive.zip", "readme.txt")
    True
)");

    m.def(
        "remove_from_zip",
        [](const std::string& zip_path, const std::string& file_path) {
            return atom::io::removeFromZip(zip_path, file_path);
        },
        py::arg("zip_path"), py::arg("file_path"),
        R"(Removes a file from a ZIP archive.

Args:
    zip_path: Path to the ZIP file
    file_path: Path of the file to remove

Returns:
    CompressionResult with success status

Examples:
    >>> result = remove_from_zip("archive.zip", "unwanted.txt")
    >>> if result:
    ...     print("File removed")
)");

    m.def(
        "get_zip_size",
        [](const std::string& zip_path) -> py::object {
            auto result = atom::io::getZipSize(zip_path);
            if (result.has_value()) {
                return py::cast(result.value());
            }
            return py::none();
        },
        py::arg("zip_path"),
        R"(Gets the total size of a ZIP archive.

Args:
    zip_path: Path to the ZIP file

Returns:
    Size of the ZIP file in bytes, or None if the file could not be read

Examples:
    >>> size = get_zip_size("archive.zip")
    >>> if size is not None:
    ...     print(f"Archive size: {size} bytes")
)");

    // =========================================================================
    // Slice-based Compression
    // =========================================================================

    m.def(
        "compress_file_in_slices",
        [](const std::string& file_path, size_t slice_size) {
            return atom::io::compressFileInSlices(file_path, slice_size);
        },
        py::arg("file_path"), py::arg("slice_size"),
        R"(Compresses a large file in slices.

Splits the input file into slices and compresses each independently.

Args:
    file_path: Path to the file to compress
    slice_size: Size of each slice in bytes

Returns:
    CompressionResult with success status

Examples:
    >>> result = compress_file_in_slices("large_file.bin", 1024*1024)
    >>> if result:
    ...     print("Slice compression succeeded")
)");

    m.def(
        "merge_compressed_slices",
        [](const std::vector<std::string>& slice_files,
           const std::string& output_path) {
            atom::io::Vector<atom::io::String> files;
            for (const auto& f : slice_files) {
                files.push_back(atom::io::String(f.c_str()));
            }
            return atom::io::mergeCompressedSlices(files, output_path);
        },
        py::arg("slice_files"), py::arg("output_path"),
        R"(Merges compressed slices back into a single file.

Args:
    slice_files: List of slice file paths to merge
    output_path: Path to the output merged file

Returns:
    CompressionResult with success status

Examples:
    >>> result = merge_compressed_slices(
    ...     ["file.part0.gz", "file.part1.gz"],
    ...     "restored_file.bin"
    ... )
    >>> if result:
    ...     print("Merge succeeded")
)");

    // =========================================================================
    // In-Memory Data Compression
    // =========================================================================

    m.def(
        "compress_data",
        [](const py::bytes& data, int level) -> py::tuple {
            std::string input = data;
            atom::io::Vector<unsigned char> input_vec(input.begin(),
                                                      input.end());
            atom::io::CompressionOptions opts;
            opts.level = level;
            auto [result, compressed] =
                atom::io::compressData(input_vec, opts);
            return py::make_tuple(
                result,
                py::bytes(
                    reinterpret_cast<const char*>(compressed.data()),
                    compressed.size()));
        },
        py::arg("data"), py::arg("level") = -1,
        R"(Compresses data in memory using zlib.

Args:
    data: The data to compress (bytes)
    level: Compression level (-1=default, 0-9)

Returns:
    Tuple of (CompressionResult, compressed_bytes)

Examples:
    >>> result, compressed = compress_data(b"Hello, World!" * 100, level=9)
    >>> if result:
    ...     print(f"Compressed to {len(compressed)} bytes")
)");

    m.def(
        "decompress_data",
        [](const py::bytes& data,
           size_t expected_size) -> py::tuple {
            std::string input = data;
            atom::io::Vector<unsigned char> input_vec(input.begin(),
                                                      input.end());
            auto [result, decompressed] =
                atom::io::decompressData(input_vec, expected_size);
            return py::make_tuple(
                result,
                py::bytes(
                    reinterpret_cast<const char*>(decompressed.data()),
                    decompressed.size()));
        },
        py::arg("data"), py::arg("expected_size") = 0,
        R"(Decompresses data in memory using zlib.

Args:
    data: The compressed data (bytes)
    expected_size: Expected decompressed size (0 = unknown)

Returns:
    Tuple of (CompressionResult, decompressed_bytes)

Examples:
    >>> result, decompressed = decompress_data(compressed)
    >>> if result:
    ...     print(decompressed.decode())
)");

    // =========================================================================
    // Backup and Restore
    // =========================================================================

    m.def(
        "create_backup",
        [](const std::string& source_path, const std::string& backup_path,
           bool compress) {
            return atom::io::createBackup(source_path, backup_path, compress);
        },
        py::arg("source_path"), py::arg("backup_path"),
        py::arg("compress") = false,
        R"(Creates a backup of a file (optionally compressed).

Args:
    source_path: Path to the source file
    backup_path: Path to the backup destination
    compress: Whether to compress the backup (default: False)

Returns:
    CompressionResult with success status

Examples:
    >>> result = create_backup("important.db", "backups/important.db.bak")
    >>> if result:
    ...     print("Backup created successfully")
)");

    m.def(
        "restore_from_backup",
        [](const std::string& backup_path, const std::string& restore_path,
           bool compressed) {
            return atom::io::restoreFromBackup(backup_path, restore_path,
                                               compressed);
        },
        py::arg("backup_path"), py::arg("restore_path"),
        py::arg("compressed") = false,
        R"(Restores a file from a backup.

Args:
    backup_path: Path to the backup file
    restore_path: Path to restore to
    compressed: Whether the backup is compressed (default: False)

Returns:
    CompressionResult with success status

Examples:
    >>> result = restore_from_backup("backups/data.bak", "restored_data")
    >>> if result:
    ...     print("Restore succeeded")
)");

    // =========================================================================
    // Async Compressor/Decompressor Classes (requires ASIO)
    // =========================================================================

    // BaseCompressor class binding (abstract class, not directly instantiable)
    py::class_<atom::io::async::BaseCompressor>(
        m, "BaseCompressor", "Base class for async compression operations")
        .def("start", &atom::io::async::BaseCompressor::start,
             "Starts the compression process");

    // SingleFileCompressor class binding
    py::class_<atom::io::async::SingleFileCompressor,
               atom::io::async::BaseCompressor>(m, "SingleFileCompressor",
                                                R"(Async compressor for single files.

Compresses a single file asynchronously using zlib compression.

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
    py::class_<atom::io::async::DirectoryCompressor,
               atom::io::async::BaseCompressor>(m, "DirectoryCompressor",
                                                R"(Async compressor for directories.

Compresses an entire directory into a single compressed file.

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

    // BaseDecompressor class binding
    py::class_<atom::io::async::BaseDecompressor>(
        m, "BaseDecompressor", "Base class for async decompression operations")
        .def("start", &atom::io::async::BaseDecompressor::start,
             "Starts the decompression process");

    // SingleFileDecompressor class binding
    py::class_<atom::io::async::SingleFileDecompressor,
               atom::io::async::BaseDecompressor>(
        m, "SingleFileDecompressor",
        R"(Async decompressor for single files.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the input compressed file
    output_folder: Path to the output folder

Examples:
    >>> import asio
    >>> from atom.io.compress import SingleFileDecompressor
    >>> io_context = asio.io_context()
    >>> decompressor = SingleFileDecompressor(io_context, "data.txt.gz", "out")
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, fs::path, fs::path>(),
             py::arg("io_context"), py::arg("input_file"),
             py::arg("output_folder"), "Constructs a SingleFileDecompressor.");

    // DirectoryDecompressor class binding
    py::class_<atom::io::async::DirectoryDecompressor,
               atom::io::async::BaseDecompressor>(
        m, "DirectoryDecompressor",
        R"(Async decompressor for directories.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the directory containing compressed files
    output_folder: Path to the output folder

Examples:
    >>> import asio
    >>> from atom.io.compress import DirectoryDecompressor
    >>> io_context = asio.io_context()
    >>> decompressor = DirectoryDecompressor(io_context, "compressed/", "out/")
    >>> decompressor.start()
    >>> io_context.run()
)")
        .def(py::init<asio::io_context&, const fs::path&, const fs::path&>(),
             py::arg("io_context"), py::arg("input_dir"),
             py::arg("output_folder"), "Constructs a DirectoryDecompressor.");

    // =========================================================================
    // Async ZIP Operation Classes
    // =========================================================================

    py::class_<atom::io::async::ZipOperation>(m, "ZipOperation",
                                              "Base class for async ZIP ops")
        .def("start", &atom::io::async::ZipOperation::start,
             "Starts the ZIP operation");

    py::class_<atom::io::async::ListFilesInZip, atom::io::async::ZipOperation>(
        m, "ListFilesInZip",
        R"(Async operation to list files in a ZIP archive.

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
             "Gets the list of files in the ZIP archive");

    py::class_<atom::io::async::FileExistsInZip, atom::io::async::ZipOperation>(
        m, "FileExistsInZip",
        R"(Async operation to check if a file exists in a ZIP archive.

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
             "Returns True if the file was found in the archive");

    py::class_<atom::io::async::RemoveFileFromZip,
               atom::io::async::ZipOperation>(
        m, "RemoveFileFromZip",
        R"(Async operation to remove a file from a ZIP archive.

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
             "Returns True if file was successfully removed");

    py::class_<atom::io::async::GetZipFileSize, atom::io::async::ZipOperation>(
        m, "GetZipFileSize",
        R"(Async operation to get the size of a ZIP file.

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
             "Gets the size of the ZIP file in bytes");

    // =========================================================================
    // Async Convenience Functions
    // =========================================================================

    m.def(
        "async_compress_file",
        [](asio::io_context& io_context, const std::string& input_file,
           const std::string& output_file) {
            auto compressor =
                std::make_unique<atom::io::async::SingleFileCompressor>(
                    io_context, input_file, output_file);
            compressor->start();
            return compressor;
        },
        py::arg("io_context"), py::arg("input_file"), py::arg("output_file"),
        R"(Convenience function to async compress a single file.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the input file
    output_file: Path to the output compressed file

Returns:
    A started SingleFileCompressor object

Examples:
    >>> compressor = async_compress_file(io_context, "data.txt", "data.gz")
    >>> io_context.run()
)");

    m.def(
        "async_compress_directory",
        [](asio::io_context& io_context, const std::string& input_dir,
           const std::string& output_file) {
            auto compressor =
                std::make_unique<atom::io::async::DirectoryCompressor>(
                    io_context, input_dir, output_file);
            compressor->start();
            return compressor;
        },
        py::arg("io_context"), py::arg("input_dir"), py::arg("output_file"),
        R"(Convenience function to async compress a directory.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the directory
    output_file: Path to the output compressed file

Returns:
    A started DirectoryCompressor object
)");

    m.def(
        "async_decompress_file",
        [](asio::io_context& io_context, const std::string& input_file,
           const std::string& output_folder) {
            auto decompressor =
                std::make_unique<atom::io::async::SingleFileDecompressor>(
                    io_context, input_file, output_folder);
            decompressor->start();
            return decompressor;
        },
        py::arg("io_context"), py::arg("input_file"), py::arg("output_folder"),
        R"(Convenience function to async decompress a file.

Args:
    io_context: The ASIO I/O context
    input_file: Path to the compressed file
    output_folder: Path to the output folder

Returns:
    A started SingleFileDecompressor object
)");

    m.def(
        "async_decompress_directory",
        [](asio::io_context& io_context, const std::string& input_dir,
           const std::string& output_folder) {
            auto decompressor =
                std::make_unique<atom::io::async::DirectoryDecompressor>(
                    io_context, input_dir, output_folder);
            decompressor->start();
            return decompressor;
        },
        py::arg("io_context"), py::arg("input_dir"), py::arg("output_folder"),
        R"(Convenience function to async decompress files in a directory.

Args:
    io_context: The ASIO I/O context
    input_dir: Path to the directory with compressed files
    output_folder: Path to the output folder

Returns:
    A started DirectoryDecompressor object
)");

    m.def(
        "async_list_files_in_zip",
        [](asio::io_context& io_context, const std::string& zip_file) {
            auto lister = std::make_unique<atom::io::async::ListFilesInZip>(
                io_context, zip_file);
            lister->start();
            auto result = lister->getFileList();
            return result;
        },
        py::arg("io_context"), py::arg("zip_file"),
        R"(Async convenience function to list files in a ZIP.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Returns:
    List of filenames in the archive
)");

    m.def(
        "async_file_exists_in_zip",
        [](asio::io_context& io_context, const std::string& zip_file,
           const std::string& file_name) {
            auto checker = std::make_unique<atom::io::async::FileExistsInZip>(
                io_context, zip_file, file_name);
            checker->start();
            return checker->found();
        },
        py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"),
        R"(Async convenience to check if file exists in a ZIP.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to check

Returns:
    True if the file exists, False otherwise
)");

    m.def(
        "async_get_zip_file_size",
        [](asio::io_context& io_context, const std::string& zip_file) {
            auto getter =
                std::make_unique<atom::io::async::GetZipFileSize>(io_context,
                                                                  zip_file);
            getter->start();
            return getter->getSizeValue();
        },
        py::arg("io_context"), py::arg("zip_file"),
        R"(Async convenience to get ZIP file size.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file

Returns:
    Size of the ZIP file in bytes
)");

    m.def(
        "async_remove_file_from_zip",
        [](asio::io_context& io_context, const std::string& zip_file,
           const std::string& file_name) {
            auto remover = std::make_unique<atom::io::async::RemoveFileFromZip>(
                io_context, zip_file, file_name);
            remover->start();
            return remover->isSuccessful();
        },
        py::arg("io_context"), py::arg("zip_file"), py::arg("file_name"),
        R"(Async convenience to remove a file from a ZIP.

Args:
    io_context: The ASIO I/O context
    zip_file: Path to the ZIP file
    file_name: Name of the file to remove

Returns:
    True if removed successfully, False otherwise
)");

    // Module metadata
    m.attr("__version__") = "2.0.0";
    m.attr("DEFAULT_COMPRESSION_LEVEL") = 6;
    m.attr("MAX_COMPRESSION_LEVEL") = 9;
    m.attr("NO_COMPRESSION") = 0;
}
