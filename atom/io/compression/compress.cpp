/*
 * compress.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Compressor using ZLib and MiniZip-ng

**************************************************/

#include "compress.hpp"

#include <minizip-ng/mz.h>
#include <minizip-ng/mz_compat.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>
#include <zlib.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/format.h>
#endif

#include <spdlog/spdlog.h>
#include "atom/containers/high_performance.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
constexpr size_t DEFAULT_CHUNK_SIZE = 16384;

// Helper function to calculate compression ratio
inline double calculateCompressionRatio(size_t compressed_size,
                                        size_t original_size) {
    if (original_size > 0) {
        return static_cast<double>(compressed_size) /
               static_cast<double>(original_size);
    }
    return 0.0;
}

// Helper function to get compression ratio percentage for display
inline double getCompressionPercentage(double compression_ratio) {
    return (compression_ratio > 0.0) ? (1.0 - compression_ratio) * 100.0 : 0.0;
}

class ZStreamGuard {
    z_stream stream_;
    bool initialized_{false};
    bool is_inflate_{false};

public:
    ZStreamGuard() noexcept {
        stream_.zalloc = Z_NULL;
        stream_.zfree = Z_NULL;
        stream_.opaque = Z_NULL;
    }

    ~ZStreamGuard() {
        if (initialized_) {
            if (is_inflate_) {
                inflateEnd(&stream_);
            } else {
                deflateEnd(&stream_);
            }
        }
    }

    // Initialize for compression
    bool initDeflate(int level, int windowBits = 7) {
        int ret = deflateInit2(&stream_, level, Z_DEFLATED, windowBits, 8,
                               Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            initialized_ = true;
            is_inflate_ = false;
            return true;
        }
        return false;
    }

    // Initialize for decompression
    bool initInflate(int windowBits = 7) {
        int ret = inflateInit2(&stream_, windowBits);
        if (ret == Z_OK) {
            initialized_ = true;
            is_inflate_ = true;
            return true;
        }
        return false;
    }

    // End the stream explicitly if needed before destruction
    void endStream() {
        if (initialized_) {
            if (is_inflate_) {
                inflateEnd(&stream_);
            } else {
                deflateEnd(&stream_);
            }
            initialized_ = false;
        }
    }

    z_stream* get() { return &stream_; }
    const z_stream* get() const { return &stream_; }
};

// Error handling helper function
// Return std::string as it's simple and doesn't need high performance here
std::string getZlibErrorMessage(int error_code) {
    switch (error_code) {
        case Z_ERRNO:
            return "File operation error";
        case Z_STREAM_ERROR:
            return "Stream state inconsistent";
        case Z_DATA_ERROR:
            return "Input data corrupted";
        case Z_MEM_ERROR:
            return "Out of memory";
        case Z_BUF_ERROR:
            return "Buffer error";
        case Z_VERSION_ERROR:
            return "zlib version incompatible";
        default:
            // Use zError if available and error_code is valid zlib error
            if (error_code < 0) {
                const char* msg = zError(error_code);
                if (msg)
                    return msg;
            }
            return "Unknown error";
    }
}

// Progress info (no changes needed)
struct ProgressInfo {
    std::atomic<size_t> bytes_processed{0};
    std::atomic<size_t> total_bytes{0};
    std::atomic<bool> cancelled{false};
};

// ZIP file closer (no changes needed)
struct ZipCloser {
    void operator()(zipFile file) const {
        if (file) {
            zipClose(file, nullptr);
        }
    }
};

// Unzip file closer (no changes needed)
struct UnzipCloser {
    void operator()(unzFile file) const {
        if (file) {
            unzClose(file);
        }
    }
};

}  // anonymous namespace

namespace atom::io {

// Use type aliases from high_performance.hpp within the implementation
using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

CompressionResult compressFile(std::string_view file_path_sv,
                               std::string_view output_folder_sv,
                               const CompressionOptions& options) {
    CompressionResult result;
    try {
        if (file_path_sv.empty() || output_folder_sv.empty()) {
            result.error_message =
                "Empty file path or output folder";  // Assign std::string
                                                     // literal
            return result;
        }

        // Use fs::path directly with string_view if supported, else convert
        fs::path input_path(file_path_sv);
        if (!fs::exists(input_path)) {
            result.error_message = "Input file does not exist";
            return result;
        }

        fs::path output_dir(output_folder_sv);
        if (!fs::exists(output_dir)) {
            if (!fs::create_directories(output_dir)) {
                result.error_message = "Failed to create output directory";
                return result;
            }
        }

        // Construct output path using filesystem operations
        fs::path output_path = output_dir / input_path.filename();
        output_path += ".gz";  // Append extension

        if (options.create_backup && fs::exists(output_path)) {
            fs::path backup_path = output_path;
            backup_path += ".bak";  // Append backup extension
            fs::rename(output_path, backup_path);
        }

        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            result.error_message = "Failed to open input file";
            return result;
        }

        input.seekg(0, std::ios::end);
        result.original_size = input.tellg();
        input.seekg(0, std::ios::beg);

        // Use fs::path::string() or u8string() for C APIs
        gzFile out = gzopen(output_path.string().c_str(), "wb");
        if (!out) {
            result.error_message = "Failed to create output file";
            return result;
        }

        gzsetparams(out, options.level, Z_DEFAULT_STRATEGY);

        // Use smart pointer for gzFile
        std::unique_ptr<gzFile_s, decltype(&gzclose)> out_guard(out, gzclose);

        // Use Vector<char> for buffer
        Vector<char> buffer(options.chunk_size);

        while (input.read(buffer.data(), buffer.size())) {
            if (gzwrite(out, buffer.data(),
                        static_cast<unsigned>(input.gcount())) <= 0) {
                result.error_message = "Failed to write compressed data";
                // out_guard will close the file
                return result;
            }
        }
        // Handle the last chunk if read size was less than buffer size but > 0
        if (input.gcount() > 0) {
            if (gzwrite(out, buffer.data(),
                        static_cast<unsigned>(input.gcount())) <= 0) {
                result.error_message = "Failed to write final compressed data";
                // out_guard will close the file
                return result;
            }
        }

        // Close explicitly before getting size (though guard handles errors)
        out_guard.reset();  // Closes the file

        result.compressed_size = fs::file_size(output_path);
        if (result.original_size > 0) {
            result.compression_ratio =
                static_cast<double>(result.compressed_size) /
                static_cast<double>(result.original_size);
        } else {
            result.compression_ratio = 0.0;
        }
        result.success = true;

        spdlog::info(
            "{} -> {} (ratio: {:.2f}%)", input_path.string(),
            output_path.string(),
            (result.original_size > 0 ? (1.0 - result.compression_ratio) * 100
                                      : 0.0));

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception during compression: ") + e.what();
        spdlog::error("{}", result.error_message);
    }

    return result;
}

CompressionResult decompressFile(
    std::string_view file_path_sv, std::string_view output_folder_sv,
    [[maybe_unused]] const DecompressionOptions&
        options) {  // Mark options as potentially unused if diagnostic persists
    CompressionResult result;
    try {
        if (file_path_sv.empty() || output_folder_sv.empty()) {
            result.error_message = "Empty file path or output folder";
            return result;
        }

        fs::path input_path(file_path_sv);
        if (!fs::exists(input_path)) {
            result.error_message = "Input file does not exist";
            return result;
        }

        // Create output directory
        fs::path output_dir(output_folder_sv);
        if (!fs::exists(output_dir)) {
            if (!fs::create_directories(output_dir)) {
                result.error_message = "Failed to create output directory";
                return result;
            }
        }

        fs::path output_path = output_dir / input_path.stem();

        // Get compressed file size
        result.compressed_size = fs::file_size(input_path);

        // Open compressed file
        gzFile in = gzopen(input_path.string().c_str(), "rb");
        if (!in) {
            result.error_message = "Failed to open compressed file";
            return result;
        }

        // Use smart pointer for automatic closing
        std::unique_ptr<gzFile_s, decltype(&gzclose)> in_guard(in, gzclose);

        // Open output file
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            result.error_message = "Failed to create output file";
            return result;
        }

        // Set buffer using Vector<char>
        Vector<char> buffer(options.chunk_size);  // options is used here

        // Decompress data
        int bytes_read;
        size_t total_bytes = 0;
        while ((bytes_read = gzread(in, buffer.data(),
                                    static_cast<unsigned>(buffer.size()))) >
               0) {
            output.write(buffer.data(), bytes_read);
            total_bytes += bytes_read;
        }

        // Check for errors during read
        if (bytes_read < 0) {
            int err_no = 0;
            const char* err_msg = gzerror(in, &err_no);
            result.error_message =
                String("Error during decompression: ") +
                (err_no == Z_ERRNO ? strerror(errno) : err_msg);
            // Files are closed by guards/destructors
            return result;
        }

        // Close files explicitly (optional, as RAII handles it)
        output.close();
        in_guard.reset();  // Closes gzFile

        result.original_size = total_bytes;
        if (result.original_size > 0) {
            result.compression_ratio =
                static_cast<double>(result.compressed_size) /
                static_cast<double>(result.original_size);
        } else {
            result.compression_ratio = 0.0;
        }
        result.success = true;

        spdlog::info(
            "Successfully decompressed {} -> {} (ratio: {:.2f}%)",
            input_path.string(), output_path.string(),
            (result.original_size > 0 ? (1.0 - result.compression_ratio) * 100
                                      : 0.0));

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception during decompression: ") + e.what();
        spdlog::error("{}", result.error_message);
    }

    return result;
}

