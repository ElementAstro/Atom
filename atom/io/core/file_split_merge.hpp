/*
 * file_split_merge.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file file_split_merge.hpp
 * @brief File splitting and merging operations.
 */

#ifndef ATOM_IO_CORE_FILE_SPLIT_MERGE_HPP
#define ATOM_IO_CORE_FILE_SPLIT_MERGE_HPP

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "atom/io/core/types.hpp"
#include "atom/io/core/file_query.hpp"

namespace atom::io {

/**
 * @brief Calculate the chunk size.
 *
 * @param fileSize The file size.
 * @param numChunks The number of chunks.
 * @return The chunk size.
 */
[[nodiscard]] constexpr auto calculateChunkSize(std::size_t fileSize,
                                                int numChunks) -> std::size_t;

/**
 * @brief Split a file into multiple parts.
 *
 * @param filePath The file path.
 * @param chunkSize The chunk size.
 * @param outputPattern The output file pattern.
 */
template <PathLike P1, PathLike P2 = const char*>
void splitFile(const P1& filePath, std::size_t chunkSize,
               const P2& outputPattern = "");

/**
 * @brief Merge multiple parts into a single file.
 *
 * @param outputFilePath The output file path.
 * @param partFiles The part files.
 */
template <PathLike P>
void mergeFiles(const P& outputFilePath,
                std::span<const std::string> partFiles);

/**
 * @brief Quickly split a file into multiple parts.
 *
 * @param filePath The file path.
 * @param numChunks The number of chunks.
 * @param outputPattern The output file pattern.
 */
template <PathLike P1, PathLike P2 = const char*>
void quickSplit(const P1& filePath, int numChunks,
                const P2& outputPattern = "");

/**
 * @brief Quickly merge multiple parts into a single file.
 *
 * @param outputFilePath The output file path.
 * @param partPattern The part file pattern.
 * @param numChunks The number of chunks.
 */
template <PathLike P1, PathLike P2>
void quickMerge(const P1& outputFilePath, const P2& partPattern, int numChunks);

// ---------------------------------------------------------------------------
// Constexpr implementations
// ---------------------------------------------------------------------------

[[nodiscard]] constexpr auto calculateChunkSize(std::size_t fileSize,
                                                int numChunks) -> std::size_t {
    // Use std::max to ensure we don't divide by zero
    return fileSize / std::max(1, numChunks) +
           (fileSize % std::max(1, numChunks) != 0);
}

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template <PathLike P1, PathLike P2>
void splitFile(const P1& filePath, std::size_t chunkSize,
               const P2& outputPattern) {
    spdlog::info(
        "splitFile called with filePath: {}, chunkSize: {}, outputPattern: {}",
        fs::path(filePath).string(), chunkSize, std::string(outputPattern));

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            spdlog::error("File does not exist: {}", path.string());
            return;
        }

        std::ifstream inputFile(path, std::ios::binary);
        if (!inputFile) {
            spdlog::error("Failed to open file: {}", path.string());
            return;
        }

        std::size_t fileSize = getFileSize(path);
        if (fileSize == 0) {
            spdlog::error("File is empty or couldn't determine size: {}",
                          path.string());
            return;
        }

        // Use a buffer with smart pointer for automatic cleanup
        auto buffer = std::make_unique<char[]>(chunkSize);
        int partNumber = 0;

        // Process file in chunks
        std::size_t remainingSize = fileSize;
        while (remainingSize > 0) {
            std::ostringstream partFileName;
            std::string outputBase = std::string(outputPattern).empty()
                                         ? path.string()
                                         : std::string(outputPattern);
            partFileName << outputBase << ".part" << partNumber;

            std::ofstream outputFile(partFileName.str(), std::ios::binary);
            if (!outputFile) {
                spdlog::error("Failed to create part file: {}",
                              partFileName.str());
                return;
            }

            std::size_t bytesToRead = std::min(chunkSize, remainingSize);
            inputFile.read(buffer.get(), bytesToRead);
            if (inputFile.fail() && !inputFile.eof()) {
                spdlog::error("Error reading from file: {}", path.string());
                return;
            }

            outputFile.write(buffer.get(), inputFile.gcount());

            remainingSize -= bytesToRead;
            ++partNumber;
        }

        spdlog::info("File split into {} parts", partNumber);
    } catch (const std::exception& e) {
        spdlog::error("Error splitting file {}: {}",
                      fs::path(filePath).string(), e.what());
    } catch (...) {
        spdlog::error("Unknown error splitting file {}",
                      fs::path(filePath).string());
    }
}

