/*
 * compress.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Compressor using ZLib

**************************************************/

#include "compress.hpp"

#include <minizip-ng/mz_compat.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_strm_buf.h>
#include <minizip-ng/mz_strm_mem.h>
#include <minizip-ng/mz_strm_split.h>
#include <minizip-ng/mz_strm_zlib.h>
#include <minizip-ng/mz_zip.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <condition_variable>
#include <coroutine>
#include <cstring>
#include <execution>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <span>
#include <string>
#include <thread>
#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/format.h>
#endif
#ifdef _WIN32
#include <windows.h>
#define PATH_SEPARATOR '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define PATH_SEPARATOR '/'
#endif

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr int CHUNK = 16384;
constexpr int BUFFER_SIZE = 8192;
constexpr int FILENAME_SIZE = 256;

namespace atom::io {

// Helper struct for RAII style zlib management
struct ZLibCloser {
    void operator()(gzFile file) const {
        if (file)
            gzclose(file);
    }
};

// Helper struct for RAII style minizip management
struct ZipCloser {
    void operator()(void* zip) const {
        if (zip)
            unzClose(zip);
    }
};

struct ZipWriterCloser {
    void operator()(void* zip) const {
        if (zip)
            zipClose(zip, nullptr);
    }
};

// Thread-safe logger wrapper
class ThreadSafeLogger {
    std::mutex mutex_;

public:
    template <typename... Args>
    void info(const char* format, Args&&... args) {
        std::lock_guard lock(mutex_);
        LOG_F(INFO, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const char* format, Args&&... args) {
        std::lock_guard lock(mutex_);
        LOG_F(ERROR, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(const char* format, Args&&... args) {
        std::lock_guard lock(mutex_);
        LOG_F(WARNING, format, std::forward<Args>(args)...);
    }
};

// Global thread-safe logger
ThreadSafeLogger g_logger;

// Structure for coroutine task
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

auto compressFile(std::string_view input_file_name,
                  std::string_view output_folder) -> bool {
    try {
        g_logger.info(
            "compressFile called with input_file_name: {}, output_folder: {}",
            input_file_name, output_folder);

        // Input validation
        if (input_file_name.empty() || output_folder.empty()) {
            g_logger.error(
                "Invalid arguments: empty input file or output folder");
            return false;
        }

        fs::path inputPath(input_file_name);
        if (!fs::exists(inputPath)) {
            g_logger.error("Input file {} does not exist.", input_file_name);
            return false;
        }

        // Create output directory if it doesn't exist
        fs::path outputDir(output_folder);
        if (!fs::exists(outputDir)) {
            try {
                fs::create_directories(outputDir);
            } catch (const fs::filesystem_error& e) {
                g_logger.error("Failed to create output directory: {}",
                               e.what());
                return false;
            }
        }

        fs::path outputPath = outputDir / inputPath.filename().concat(".gz");

        // Use unique_ptr with custom deleter for RAII
        std::unique_ptr<gzFile_s, ZLibCloser> out(
            gzopen(outputPath.string().data(), "wb"));
        if (!out) {
            g_logger.error("Failed to create compressed file {}",
                           outputPath.string());
            return false;
        }

        std::ifstream input(input_file_name.data(), std::ios::binary);
        if (!input) {
            g_logger.error("Failed to open input file {}", input_file_name);
            return false;
        }

        std::array<char, CHUNK> buf;
        while (input.read(buf.data(), buf.size())) {
            if (gzwrite(out.get(), buf.data(),
                        static_cast<unsigned>(input.gcount())) == 0) {
                g_logger.error("Failed to compress file {}", input_file_name);
                return false;
            }
        }

        if (input.eof()) {
            if (gzwrite(out.get(), buf.data(),
                        static_cast<unsigned>(input.gcount())) == 0) {
                g_logger.error("Failed to compress file {}", input_file_name);
                return false;
            }
        } else if (input.bad()) {
            g_logger.error("Failed to read input file {}", input_file_name);
            return false;
        }

        g_logger.info("Compressed file {} -> {}", input_file_name,
                      outputPath.string());
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in compressFile: {}", e.what());
        return false;
    }
}

auto compressFile(const fs::path& file, gzFile out) -> bool {
    try {
        g_logger.info("compressFile called with file: {}", file.string());

        // Input validation
        if (!fs::exists(file)) {
            g_logger.error("Input file {} does not exist.", file.string());
            return false;
        }

        if (!out) {
            g_logger.error("Invalid gzFile handle");
            return false;
        }

        std::ifstream input_file(file, std::ios::binary);
        if (!input_file) {
            g_logger.error("Failed to open file {}", file.string());
            return false;
        }

        std::array<char, CHUNK> buf;
        while (input_file.read(buf.data(), buf.size())) {
            if (gzwrite(out, buf.data(),
                        static_cast<unsigned>(input_file.gcount())) !=
                static_cast<int>(input_file.gcount())) {
                g_logger.error("Failed to compress file {}", file.string());
                return false;
            }
        }

        if (input_file.eof()) {
            if (gzwrite(out, buf.data(),
                        static_cast<unsigned>(input_file.gcount())) !=
                static_cast<int>(input_file.gcount())) {
                g_logger.error("Failed to compress file {}", file.string());
                return false;
            }
        } else if (input_file.bad()) {
            g_logger.error("Failed to read file {}", file.string());
            return false;
        }

        g_logger.info("Compressed file {}", file.string());
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in compressFile: {}", e.what());
        return false;
    }
}

auto decompressFile(std::string_view input_file_name,
                    std::string_view output_folder) -> bool {
    try {
        g_logger.info(
            "decompressFile called with input_file_name: {}, output_folder: {}",
            input_file_name, output_folder);

        // Input validation
        if (input_file_name.empty() || output_folder.empty()) {
            g_logger.error(
                "Invalid arguments: empty input file or output folder");
            return false;
        }

        fs::path inputPath(input_file_name);
        if (!fs::exists(inputPath)) {
            g_logger.error("Input file {} does not exist.", input_file_name);
            return false;
        }

        // Create output directory if it doesn't exist
        fs::path outputDir(output_folder);
        if (!fs::exists(outputDir)) {
            try {
                fs::create_directories(outputDir);
            } catch (const fs::filesystem_error& e) {
                g_logger.error("Failed to create output directory: {}",
                               e.what());
                return false;
            }
        }

        fs::path outputPath =
            outputDir / inputPath.filename().stem().concat(".out");

        std::unique_ptr<FILE, decltype([](FILE* f) {
                            if (f)
                                fclose(f);
                        })>
            out(fopen(outputPath.string().data(), "wb"));

        if (!out) {
            g_logger.error("Failed to create decompressed file {}",
                           outputPath.string());
            return false;
        }

        std::unique_ptr<gzFile_s, ZLibCloser> in(
            gzopen(input_file_name.data(), "rb"));
        if (!in) {
            g_logger.error("Failed to open compressed file {}",
                           input_file_name);
            return false;
        }

        std::array<char, CHUNK> buf;
        int bytesRead;
        while ((bytesRead = gzread(in.get(), buf.data(), buf.size())) > 0) {
            if (fwrite(buf.data(), 1, bytesRead, out.get()) !=
                static_cast<size_t>(bytesRead)) {
                g_logger.error("Failed to decompress file {}", input_file_name);
                return false;
            }
        }

        if (bytesRead < 0) {
            g_logger.error("Failed to read compressed file {}",
                           input_file_name);
            return false;
        }

        g_logger.info("Decompressed file {} -> {}", input_file_name,
                      outputPath.string());
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in decompressFile: {}", e.what());
        return false;
    }
}

auto compressFolder(const fs::path& folder_name) -> bool {
    try {
        g_logger.info("compressFolder called with folder_name: {}",
                      folder_name.string());

        // Input validation
        if (!fs::exists(folder_name) || !fs::is_directory(folder_name)) {
            g_logger.error("Folder {} does not exist or is not a directory",
                           folder_name.string());
            return false;
        }

        auto outfileName = folder_name.string() + ".gz";
        std::unique_ptr<gzFile_s, ZLibCloser> out(
            gzopen(outfileName.data(), "wb"));
        if (!out) {
            g_logger.error("Failed to create compressed file {}", outfileName);
            return false;
        }

        bool success = true;

// Use C++20 ranges and views for cleaner iteration
#if defined(__cpp_lib_ranges) && defined(__cpp_lib_execution)
        try {
            std::vector<fs::path> files;
            for (const auto& entry :
                 fs::recursive_directory_iterator(folder_name)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                }
            }

            std::atomic<bool> compressionSuccess = true;
            std::for_each(std::execution::par, files.begin(), files.end(),
                          [&out, &compressionSuccess](const fs::path& file) {
                              if (!compressFile(file, out.get())) {
                                  compressionSuccess = false;
                              }
                          });

            success = compressionSuccess.load();
        } catch (const std::exception& e) {
            g_logger.error("Error during parallel compression: {}", e.what());
            success = false;
        }
#else
        // Fall back to sequential processing if C++20 features unavailable
        for (const auto& entry :
             fs::recursive_directory_iterator(folder_name)) {
            if (entry.is_regular_file()) {
                if (!compressFile(entry.path(), out.get())) {
                    success = false;
                    break;
                }
            }
        }
#endif

        if (success) {
            g_logger.info("Compressed folder {} -> {}", folder_name.string(),
                          outfileName);
        }
        return success;
    } catch (const std::exception& e) {
        g_logger.error("Exception in compressFolder: {}", e.what());
        return false;
    }
}

auto compressFolder(const char* folder_name) -> bool {
    if (!folder_name) {
        g_logger.error("Invalid null folder name");
        return false;
    }
    g_logger.info("compressFolder called with folder_name: {}", folder_name);
    return compressFolder(fs::path(folder_name));
}

void compressFileSlice(const std::string& inputFile, size_t sliceSize,
                       size_t numThreads) {
    try {
        // Input validation
        if (inputFile.empty() || sliceSize == 0) {
            g_logger.error(
                "Invalid arguments: empty input file or zero slice size");
            return;
        }

        if (!fs::exists(inputFile)) {
            g_logger.error("Input file {} does not exist", inputFile);
            return;
        }

        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile) {
            g_logger.error("Failed to open input file {}.", inputFile);
            return;
        }

        // Get file size for progress tracking
        inFile.seekg(0, std::ios::end);
        const auto fileSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        // Create thread pool for parallel compression
        std::vector<std::future<void>> futures;
        std::atomic<int> completedSlices = 0;
        numThreads = std::max(size_t(1), numThreads);

        std::mutex queueMutex;
        std::condition_variable queueCondition;
        std::queue<size_t> sliceQueue;
        std::atomic<bool> done = false;

        // Launch worker threads
        for (size_t i = 0; i < numThreads; ++i) {
            futures.push_back(std::async(std::launch::async, [&]() {
                std::vector<char> buffer(sliceSize);

                while (true) {
                    size_t sliceIndex;

                    {
                        std::unique_lock lock(queueMutex);
                        queueCondition.wait(
                            lock, [&] { return !sliceQueue.empty() || done; });

                        if (sliceQueue.empty() && done)
                            break;

                        sliceIndex = sliceQueue.front();
                        sliceQueue.pop();
                    }

                    // Calculate slice offset
                    const auto offset = sliceIndex * sliceSize;

                    // Thread-safe file reading
                    size_t bytesRead;
                    {
                        std::unique_lock lock(
                            queueMutex);  // Protect file access
                        inFile.seekg(offset);
                        inFile.read(buffer.data(), sliceSize);
                        bytesRead = inFile.gcount();
                    }

                    if (bytesRead > 0) {
                        // Prepare compressed data
                        std::vector<unsigned char> compressedData(
                            compressBound(bytesRead));
                        uLongf compressedSize = compressedData.size();

                        // Compress the data
                        if (compress(
                                reinterpret_cast<Bytef*>(compressedData.data()),
                                &compressedSize,
                                reinterpret_cast<const Bytef*>(buffer.data()),
                                bytesRead) != Z_OK) {
                            g_logger.error("Compression failed for slice {}.",
                                           sliceIndex);
                            continue;
                        }

                        // Write the compressed data to a new file
                        std::string compressedFileName =
                            inputFile + ".slice_" + std::to_string(sliceIndex) +
                            ".zlib";
                        std::ofstream outFile(compressedFileName,
                                              std::ios::binary);
                        if (!outFile) {
                            g_logger.error(
                                "Failed to open output file for slice {}.",
                                sliceIndex);
                            continue;
                        }

                        // Write header with metadata for reconstruction
                        const uint32_t magic = 0x5A4C4942;  // "ZLIB"
                        const uint32_t version = 1;
                        const uint64_t originalSize = bytesRead;

                        outFile.write(reinterpret_cast<const char*>(&magic),
                                      sizeof(magic));
                        outFile.write(reinterpret_cast<const char*>(&version),
                                      sizeof(version));
                        outFile.write(
                            reinterpret_cast<const char*>(&sliceIndex),
                            sizeof(sliceIndex));
                        outFile.write(
                            reinterpret_cast<const char*>(&originalSize),
                            sizeof(originalSize));
                        outFile.write(
                            reinterpret_cast<const char*>(&compressedSize),
                            sizeof(compressedSize));

                        // Write the compressed data
                        outFile.write(
                            reinterpret_cast<char*>(compressedData.data()),
                            compressedSize);

                        // Update progress
                        ++completedSlices;
                        if (fileSize > 0) {
                            const double progress = std::min(
                                100.0, (offset + bytesRead) * 100.0 / fileSize);
                            g_logger.info(
                                "Compression progress: {:.1f}% (slice {} "
                                "complete)",
                                progress, sliceIndex);
                        }
                    }
                }
            }));
        }

        // Calculate total slices
        const auto totalSlices =
            (static_cast<size_t>(fileSize) + sliceSize - 1) / sliceSize;

        // Enqueue all slice indices
        for (size_t i = 0; i < totalSlices; ++i) {
            std::lock_guard lock(queueMutex);
            sliceQueue.push(i);
            queueCondition.notify_one();
        }

        // Signal completion and wait for all workers
        {
            std::lock_guard lock(queueMutex);
            done = true;
            queueCondition.notify_all();
        }

        // Wait for all threads to complete
        for (auto& future : futures) {
            future.wait();
        }

        // Create manifest file with metadata
        json manifest;
        manifest["filename"] = fs::path(inputFile).filename().string();
        manifest["original_size"] = static_cast<std::int64_t>(fileSize);
        manifest["slice_size"] = static_cast<std::int64_t>(sliceSize);
        manifest["total_slices"] = totalSlices;
        manifest["compression_method"] = "zlib";
        manifest["version"] = "1.0";

        std::ofstream manifestFile(inputFile + ".manifest.json");
        manifestFile << manifest.dump(4);

        g_logger.info("File sliced and compressed successfully into {} slices.",
                      totalSlices);
    } catch (const std::exception& e) {
        g_logger.error("Exception in compressFileSlice: {}", e.what());
    }
}

void decompressFileSlices(const std::vector<std::string>& sliceFiles,
                          size_t sliceSize, const std::string& outputFile) {
    try {
        // Input validation
        if (sliceFiles.empty() || sliceSize == 0 || outputFile.empty()) {
            g_logger.error(
                "Invalid arguments: empty slice files, zero slice size, or "
                "empty output file");
            return;
        }

        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            g_logger.error("Failed to open output file {}.", outputFile);
            return;
        }

        for (const auto& sliceFile : sliceFiles) {
            std::ifstream inFile(sliceFile, std::ios::binary);
            if (!inFile) {
                g_logger.error("Failed to open slice file {}.", sliceFile);
                continue;
            }

            // Read header with metadata for reconstruction
            uint32_t magic, version;
            size_t sliceIndex, originalSize, compressedSize;

            inFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            inFile.read(reinterpret_cast<char*>(&version), sizeof(version));
            inFile.read(reinterpret_cast<char*>(&sliceIndex),
                        sizeof(sliceIndex));
            inFile.read(reinterpret_cast<char*>(&originalSize),
                        sizeof(originalSize));
            inFile.read(reinterpret_cast<char*>(&compressedSize),
                        sizeof(compressedSize));

            if (magic != 0x5A4C4942 || version != 1) {
                g_logger.error("Invalid slice file format: {}", sliceFile);
                continue;
            }

            // Read compressed data
            std::vector<unsigned char> compressedData(compressedSize);
            inFile.read(reinterpret_cast<char*>(compressedData.data()),
                        compressedSize);

            // Prepare buffer for decompressed data
            std::vector<unsigned char> decompressedData(originalSize);
            uLongf decompressedSize = originalSize;

            // Decompress the data
            if (uncompress(
                    reinterpret_cast<Bytef*>(decompressedData.data()),
                    &decompressedSize,
                    reinterpret_cast<const Bytef*>(compressedData.data()),
                    compressedSize) != Z_OK) {
                g_logger.error("Decompression failed for slice file: {}",
                               sliceFile);
                continue;
            }

            // Write the decompressed data to the output file
            outFile.write(reinterpret_cast<char*>(decompressedData.data()),
                          decompressedSize);
        }

        g_logger.info("File slices decompressed successfully into {}.",
                      outputFile);
    } catch (const std::exception& e) {
        g_logger.error("Exception in decompressFileSlices: {}", e.what());
    }
}

void listCompressedFiles() {
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().extension() == ".zlib") {
            g_logger.info("{}", entry.path().filename().string());
        }
    }
}

