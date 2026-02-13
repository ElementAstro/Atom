/*
 * gz_compress.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: GZ file compression and decompression using ZLib

**************************************************/

#include "gz_compress.hpp"

#include <zlib.h>

#include <fstream>
#include <memory>

#include <spdlog/spdlog.h>

namespace atom::io {

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

        // Edge case: Check if input path is valid
        std::error_code ec;
        if (!fs::exists(input_path, ec) || ec) {
            result.error_message =
                "Input file does not exist or is inaccessible";
            return result;
        }

        // Edge case: Check if input is actually a regular file
        if (!fs::is_regular_file(input_path, ec) || ec) {
            result.error_message = "Input path is not a regular file";
            return result;
        }

        // Edge case: Check file size (avoid compressing empty files or
        // extremely large files)
        auto file_size = fs::file_size(input_path, ec);
        if (ec) {
            result.error_message = "Cannot determine input file size";
            return result;
        }

        if (file_size == 0) {
            result.error_message = "Cannot compress empty file";
            return result;
        }

        // Edge case: Check for extremely large files (> 4GB might cause issues
        // with some zip implementations)
        constexpr auto MAX_FILE_SIZE =
            static_cast<std::uintmax_t>(4ULL * 1024 * 1024 * 1024);  // 4GB
        if (file_size > MAX_FILE_SIZE) {
            spdlog::warn(
                "Compressing very large file ({}GB), this may take a long time",
                file_size / (1024.0 * 1024.0 * 1024.0));
        }

        fs::path output_dir(output_folder_sv);

        // Edge case: Check if output directory path is valid
        if (output_dir.empty()) {
            result.error_message = "Invalid output directory path";
            return result;
        }

        if (!fs::exists(output_dir, ec)) {
            if (!fs::create_directories(output_dir, ec) || ec) {
                result.error_message =
                    "Failed to create output directory: " + ec.message();
                return result;
            }
        } else {
            // Edge case: Check if output path is actually a directory
            if (!fs::is_directory(output_dir, ec) || ec) {
                result.error_message =
                    "Output path exists but is not a directory";
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

CompressionResult decompressFile(std::string_view file_path_sv,
                                 std::string_view output_folder_sv,
                                 const DecompressionOptions& options) {
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

}  // namespace atom::io
