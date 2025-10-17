#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace atom::image::core {

/**
 * @brief Cache policy for image cache management
 */
enum class CachePolicy {
    LRU,        ///< Least Recently Used
    LFU,        ///< Least Frequently Used
    FIFO,       ///< First In First Out
    RANDOM      ///< Random replacement
};

/**
 * @brief Cache statistics
 */
struct CacheStats {
    size_t hitCount = 0;
    size_t missCount = 0;
    size_t evictionCount = 0;
    size_t currentSize = 0;
    size_t maxSize = 0;
    double hitRatio() const {
        return (hitCount + missCount > 0) ?
               static_cast<double>(hitCount) / (hitCount + missCount) : 0.0;
    }
};

/**
 * @brief Cache manager for image data
 * @tparam T Type of cached data
 */
template<typename T>
class CacheManager {
public:
    /**
     * @brief Constructor
     * @param maxSize Maximum cache size in bytes
     * @param policy Cache replacement policy
     */
    explicit CacheManager(size_t maxSize = 100 * 1024 * 1024,
                         CachePolicy policy = CachePolicy::LRU);

    /**
     * @brief Destructor
     */
    ~CacheManager() = default;

    // Copy and move operations
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;
    CacheManager(CacheManager&&) = default;
    CacheManager& operator=(CacheManager&&) = default;

    /**
     * @brief Put data into cache
     * @param key Cache key
     * @param data Data to cache
     * @param size Size of data in bytes
     */
    void put(const std::string& key, std::shared_ptr<T> data, size_t size = 0);

    /**
     * @brief Get data from cache
     * @param key Cache key
     * @return Shared pointer to cached data, or nullptr if not found
     */
    std::shared_ptr<T> get(const std::string& key);

    /**
     * @brief Remove data from cache
     * @param key Cache key
     * @return True if data was removed
     */
    bool remove(const std::string& key);

    /**
     * @brief Clear all cache entries
     */
    void clear();

    /**
     * @brief Check if key exists in cache
     * @param key Cache key
     * @return True if key exists
     */
    bool contains(const std::string& key) const;

    /**
     * @brief Get cache statistics
     * @return Cache statistics
     */
    CacheStats getStats() const;

    /**
     * @brief Set maximum cache size
     * @param maxSize Maximum size in bytes
     */
    void setMaxSize(size_t maxSize);

    /**
     * @brief Set cache policy
     * @param policy Cache replacement policy
     */
    void setPolicy(CachePolicy policy);

    /**
     * @brief Get current cache size
     * @return Current cache size in bytes
     */
    size_t getCurrentSize() const;

    /**
     * @brief Get number of cached items
     * @return Number of cached items
     */
    size_t getItemCount() const;

private:
    struct CacheEntry {
        std::shared_ptr<T> data;
        size_t size;
        std::chrono::steady_clock::time_point lastAccess;
        size_t accessCount;

        CacheEntry(std::shared_ptr<T> d, size_t s)
            : data(std::move(d)), size(s),
              lastAccess(std::chrono::steady_clock::now()),
              accessCount(1) {}
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    size_t maxSize_;
    size_t currentSize_;
    CachePolicy policy_;
    CacheStats stats_;

    void evictIfNeeded();
    std::string selectEvictionCandidate();
    void updateAccessOrder(const std::string& key);
};

// Template implementations

template<typename T>
CacheManager<T>::CacheManager(size_t maxSize, CachePolicy policy)
    : maxSize_(maxSize), currentSize_(0), policy_(policy) {}

template<typename T>
void CacheManager<T>::put(const std::string& key, std::shared_ptr<T> data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        currentSize_ -= it->second.size;
        cache_.erase(it);
    }

    cache_.emplace(key, CacheEntry(std::move(data), size));
    currentSize_ += size;

    evictIfNeeded();
}

template<typename T>
std::shared_ptr<T> CacheManager<T>::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        updateAccessOrder(key);
        it->second.accessCount++;
        it->second.lastAccess = std::chrono::steady_clock::now();
        stats_.hitCount++;
        return it->second.data;
    }

    stats_.missCount++;
    return nullptr;
}

template<typename T>
bool CacheManager<T>::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        currentSize_ -= it->second.size;
        cache_.erase(it);
        return true;
    }
    return false;
}

template<typename T>
void CacheManager<T>::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    currentSize_ = 0;
    stats_.evictionCount = 0;
}

template<typename T>
bool CacheManager<T>::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

template<typename T>
CacheStats CacheManager<T>::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheStats result = stats_;
    result.currentSize = currentSize_;
    result.maxSize = maxSize_;
    return result;
}

template<typename T>
void CacheManager<T>::setMaxSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxSize_ = maxSize;
    evictIfNeeded();
}

template<typename T>
void CacheManager<T>::setPolicy(CachePolicy policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_ = policy;
}

template<typename T>
size_t CacheManager<T>::getCurrentSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSize_;
}

template<typename T>
size_t CacheManager<T>::getItemCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

template<typename T>
void CacheManager<T>::evictIfNeeded() {
    while (currentSize_ > maxSize_ && !cache_.empty()) {
        std::string key = selectEvictionCandidate();
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            currentSize_ -= it->second.size;
            cache_.erase(it);
            stats_.evictionCount++;
        }
    }
}

template<typename T>
std::string CacheManager<T>::selectEvictionCandidate() {
    switch (policy_) {
        case CachePolicy::LRU: {
            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.lastAccess < oldest->second.lastAccess) {
                    oldest = it;
                }
            }
            return oldest->first;
        }
        case CachePolicy::LFU: {
            auto leastUsed = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.accessCount < leastUsed->second.accessCount) {
                    leastUsed = it;
                }
            }
            return leastUsed->first;
        }
        case CachePolicy::FIFO: {
            return cache_.begin()->first;
        }
        case CachePolicy::RANDOM: {
            auto it = cache_.begin();
            std::advance(it, rand() % cache_.size());
            return it->first;
        }
    }
    return cache_.begin()->first;
}

template<typename T>
void CacheManager<T>::updateAccessOrder(const std::string& key) {
    // Implementation depends on the specific cache policy
    // This is a placeholder for LRU-based access tracking
}

} // namespace atom::image::core