CompressionResult compressFolder(std::string_view folder_path_sv,
                                 std::string_view output_path_sv,
                                 const CompressionOptions& options) {
    CompressionResult result;
    zipFile zip_file_handle = nullptr;  // Use raw handle for RAII management
    try {
        // Input validation
        fs::path input_dir(folder_path_sv);
        if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
            result.error_message = "Invalid input directory";
            return result;
        }

        // Prepare output file path
        fs::path zip_fs_path(output_path_sv);
        // Ensure the output path has a .zip extension
        if (zip_fs_path.extension() != ".zip") {
            zip_fs_path.replace_extension(".zip");
        }

        // Create ZIP file using minizip-ng
        // Use APPEND_STATUS_CREATE to create a new file
        zip_file_handle =
            zipOpen64(zip_fs_path.string().c_str(), APPEND_STATUS_CREATE);
        if (!zip_file_handle) {
            result.error_message = "Failed to create ZIP file";
            return result;
        }
        // RAII guard for the zip file handle
        std::unique_ptr<void, ZipCloser> zip_file(zip_file_handle);

        // Collect all files recursively (keep using std::vector<fs::path>)
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
            if (fs::is_regular_file(entry.path())) {
                files.push_back(entry.path());
            } else if (fs::is_directory(entry.path())) {
                // Optionally add directory entries to the zip
                // fs::path rel_path = fs::relative(entry.path(), input_dir);
                // String entry_name = String(rel_path.generic_string()) + "/";
                // zip_fileinfo zi = {};
                // // Set timestamp for directory if needed
                // zipOpenNewFileInZip(zip_file.get(), entry_name.c_str(), &zi,
                // nullptr, 0, nullptr, 0, nullptr, 0, 0);
                // zipCloseFileInZip(zip_file.get());
            }
        }

        result.original_size = 0;
        result.compressed_size = 0;  // Will be calculated at the end

        // Buffer for reading files
        Vector<char> buffer(options.chunk_size);

        // Process files (sequential implementation for simplicity first)
        for (const auto& file_path : files) {
            // Calculate relative path for storing in ZIP
            fs::path rel_path = fs::relative(file_path, input_dir);
            // Use generic_string for cross-platform compatibility inside ZIP
            String entry_name(rel_path.generic_string());

            // Get file modification time
            zip_fileinfo zi = {};
            auto ftime = fs::last_write_time(file_path);
            try {
                auto sctp = std::chrono::time_point_cast<
                    std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() +
                    std::chrono::system_clock::now());
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                std::tm* tm_local = std::localtime(&tt);
                if (tm_local) {
                    zi.tmz_date.tm_year = tm_local->tm_year;
                    zi.tmz_date.tm_mon = tm_local->tm_mon;
                    zi.tmz_date.tm_mday = tm_local->tm_mday;
                    zi.tmz_date.tm_hour = tm_local->tm_hour;
                    zi.tmz_date.tm_min = tm_local->tm_min;
                    zi.tmz_date.tm_sec = tm_local->tm_sec;
                }
            } catch (...) {
                spdlog::warn("Could not get valid timestamp for file: {}",
                             file_path.string());
            }

            // Add file entry to ZIP
            // Use password if provided
            const char* password_cstr =
                options.password.empty() ? nullptr : options.password.c_str();
            int zip64 = 1;  // Enable Zip64 for large files

            // Open a new file entry in the ZIP archive with appropriate
            // compression settings
            int result_code = zipOpenNewFileInZip3_64(
                zip_file.get(),      // ZIP file handle
                entry_name.c_str(),  // Entry name within ZIP
                &zi,                 // File information (timestamps, etc.)
                nullptr, 0,          // No local extra field
                nullptr, 0,          // No global extra field
                nullptr,             // No comment
                Z_DEFLATED,          // Use DEFLATE compression method
                options.level,       // Compression level from options
                0,                   // Raw flag (0 = not raw)
                -MAX_WBITS,  // Window bits for zlib (negative for raw deflate)
                DEF_MEM_LEVEL,       // Memory level for zlib
                Z_DEFAULT_STRATEGY,  // Compression strategy
                password_cstr,       // Password (null if none)
                0,                   // CRC value (0 = auto-compute)
                zip64                // Enable ZIP64 extensions if needed
            );

            if (result_code != ZIP_OK) {
                result.error_message =
                    String("Failed to add file to ZIP: ") + entry_name;
                return result;  // zip_file guard will close the main zip
            }

            // Open input file and write its content to ZIP
            std::ifstream file(file_path, std::ios::binary);
            if (!file) {
                zipCloseFileInZip(zip_file.get());  // Close the entry in zip
                result.error_message = String("Failed to open input file: ") +
                                       String(file_path.string());
                return result;  // zip_file guard will close the main zip
            }

            size_t file_original_size = 0;
            while (file.read(buffer.data(), buffer.size())) {
                size_t read_count = static_cast<size_t>(file.gcount());
                if (zipWriteInFileInZip(
                        zip_file.get(), buffer.data(),
                        static_cast<unsigned int>(read_count)) != ZIP_OK) {
                    zipCloseFileInZip(
                        zip_file.get());  // Close the current file entry before
                                          // returning
                    result.error_message =
                        String("Failed to write file data to ZIP: ") +
                        entry_name;
                    return result;  // zip_file guard will close the main zip
                }
                file_original_size += read_count;
            }
            // Handle last chunk
            size_t read_count = static_cast<size_t>(file.gcount());
            if (read_count > 0) {
                if (zipWriteInFileInZip(
                        zip_file.get(), buffer.data(),
                        static_cast<unsigned int>(read_count)) != ZIP_OK) {
                    zipCloseFileInZip(
                        zip_file.get());  // Close the current file entry before
                                          // returning
                    result.error_message =
                        String("Failed to write final file data to ZIP: ") +
                        entry_name;
                    return result;  // zip_file guard will close the main zip
                }
                file_original_size += read_count;
            }

            // Close the current file entry in ZIP
            if (zipCloseFileInZip(zip_file.get()) != ZIP_OK) {
                result.error_message =
                    String("Failed to close file in ZIP: ") + entry_name;
                return result;  // zip_file guard will close the main zip
            }

            result.original_size +=
                file_original_size;  // Accumulate original size
        }

        // Close the ZIP file itself (handled by ZipCloser)
        zip_file.reset();

        // Get compressed size after closing
        result.compressed_size = fs::file_size(zip_fs_path);
        if (result.original_size > 0) {
            result.compression_ratio =
                static_cast<double>(result.compressed_size) /
                static_cast<double>(result.original_size);
        } else {
            result.compression_ratio = 0.0;
        }
        result.success = true;

        spdlog::info(
            "Successfully compressed folder {} -> {} (ratio: {:.2f}%)",
            input_dir.string(), zip_fs_path.string(),
            (result.original_size > 0 ? (1.0 - result.compression_ratio) * 100
                                      : 0.0));

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception during folder compression: ") + e.what();
        spdlog::error("{}", result.error_message);
    }

    return result;
}

