/*
 * backup.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Backup, restore, and async file processing operations

**************************************************/

#include "backup.hpp"

#include <future>
#include <string>

#include <spdlog/spdlog.h>

#include "gz_compress.hpp"
#include "zip_operations.hpp"

namespace atom::io {

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

}  // namespace atom::io
