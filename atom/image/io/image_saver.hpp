#ifndef ATOM_IMAGE_SAVER_HPP
#define ATOM_IMAGE_SAVER_HPP

/**
 * @file image_saver.hpp
 * @brief Advanced image saving operations
 *
 * This module provides comprehensive image saving capabilities with support
 * for multiple formats, compression options, metadata preservation, and
 * batch operations.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/image_blob.hpp"
#include "../exceptions.hpp"
#include "format_detector.hpp"

namespace atom::image {

/**
 * @brief Image compression types
 */
enum class CompressionType {
    NONE,      // No compression
    LOSSLESS,  // Lossless compression
    LOSSY,     // Lossy compression
    ADAPTIVE,  // Adaptive compression based on content
    CUSTOM     // Custom compression settings
};

/**
 * @brief Image saving options
 */
struct SaveOptions {
    ImageFormat targetFormat =
        ImageFormat::UNKNOWN;  // Auto-detect from extension if UNKNOWN
    CompressionType compression = CompressionType::ADAPTIVE;
    int quality = 95;                  // Quality for lossy formats (0-100)
    bool preserveMetadata = true;      // Preserve original metadata
    bool optimizeSize = false;         // Optimize for file size
    bool progressiveEncoding = false;  // Use progressive encoding (JPEG)
    int compressionLevel = 6;          // Compression level (0-9)
    std::unordered_map<std::string, std::string>
        customMetadata;  // Additional metadata
    std::unordered_map<std::string, std::string>
        formatOptions;                  // Format-specific options
    bool overwriteExisting = true;      // Overwrite existing files
    bool createDirectories = true;      // Create directories if needed
    std::string backupSuffix = ".bak";  // Backup suffix for existing files

    // Default constructor
    SaveOptions() = default;
};

/**
 * @brief Saving progress callback
 */
using SaveProgressCallback =
    std::function<void(float progress, const std::string& status)>;

/**
 * @brief Saving result
 */
struct SaveResult {
    std::filesystem::path savedPath;
    ImageFormat usedFormat = ImageFormat::UNKNOWN;
    size_t outputSize = 0;
    std::string errorMessage;
    bool success = false;
    std::chrono::milliseconds saveTime{0};
    float compressionRatio = 1.0f;
};

/**
 * @brief Batch saving result
 */
struct BatchSaveResult {
    std::vector<SaveResult> results;
    size_t successCount = 0;
    size_t failureCount = 0;
    std::chrono::milliseconds totalTime{0};
    size_t totalOutputSize = 0;
};

/**
 * @brief Advanced image saver
 */
class ImageSaver {
public:
    ImageSaver();
    virtual ~ImageSaver() = default;