void deleteCompressedFiles() {
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().extension() == ".zlib") {
            std::filesystem::remove(entry.path());
            g_logger.info("Deleted: {}", entry.path().filename().string());
        }
    }
}

auto extractZip(std::string_view zip_file,
                std::string_view destination_folder) -> bool {
    try {
        g_logger.info(
            "extractZip called with zip_file: {}, destination_folder: {}",
            zip_file, destination_folder);

        // Input validation
        if (zip_file.empty() || destination_folder.empty()) {
            g_logger.error(
                "Invalid arguments: empty zip file or destination folder");
            return false;
        }

        std::unique_ptr<void, ZipCloser> zipReader(unzOpen(zip_file.data()));
        if (!zipReader) {
            g_logger.error("Failed to open ZIP file: {}", zip_file);
            return false;
        }

        if (unzGoToFirstFile(zipReader.get()) != UNZ_OK) {
            g_logger.error("Failed to read first file in ZIP: {}", zip_file);
            return false;
        }

        do {
            std::array<char, FILENAME_SIZE> filename;
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(zipReader.get(), &fileInfo,
                                      filename.data(), filename.size(), nullptr,
                                      0, nullptr, 0) != UNZ_OK) {
                g_logger.error("Failed to get file info in ZIP: {}", zip_file);
                return false;
            }

            std::string filePath = "./" +
                                   fs::path(destination_folder).string() + "/" +
                                   filename.data();
            if (filename[filename.size() - 1] == '/') {
                fs::create_directories(filePath);
                continue;
            }

            if (unzOpenCurrentFile(zipReader.get()) != UNZ_OK) {
                g_logger.error("Failed to open file in ZIP: {}",
                               filename.data());
                return false;
            }

            std::ofstream outFile(filePath, std::ios::binary);
            if (!outFile) {
                g_logger.error("Failed to create file: {}", filePath);
                unzCloseCurrentFile(zipReader.get());
                return false;
            }

            std::array<char, BUFFER_SIZE> buffer;
            int readSize = 0;
            while ((readSize = unzReadCurrentFile(
                        zipReader.get(), buffer.data(), buffer.size())) > 0) {
                outFile.write(buffer.data(), readSize);
            }

            unzCloseCurrentFile(zipReader.get());
            outFile.close();
            g_logger.info("Extracted file {}", filePath);
        } while (unzGoToNextFile(zipReader.get()) != UNZ_END_OF_LIST_OF_FILE);

        g_logger.info("Extracted ZIP file {}", zip_file);
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in extractZip: {}", e.what());
        return false;
    }
}

