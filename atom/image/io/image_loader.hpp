#ifndef ATOM_IMAGE_LOADER_HPP
#define ATOM_IMAGE_LOADER_HPP

/**
 * @file image_loader.hpp
 * @brief Advanced image loading operations
 *
 * This module provides comprehensive image loading capabilities with support
 * for multiple formats, streaming I/O, memory mapping, and batch operations.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include "../core/image_blob.hpp"
#include "format_detector.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>
#include <future>
#include <unordered_map>

namespace atom::image {

/**
 * @brief Image loading options
 */
struct LoadOptions {
    ImageFormat preferredFormat = ImageFormat::UNKNOWN;  // Auto-detect if UNKNOWN
    bool convertToRGB = false;                           // Convert to RGB color space
    bool normalizePixels = false;                        // Normalize pixel values to [0,1]
    int targetWidth = -1;                               // Resize to target width (-1 = no resize)
    int targetHeight = -1;                              // Resize to target height (-1 = no resize)
    bool preserveAspectRatio = true;                    // Preserve aspect ratio during resize
    bool useMemoryMapping = false;                      // Use memory mapping for large files
    size_t maxMemoryUsage = 1024 * 1024 * 1024;       // Maximum memory usage (1GB)
    bool enableCaching = true;                          // Enable result caching
    std::string cacheKey;                              // Custom cache key
    std::unordered_map<std::string, std::string> customOptions;  // Format-specific options
};

/**
 * @brief Loading progress callback
 */
using ProgressCallback = std::function<void(float progress, const std::string& status)>;

/**
 * @brief Loading result
 */
struct LoadResult {
    blob imageData;
    ImageFormat detectedFormat = ImageFormat::UNKNOWN;
    std::unordered_map<std::string, std::string> metadata;
    std::string errorMessage;
    bool success = false;
    std::chrono::milliseconds loadTime{0};
};

/**
 * @brief Batch loading result
 */
struct BatchLoadResult {
    std::vector<LoadResult> results;
    size_t successCount = 0;
    size_t failureCount = 0;
    std::chrono::milliseconds totalTime{0};
};

/**
 * @brief Advanced image loader
 */
class ImageLoader {
public:
    ImageLoader();
    virtual ~ImageLoader() = default;

    /**
     * @brief Load image from file
     * @param filePath Path to image file
     * @param options Loading options
     * @param progressCallback Optional progress callback
     * @return Loading result
     */
    virtual LoadResult loadFromFile(const std::filesystem::path& filePath,
                                   const LoadOptions& options = {},
                                   ProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Load image from memory buffer
     * @param data Pointer to image data
     * @param size Size of data buffer
     * @param options Loading options
     * @param progressCallback Optional progress callback
     * @return Loading result
     */
    virtual LoadResult loadFromMemory(const void* data, size_t size,
                                     const LoadOptions& options = {},
                                     ProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Load image from URL (HTTP/HTTPS)
     * @param url Image URL
     * @param options Loading options
     * @param progressCallback Optional progress callback
     * @return Loading result
     */
    virtual LoadResult loadFromURL(const std::string& url,
                                  const LoadOptions& options = {},
                                  ProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Load multiple images in batch
     * @param filePaths Vector of file paths
     * @param options Loading options
     * @param maxConcurrency Maximum concurrent operations
     * @param progressCallback Optional progress callback
     * @return Batch loading result
     */
    virtual BatchLoadResult loadBatch(const std::vector<std::filesystem::path>& filePaths,
                                     const LoadOptions& options = {},
                                     size_t maxConcurrency = 4,
                                     ProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Load image asynchronously
     * @param filePath Path to image file
     * @param options Loading options
     * @param progressCallback Optional progress callback
     * @return Future containing loading result
     */
    virtual std::future<LoadResult> loadAsync(const std::filesystem::path& filePath,
                                             const LoadOptions& options = {},
                                             ProgressCallback progressCallback = nullptr) const;

    /**
     * @brief Check if file can be loaded
     * @param filePath Path to image file
     * @return True if file can be loaded
     */
    virtual bool canLoad(const std::filesystem::path& filePath) const;

    /**
     * @brief Get supported formats for loading
     * @return Vector of supported formats
     */
    virtual std::vector<ImageFormat> getSupportedFormats() const;

    /**
     * @brief Set cache size limit
     * @param maxSize Maximum cache size in bytes
     */
    virtual void setCacheSize(size_t maxSize);

    /**
     * @brief Clear image cache
     */
    virtual void clearCache();

    /**
     * @brief Get cache statistics
     * @return Cache statistics as key-value pairs
     */
    virtual std::unordered_map<std::string, size_t> getCacheStats() const;

    /**
     * @brief Register custom format loader
     * @param format Image format
     * @param loader Custom loader function
     */
    virtual void registerCustomLoader(ImageFormat format,
                                     std::function<LoadResult(const void*, size_t, const LoadOptions&)> loader);

protected:
    /**
     * @brief Load specific format
     * @param data Image data
     * @param size Data size
     * @param format Image format
     * @param options Loading options
     * @return Loading result
     */
    virtual LoadResult loadFormat(const void* data, size_t size,
                                 ImageFormat format, const LoadOptions& options) const;

    /**
     * @brief Apply post-processing options
     * @param imageData Image data to process
     * @param options Loading options
     * @return Processed image data
     */
    virtual blob applyPostProcessing(const blob& imageData, const LoadOptions& options) const;

    /**
     * @brief Extract metadata from image data
     * @param data Image data
     * @param size Data size
     * @param format Image format
     * @return Metadata map
     */
    virtual std::unordered_map<std::string, std::string> extractMetadata(
        const void* data, size_t size, ImageFormat format) const;

private:
    std::unique_ptr<FormatDetector> formatDetector_;
    std::unordered_map<ImageFormat, std::function<LoadResult(const void*, size_t, const LoadOptions&)>> customLoaders_;
    
    // Cache management
    mutable std::unordered_map<std::string, blob> imageCache_;
    mutable size_t cacheSize_ = 0;
    size_t maxCacheSize_ = 100 * 1024 * 1024;  // 100MB default
    
    // Statistics
    mutable size_t cacheHits_ = 0;
    mutable size_t cacheMisses_ = 0;
};

/**
 * @brief Create optimized image loader
 * @return Unique pointer to image loader
 */
std::unique_ptr<ImageLoader> createImageLoader();

/**
 * @brief Quick image loading function
 * @param filePath Path to image file
 * @return Loaded image blob (empty if failed)
 */
blob quickLoadImage(const std::filesystem::path& filePath);

/**
 * @brief Quick batch image loading
 * @param filePaths Vector of file paths
 * @param maxConcurrency Maximum concurrent operations
 * @return Vector of loaded image blobs
 */
std::vector<blob> quickLoadBatch(const std::vector<std::filesystem::path>& filePaths,
                                size_t maxConcurrency = 4);

}  // namespace atom::image

#endif  // ATOM_IMAGE_LOADER_HPP