CompressionResult extractZip(std::string_view zip_path_sv,
                             std::string_view output_folder_sv,
                             const DecompressionOptions& options) {
    CompressionResult result;
    unzFile unz = nullptr;  // Use raw handle for RAII
    try {
        // Input validation
        if (zip_path_sv.empty() || output_folder_sv.empty()) {
            result.error_message = "Empty ZIP path or output folder";
            return result;
        }

        fs::path zip_fs_path(zip_path_sv);
        if (!fs::exists(zip_fs_path)) {
            result.error_message = "ZIP file does not exist";
            return result;
        }

        // Create output directory
        fs::path output_dir(output_folder_sv);
        if (!fs::exists(output_dir)) {
            if (!fs::create_directories(output_dir)) {
                result.error_message = "Failed to create output directory";
                return result;
            }
        }

        // Open ZIP file
        unz = unzOpen64(zip_fs_path.string().c_str());
        if (!unz) {
            result.error_message = "Failed to open ZIP file";
            return result;
        }
        // RAII guard for the main unzFile handle
        std::unique_ptr<void, UnzipCloser> unz_guard(unz);

        result.compressed_size = fs::file_size(zip_fs_path);
        result.original_size = 0;

        // Get global ZIP file info
        unz_global_info64 gi;
        if (unzGetGlobalInfo64(unz, &gi) != UNZ_OK) {
            result.error_message = "Failed to get ZIP file info";
            return result;  // unz_guard handles closing
        }

        // Go to the first file in the archive
        if (unzGoToFirstFile(unz) != UNZ_OK) {
            // Handle case where zip might be empty but not necessarily an error
            if (gi.number_entry == 0) {
                result.success = true;
                spdlog::info("ZIP file is empty: {}", zip_fs_path.string());
                return result;  // unz_guard handles closing
            }
            result.error_message = "Failed to go to first file in ZIP";
            return result;  // unz_guard handles closing
        }

        // Buffer for extraction
        Vector<char> buffer(options.chunk_size);

        // Loop through all files in the ZIP archive
        do {
            // Get current file info
            unz_file_info64 file_info;
            char filename_c[512];  // Use C-style buffer for minizip API
            if (unzGetCurrentFileInfo64(unz, &file_info, filename_c,
                                        sizeof(filename_c), nullptr, 0, nullptr,
                                        0) != UNZ_OK) {
                result.error_message = "Failed to get file info from ZIP";
                return result;  // unz_guard handles closing
            }

            String filename(filename_c);  // Convert to String
            fs::path target_path =
                output_dir / fs::path(filename);  // Construct path

            // Check if it's a directory entry (ends with '/')
            if (!filename.empty() &&
                (filename.back() == '/' ||
                 filename.back() == '\\')) {  // Check both separators
                fs::create_directories(target_path);
                continue;  // Skip to next entry
            }

            // Ensure parent directory exists for the file
            if (target_path.has_parent_path()) {
                fs::create_directories(target_path.parent_path());
            }

            // Open the current file within the ZIP archive
            const char* password_cstr =
                options.password.empty() ? nullptr : options.password.c_str();
            if (unzOpenCurrentFilePassword(unz, password_cstr) != UNZ_OK) {
                result.error_message =
                    String("Failed to open file in ZIP: ") + filename;
                return result;  // unz_guard handles closing
            }
            // No separate handle for current file, managed by 'unz' state

            // Open the output file for writing
            std::ofstream outfile(target_path, std::ios::binary);
            if (!outfile) {
                unzCloseCurrentFile(
                    unz);  // Manually close current file before returning
                result.error_message =
                    String("Failed to create output file: ") +
                    String(target_path.string());
                return result;  // unz_guard handles closing main zip
            }

            // Read from ZIP and write to output file
            int read_error = UNZ_OK;
            do {
                read_error = unzReadCurrentFile(
                    unz, buffer.data(), static_cast<unsigned>(buffer.size()));
                if (read_error < 0) {
                    outfile.close();  // Close output file on error
                    unzCloseCurrentFile(
                        unz);  // Manually close current file before returning
                    result.error_message =
                        String("Error reading file from ZIP: ") + filename +
                        " (Error code: " + std::to_string(read_error) + ")";
                    return result;  // unz_guard handles closing main zip
                }

                if (read_error > 0) {
                    outfile.write(buffer.data(), read_error);
                    if (!outfile) {
                        unzCloseCurrentFile(unz);  // Manually close current
                                                   // file before returning
                        result.error_message =
                            String("Error writing to output file: ") +
                            String(target_path.string());
                        return result;  // unz_guard handles closing main zip
                    }
                    result.original_size +=
                        read_error;  // Accumulate original size
                }
            } while (read_error > 0);

            // Close output file
            outfile.close();

            // Close current file in ZIP
            if (unzCloseCurrentFile(unz) != UNZ_OK) {
                spdlog::warn("Failed to close current file in ZIP: {}",
                             filename);
                // Continue to next file? Or treat as error? Let's log and
                // continue for now.
            }

            // Optionally set file modification time based on zip info
            // This requires converting unz_file_info64 time to
            // fs::file_time_type

            spdlog::info("Extracted: {}", filename);

        } while (unzGoToNextFile(unz) == UNZ_OK);

        // Check if loop finished because of end-of-archive or an error
        // The loop condition handles reaching the end correctly. Errors are
        // handled inside.

        result.success = true;
        if (result.original_size > 0) {
            result.compression_ratio =
                static_cast<double>(result.compressed_size) /
                static_cast<double>(result.original_size);
        } else {
            result.compression_ratio = 0.0;
        }

        spdlog::info("Successfully extracted {} files from {} -> {}",
                     gi.number_entry, zip_fs_path.string(),
                     output_dir.string());

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception during extraction: ") + e.what();
        spdlog::error("{}", result.error_message);
        // unz_guard handles closing if unz was opened
    }

    return result;
}

// createZip implementation would be similar to compressFolder,
// but potentially handling single files as input too.
CompressionResult createZip(std::string_view source_path_sv,
                            std::string_view zip_path_sv,
                            const CompressionOptions& options) {
    fs::path source_path(source_path_sv);
    if (fs::is_directory(source_path)) {
        return compressFolder(source_path_sv, zip_path_sv, options);
    } else if (fs::is_regular_file(source_path)) {
        // Implementation to zip a single file
        CompressionResult result;
        zipFile zip_file_handle = nullptr;  // Use raw handle for RAII
        try {
            fs::path zip_fs_path(zip_path_sv);
            if (zip_fs_path.extension() != ".zip") {
                zip_fs_path.replace_extension(".zip");
            }

            zip_file_handle =
                zipOpen64(zip_fs_path.string().c_str(), APPEND_STATUS_CREATE);
            if (!zip_file_handle) {
                result.error_message = "Failed to create ZIP file";
                return result;
            }
            std::unique_ptr<void, ZipCloser> zip_file(zip_file_handle);

            String entry_name(
                source_path.filename().string());  // Use filename as entry name

            zip_fileinfo zi = {};
            // Set timestamp... (similar to compressFolder)
            auto ftime = fs::last_write_time(source_path);
            try {
                auto sctp = std::chrono::time_point_cast<
                    std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() +
                    std::chrono::system_clock::now());
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                std::tm* tm_local = std::localtime(&tt);
                if (tm_local) {
                    zi.tmz_date.tm_year = tm_local->tm_year;
                    zi.tmz_date.tm_mon = tm_local->tm_mon;
                    zi.tmz_date.tm_mday = tm_local->tm_mday;
                    zi.tmz_date.tm_hour = tm_local->tm_hour;
                    zi.tmz_date.tm_min = tm_local->tm_min;
                    zi.tmz_date.tm_sec = tm_local->tm_sec;
                }
            } catch (...) {
                spdlog::warn("Could not get valid timestamp for file: {}",
                             source_path.string());
            }

            const char* password_cstr =
                options.password.empty() ? nullptr : options.password.c_str();
            int zip64 = 1;

            // Cast zip_file.get() to zipFile explicitly if needed
            int result_code = zipOpenNewFileInZip3_64(
                zip_file.get(),      // ZIP file handle
                entry_name.c_str(),  // Entry name within ZIP
                &zi,                 // File information (timestamps, etc.)
                nullptr, 0,          // No local extra field
                nullptr, 0,          // No global extra field
                nullptr,             // No comment
                Z_DEFLATED,          // Use DEFLATE compression method
                options.level,       // Compression level from options
                0,                   // Raw flag (0 = not raw)
                -MAX_WBITS,  // Window bits for zlib (negative for raw deflate)
                DEF_MEM_LEVEL,       // Memory level for zlib
                Z_DEFAULT_STRATEGY,  // Compression strategy
                password_cstr,       // Password (null if none)
                0,                   // CRC value (0 = auto-compute)
                zip64                // Enable ZIP64 extensions if needed
            );

            if (result_code != ZIP_OK) {
                result.error_message =
                    String("Failed to add file to ZIP: ") + entry_name;
                return result;  // zip_file guard handles closing
            }

            std::ifstream file(source_path, std::ios::binary);
            if (!file) {
                zipCloseFileInZip(zip_file.get());  // Close current file entry
                result.error_message = String("Failed to open input file: ") +
                                       String(source_path.string());
                return result;  // zip_file guard handles closing
            }

            Vector<char> buffer(options.chunk_size);
            result.original_size = 0;
            while (file.read(buffer.data(), buffer.size())) {
                size_t read_count = static_cast<size_t>(file.gcount());
                if (zipWriteInFileInZip(
                        zip_file.get(), buffer.data(),
                        static_cast<unsigned int>(read_count)) != ZIP_OK) {
                    zipCloseFileInZip(
                        zip_file.get());  // Close current file entry
                    result.error_message =
                        String("Failed to write file data to ZIP: ") +
                        entry_name;
                    return result;  // zip_file guard handles closing
                }
                result.original_size += read_count;
            }
            size_t read_count = static_cast<size_t>(file.gcount());
            if (read_count > 0) {
                if (zipWriteInFileInZip(
                        zip_file.get(), buffer.data(),
                        static_cast<unsigned int>(read_count)) != ZIP_OK) {
                    zipCloseFileInZip(
                        zip_file.get());  // Close current file entry
                    result.error_message =
                        String("Failed to write final file data to ZIP: ") +
                        entry_name;
                    return result;  // zip_file guard handles closing
                }
                result.original_size += read_count;
            }

            if (zipCloseFileInZip(zip_file.get()) != ZIP_OK) {
                result.error_message =
                    String("Failed to close file in ZIP: ") + entry_name;
                return result;  // zip_file guard handles closing
            }

            zip_file.reset();  // Close zip file

            result.compressed_size = fs::file_size(zip_fs_path);
            if (result.original_size > 0) {
                result.compression_ratio =
                    static_cast<double>(result.compressed_size) /
                    static_cast<double>(result.original_size);
            } else {
                result.compression_ratio = 0.0;
            }
            result.success = true;
            spdlog::info("Successfully created ZIP {} from file {}",
                         zip_fs_path.string(), source_path.string());

        } catch (const std::exception& e) {
            result.error_message =
                String("Exception during single file zip creation: ") +
                e.what();
            spdlog::error("{}", result.error_message);
            // zip_file unique_ptr handles closing if zip_file_handle was opened
        }
        return result;
    } else {
        CompressionResult result;
        result.error_message = "Source path is neither a file nor a directory";
        return result;
    }
}