auto createZip(std::string_view source_folder, std::string_view zip_file,
               int compression_level) -> bool {
    try {
        g_logger.info(
            "createZip called with source_folder: {}, zip_file: {}, "
            "compression_level: {}",
            source_folder, zip_file, compression_level);

        // Input validation
        if (source_folder.empty() || zip_file.empty()) {
            g_logger.error(
                "Invalid arguments: empty source folder or zip file");
            return false;
        }

        std::unique_ptr<void, ZipWriterCloser> zipWriter(
            zipOpen(zip_file.data(), APPEND_STATUS_CREATE));
        if (!zipWriter) {
            g_logger.error("Failed to create ZIP file: {}", zip_file);
            return false;
        }

        try {
            for (const auto& entry :
                 fs::recursive_directory_iterator(source_folder)) {
                if (fs::is_regular_file(entry)) {
                    std::string filePath = entry.path().string();
                    std::string relativePath =
                        fs::relative(filePath, source_folder).string();

                    zip_fileinfo fileInfo = {};
                    if (zipOpenNewFileInZip(
                            zipWriter.get(), relativePath.data(), &fileInfo,
                            nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                            compression_level) != ZIP_OK) {
                        g_logger.error("Failed to add file to ZIP: {}",
                                       relativePath);
                        return false;
                    }

                    std::ifstream inFile(filePath, std::ios::binary);
                    if (!inFile) {
                        g_logger.error("Failed to open file for reading: {}",
                                       filePath);
                        zipCloseFileInZip(zipWriter.get());
                        return false;
                    }

                    std::array<char, BUFFER_SIZE> buffer;
                    while (inFile.read(buffer.data(), buffer.size())) {
                        zipWriteInFileInZip(zipWriter.get(), buffer.data(),
                                            inFile.gcount());
                    }
                    if (inFile.gcount() > 0) {
                        zipWriteInFileInZip(zipWriter.get(), buffer.data(),
                                            inFile.gcount());
                    }

                    inFile.close();
                    zipCloseFileInZip(zipWriter.get());
                }
            }

            g_logger.info("ZIP file created successfully: {}", zip_file);
            return true;
        } catch (const std::exception& e) {
            g_logger.error("Failed to create ZIP file: {}", zip_file);
            return false;
        }
    } catch (const std::exception& e) {
        g_logger.error("Exception in createZip: {}", e.what());
        return false;
    }
}

