/*
 * zip_operations.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: ZIP file operations using MiniZip-ng

**************************************************/

#include "zip_operations.hpp"

#ifndef ATOM_IO_NO_MINIZIP
#include <minizip-ng/mz.h>
#include <minizip-ng/mz_compat.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>
#endif
#include <zlib.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "utils.hpp"

namespace {

#ifndef ATOM_IO_NO_MINIZIP
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
#endif

}  // anonymous namespace

namespace atom::io {

// Use type aliases from high_performance.hpp within the implementation
using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

#ifndef ATOM_IO_NO_MINIZIP
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
#endif  // ATOM_IO_NO_MINIZIP

#ifndef ATOM_IO_NO_MINIZIP
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
        Vector<char> buffer(detail::DEFAULT_CHUNK_SIZE);

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
#endif  // ATOM_IO_NO_MINIZIP

#ifdef ATOM_IO_NO_MINIZIP
// Stub implementations when minizip-ng is not available
CompressionResult compressFolder(std::string_view, std::string_view,
                                 const CompressionOptions&) {
    CompressionResult result;
    result.success = false;
    result.error_message = "ZIP support not available - minizip-ng not found";
    return result;
}

CompressionResult extractZip(std::string_view, std::string_view,
                             const DecompressionOptions&) {
    CompressionResult result;
    result.success = false;
    result.error_message = "ZIP support not available - minizip-ng not found";
    return result;
}

CompressionResult createZip(std::string_view, std::string_view,
                            const CompressionOptions&) {
    CompressionResult result;
    result.success = false;
    result.error_message = "ZIP support not available - minizip-ng not found";
    return result;
}

CompressionResult removeFromZip(std::string_view, std::string_view) {
    CompressionResult result;
    result.success = false;
    result.error_message = "ZIP support not available - minizip-ng not found";
    return result;
}

Vector<ZipFileInfo> listZipContents(std::string_view) {
    return Vector<ZipFileInfo>{};
}

bool fileExistsInZip(std::string_view, std::string_view) { return false; }

std::optional<size_t> getZipSize(std::string_view) { return std::nullopt; }
#endif  // ATOM_IO_NO_MINIZIP

}  // namespace atom::io