template <PathLike P>
void mergeFiles(const P& outputFilePath,
                std::span<const std::string> partFiles) {
    spdlog::info(
        "mergeFiles called with outputFilePath: {}, partFiles size: {}",
        fs::path(outputFilePath).string(), partFiles.size());

    try {
        fs::path outPath(outputFilePath);

        // Create parent directory if it doesn't exist
        fs::path outDir = outPath.parent_path();
        if (!outDir.empty()) {
            std::error_code ec;
            fs::create_directories(outDir, ec);
            if (ec) {
                spdlog::error("Failed to create output directory {}: {}",
                              outDir.string(), ec.message());
                return;
            }
        }

        std::ofstream outputFile(outPath, std::ios::binary);
        if (!outputFile) {
            spdlog::error("Failed to create output file: {}", outPath.string());
            return;
        }

        // Use a 64KB buffer for efficient file I/O
        constexpr std::size_t bufferSize = 65536;
        auto buffer = std::make_unique<char[]>(bufferSize);

        // Process each part file
        for (const auto& partFile : partFiles) {
            std::ifstream inputFile(partFile, std::ios::binary);
            if (!inputFile) {
                spdlog::error("Failed to open part file: {}", partFile);
                return;
            }

            while (inputFile) {
                inputFile.read(buffer.get(), bufferSize);
                std::streamsize bytesRead = inputFile.gcount();
                if (bytesRead > 0) {
                    outputFile.write(buffer.get(), bytesRead);
                    if (outputFile.fail()) {
                        spdlog::error("Error writing to output file: {}",
                                      outPath.string());
                        return;
                    }
                }
            }
        }

        spdlog::info("Files merged successfully into {}", outPath.string());
    } catch (const std::exception& e) {
        spdlog::error("Error merging files to {}: {}",
                      fs::path(outputFilePath).string(), e.what());
    } catch (...) {
        spdlog::error("Unknown error merging files to {}",
                      fs::path(outputFilePath).string());
    }
}

template <PathLike P1, PathLike P2>
void quickSplit(const P1& filePath, int numChunks, const P2& outputPattern) {
    spdlog::info(
        "quickSplit called with filePath: {}, numChunks: {}, outputPattern: {}",
        fs::path(filePath).string(), numChunks, std::string(outputPattern));

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            spdlog::error("File does not exist: {}", path.string());
            return;
        }

        std::size_t fileSize = getFileSize(path);
        if (fileSize == 0) {
            spdlog::error("File is empty or couldn't determine size: {}",
                          path.string());
            return;
        }

        std::size_t chunkSize = calculateChunkSize(fileSize, numChunks);
        spdlog::info("Calculated chunk size: {} bytes for {} chunks", chunkSize,
                     numChunks);

        splitFile(path, chunkSize, outputPattern);
    } catch (const std::exception& e) {
        spdlog::error("Error in quickSplit for {}: {}",
                      fs::path(filePath).string(), e.what());
    } catch (...) {
        spdlog::error("Unknown error in quickSplit for {}",
                      fs::path(filePath).string());
    }
}

template <PathLike P1, PathLike P2>
void quickMerge(const P1& outputFilePath, const P2& partPattern,
                int numChunks) {
    spdlog::info(
        "quickMerge called with outputFilePath: {}, partPattern: {}, "
        "numChunks: {}",
        fs::path(outputFilePath).string(), std::string(partPattern), numChunks);

    try {
        if (numChunks <= 0) {
            spdlog::error("Invalid number of chunks: {}", numChunks);
            return;
        }

        std::vector<std::string> partFiles;
        partFiles.reserve(numChunks);

        for (int i = 0; i < numChunks; ++i) {
            std::ostringstream partFileName;
            partFileName << std::string(partPattern) << ".part" << i;
            partFiles.push_back(partFileName.str());
        }

        mergeFiles(outputFilePath, partFiles);
    } catch (const std::exception& e) {
        spdlog::error("Error in quickMerge for {}: {}",
                      fs::path(outputFilePath).string(), e.what());
    } catch (...) {
        spdlog::error("Unknown error in quickMerge for {}",
                      fs::path(outputFilePath).string());
    }
}

}  // namespace atom::io

#endif  // ATOM_IO_CORE_FILE_SPLIT_MERGE_HPP