auto listFilesInZip(std::string_view zip_file) -> std::vector<std::string> {
    try {
        g_logger.info("listFilesInZip called with zip_file: {}", zip_file);
        std::vector<std::string> fileList;

        // Input validation
        if (zip_file.empty()) {
            g_logger.error("Invalid argument: empty zip file");
            return fileList;
        }

        std::unique_ptr<void, ZipCloser> zipReader(unzOpen(zip_file.data()));
        if (!zipReader) {
            g_logger.error("Failed to open ZIP file: {}", zip_file);
            return fileList;
        }

        if (unzGoToFirstFile(zipReader.get()) != UNZ_OK) {
            g_logger.error("Failed to read first file in ZIP: {}", zip_file);
            return fileList;
        }

        do {
            std::array<char, FILENAME_SIZE> filename;
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(zipReader.get(), &fileInfo,
                                      filename.data(), filename.size(), nullptr,
                                      0, nullptr, 0) != UNZ_OK) {
                g_logger.error("Failed to get file info in ZIP: {}", zip_file);
                return fileList;
            }
            fileList.emplace_back(filename.data());
            g_logger.info("Found file in ZIP: {}", filename.data());
        } while (unzGoToNextFile(zipReader.get()) != UNZ_END_OF_LIST_OF_FILE);

        g_logger.info("Listed files in ZIP: {}", zip_file);
        return fileList;
    } catch (const std::exception& e) {
        g_logger.error("Exception in listFilesInZip: {}", e.what());
        return {};
    }
}