Vector<ZipFileInfo> listZipContents(std::string_view zip_path_sv) {
    Vector<ZipFileInfo> result_vec;
    unzFile unz = nullptr;
    try {
        fs::path zip_fs_path(zip_path_sv);
        // Open ZIP file
        unz = unzOpen64(zip_fs_path.string().c_str());
        if (!unz) {
            spdlog::error("Failed to open ZIP file: {}", zip_fs_path.string());
            return result_vec;
        }
        std::unique_ptr<void, UnzipCloser> unz_guard(unz);

        // Get global info
        unz_global_info64 gi;
        if (unzGetGlobalInfo64(unz, &gi) != UNZ_OK) {
            spdlog::error("Failed to get ZIP file info for {}",
                          zip_fs_path.string());
            return result_vec;
        }

        // Preallocate space if possible (Vector might have reserve)
        // result_vec.reserve(gi.number_entry);

        // Go to first file
        if (unzGoToFirstFile(unz) != UNZ_OK) {
            if (gi.number_entry == 0)
                return result_vec;  // Empty zip is ok
            spdlog::error("Failed to go to first file in ZIP: {}",
                          zip_fs_path.string());
            return result_vec;  // unz_guard handles closing
        }

        // Iterate through files
        do {
            unz_file_info64 file_info;
            char filename_c[512];

            if (unzGetCurrentFileInfo64(unz, &file_info, filename_c,
                                        sizeof(filename_c), nullptr, 0, nullptr,
                                        0) != UNZ_OK) {
                spdlog::error("Failed to get file info in ZIP: {}",
                              zip_fs_path.string());
                continue;  // Skip this entry
            }

            ZipFileInfo info;
            info.name = String(filename_c);  // Convert C string to String
            info.size = file_info.uncompressed_size;
            info.compressed_size = file_info.compressed_size;
            info.is_directory =
                (!info.name.empty() &&
                 (info.name.back() == '/' ||
                  info.name.back() == '\\'));  // Check both separators
            info.is_encrypted = (file_info.flag & 1) != 0;
            info.crc = file_info.crc;

            // Format datetime string
            char datetime_c[32];
            // Use tm_year + 1900 if tm_year is years since 1900
            // Use tm_mon + 1 because tm_mon is 0-11
            std::snprintf(
                datetime_c, sizeof(datetime_c), "%04d-%02d-%02d %02d:%02d:%02d",
                file_info.tmu_date
                    .tm_year,  // Assuming already 4 digits or adjust
                file_info.tmu_date.tm_mon + 1, file_info.tmu_date.tm_mday,
                file_info.tmu_date.tm_hour, file_info.tmu_date.tm_min,
                file_info.tmu_date.tm_sec);
            info.datetime = String(datetime_c);  // Convert C string to String

            result_vec.emplace_back(std::move(info));  // Add to Vector

        } while (unzGoToNextFile(unz) == UNZ_OK);

        spdlog::info("Listed {} files in ZIP: {}", result_vec.size(),
                     zip_fs_path.string());

    } catch (const std::exception& e) {
        spdlog::error("Exception in listZipContents: {}", e.what());
        result_vec.clear();  // Clear results on exception
        // unz_guard handles closing if unz was opened
    }

    return result_vec;
}

bool fileExistsInZip(std::string_view zip_path_sv,
                     std::string_view file_path_sv) {
    unzFile unz = nullptr;  // Use raw handle for RAII
    try {
        fs::path zip_fs_path(zip_path_sv);
        // Open ZIP file
        unz = unzOpen64(zip_fs_path.string().c_str());
        if (!unz) {
            spdlog::error("Failed to open ZIP file: {}", zip_fs_path.string());
            return false;
        }
        std::unique_ptr<void, UnzipCloser> unz_guard(unz);

        // Locate file (case sensitivity depends on the third argument)
        // Use 0 for case-sensitive (default on Unix-like), 1 for
        // case-insensitive Use 2 for OS default (recommended by minizip-ng
        // docs)
        if (unzLocateFile(unz, file_path_sv.data(), 2) != UNZ_OK) {
            // File not found is not necessarily an error, just return false
            // File not found in ZIP
            return false;  // unz_guard handles closing
        }

        // File found
        return true;  // unz_guard handles closing

    } catch (const std::exception& e) {
        spdlog::error("Exception in fileExistsInZip: {}", e.what());
        // unz_guard handles closing if unz was opened
        return false;
    }
}

// removeFromZip is complex as it requires rewriting the entire archive.
// minizip-ng provides mz_zip_writer functions which might be better suited.
// This implementation rebuilds the zip excluding the target file.
CompressionResult removeFromZip(std::string_view zip_path_sv,
                                std::string_view file_path_to_remove_sv) {
    CompressionResult result;
    fs::path zip_fs_path(zip_path_sv);
    fs::path temp_zip_fs_path = zip_fs_path;
    temp_zip_fs_path += ".tmp";  // Create temp file path

    unzFile src_zip_handle = nullptr;  // Raw handles for RAII
    zipFile dst_zip_handle = nullptr;

    std::unique_ptr<void, UnzipCloser> src_zip_guard(nullptr);
    std::unique_ptr<void, ZipCloser> dst_zip_guard(nullptr);

    try {
        // Input validation
        if (zip_path_sv.empty() || file_path_to_remove_sv.empty()) {
            result.error_message = "Empty ZIP path or file path to remove";
            return result;
        }

        if (!fs::exists(zip_fs_path)) {
            result.error_message = "ZIP file does not exist";
            return result;
        }

        // Open source ZIP for reading
        src_zip_handle = unzOpen64(zip_fs_path.string().c_str());
        if (!src_zip_handle) {
            result.error_message = "Failed to open source ZIP file";
            return result;
        }
        src_zip_guard.reset(src_zip_handle);  // Assign to guard

        // Create destination ZIP for writing
        dst_zip_handle =
            zipOpen64(temp_zip_fs_path.string().c_str(), APPEND_STATUS_CREATE);
        if (!dst_zip_handle) {
            result.error_message = "Failed to create temporary ZIP file";
            return result;  // src_zip_guard handles closing
        }
        dst_zip_guard.reset(dst_zip_handle);  // Assign to guard

        // Get global info from source
        unz_global_info64 gi;
        if (unzGetGlobalInfo64(src_zip_handle, &gi) != UNZ_OK) {
            result.error_message = "Failed to get source ZIP file info";
            return result;  // Guards handle closing
        }

        // Buffer for copying data
        Vector<char> buffer(DEFAULT_CHUNK_SIZE);

        // Go to first file in source
        if (unzGoToFirstFile(src_zip_handle) != UNZ_OK) {
            if (gi.number_entry == 0) {  // Source is empty
                result.success = true;
                return result;  // Guards handle closing
            }
            result.error_message = "Failed to go to first file in source ZIP";
            return result;  // Guards handle closing
        }

        // Iterate and copy files, skipping the one to remove
        do {
            unz_file_info64 file_info;
            char filename_c[512];
            if (unzGetCurrentFileInfo64(src_zip_handle, &file_info, filename_c,
                                        sizeof(filename_c), nullptr, 0, nullptr,
                                        0) != UNZ_OK) {
                result.error_message =
                    "Failed to get file info from source ZIP";
                return result;  // Guards handle closing
            }

            String current_filename(filename_c);

            // Skip the file to be removed
            // Need exact match, consider case sensitivity and path separators
            if (current_filename == String(file_path_to_remove_sv)) {
                spdlog::info("Skipping file for removal: {}",
                             current_filename.c_str());
                continue;
            }

            // Open current file in source ZIP
            // Assume no password needed for reading, or add logic if required
            if (unzOpenCurrentFile(src_zip_handle) != UNZ_OK) {
                result.error_message =
                    String("Failed to open file in source ZIP: ") +
                    current_filename;
                return result;  // Guards handle closing
            }
            // No separate guard needed for current source file

            // Prepare file info for destination ZIP
            zip_fileinfo zi = {};
            zi.tmz_date = file_info.tmu_date;  // Copy timestamp
            // Copy other relevant info if needed (e.g., external attributes)
            zi.external_fa = file_info.external_fa;
            zi.internal_fa = file_info.internal_fa;

            // Add file entry to destination ZIP
            // Preserve compression method and level if possible, or use
            // defaults Need to handle encryption if the original file was
            // encrypted
            const char* password_cstr =
                nullptr;  // Add logic if password needed (e.g., check
                          // file_info.flag & 1)
            int zip64 = (file_info.uncompressed_size >= 0xFFFFFFFF ||
                         file_info.compressed_size >= 0xFFFFFFFF ||
                         file_info.disk_num_start >= 0xFFFF);
            int level =
                Z_DEFAULT_COMPRESSION;  // Default level, minizip doesn't store
                                        // the original level easily
            if (file_info.compression_method == 0)
                level = 0;  // Store method uses level 0

            // Cast dst_zip_handle explicitly if needed
            if (zipOpenNewFileInZip4_64(
                    (zipFile)dst_zip_handle, current_filename.c_str(), &zi,
                    nullptr, 0,  // local extra field
                    nullptr, 0,  // global extra field
                    nullptr,     // comment
                    file_info.compression_method, level,
                    0,                   // raw
                    -MAX_WBITS,          // windowBits (ignored for store)
                    DEF_MEM_LEVEL,       // memLevel (ignored for store)
                    Z_DEFAULT_STRATEGY,  // strategy (ignored for store)
                    password_cstr,
                    0,                  // crcForCrypting (deprecated)
                    file_info.version,  // version made by
                    file_info.flag,     // flag base
                    zip64) != ZIP_OK) {
                unzCloseCurrentFile(
                    src_zip_handle);  // Close current source file
                result.error_message =
                    String("Failed to create file in destination ZIP: ") +
                    current_filename;
                return result;  // Guards handle closing
            }
            // No separate guard needed for current destination file

            // Copy file content
            int read_error = UNZ_OK;
            do {
                read_error =
                    unzReadCurrentFile(src_zip_handle, buffer.data(),
                                       static_cast<unsigned>(buffer.size()));
                if (read_error < 0) {
                    unzCloseCurrentFile(
                        src_zip_handle);  // Close current source file
                    zipCloseFileInZip(
                        dst_zip_handle);  // Close current destination file
                    result.error_message =
                        String("Error reading from source ZIP file: ") +
                        current_filename +
                        " (Error code: " + std::to_string(read_error) + ")";
                    return result;  // Guards handle closing
                }
                if (read_error > 0) {
                    if (zipWriteInFileInZip(
                            dst_zip_handle, buffer.data(),
                            static_cast<unsigned>(read_error)) != ZIP_OK) {
                        unzCloseCurrentFile(
                            src_zip_handle);  // Close current source file
                        zipCloseFileInZip(
                            dst_zip_handle);  // Close current destination file
                        result.error_message =
                            String("Error writing to destination ZIP file: ") +
                            current_filename;
                        return result;  // Guards handle closing
                    }
                }
            } while (read_error > 0);

            // Close current file entries
            if (unzCloseCurrentFile(src_zip_handle) != UNZ_OK) {
                spdlog::warn("Failed to close current file in source ZIP: {}",
                             current_filename.c_str());
                // Continue?
            }
            if (zipCloseFileInZip(dst_zip_handle) != ZIP_OK) {
                result.error_message =
                    String("Failed to close file in destination ZIP: ") +
                    current_filename;
                return result;  // Guards handle closing
            }

        } while (unzGoToNextFile(src_zip_handle) == UNZ_OK);

        // Close ZIP files (guards handle this)
        src_zip_guard.reset();
        dst_zip_guard.reset();

        // Replace original file with temporary file
        fs::remove(zip_fs_path);                    // Remove original
        fs::rename(temp_zip_fs_path, zip_fs_path);  // Rename temp to original

        result.success = true;
        spdlog::info("Successfully removed {} from ZIP file {}",
                     file_path_to_remove_sv.data(), zip_path_sv.data());

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception during file removal from ZIP: ") + e.what();
        spdlog::error("{}", result.error_message.c_str());
        // Guards handle closing
        // Clean up temp file if it exists
        if (fs::exists(temp_zip_fs_path)) {
            fs::remove(temp_zip_fs_path);
        }
    }

    return result;
}

