/*
 * env_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_ENV_CACHE_HPP
#define ATOM_SYSTEM_ENV_CACHE_HPP

#include <algorithm>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/containers/high_performance.hpp"
#include "env_config.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

/**
 * @brief Cache entry with TTL and access tracking
 */
template<typename T>
struct EnvCacheEntry {
    T value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point lastAccess;
    std::chrono::milliseconds ttl;
    size_t accessCount;

    EnvCacheEntry(T val, std::chrono::milliseconds ttl_ms)
        : value(std::move(val)),
          timestamp(std::chrono::steady_clock::now()),
          lastAccess(timestamp),
          ttl(ttl_ms),
          accessCount(1) {}

    bool isExpired() const {
        auto now = std::chrono::steady_clock::now();
        return (now - timestamp) >= ttl;
    }

    void updateAccess() {
        lastAccess = std::chrono::steady_clock::now();
        accessCount++;
    }
};

/**
 * @brief LRU cache with TTL support and advanced features
 */
template<typename Key, typename Value>
class EnvLRUCache {
public:
    explicit EnvLRUCache(size_t maxSize, std::chrono::milliseconds defaultTTL)
        : maxSize_(maxSize), defaultTTL_(defaultTTL) {}

    /**
     * @brief Get value from cache
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it == cache_.end()) {
            ENV_EMIT_EVENT(EnvEventType::CACHE_MISS, String(key), "", "");
            return std::nullopt;
        }

        if (it->second.isExpired()) {
            removeEntry(it);
            ENV_EMIT_EVENT(EnvEventType::CACHE_MISS, String(key), "", "expired");
            return std::nullopt;
        }

        // Move to front (most recently used)
        moveToFront(key);
        it->second.updateAccess();

        ENV_EMIT_EVENT(EnvEventType::CACHE_HIT, String(key), "", "");
        return it->second.value;
    }

    /**
     * @brief Put value into cache
     */
    void put(const Key& key, const Value& value,
             std::chrono::milliseconds ttl = std::chrono::milliseconds::zero()) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (ttl == std::chrono::milliseconds::zero()) {
            ttl = defaultTTL_;
        }

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Update existing entry
            it->second = EnvCacheEntry<Value>(value, ttl);
            moveToFront(key);
        } else {
            // Add new entry
            if (cache_.size() >= maxSize_) {
                evictLRU();
            }

            cache_.emplace(key, EnvCacheEntry<Value>(value, ttl));
            accessOrder_.push_front(key);
        }
    }

    /**
     * @brief Remove entry from cache
     */
    void remove(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            removeEntry(it);
        }
    }

    /**
     * @brief Clear all entries
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        accessOrder_.clear();
    }

    /**
     * @brief Get cache statistics
     */
    struct Stats {
        size_t size;
        size_t maxSize;
        size_t expiredEntries;
        size_t totalAccesses;
        double averageAccessCount;
    };

    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t expired = 0;
        size_t totalAccesses = 0;

        for (const auto& [key, entry] : cache_) {
            if (entry.isExpired()) {
                expired++;
            }
            totalAccesses += entry.accessCount;
        }

        double avgAccess = cache_.empty() ? 0.0 :
                          static_cast<double>(totalAccesses) / cache_.size();

        return {cache_.size(), maxSize_, expired, totalAccesses, avgAccess};
    }

    /**
     * @brief Clean up expired entries
     */
    size_t cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t removed = 0;
        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (it->second.isExpired()) {
                accessOrder_.remove(it->first);
                it = cache_.erase(it);
                removed++;
            } else {
                ++it;
            }
        }

        return removed;
    }

    /**
     * @brief Get keys sorted by access frequency
     */
    std::vector<Key> getKeysByFrequency() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::pair<Key, size_t>> keyFreq;
        for (const auto& [key, entry] : cache_) {
            keyFreq.emplace_back(key, entry.accessCount);
        }

        std::sort(keyFreq.begin(), keyFreq.end(),
                 [](const auto& a, const auto& b) {
                     return a.second > b.second;
                 });

        std::vector<Key> result;
        result.reserve(keyFreq.size());
        for (const auto& [key, freq] : keyFreq) {
            result.push_back(key);
        }

        return result;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<Key, EnvCacheEntry<Value>> cache_;
    std::list<Key> accessOrder_; // Most recent at front
    size_t maxSize_;
    std::chrono::milliseconds defaultTTL_;

    void moveToFront(const Key& key) {
        accessOrder_.remove(key);
        accessOrder_.push_front(key);
    }

    void evictLRU() {
        if (!accessOrder_.empty()) {
            const Key& lru = accessOrder_.back();
            cache_.erase(lru);
            accessOrder_.pop_back();
        }
    }

    void removeEntry(typename std::unordered_map<Key, EnvCacheEntry<Value>>::iterator it) {
        accessOrder_.remove(it->first);
        cache_.erase(it);
    }
};

/**
 * @brief Specialized cache manager for environment system
 */
class EnvCacheManager {
public:
    static EnvCacheManager& getInstance();

    /**
     * @brief Get cached environment variable
     */
    std::optional<String> getEnvVar(const String& key);

    /**
     * @brief Cache environment variable
     */
    void cacheEnvVar(const String& key, const String& value);

    /**
     * @brief Get cached path entries
     */
    std::optional<std::vector<String>> getPathEntries();

    /**
     * @brief Cache path entries
     */
    void cachePathEntries(const std::vector<String>& paths);

    /**
     * @brief Get cached system information
     */
    template<typename T>
    std::optional<T> getSystemInfo(const String& key);

    /**
     * @brief Cache system information
     */
    template<typename T>
    void cacheSystemInfo(const String& key, const T& value);

    /**
     * @brief Clear all caches
     */
    void clearAll();

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        EnvLRUCache<String, String>::Stats envVars;
        EnvLRUCache<String, std::vector<String>>::Stats pathEntries;
        EnvLRUCache<String, String>::Stats systemInfo;
    };

    CacheStats getStats() const;

    /**
     * @brief Cleanup expired entries in all caches
     */
    size_t cleanup();

    /**
     * @brief Invalidate cache entries matching pattern
     */
    void invalidatePattern(const String& pattern);

private:
    EnvCacheManager();
    ~EnvCacheManager() = default;

    EnvCacheManager(const EnvCacheManager&) = delete;
    EnvCacheManager& operator=(const EnvCacheManager&) = delete;

    std::unique_ptr<EnvLRUCache<String, String>> envVarCache_;
    std::unique_ptr<EnvLRUCache<String, std::vector<String>>> pathCache_;
    std::unique_ptr<EnvLRUCache<String, String>> systemInfoCache_;

    std::thread cleanupThread_;
    std::atomic<bool> shutdown_{false};

    void cleanupLoop();
};

} // namespace atom::utils

#endif // ATOM_SYSTEM_ENV_CACHE_HPP