auto fileExistsInZip(std::string_view zip_file,
                     std::string_view file_name) -> bool {
    try {
        g_logger.info("fileExistsInZip called with zip_file: {}, file_name: {}",
                      zip_file, file_name);

        // Input validation
        if (zip_file.empty() || file_name.empty()) {
            g_logger.error("Invalid arguments: empty zip file or file name");
            return false;
        }

        std::unique_ptr<void, ZipCloser> zipReader(unzOpen(zip_file.data()));
        if (!zipReader) {
            g_logger.error("Failed to open ZIP file: {}", zip_file);
            return false;
        }

        bool exists =
            unzLocateFile(zipReader.get(), file_name.data(), 0) == UNZ_OK;
        if (exists) {
            g_logger.info("File found in ZIP: {}", file_name);
        } else {
            g_logger.warning("File not found in ZIP: {}", file_name);
        }

        return exists;
    } catch (const std::exception& e) {
        g_logger.error("Exception in fileExistsInZip: {}", e.what());
        return false;
    }
}

auto removeFileFromZip(std::string_view zip_file,
                       std::string_view file_name) -> bool {
    try {
        g_logger.info(
            "removeFileFromZip called with zip_file: {}, file_name: {}",
            zip_file, file_name);

        // Input validation
        if (zip_file.empty() || file_name.empty()) {
            g_logger.error("Invalid arguments: empty zip file or file name");
            return false;
        }

        std::unique_ptr<void, ZipCloser> zipReader(unzOpen(zip_file.data()));
        if (!zipReader) {
            g_logger.error("Failed to open ZIP file: {}", zip_file);
            return false;
        }

        if (unzLocateFile(zipReader.get(), file_name.data(), 0) != UNZ_OK) {
            g_logger.error("File not found in ZIP: {}", file_name);
            return false;
        }

        std::string tempZipFile = std::string(zip_file) + ".tmp";
        std::unique_ptr<void, ZipWriterCloser> zipWriter(
            zipOpen(tempZipFile.c_str(), APPEND_STATUS_CREATE));
        if (!zipWriter) {
            g_logger.error("Failed to create temporary ZIP file: {}",
                           tempZipFile);
            return false;
        }

        if (unzGoToFirstFile(zipReader.get()) != UNZ_OK) {
            g_logger.error("Failed to read first file in ZIP: {}", zip_file);
            return false;
        }

        do {
            std::array<char, FILENAME_SIZE> filename;
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(zipReader.get(), &fileInfo,
                                      filename.data(), filename.size(), nullptr,
                                      0, nullptr, 0) != UNZ_OK) {
                g_logger.error("Failed to get file info in ZIP: {}", zip_file);
                return false;
            }

            if (file_name == filename.data()) {
                g_logger.info("Skipping file: {} for removal", filename.data());
                continue;
            }

            if (unzOpenCurrentFile(zipReader.get()) != UNZ_OK) {
                g_logger.error("Failed to open file in ZIP: {}",
                               filename.data());
                return false;
            }

            zip_fileinfo fileInfoOut = {};
            if (zipOpenNewFileInZip(zipWriter.get(), filename.data(),
                                    &fileInfoOut, nullptr, 0, nullptr, 0,
                                    nullptr, Z_DEFLATED,
                                    Z_DEFAULT_COMPRESSION) != ZIP_OK) {
                g_logger.error("Failed to add file to temporary ZIP: {}",
                               filename.data());
                unzCloseCurrentFile(zipReader.get());
                return false;
            }

            std::array<char, BUFFER_SIZE> buffer;
            int readSize = 0;
            while ((readSize = unzReadCurrentFile(
                        zipReader.get(), buffer.data(), buffer.size())) > 0) {
                zipWriteInFileInZip(zipWriter.get(), buffer.data(), readSize);
            }

            unzCloseCurrentFile(zipReader.get());
            zipCloseFileInZip(zipWriter.get());
        } while (unzGoToNextFile(zipReader.get()) != UNZ_END_OF_LIST_OF_FILE);

        fs::remove(zip_file);
        fs::rename(tempZipFile, zip_file);

        g_logger.info("File removed from ZIP: {}", file_name);
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in removeFileFromZip: {}", e.what());
        return false;
    }
}