std::optional<size_t> getZipSize(std::string_view zip_path_sv) {
    try {
        if (zip_path_sv.empty()) {
            spdlog::error("Empty ZIP path provided to getZipSize");
            return std::nullopt;
        }

        fs::path zip_fs_path(zip_path_sv);
        if (!fs::exists(zip_fs_path)) {
            // Don't log error here, just return nullopt as file not existing
            // isn't exceptional
            return std::nullopt;
        }

        // Use filesystem::file_size
        std::error_code ec;
        size_t size = fs::file_size(zip_fs_path, ec);
        if (ec) {
            spdlog::error("Failed to get file size for {}: {}",
                          zip_fs_path.string().c_str(), ec.message().c_str());
            return std::nullopt;
        }
        // ZIP file size calculation complete
        return size;

    } catch (const std::exception& e) {
        // Catch potential filesystem exceptions
        spdlog::error("Exception in getZipSize for {}: {}", zip_path_sv.data(),
                      e.what());
        return std::nullopt;
    }
}

// compressFileInSlices needs careful handling of filenames and manifest
CompressionResult compressFileInSlices(std::string_view file_path_sv,
                                       size_t slice_size,
                                       const CompressionOptions& options) {
    CompressionResult result;
    try {
        if (file_path_sv.empty() || slice_size == 0) {
            result.error_message = "Invalid parameters for slicing";
            return result;
        }

        fs::path input_path(file_path_sv);
        if (!fs::exists(input_path) || !fs::is_regular_file(input_path)) {
            result.error_message =
                "Input file does not exist or is not a regular file";
            return result;
        }

        // Get original file size
        std::error_code ec;
        result.original_size = fs::file_size(input_path, ec);
        if (ec) {
            result.error_message =
                String("Failed to get input file size: ") + ec.message();
            return result;
        }
        result.compressed_size = 0;  // Initialize

        // Calculate number of slices
        size_t num_slices =
            (result.original_size == 0)
                ? 0
                : (result.original_size + slice_size - 1) / slice_size;
        if (num_slices == 0 && result.original_size > 0)
            num_slices = 1;  // At least one slice for non-empty file

        // Open input file
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            result.error_message = "Failed to open input file";
            return result;
        }

        // Create JSON manifest data
        json manifest;
        manifest["original_file"] =
            input_path.filename().string();  // Use std::string here
        manifest["original_size"] = result.original_size;
        manifest["slice_size"] = slice_size;
        manifest["num_slices"] = num_slices;
        manifest["compression_level"] = options.level;
        // Use string representation for timestamp
        auto now = std::chrono::system_clock::now();
        // auto now_c = std::chrono::system_clock::to_time_t(now); // Unused
        // variable 'now_c' manifest["created_at"] =
        // std::put_time(std::localtime(&now_c), "%FT%T%z"); // Requires
        // <iomanip>
        manifest["created_at_epoch_ms"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
        Vector<String> slice_filenames;  // Use Vector<String>
        slice_filenames.reserve(num_slices);

        // --- Slice Processing ---
        Vector<char> buffer(slice_size);  // Reusable buffer
        std::atomic<size_t> total_compressed_size_atomic{0};
        std::atomic<bool> error_flag{false};
        String shared_error_message;  // Needs mutex protection if written from
                                      // multiple threads
        std::mutex error_mutex;

        auto compress_slice_task = [&](size_t slice_index, size_t offset,
                                       size_t current_slice_bytes) -> bool {
            try {
// Create slice filename
// Use fmt::format or std::format for safer formatting
#ifdef __cpp_lib_format
                String slice_filename = std::format(
                    "{}.slice_{:04d}.gz", input_path.string(), slice_index);
#else
                String slice_filename = fmt::format(
                    "{}.slice_{:04d}.gz", input_path.string(), slice_index);
#endif

                // Read data for the slice (needs thread-safe read or pre-read)
                // For simplicity, let's read within the task (less efficient
                // for parallel)
                Vector<char> slice_data(current_slice_bytes);
                {  // Scope for input stream
                    std::ifstream slice_input(input_path, std::ios::binary);
                    if (!slice_input) {
                        std::lock_guard lock(error_mutex);
                        shared_error_message =
                            "Failed to open input file for reading slice";
                        return false;
                    }
                    slice_input.seekg(offset);
                    slice_input.read(slice_data.data(), current_slice_bytes);
                    if (!slice_input) {
                        std::lock_guard lock(error_mutex);
                        shared_error_message = "Failed to read data for slice";
                        return false;
                    }
                }  // Input stream closed

                // Compress data
                gzFile out = gzopen(slice_filename.c_str(), "wb");
                if (!out) {
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Failed to create compressed slice file: ") +
                        slice_filename;
                    return false;
                }
                std::unique_ptr<gzFile_s, decltype(&gzclose)> out_guard(
                    out, gzclose);

                gzsetparams(out, options.level, Z_DEFAULT_STRATEGY);

                if (gzwrite(out, slice_data.data(),
                            static_cast<unsigned>(current_slice_bytes)) <= 0) {
                    int err_no = 0;
                    const char* err_msg = gzerror(out, &err_no);
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Failed to write compressed data for slice ") +
                        String(std::to_string(slice_index)) + ": " +
                        (err_no == Z_ERRNO ? strerror(errno) : err_msg);
                    return false;
                }

                out_guard.reset();  // Close file to get size

                // Get compressed size and add to total
                std::error_code slice_ec;
                size_t compressed_slice_size =
                    fs::file_size(fs::path(slice_filename), slice_ec);
                if (slice_ec) {
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Failed to get size of compressed slice: ") +
                        slice_filename;
                    return false;
                }
                total_compressed_size_atomic += compressed_slice_size;

                // Add filename to list (needs mutex if manifest is shared and
                // modified here) It's safer to collect filenames after all
                // tasks complete.

                return true;

            } catch (const std::exception& e) {
                std::lock_guard lock(error_mutex);
                shared_error_message =
                    String("Exception during slice compression: ") + e.what();
                return false;
            }
        };

        // Execute tasks (parallel or sequential)
        if (options.use_parallel && num_slices > 1) {
            std::vector<std::future<bool>> futures;
            futures.reserve(num_slices);
            for (size_t i = 0; i < num_slices; ++i) {
                size_t offset = i * slice_size;
                size_t current_slice_bytes =
                    (i == num_slices - 1) ? (result.original_size - offset)
                                          : slice_size;
                if (current_slice_bytes == 0)
                    continue;  // Skip empty slices if any

                futures.push_back(std::async(std::launch::async,
                                             compress_slice_task, i, offset,
                                             current_slice_bytes));
            }

            // Wait for all tasks and check results
            for (auto& fut : futures) {
                if (!fut.get()) {
                    error_flag = true;
                    // Don't break, let all tasks finish to potentially clean up
                }
            }
        } else {
            // Sequential execution
            for (size_t i = 0; i < num_slices; ++i) {
                size_t offset = i * slice_size;
                size_t current_slice_bytes =
                    (i == num_slices - 1) ? (result.original_size - offset)
                                          : slice_size;
                if (current_slice_bytes == 0)
                    continue;

                if (!compress_slice_task(i, offset, current_slice_bytes)) {
                    error_flag = true;
                    break;  // Stop on first error in sequential mode
                }
            }
        }

        // Check for errors
        if (error_flag) {
            result.error_message = shared_error_message;
            // Consider cleaning up partially created slice files here
            return result;
        }

        // Collect slice filenames (now that tasks are done)
        for (size_t i = 0; i < num_slices; ++i) {
#ifdef __cpp_lib_format
            String slice_filename =
                std::format("{}.slice_{:04d}.gz", input_path.string(), i);
#else
            String slice_filename =
                fmt::format("{}.slice_{:04d}.gz", input_path.string(), i);
#endif
            slice_filenames.push_back(slice_filename);
        }

        // Finalize manifest
        // Convert Vector<String> to json array of strings
        json slice_filenames_json = json::array();
        for (const auto& s_fn : slice_filenames) {
            slice_filenames_json.push_back(
                s_fn);  // Assuming json can take String directly or needs
                        // .c_str()
        }
        manifest["slice_files"] = slice_filenames_json;
        result.compressed_size = total_compressed_size_atomic;
        manifest["compressed_size"] = result.compressed_size;
        if (result.original_size > 0) {
            result.compression_ratio =
                static_cast<double>(result.compressed_size) /
                static_cast<double>(result.original_size);
            manifest["compression_ratio"] = result.compression_ratio;
        } else {
            result.compression_ratio = 0.0;
            manifest["compression_ratio"] = 0.0;
        }

        // Write manifest file
        fs::path manifest_path = input_path;
        manifest_path += ".manifest.json";
        std::ofstream manifest_file(manifest_path);
        if (!manifest_file) {
            result.error_message = "Failed to create manifest file";
            // Cleanup slices?
            return result;
        }
        manifest_file << manifest.dump(4);  // Pretty print JSON
        manifest_file.close();

        result.success = true;
        spdlog::info(
            "Successfully created {} slices for {} (ratio: {:.2f}%)",
            num_slices, file_path_sv.data(),
            (result.original_size > 0 ? (1.0 - result.compression_ratio) * 100
                                      : 0.0));

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception in slice compression: ") + e.what();
        spdlog::error("{}", result.error_message.c_str());
        // Consider cleanup
    }

    return result;
}

