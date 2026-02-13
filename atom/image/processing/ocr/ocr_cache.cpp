#include "ocr_cache.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

#include <opencv2/imgcodecs.hpp>

namespace atom::image::ocr {

std::string OCRCache::calculateHash(const cv::Mat& img) const {
    std::vector<uint8_t> buffer;
    cv::imencode(".jpg", img, buffer);

    // Simple hash function
    size_t hash = 0;
    for (const auto& byte : buffer) {
        hash = (hash * 31) + byte;
    }

    return std::to_string(hash);
}

fs::path OCRCache::getCacheFilePath(const std::string& key) const {
    return fs::path(m_cacheDir) / (key + ".txt");
}

OCRCache::OCRCache(const std::string& cacheDir, size_t maxCacheSize)
    : m_cacheDir(cacheDir), m_maxCacheSize(maxCacheSize) {
    if (!fs::exists(m_cacheDir)) {
        fs::create_directories(m_cacheDir);
    }
}

std::optional<std::string> OCRCache::get(const cv::Mat& img) {
    std::string key = calculateHash(img);

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Check memory cache first
    auto memIter = m_memoryCache.find(key);
    if (memIter != m_memoryCache.end()) {
        return memIter->second;
    }

    // Check file cache
    fs::path cachePath = getCacheFilePath(key);
    if (fs::exists(cachePath)) {
        std::ifstream file(cachePath);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

            // Update memory cache for small results
            if (content.size() < 1024 * 10) {
                m_memoryCache[key] = content;
            }

            return content;
        }
    }

    return std::nullopt;
}

void OCRCache::store(const cv::Mat& img, const std::string& result) {
    std::string key = calculateHash(img);

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Update memory cache for small results
    if (result.size() < 1024 * 10) {
        m_memoryCache[key] = result;
    }

    // Update file cache
    fs::path cachePath = getCacheFilePath(key);
    std::ofstream file(cachePath);
    if (file) {
        file << result;
    }

    cleanCacheIfNeeded();
}

void OCRCache::cleanCacheIfNeeded() {
    size_t totalSize = 0;
    std::vector<std::pair<fs::path, std::filesystem::file_time_type>> files;

    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) {
            totalSize += entry.file_size();
            files.emplace_back(entry.path(), entry.last_write_time());
        }
    }

    if (totalSize > m_maxCacheSize) {
        // Sort by last write time (oldest first)
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        // Remove oldest files until under limit
        for (const auto& [path, time] : files) {
            if (totalSize <= m_maxCacheSize * 0.8) {
                break;
            }

            totalSize -= fs::file_size(path);
            fs::remove(path);
        }
    }
}

void OCRCache::clear() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_memoryCache.clear();

    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) {
            fs::remove(entry.path());
        }
    }
}

size_t OCRCache::getCurrentSize() const {
    size_t totalSize = 0;
    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) {
            totalSize += entry.file_size();
        }
    }
    return totalSize;
}

size_t OCRCache::getItemCount() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_memoryCache.size();
}

bool OCRCache::contains(const cv::Mat& img) const {
    std::string key = calculateHash(img);

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    if (m_memoryCache.find(key) != m_memoryCache.end()) {
        return true;
    }

    return fs::exists(getCacheFilePath(key));
}

}  // namespace atom::image::ocr