    /**
     * @brief Save image to file
     * @param imageData Image data to save
     * @param filePath Output file path
     * @param options Saving options
     * @param progressCallback Optional progress callback
     * @return Saving result
     */
    virtual SaveResult saveToFile(
        const blob& imageData, const std::filesystem::path& filePath,
        const SaveOptions& options = {},
        SaveProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Save image to memory buffer
     * @param imageData Image data to save
     * @param format Target format
     * @param options Saving options
     * @param progressCallback Optional progress callback
     * @return Encoded image data and result
     */
    virtual std::pair<std::vector<uint8_t>, SaveResult> saveToMemory(
        const blob& imageData, ImageFormat format,
        const SaveOptions& options = {},
        SaveProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Save multiple images in batch
     * @param imageData Vector of image data
     * @param filePaths Vector of output file paths
     * @param options Saving options
     * @param maxConcurrency Maximum concurrent operations
     * @param progressCallback Optional progress callback
     * @return Batch saving result
     */
    virtual BatchSaveResult saveBatch(
        const std::vector<blob>& imageData,
        const std::vector<std::filesystem::path>& filePaths,
        const SaveOptions& options = {}, size_t maxConcurrency = 4,
        SaveProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Save image asynchronously
     * @param imageData Image data to save
     * @param filePath Output file path
     * @param options Saving options
     * @param progressCallback Optional progress callback
     * @return Future containing saving result
     */
    virtual std::future<SaveResult> saveAsync(
        const blob& imageData, const std::filesystem::path& filePath,
        const SaveOptions& options = {},
        SaveProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Check if format is supported for saving
     * @param format Image format to check
     * @return True if format can be saved
     */
    virtual bool canSave(ImageFormat format) const;

    /**
     * @brief Get supported formats for saving
     * @return Vector of supported formats
     */
    virtual std::vector<ImageFormat> getSupportedFormats() const;

    /**
     * @brief Get optimal format for image content
     * @param imageData Image data to analyze
     * @param preferLossless Prefer lossless formats
     * @return Recommended format
     */
    virtual ImageFormat getOptimalFormat(const blob& imageData,
                                         bool preferLossless = false) const;

    /**
     * @brief Estimate output size for format and options
     * @param imageData Image data
     * @param format Target format
     * @param options Saving options
     * @return Estimated output size in bytes
     */
    virtual size_t estimateOutputSize(const blob& imageData, ImageFormat format,
                                      const SaveOptions& options) const;

    /**
     * @brief Register custom format saver
     * @param format Image format
     * @param saver Custom saver function
     */
    virtual void registerCustomSaver(
        ImageFormat format,
        std::function<SaveResult(const blob&, const SaveOptions&)> saver);

protected:
    /**
     * @brief Save specific format
     * @param imageData Image data to save
     * @param format Target format
     * @param options Saving options
     * @return Encoded data and result
     */
    virtual std::pair<std::vector<uint8_t>, SaveResult> saveFormat(
        const blob& imageData, ImageFormat format,
        const SaveOptions& options) const;

    /**
     * @brief Apply pre-processing before saving
     * @param imageData Image data to process
     * @param options Saving options
     * @return Processed image data
     */
    virtual blob applyPreProcessing(const blob& imageData,
                                    const SaveOptions& options) const;

    /**
     * @brief Write metadata to encoded data
     * @param encodedData Encoded image data
     * @param format Image format
     * @param metadata Metadata to write
     * @return Data with embedded metadata
     */
    virtual std::vector<uint8_t> embedMetadata(
        const std::vector<uint8_t>& encodedData, ImageFormat format,
        const std::unordered_map<std::string, std::string>& metadata) const;

    /**
     * @brief Create backup of existing file
     * @param filePath File to backup
     * @param backupSuffix Backup suffix
     * @return Success status
     */
    virtual bool createBackup(const std::filesystem::path& filePath,
                              const std::string& backupSuffix) const;

private:
    std::unique_ptr<FormatDetector> formatDetector_;
    std::unordered_map<
        ImageFormat, std::function<SaveResult(const blob&, const SaveOptions&)>>
        customSavers_;
};

/**
 * @brief Create optimized image saver
 * @return Unique pointer to image saver
 */
std::unique_ptr<ImageSaver> createImageSaver();

/**
 * @brief Quick image saving function
 * @param imageData Image data to save
 * @param filePath Output file path
 * @param quality Quality for lossy formats (0-100)
 * @return Success status
 */
bool quickSaveImage(const blob& imageData,
                    const std::filesystem::path& filePath, int quality = 95);

/**
 * @brief Quick batch image saving
 * @param imageData Vector of image data
 * @param filePaths Vector of output file paths
 * @param quality Quality for lossy formats (0-100)
 * @param maxConcurrency Maximum concurrent operations
 * @return Number of successfully saved images
 */
size_t quickSaveBatch(const std::vector<blob>& imageData,
                      const std::vector<std::filesystem::path>& filePaths,
                      int quality = 95, size_t maxConcurrency = 4);

}  // namespace atom::image

#endif  // ATOM_IMAGE_SAVER_HPP