CompressionResult mergeCompressedSlices(
    const Vector<String>& slice_files,  // Use Vector<String>
    std::string_view output_path_sv,
    [[maybe_unused]] const DecompressionOptions&
        options) {  // Mark options as potentially unused if diagnostic persists
    CompressionResult result;
    try {
        if (slice_files.empty() || output_path_sv.empty()) {
            result.error_message = "Invalid parameters for merging slices";
            return result;
        }

        fs::path output_path(output_path_sv);

        // Open output file
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            result.error_message = "Failed to create output file";
            return result;
        }

        result.original_size = 0;    // Will be total decompressed size
        result.compressed_size = 0;  // Will be total size of slice files

        // --- Slice Decompression and Merging ---
        std::atomic<size_t> total_original_size_atomic{0};
        std::atomic<size_t> total_compressed_size_atomic{0};
        std::atomic<bool> error_flag{false};
        String shared_error_message;
        std::mutex error_mutex;
        std::mutex
            write_mutex;  // Mutex for writing to the output file sequentially

        // Task to decompress a single slice
        auto decompress_slice_task =
            [&](const String& slice_filename,
                size_t slice_index) -> std::pair<bool, Vector<unsigned char>> {
            Vector<unsigned char> decompressed_data;
            try {
                fs::path slice_path(
                    slice_filename);  // Use String directly if fs::path
                                      // supports it, else .c_str()
                if (!fs::exists(slice_path)) {
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Slice file not found: ") + slice_filename;
                    return {false, decompressed_data};
                }

                std::error_code ec;
                size_t compressed_slice_size = fs::file_size(slice_path, ec);
                if (ec) {
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Failed to get size of slice file: ") +
                        slice_filename;
                    return {false, decompressed_data};
                }
                total_compressed_size_atomic +=
                    compressed_slice_size;  // Add compressed size

                gzFile in = gzopen(slice_filename.c_str(), "rb");
                if (!in) {
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Failed to open slice file: ") + slice_filename;
                    return {false, decompressed_data};
                }
                std::unique_ptr<gzFile_s, decltype(&gzclose)> in_guard(in,
                                                                       gzclose);

                Vector<char> chunk(options.chunk_size);  // options is used here
                int bytes_read;
                Vector<unsigned char>
                    temp_buffer;  // Temporary buffer for this slice's data

                while ((bytes_read =
                            gzread(in, chunk.data(),
                                   static_cast<unsigned>(chunk.size()))) > 0) {
                    // Insert into temp_buffer
                    temp_buffer.insert(
                        temp_buffer.end(),
                        reinterpret_cast<unsigned char*>(chunk.data()),
                        reinterpret_cast<unsigned char*>(chunk.data()) +
                            bytes_read);
                }

                if (bytes_read < 0) {
                    int err_no = 0;
                    const char* err_msg = gzerror(in, &err_no);
                    std::lock_guard lock(error_mutex);
                    shared_error_message =
                        String("Error reading compressed data from slice ") +
                        String(std::to_string(slice_index)) + ": " +
                        (err_no == Z_ERRNO ? strerror(errno) : err_msg);
                    return {false, decompressed_data};
                }

                decompressed_data = std::move(temp_buffer);  // Move data
                total_original_size_atomic +=
                    decompressed_data.size();  // Add decompressed size
                return {
                    true,
                    std::move(decompressed_data)};  // Return success and data

            } catch (const std::exception& e) {
                std::lock_guard lock(error_mutex);
                shared_error_message =
                    String("Exception during slice decompression: ") + e.what();
                return {false, decompressed_data};
            }
        };

        // Execute tasks (parallel or sequential)
        // We need to write sequentially, so parallel decompression needs
        // buffering
        if (options.use_parallel && slice_files.size() > 1) {
            std::vector<std::future<std::pair<bool, Vector<unsigned char>>>>
                futures;
            futures.reserve(slice_files.size());

            for (size_t i = 0; i < slice_files.size(); ++i) {
                futures.push_back(std::async(std::launch::async,
                                             decompress_slice_task,
                                             std::ref(slice_files[i]), i));
            }

            // Collect results and write sequentially
            for (size_t i = 0; i < futures.size(); ++i) {
                auto result_pair = futures[i].get();
                if (!result_pair.first) {  // Check success flag
                    error_flag = true;
                    // Error message is already set in shared_error_message
                    break;  // Stop processing further slices on error
                }

                // Write the decompressed data for this slice
                const auto& data_to_write = result_pair.second;
                if (!data_to_write.empty()) {
                    // No mutex needed here as we process futures sequentially
                    output.write(
                        reinterpret_cast<const char*>(data_to_write.data()),
                        data_to_write.size());
                    if (!output) {
                        error_flag = true;
                        std::lock_guard lock(
                            error_mutex);  // Lock needed if error_message is
                                           // shared
                        shared_error_message =
                            "Failed to write merged data to output file";
                        break;
                    }
                }
            }

        } else {
            // Sequential execution
            for (size_t i = 0; i < slice_files.size(); ++i) {
                auto result_pair = decompress_slice_task(slice_files[i], i);
                if (!result_pair.first) {
                    error_flag = true;
                    break;
                }
                const auto& data_to_write = result_pair.second;
                if (!data_to_write.empty()) {
                    output.write(
                        reinterpret_cast<const char*>(data_to_write.data()),
                        data_to_write.size());
                    if (!output) {
                        error_flag = true;
                        std::lock_guard lock(error_mutex);
                        shared_error_message =
                            "Failed to write merged data to output file";
                        break;
                    }
                }
            }
        }

        // Close output file
        output.close();

        // Check for errors
        if (error_flag) {
            result.error_message = shared_error_message;
            // Clean up output file?
            fs::remove(output_path);
            return result;
        }

        // Finalize result
        result.original_size = total_original_size_atomic;
        result.compressed_size =
            total_compressed_size_atomic;  // Sum of slice file sizes
        if (result.original_size > 0) {
            result.compression_ratio =
                static_cast<double>(result.compressed_size) /
                static_cast<double>(result.original_size);
        } else {
            result.compression_ratio = 0.0;
        }
        result.success = true;

        spdlog::info(
            "Successfully merged {} slices into {} (ratio: {:.2f}%)",
            slice_files.size(), output_path_sv.data(),
            (result.original_size > 0 ? (1.0 - result.compression_ratio) * 100
                                      : 0.0));

    } catch (const std::exception& e) {
        result.error_message =
            String("Exception in slice merging: ") + e.what();
        spdlog::error("{}", result.error_message.c_str());
        // Clean up output file?
        try {
            fs::remove(fs::path(output_path_sv));
        } catch (...) {
        }
    }

    return result;
}

// processFilesAsync needs implementation using std::async or a thread pool
std::future<Vector<CompressionResult>> processFilesAsync(
    const Vector<String>& file_paths,  // Use Vector<String>
    const CompressionOptions& options) {
    // Use std::packaged_task and std::async for simplicity
    return std::async(std::launch::async, [file_paths, options]() {
        Vector<CompressionResult> results;  // Use Vector
        results.reserve(file_paths.size());

        // For actual parallelism, a thread pool would be better than launching
        // unlimited std::async tasks, especially for many files.
        // This simple version just runs them sequentially within the async
        // task. A true parallel version would need to manage threads.

        for (const auto& file_path : file_paths) {
            fs::path p(file_path);  // Use String directly if fs::path supports
                                    // it, else .c_str()
            if (fs::is_directory(p)) {
                // Decide how to handle directories (e.g., compress as folder or
                // skip) Assuming compressFolder for now Need an output path
                // convention
                String output_zip = file_path + ".zip";
                results.push_back(compressFolder(file_path.c_str(),
                                                 output_zip.c_str(), options));
            } else if (fs::is_regular_file(p)) {
                // Compress single file
                // Need an output directory convention
                fs::path output_dir =
                    p.parent_path() / "compressed";  // Example output dir
                results.push_back(compressFile(
                    file_path.c_str(), output_dir.string().c_str(), options));
            } else {
                // Handle other cases or invalid paths
                CompressionResult r;
                r.success = false;
                r.error_message =
                    String("Invalid path or not a file/directory: ") +
                    file_path;
                results.push_back(r);
            }
        }
        return results;
    });
}

