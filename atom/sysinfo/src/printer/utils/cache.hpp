/**
 * @file cache.hpp
 * @brief Caching system for system information
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_UTILS_CACHE_HPP
#define ATOM_SYSINFO_PRINTER_UTILS_CACHE_HPP

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <functional>

namespace atom::system::utils {

/**
 * @struct CacheEntry
 * @brief Cache entry with timestamp and data
 */
template<typename T>
struct CacheEntry {
    T data;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::seconds ttl;
    
    [[nodiscard]] auto isExpired() const -> bool {
        auto now = std::chrono::steady_clock::now();
        return (now - timestamp) > ttl;
    }
};

/**
 * @class Cache
 * @brief Thread-safe cache with TTL support
 */
template<typename T>
class Cache {
public:
    /**
     * @brief Constructor with default TTL
     * @param defaultTtl Default time-to-live for cache entries
     */
    explicit Cache(std::chrono::seconds defaultTtl = std::chrono::seconds(300))
        : defaultTtl_(defaultTtl) {}

    /**
     * @brief Get cached value or compute if not cached/expired
     * @param key Cache key
     * @param computer Function to compute value if not cached
     * @param ttl Time-to-live for this entry (optional)
     * @return Cached or computed value
     */
    auto getOrCompute(const std::string& key, 
                     std::function<T()> computer,
                     std::chrono::seconds ttl = std::chrono::seconds(0)) -> T {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end() && !it->second.isExpired()) {
            return it->second.data;
        }
        
        // Compute new value
        T value = computer();
        
        // Store in cache
        CacheEntry<T> entry;
        entry.data = value;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.ttl = (ttl.count() > 0) ? ttl : defaultTtl_;
        
        cache_[key] = entry;
        
        return value;
    }

    /**
     * @brief Get cached value
     * @param key Cache key
     * @return Cached value or default-constructed T if not found/expired
     */
    auto get(const std::string& key) -> std::optional<T> {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end() && !it->second.isExpired()) {
            return it->second.data;
        }
        
        return std::nullopt;
    }

    /**
     * @brief Put value in cache
     * @param key Cache key
     * @param value Value to cache
     * @param ttl Time-to-live (optional)
     */
    void put(const std::string& key, const T& value, 
             std::chrono::seconds ttl = std::chrono::seconds(0)) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        CacheEntry<T> entry;
        entry.data = value;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.ttl = (ttl.count() > 0) ? ttl : defaultTtl_;
        
        cache_[key] = entry;
    }

    /**
     * @brief Remove entry from cache
     * @param key Cache key
     */
    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(key);
    }

    /**
     * @brief Clear all cache entries
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

    /**
     * @brief Clean up expired entries
     */
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (it->second.isExpired()) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Get cache size
     * @return Number of entries in cache
     */
    [[nodiscard]] auto size() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    /**
     * @brief Check if cache is empty
     * @return true if cache is empty
     */
    [[nodiscard]] auto empty() const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.empty();
    }

private:
    std::unordered_map<std::string, CacheEntry<T>> cache_;
    std::chrono::seconds defaultTtl_;
    mutable std::mutex mutex_;
};

/**
 * @class SystemInfoCache
 * @brief Specialized cache for system information
 */
class SystemInfoCache {
public:
    static SystemInfoCache& getInstance();
    
    // Cache for different types of system information
    Cache<std::string> stringCache;
    Cache<double> numericCache;
    Cache<bool> booleanCache;
    
private:
    SystemInfoCache() = default;
};

} // namespace atom::system::utils

#endif // ATOM_SYSINFO_PRINTER_UTILS_CACHE_HPP