auto getZipFileSize(std::string_view zip_file) -> size_t {
    try {
        g_logger.info("getZipFileSize called with zip_file: {}", zip_file);

        // Input validation
        if (zip_file.empty()) {
            g_logger.error("Invalid argument: empty zip file");
            return 0;
        }

        std::ifstream inputFile(zip_file.data(),
                                std::ifstream::ate | std::ifstream::binary);
        size_t size = inputFile.tellg();
        g_logger.info("Size of ZIP file {}: {}", zip_file, size);
        return size;
    } catch (const std::exception& e) {
        g_logger.error("Exception in getZipFileSize: {}", e.what());
        return 0;
    }
}

bool decompressChunk(const std::vector<unsigned char>& chunkData,
                     std::vector<unsigned char>& outputBuffer) {
    try {
        g_logger.info("decompressChunk called");
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        if (inflateInit(&stream) != Z_OK) {
            g_logger.error("Error initializing zlib inflate.");
            return false;
        }

        stream.avail_in = static_cast<uInt>(chunkData.size());
        stream.next_in = const_cast<Bytef*>(chunkData.data());

        do {
            stream.avail_out = static_cast<uInt>(outputBuffer.size());
            stream.next_out = outputBuffer.data();

            int ret = inflate(&stream, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR) {
                g_logger.error(
                    "Data error detected. Skipping corrupted chunk.");
                inflateEnd(&stream);
                return false;
            }

        } while (stream.avail_out == 0);

        inflateEnd(&stream);
        g_logger.info("Chunk decompressed successfully");
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in decompressChunk: {}", e.what());
        return false;
    }
}

constexpr int CHUNK_SIZE = 4096;

void processFilesInParallel(const std::vector<std::string>& filenames) {
    try {
        g_logger.info("processFilesInParallel called with {} files",
                      filenames.size());

        // Input validation
        if (filenames.empty()) {
            g_logger.error("Invalid argument: empty filenames");
            return;
        }

        std::vector<std::future<void>> futures;

        for (const auto& filename : filenames) {
            futures.push_back(std::async(std::launch::async, [filename]() {
                g_logger.info("Processing file: {}", filename);
                std::ifstream file(filename, std::ios::binary);

                if (!file) {
                    g_logger.error("Failed to open file: {}", filename);
                    return;
                }

                std::vector<unsigned char> chunkData(CHUNK_SIZE);
                std::vector<unsigned char> outputBuffer(CHUNK_SIZE * 2);

                while (file.read(reinterpret_cast<char*>(chunkData.data()),
                                 CHUNK_SIZE) ||
                       file.gcount() > 0) {
                    if (!decompressChunk(chunkData, outputBuffer)) {
                        g_logger.error(
                            "Failed to decompress chunk for file: {}",
                            filename);
                    }
                }
                g_logger.info("Finished processing file: {}", filename);
            }));
        }

        for (auto& future : futures) {
            future.get();
        }
        g_logger.info("All files processed in parallel");
    } catch (const std::exception& e) {
        g_logger.error("Exception in processFilesInParallel: {}", e.what());
    }
}

bool createBackup(const std::string& originalFile,
                  const std::string& backupFile) {
    try {
        g_logger.info(
            "createBackup called with originalFile: {}, backupFile: {}",
            originalFile, backupFile);

        // Input validation
        if (originalFile.empty() || backupFile.empty()) {
            g_logger.error(
                "Invalid arguments: empty original file or backup file");
            return false;
        }

        std::filesystem::copy(
            originalFile, backupFile,
            std::filesystem::copy_options::overwrite_existing);
        g_logger.info("Backup created: {}", backupFile);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        g_logger.error("Failed to create backup: {}", e.what());
        return false;
    }
}

bool restoreBackup(const std::string& backupFile,
                   const std::string& originalFile) {
    try {
        g_logger.info(
            "restoreBackup called with backupFile: {}, originalFile: {}",
            backupFile, originalFile);

        // Input validation
        if (backupFile.empty() || originalFile.empty()) {
            g_logger.error(
                "Invalid arguments: empty backup file or original file");
            return false;
        }

        std::filesystem::copy(
            backupFile, originalFile,
            std::filesystem::copy_options::overwrite_existing);
        g_logger.info("Backup restored: {}", originalFile);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        g_logger.error("Failed to restore backup: {}", e.what());
        return false;
    }
}