// createBackup implementation
CompressionResult createBackup(std::string_view source_path_sv,
                               std::string_view backup_path_sv,
                               bool compress_backup,  // Renamed parameter
                               const CompressionOptions& options) {
    CompressionResult result;
    try {
        fs::path source_path(source_path_sv);
        fs::path backup_path(backup_path_sv);

        if (!fs::exists(source_path)) {
            result.error_message = "Source path does not exist";
            return result;
        }

        // Ensure backup directory exists
        if (backup_path.has_parent_path()) {
            fs::create_directories(backup_path.parent_path());
        }

        if (compress_backup) {
            // Compress the source to the backup path
            if (fs::is_directory(source_path)) {
                // Ensure backup path ends with .zip for folder compression
                if (backup_path.extension() != ".zip") {
                    backup_path.replace_extension(".zip");
                }
                result = compressFolder(source_path_sv,
                                        backup_path.string().c_str(), options);
            } else {
                // Ensure backup path ends with .gz for file compression
                if (backup_path.extension() != ".gz") {
                    backup_path.replace_extension(".gz");
                }
                // compressFile expects output *folder*, not file path
                result = compressFile(
                    source_path_sv, backup_path.parent_path().string().c_str(),
                    options);
                // Need to potentially rename the output of compressFile if it
                // doesn't match backup_path This part needs refinement based on
                // compressFile's exact behavior. Assuming compressFile creates
                // source_path.filename() + ".gz" in the output folder.
                fs::path compressed_output =
                    backup_path.parent_path() / source_path.filename();
                compressed_output += ".gz";
                if (fs::exists(compressed_output) &&
                    compressed_output != backup_path) {
                    fs::rename(compressed_output, backup_path);
                } else if (!fs::exists(backup_path)) {
                    // If compressFile failed or didn't produce the expected
                    // file
                    if (result.success) {  // If compressFile reported success
                                           // but file is wrong
                        result.success = false;
                        result.error_message =
                            "Compressed backup file mismatch";
                    }
                }
            }
        } else {
            // Simple copy
            std::error_code ec;
            fs::copy(source_path, backup_path,
                     fs::copy_options::overwrite_existing |
                         fs::copy_options::recursive,
                     ec);
            if (ec) {
                result.error_message =
                    String("Failed to copy backup: ") + ec.message();
                result.success = false;
            } else {
                result.success = true;
                result.original_size = fs::is_regular_file(source_path)
                                           ? fs::file_size(source_path)
                                           : 0;  // Approx size
                result.compressed_size =
                    result.original_size;  // No compression
                result.compression_ratio = 1.0;
                spdlog::info(
                    "Successfully created uncompressed backup: {} -> {}",
                    source_path_sv.data(), backup_path_sv.data());
            }
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message =
            String("Exception during backup creation: ") + e.what();
        spdlog::error("{}", result.error_message.c_str());
    }
    return result;
}

// restoreFromBackup implementation
CompressionResult restoreFromBackup(
    std::string_view backup_path_sv, std::string_view restore_path_sv,
    bool compressed_backup,  // Renamed parameter
    const DecompressionOptions& options) {
    CompressionResult result;
    try {
        fs::path backup_path(backup_path_sv);
        fs::path restore_path(restore_path_sv);

        if (!fs::exists(backup_path)) {
            result.error_message = "Backup path does not exist";
            return result;
        }

        // Ensure restore directory exists
        if (restore_path.has_parent_path()) {
            fs::create_directories(restore_path.parent_path());
        }

        if (compressed_backup) {
            // Decompress/Extract the backup to the restore path
            String ext = backup_path.extension().string();  // Use String
            if (ext == ".zip") {
                // Extract zip archive to the restore path (assuming
                // restore_path is a directory)
                result = extractZip(backup_path_sv, restore_path_sv, options);
            } else if (ext == ".gz") {
                // Decompress single file to the restore path (assuming
                // restore_path is a directory)
                result =
                    decompressFile(backup_path_sv, restore_path_sv, options);
                // decompressFile creates backup_path.stem() in the output
                // folder. We might need to rename it if restore_path is a
                // specific file name.
                fs::path decompressed_output =
                    fs::path(restore_path_sv) / backup_path.stem();
                if (fs::exists(decompressed_output) &&
                    fs::is_regular_file(restore_path) &&
                    decompressed_output != restore_path) {
                    fs::rename(decompressed_output, restore_path);
                } else if (fs::is_directory(restore_path) &&
                           fs::exists(decompressed_output)) {
                    // If restore_path is a directory, the output is already in
                    // the right place.
                } else if (!fs::exists(restore_path) &&
                           !fs::exists(decompressed_output)) {
                    if (result.success) {  // Decompress reported success but
                                           // file missing
                        result.success = false;
                        result.error_message =
                            "Restored file mismatch or missing";
                    }
                }

            } else {
                result.error_message =
                    "Unsupported compressed backup format (expected .zip or "
                    ".gz)";
                result.success = false;
            }
        } else {
            // Simple copy
            std::error_code ec;
            fs::copy(backup_path, restore_path,
                     fs::copy_options::overwrite_existing |
                         fs::copy_options::recursive,
                     ec);
            if (ec) {
                result.error_message =
                    String("Failed to copy from backup: ") + ec.message();
                result.success = false;
            } else {
                result.success = true;
                result.compressed_size = fs::is_regular_file(backup_path)
                                             ? fs::file_size(backup_path)
                                             : 0;  // Approx size
                result.original_size =
                    result.compressed_size;  // No compression
                result.compression_ratio = 1.0;
                spdlog::info(
                    "Successfully restored from uncompressed backup: {} -> {}",
                    backup_path_sv.data(), restore_path_sv.data());
            }
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message =
            String("Exception during backup restoration: ") + e.what();
        spdlog::error("{}", result.error_message.c_str());
    }
    return result;
}

// --- Template Implementations ---

// Generic data compression template
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             (!std::is_same_v<
                 std::remove_cvref_t<std::ranges::range_value_t<T>>,
                 wchar_t>)  // Exclude wide char ranges for now
std::pair<CompressionResult, Vector<unsigned char>> compressData(
    const T& data, const CompressionOptions& options) {
    std::pair<CompressionResult, Vector<unsigned char>> result_pair;
    auto& [compression_result, compressed_data] =
        result_pair;  // Use structured binding

    try {
        // Get data pointer and size using std::ranges::data and
        // std::ranges::size
        const auto* data_ptr = std::ranges::data(data);
        size_t data_size =
            std::ranges::size(data) *
            sizeof(std::ranges::range_value_t<T>);  // Size in bytes

        if (data_size == 0) {
            compression_result.error_message = "Empty input data";
            return result_pair;
        }

        compression_result.original_size = data_size;

        // Estimate compressed size using zlib's compressBound
        uLong compressed_bound = compressBound(data_size);
        compressed_data.resize(
            compressed_bound);  // Resize Vector<unsigned char>

        // Use advanced deflate with specified window_bits instead of simple
        // compress2
        z_stream zs{};
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = static_cast<uInt>(data_size);
        zs.next_in =
            const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data_ptr));
        zs.avail_out = static_cast<uInt>(compressed_bound);
        zs.next_out = reinterpret_cast<Bytef*>(compressed_data.data());

        // Initialize deflate with window_bits from options
        int ret = deflateInit2(&zs, options.level, Z_DEFLATED,
                               options.window_bits, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            compression_result.error_message = getZlibErrorMessage(ret);
            return result_pair;
        }

        // Use RAII for zstream cleanup
        std::unique_ptr<z_stream, decltype(&deflateEnd)> deflate_guard(
            &zs, deflateEnd);

        // Perform compression in one step
        ret = deflate(&zs, Z_FINISH);

        if (ret != Z_STREAM_END) {
            compression_result.error_message =
                String("Compression failed: ") + getZlibErrorMessage(ret);
            return result_pair;
        }

        // Use actual bytes written
        uLongf actual_compressed_size = zs.total_out;

        if (ret != Z_OK) {
            compression_result.error_message =
                getZlibErrorMessage(ret);  // Use helper
            compressed_data.clear();       // Clear data on error
            return result_pair;
        }

        // Resize buffer to actual compressed size
        compressed_data.resize(actual_compressed_size);
        compression_result.compressed_size = actual_compressed_size;
        compression_result.compression_ratio = calculateCompressionRatio(
            actual_compressed_size, compression_result.original_size);

        compression_result.success = true;

        spdlog::info(
            "Successfully compressed {} bytes to {} bytes (ratio: {:.2f}%)",
            compression_result.original_size, actual_compressed_size,
            getCompressionPercentage(compression_result.compression_ratio));

    } catch (const std::exception& e) {
        compression_result.error_message =
            String("Exception during data compression: ") + e.what();
        spdlog::error("{}", compression_result.error_message.c_str());
        compressed_data.clear();  // Ensure data is cleared on exception
    }

    return result_pair;
}

