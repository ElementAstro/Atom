/**
 * @file ocr_cache.hpp
 * @brief Caching system for OCR results
 */

#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <opencv2/core.hpp>

namespace fs = std::filesystem;

namespace atom::image::ocr {

/**
 * @class OCRCache
 * @brief Caching system for OCR results
 *
 * Implements both in-memory and filesystem caching of OCR results
 * to improve performance on repeated processing of the same images.
 */
class OCRCache {
public:
    /**
     * @brief Construct a new OCRCache
     * @param cacheDir Cache directory path
     * @param maxCacheSize Maximum cache size in bytes
     */
    OCRCache(const std::string& cacheDir, size_t maxCacheSize);

    ~OCRCache() = default;

    /**
     * @brief Get cached result if available
     * @param img Input image to check
     * @return Optional containing result if found, empty otherwise
     */
    std::optional<std::string> get(const cv::Mat& img);

    /**
     * @brief Store result in cache
     * @param img Input image
     * @param result OCR result to cache
     */
    void store(const cv::Mat& img, const std::string& result);

    /**
     * @brief Clean cache if over size limit
     *
     * Removes oldest cache entries until under size limit
     */
    void cleanCacheIfNeeded();

    /**
     * @brief Clear all cached results
     */
    void clear();

    /**
     * @brief Get current cache size in bytes
     * @return Cache size in bytes
     */
    size_t getCurrentSize() const;

    /**
     * @brief Get number of cached items
     * @return Number of items in memory cache
     */
    size_t getItemCount() const;

    /**
     * @brief Check if cache contains result for image
     * @param img Input image
     * @return True if result is cached
     */
    bool contains(const cv::Mat& img) const;

private:
    std::unordered_map<std::string, std::string>
        m_memoryCache;                ///< In-memory cache
    std::string m_cacheDir;           ///< Filesystem cache directory
    size_t m_maxCacheSize;            ///< Maximum cache size in bytes
    mutable std::mutex m_cacheMutex;  ///< Mutex for thread safety

    /**
     * @brief Calculate hash of image for cache key
     * @param img Input image
     * @return Hash string
     */
    std::string calculateHash(const cv::Mat& img) const;

    /**
     * @brief Get filesystem path for cached result
     * @param key Cache key (image hash)
     * @return Full filesystem path to cache file
     */
    fs::path getCacheFilePath(const std::string& key) const;
};

}  // namespace atom::image::ocr
