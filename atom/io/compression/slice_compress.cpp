/*
 * slice_compress.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Slice-based file compression and merging

**************************************************/

#include "slice_compress.hpp"

#include <zlib.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/format.h>
#endif

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace atom::io {

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

}  // namespace atom::io