// Generic data decompression template
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             (!std::is_same_v<
                 std::remove_cvref_t<std::ranges::range_value_t<T>>, wchar_t>)
std::pair<CompressionResult, Vector<unsigned char>> decompressData(
    const T& compressed_data_range, size_t expected_size,
    [[maybe_unused]] const DecompressionOptions&
        options) {  // Mark options as potentially unused

    std::pair<CompressionResult, Vector<unsigned char>> result_pair;
    auto& [compression_result, decompressed_data] = result_pair;

    try {
        const auto* compressed_data_ptr =
            std::ranges::data(compressed_data_range);
        size_t compressed_data_size = std::ranges::size(compressed_data_range) *
                                      sizeof(std::ranges::range_value_t<T>);

        if (compressed_data_size == 0) {
            compression_result.error_message = "Empty compressed data";
            return result_pair;
        }

        compression_result.compressed_size = compressed_data_size;

        // Optimized buffer size estimation
        // For small inputs, allocate a minimum buffer
        // For larger inputs with known expected size, use that
        // For larger inputs with unknown size, use a multiplier based on
        // compression type detection
        size_t buffer_size = 0;
        if (expected_size > 0) {
            // If we know the expected size, allocate exactly that
            buffer_size = expected_size;
        } else {
            // Try to detect compression type from header bytes for better
            // buffer estimation
            if (compressed_data_size >= 2) {
                const unsigned char* header =
                    reinterpret_cast<const unsigned char*>(compressed_data_ptr);

                // Check for gzip magic signature (0x1F, 0x8B)
                if (header[0] == 0x1F && header[1] == 0x8B) {
                    // Gzip typically has 2:1 to 10:1 compression ratio
                    buffer_size = compressed_data_size * 5;
                }
                // Check for zlib header (first byte bits 0-3 is 8 for deflate,
                // bits 4-7 for window size)
                else if ((header[0] & 0x0F) == 0x08) {
                    // Zlib typically has similar compression ratio to gzip
                    buffer_size = compressed_data_size * 5;
                } else {
                    // Unknown format, use conservative 4:1 ratio
                    buffer_size = compressed_data_size * 4;
                }
            } else {
                // Very small input, allocate a modest buffer
                buffer_size = 4096;
            }
        }

        // Ensure minimum buffer size
        if (buffer_size < 1024) {
            buffer_size = 1024;
        }

        decompressed_data.resize(buffer_size);

        // Use z_stream for more control, especially for potential resizing
        z_stream zs = {};
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = static_cast<uInt>(compressed_data_size);
        // Need const_cast because zlib API is not const-correct
        zs.next_in = const_cast<Bytef*>(
            reinterpret_cast<const Bytef*>(compressed_data_ptr));

        // Initialize for decompression (inflate)
        // Use window_bits from options (context7 = 7)
        // For gzip/zlib auto-detection, add 32 (15+32)
        // For raw deflate with no header, use negative value (-15)
        int windowBits = options.window_bits;

        // Auto-detect based on header bytes if possible
        if (compressed_data_size >= 2) {
            const unsigned char* header =
                reinterpret_cast<const unsigned char*>(compressed_data_ptr);
            // Check for gzip magic signature (0x1F, 0x8B)
            if (header[0] == 0x1F && header[1] == 0x8B) {
                // Need at least 15 or add 16 for gzip
                windowBits = std::max(15, abs(windowBits)) + 16;
            }
            // Check for zlib header
            else if ((header[0] & 0x0F) == 0x08) {
                // Use absolute value to ensure positive window bits for zlib
                windowBits = std::max(8, abs(windowBits));
            }
            // If not recognized, use as-is (for raw deflate)
        }

        int ret = inflateInit2(&zs, windowBits);
        if (ret != Z_OK) {
            compression_result.error_message = getZlibErrorMessage(ret);
            return result_pair;
        }
        // Guard for inflateEnd
        std::unique_ptr<z_stream, decltype(&inflateEnd)> inflate_guard(
            &zs, inflateEnd);

        // Decompression loop to handle buffer resizing
        int inflate_ret = Z_OK;
        do {
            zs.avail_out =
                static_cast<uInt>(decompressed_data.size() - zs.total_out);
            zs.next_out = reinterpret_cast<Bytef*>(decompressed_data.data() +
                                                   zs.total_out);

            if (zs.avail_out == 0) {
                // Buffer is full, resize it with an optimized growth strategy
                size_t old_size = decompressed_data.size();

                // Smart growth strategy:
                // - For small buffers (<64KB): double the size
                // - For medium buffers (64KB-1MB): grow by 50%
                // - For large buffers (>1MB): grow by 25% or a fixed chunk
                // (1MB), whichever is larger
                size_t new_size;
                if (old_size < 65536) {
                    new_size = old_size * 2;
                } else if (old_size < 1048576) {
                    new_size = old_size + (old_size / 2);
                } else {
                    size_t increment = std::max(old_size / 4, size_t(1048576));
                    new_size = old_size + increment;
                }

                // Check for overflow
                if (new_size <= old_size) {
                    compression_result.error_message =
                        "Decompression buffer size overflow";
                    return result_pair;  // inflate_guard handles cleanup
                }

                // Allocate new buffer
                try {
                    decompressed_data.resize(new_size);
                } catch (const std::bad_alloc&) {
                    compression_result.error_message =
                        "Memory allocation failed during decompression";
                    return result_pair;
                }

                // Update stream pointers after resize
                zs.avail_out =
                    static_cast<uInt>(decompressed_data.size() - zs.total_out);
                zs.next_out = reinterpret_cast<Bytef*>(
                    decompressed_data.data() + zs.total_out);
            }

            inflate_ret = inflate(&zs, Z_NO_FLUSH);

            if (inflate_ret == Z_STREAM_ERROR) {
                compression_result.error_message = "Decompression stream error";
                return result_pair;  // inflate_guard handles cleanup
            }
            if (inflate_ret == Z_NEED_DICT) {
                compression_result.error_message =
                    "Decompression needs dictionary (not supported)";
                return result_pair;  // inflate_guard handles cleanup
            }
            if (inflate_ret == Z_DATA_ERROR) {
                compression_result.error_message =
                    "Decompression data error (input corrupted?)";
                return result_pair;  // inflate_guard handles cleanup
            }
            if (inflate_ret == Z_MEM_ERROR) {
                compression_result.error_message = "Decompression memory error";
                return result_pair;  // inflate_guard handles cleanup
            }

        } while (inflate_ret != Z_STREAM_END &&
                 zs.avail_in >
                     0);  // Continue if input remains and not finished

        // Check if decompression finished successfully
        if (inflate_ret != Z_STREAM_END) {
            // It might be Z_OK if the buffer was exactly the right size on the
            // last call Or Z_BUF_ERROR if output buffer was full but input
            // wasn't exhausted (should have resized) Or some other error
            // occurred. Check if all input was consumed. If not, it's likely an
            // error or truncated input.
            if (zs.avail_in != 0) {
                compression_result.error_message =
                    String(
                        "Decompression failed, stream did not end correctly "
                        "and input remains. Ret: ") +
                    String(std::to_string(inflate_ret));
                return result_pair;  // inflate_guard handles cleanup
            }
            // If input is consumed but stream end wasn't reached, it might be
            // ok if the buffer was just right, but often indicates truncated
            // data if the original size wasn't known. Let's consider it
            // successful if input is consumed and no error occurred.
            spdlog::warn(
                "Decompression finished with code {} (Z_STREAM_END is {}), but "
                "all input consumed.",
                inflate_ret, Z_STREAM_END);
        }

        // Resize to actual decompressed size
        size_t actual_decompressed_size = zs.total_out;
        decompressed_data.resize(actual_decompressed_size);

        compression_result.original_size = actual_decompressed_size;
        compression_result.compression_ratio = calculateCompressionRatio(
            compression_result.compressed_size, actual_decompressed_size);

        compression_result.success = true;

        spdlog::info(
            "Successfully decompressed {} bytes to {} bytes (ratio: {:.2f}%)",
            compression_result.compressed_size, actual_decompressed_size,
            getCompressionPercentage(compression_result.compression_ratio));

    } catch (const std::exception& e) {
        compression_result.error_message =
            String("Exception during data decompression: ") + e.what();
        spdlog::error("{}", compression_result.error_message.c_str());
        decompressed_data.clear();
    }

    return result_pair;
}

// Explicit template instantiations using Vector<unsigned char>
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<Vector<unsigned char>>(const Vector<unsigned char>&,
                                    const CompressionOptions&);
// Add instantiations for other types if needed, e.g., Vector<char>, String
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<Vector<char>>(const Vector<char>&, const CompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<String>(const String&, const CompressionOptions&);
// Instantiation for std::span might require C++20
#if __cplusplus >= 202002L
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>&, const CompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<std::span<const char>>(const std::span<const char>&,
                                    const CompressionOptions&);
#endif

template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<Vector<unsigned char>>(const Vector<unsigned char>&, size_t,
                                      const DecompressionOptions&);
// Add instantiations for other types if needed
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<Vector<char>>(const Vector<char>&, size_t,
                             const DecompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<String>(const String&, size_t, const DecompressionOptions&);
#if __cplusplus >= 202002L
// #include <span> // Already included above
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>&, size_t, const DecompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<std::span<const char>>(const std::span<const char>&, size_t,
                                      const DecompressionOptions&);
#endif

}  // namespace atom::io