// 实现并发处理文件的协程版本
std::future<void> processFilesAsync(const std::vector<std::string>& filenames) {
    try {
        g_logger.info("processFilesAsync called with {} files",
                      filenames.size());

        // 输入验证
        if (filenames.empty()) {
            g_logger.error("Invalid argument: empty filenames list");
            std::promise<void> promise;
            promise.set_value();
            return promise.get_future();
        }

        // 创建一个promise来返回future
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        // 启动线程来处理协程
        std::thread([promise, filenames = filenames]() {
            try {
// 使用std::execution::par_unseq来并行处理文件
#if defined(__cpp_lib_parallel_algorithm)
                std::for_each(
                    std::execution::par_unseq, filenames.begin(),
                    filenames.end(), [](const std::string& filename) {
                        g_logger.info("Async processing file: {}", filename);
                        // 检查文件是否存在
                        if (!fs::exists(filename)) {
                            g_logger.error("File does not exist: {}", filename);
                            return;
                        }

                        std::ifstream file(filename, std::ios::binary);
                        if (!file) {
                            g_logger.error("Failed to open file: {}", filename);
                            return;
                        }

                        std::vector<unsigned char> chunkData(CHUNK_SIZE);
                        std::vector<unsigned char> outputBuffer(CHUNK_SIZE * 2);

                        while (
                            file.read(reinterpret_cast<char*>(chunkData.data()),
                                      CHUNK_SIZE) ||
                            file.gcount() > 0) {
                            // 处理每个数据块
                            if (!decompressChunk(chunkData, outputBuffer)) {
                                g_logger.error(
                                    "Failed to process chunk for file: {}",
                                    filename);
                            }
                        }
                        g_logger.info("Finished async processing file: {}",
                                      filename);
                    });
#else
                // 退回到标准线程池模式
                std::vector<std::future<void>> futures;
                for (const auto& filename : filenames) {
                    futures.push_back(
                        std::async(std::launch::async, [filename]() {
                            g_logger.info("Async processing file: {}",
                                          filename);
                            if (!fs::exists(filename)) {
                                g_logger.error("File does not exist: {}",
                                               filename);
                                return;
                            }

                            std::ifstream file(filename, std::ios::binary);
                            if (!file) {
                                g_logger.error("Failed to open file: {}",
                                               filename);
                                return;
                            }

                            std::vector<unsigned char> chunkData(CHUNK_SIZE);
                            std::vector<unsigned char> outputBuffer(CHUNK_SIZE *
                                                                    2);

                            while (file.read(reinterpret_cast<char*>(
                                                 chunkData.data()),
                                             CHUNK_SIZE) ||
                                   file.gcount() > 0) {
                                if (!decompressChunk(chunkData, outputBuffer)) {
                                    g_logger.error(
                                        "Failed to process chunk for file: {}",
                                        filename);
                                }
                            }
                            g_logger.info("Finished async processing file: {}",
                                          filename);
                        }));
                }

                // 等待所有任务完成
                for (auto& future : futures) {
                    future.get();
                }
#endif

                g_logger.info("All files processed asynchronously");
                promise->set_value();
            } catch (const std::exception& e) {
                g_logger.error("Exception in processFilesAsync thread: {}",
                               e.what());
                promise->set_exception(std::current_exception());
            }
        }).detach();

        return future;
    } catch (const std::exception& e) {
        g_logger.error("Exception in processFilesAsync: {}", e.what());
        std::promise<void> promise;
        promise.set_exception(std::current_exception());
        return promise.get_future();
    }
}

bool createBackup(const std::string& originalFile,
                  const std::string& backupFile, bool compress) {
    try {
        g_logger.info(
            "createBackup called with originalFile: {}, backupFile: {}, "
            "compress: {}",
            originalFile, backupFile, compress);

        // 输入验证
        if (originalFile.empty() || backupFile.empty()) {
            g_logger.error(
                "Invalid arguments: empty original file or backup file path");
            return false;
        }

        if (!fs::exists(originalFile)) {
            g_logger.error("Original file does not exist: {}", originalFile);
            return false;
        }

        // 创建备份目录（如果不存在）
        fs::path backupPath(backupFile);
        if (auto parentPath = backupPath.parent_path(); !parentPath.empty()) {
            if (!fs::exists(parentPath)) {
                fs::create_directories(parentPath);
            }
        }

        if (compress) {
            // 使用压缩功能创建备份
            std::ifstream inFile(originalFile, std::ios::binary);
            if (!inFile) {
                g_logger.error("Failed to open original file for reading: {}",
                               originalFile);
                return false;
            }

            std::unique_ptr<gzFile_s, ZLibCloser> out(
                gzopen(backupFile.c_str(), "wb"));
            if (!out) {
                g_logger.error("Failed to create compressed backup file: {}",
                               backupFile);
                return false;
            }

            std::array<char, CHUNK> buffer;
            while (inFile.read(buffer.data(), buffer.size())) {
                if (gzwrite(out.get(), buffer.data(),
                            static_cast<unsigned>(inFile.gcount())) == 0) {
                    g_logger.error(
                        "Failed to write to compressed backup file: {}",
                        backupFile);
                    return false;
                }
            }

            if (inFile.eof() && inFile.gcount() > 0) {
                if (gzwrite(out.get(), buffer.data(),
                            static_cast<unsigned>(inFile.gcount())) == 0) {
                    g_logger.error(
                        "Failed to write final chunk to compressed backup: {}",
                        backupFile);
                    return false;
                }
            }

            g_logger.info("Compressed backup created: {}", backupFile);
        } else {
            // 直接复制文件
            fs::copy(originalFile, backupFile,
                     fs::copy_options::overwrite_existing);
            g_logger.info("Backup created: {}", backupFile);
        }

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in createBackup: {}", e.what());
        return false;
    }
}

bool restoreBackup(const std::string& backupFile,
                   const std::string& originalFile, bool decompress) {
    try {
        g_logger.info(
            "restoreBackup called with backupFile: {}, originalFile: {}, "
            "decompress: {}",
            backupFile, originalFile, decompress);

        // 输入验证
        if (backupFile.empty() || originalFile.empty()) {
            g_logger.error(
                "Invalid arguments: empty backup file or original file path");
            return false;
        }

        if (!fs::exists(backupFile)) {
            g_logger.error("Backup file does not exist: {}", backupFile);
            return false;
        }

        // 创建目标目录（如果不存在）
        fs::path originalPath(originalFile);
        if (auto parentPath = originalPath.parent_path(); !parentPath.empty()) {
            if (!fs::exists(parentPath)) {
                fs::create_directories(parentPath);
            }
        }

        if (decompress) {
            // 解压缩备份
            std::unique_ptr<gzFile_s, ZLibCloser> in(
                gzopen(backupFile.c_str(), "rb"));
            if (!in) {
                g_logger.error("Failed to open compressed backup file: {}",
                               backupFile);
                return false;
            }

            std::unique_ptr<FILE, decltype([](FILE* f) {
                                if (f)
                                    fclose(f);
                            })>
                out(fopen(originalFile.c_str(), "wb"));

            if (!out) {
                g_logger.error("Failed to create original file: {}",
                               originalFile);
                return false;
            }

            std::array<char, CHUNK> buffer;
            int bytesRead;

            while ((bytesRead =
                        gzread(in.get(), buffer.data(), buffer.size())) > 0) {
                if (fwrite(buffer.data(), 1, bytesRead, out.get()) !=
                    static_cast<size_t>(bytesRead)) {
                    g_logger.error("Failed to write to original file: {}",
                                   originalFile);
                    return false;
                }
            }

            if (bytesRead < 0) {
                g_logger.error("Failed to read compressed backup file: {}",
                               backupFile);
                return false;
            }

            g_logger.info("Backup decompressed and restored: {}", originalFile);
        } else {
            // 直接复制文件
            fs::copy(backupFile, originalFile,
                     fs::copy_options::overwrite_existing);
            g_logger.info("Backup restored: {}", originalFile);
        }

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception in restoreBackup: {}", e.what());
        return false;
    }
}

// 通用数据压缩模板实现
template <typename T>
    requires std::ranges::contiguous_range<T>
std::vector<unsigned char> compressData(const T& data) {
    try {
        g_logger.info("compressData called with data of size {}",
                      std::size(data));

        // 输入验证
        if (std::empty(data)) {
            g_logger.error("Empty data provided for compression");
            return {};
        }

        // 计算压缩后可能的最大大小
        uLong sourceSize = static_cast<uLong>(std::size(data) *
                                              sizeof(typename T::value_type));
        uLong destSize = compressBound(sourceSize);

        // 准备输出缓冲区
        std::vector<unsigned char> compressedData(destSize);

        // 压缩数据
        int result = compress2(compressedData.data(), &destSize,
                               reinterpret_cast<const Bytef*>(std::data(data)),
                               sourceSize, Z_BEST_COMPRESSION);

        if (result != Z_OK) {
            g_logger.error("Compression failed with error code: {}", result);
            return {};
        }

        // 调整缓冲区大小以匹配实际压缩后的大小
        compressedData.resize(destSize);
        g_logger.info(
            "Data compressed successfully. Original size: {}, compressed size: "
            "{}",
            sourceSize, destSize);

        return compressedData;
    } catch (const std::exception& e) {
        g_logger.error("Exception in compressData: {}", e.what());
        return {};
    }
}

// 通用数据解压缩模板实现
template <typename T>
    requires std::ranges::contiguous_range<T>
std::vector<unsigned char> decompressData(const T& compressedData,
                                          size_t expectedSize) {
    try {
        g_logger.info("decompressData called with compressed data of size {}",
                      std::size(compressedData));

        // 输入验证
        if (std::empty(compressedData)) {
            g_logger.error("Empty compressed data provided for decompression");
            return {};
        }

        // 如果未知预期大小，先尝试一个合理的大小再动态调整
        size_t outputSize =
            expectedSize > 0 ? expectedSize : std::size(compressedData) * 4;
        std::vector<unsigned char> decompressedData(outputSize);
        uLongf destLen = static_cast<uLongf>(outputSize);

        int result = uncompress(
            decompressedData.data(), &destLen,
            reinterpret_cast<const Bytef*>(std::data(compressedData)),
            static_cast<uLong>(std::size(compressedData)));

        if (result == Z_BUF_ERROR && expectedSize == 0) {
            // 缓冲区不足，尝试更大的缓冲区
            g_logger.info("Buffer too small, trying with larger buffer");
            outputSize *= 4;  // 扩大4倍
            decompressedData.resize(outputSize);
            destLen = static_cast<uLongf>(outputSize);

            result = uncompress(
                decompressedData.data(), &destLen,
                reinterpret_cast<const Bytef*>(std::data(compressedData)),
                static_cast<uLong>(std::size(compressedData)));
        }

        if (result != Z_OK) {
            g_logger.error("Decompression failed with error code: {}", result);
            return {};
        }

        // 调整缓冲区大小以匹配实际解压后的大小
        decompressedData.resize(destLen);
        g_logger.info(
            "Data decompressed successfully. Compressed size: {}, decompressed "
            "size: {}",
            std::size(compressedData), destLen);

        return decompressedData;
    } catch (const std::exception& e) {
        g_logger.error("Exception in decompressData: {}", e.what());
        return {};
    }
}

// 显式模板实例化，以支持常用类型
template std::vector<unsigned char> compressData<std::vector<unsigned char>>(
    const std::vector<unsigned char>& data);
template std::vector<unsigned char> compressData<std::string>(
    const std::string& data);
template std::vector<unsigned char> compressData<
    std::span<const unsigned char>>(const std::span<const unsigned char>& data);

template std::vector<unsigned char> decompressData<std::vector<unsigned char>>(
    const std::vector<unsigned char>& compressedData, size_t expectedSize);
template std::vector<unsigned char> decompressData<std::string>(
    const std::string& compressedData, size_t expectedSize);
template std::vector<unsigned char>
decompressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>& compressedData, size_t expectedSize);
}  // namespace atom::io
