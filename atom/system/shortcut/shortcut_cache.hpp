/*
 * shortcut_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef SHORTCUT_CACHE_HPP
#define SHORTCUT_CACHE_HPP

#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "shortcut_config.hpp"
#include "shortcut.h"
#include "status.h"

namespace shortcut_detector {

/**
 * @brief Cache entry with TTL and access tracking
 */
template<typename T>
struct ShortcutCacheEntry {
    T value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point lastAccess;
    std::chrono::milliseconds ttl;
    size_t accessCount;
    
    ShortcutCacheEntry(T val, std::chrono::milliseconds ttl_ms)
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
 * @brief LRU cache with TTL support for shortcut system
 */
template<typename Key, typename Value>
class ShortcutLRUCache {
public:
    explicit ShortcutLRUCache(size_t maxSize, std::chrono::milliseconds defaultTTL)
        : maxSize_(maxSize), defaultTTL_(defaultTTL) {}
    
    /**
     * @brief Get value from cache
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            misses_++;
            SHORTCUT_EMIT_EVENT(ShortcutEventType::CACHE_MISS, std::string(key), "", "");
            return std::nullopt;
        }
        
        if (it->second.isExpired()) {
            removeEntry(it);
            misses_++;
            SHORTCUT_EMIT_EVENT(ShortcutEventType::CACHE_MISS, std::string(key), "", "expired");
            return std::nullopt;
        }
        
        // Move to front (most recently used)
        moveToFront(key);
        it->second.updateAccess();
        hits_++;
        
        SHORTCUT_EMIT_EVENT(ShortcutEventType::CACHE_HIT, std::string(key), "", "");
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
            it->second = ShortcutCacheEntry<Value>(value, ttl);
            moveToFront(key);
        } else {
            // Add new entry
            if (cache_.size() >= maxSize_) {
                evictLRU();
            }
            
            cache_.emplace(key, ShortcutCacheEntry<Value>(value, ttl));
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
        hits_ = 0;
        misses_ = 0;
    }
    
    /**
     * @brief Get cache statistics
     */
    struct Stats {
        size_t size;
        size_t maxSize;
        size_t hits;
        size_t misses;
        size_t expiredEntries;
        double hitRatio;
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t expired = 0;
        for (const auto& [key, entry] : cache_) {
            if (entry.isExpired()) {
                expired++;
            }
        }
        
        auto totalAccess = hits_ + misses_;
        double hitRatio = totalAccess > 0 ? static_cast<double>(hits_) / totalAccess : 0.0;
        
        return {cache_.size(), maxSize_, hits_, misses_, expired, hitRatio};
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

private:
    mutable std::mutex mutex_;
    std::unordered_map<Key, ShortcutCacheEntry<Value>> cache_;
    std::list<Key> accessOrder_; // Most recent at front
    size_t maxSize_;
    std::chrono::milliseconds defaultTTL_;
    
    // Statistics
    mutable size_t hits_{0};
    mutable size_t misses_{0};
    
    void moveToFront(const Key& key) {
        accessOrder_.remove(key);
        accessOrder_.push_front(key);
    }
    
    void evictLRU() {
        if (!accessOrder_.empty()) {
            const Key& lru = accessOrder_.back();
            cache_.erase(lru);
            accessOrder_.pop_back();
            SHORTCUT_EMIT_EVENT(ShortcutEventType::CACHE_EVICTION, std::string(lru), "", "LRU");
        }
    }
    
    void removeEntry(typename std::unordered_map<Key, ShortcutCacheEntry<Value>>::iterator it) {
        accessOrder_.remove(it->first);
        cache_.erase(it);
    }
};

/**
 * @brief Specialized cache manager for shortcut system
 */
class ShortcutCacheManager {
public:
    static ShortcutCacheManager& getInstance();
    
    /**
     * @brief Cache shortcut detection results
     */
    void cacheDetectionResult(const std::string& shortcutKey, const ShortcutCheckResult& result);
    std::optional<ShortcutCheckResult> getCachedDetectionResult(const std::string& shortcutKey);
    
    /**
     * @brief Cache keyboard hook status
     */
    void cacheHookStatus(bool hasHook);
    std::optional<bool> getCachedHookStatus();
    
    /**
     * @brief Cache process information
     */
    void cacheProcessList(const std::vector<std::string>& processes);
    std::optional<std::vector<std::string>> getCachedProcessList();
    
    /**
     * @brief Cache shortcut strings
     */
    void cacheShortcutString(const std::string& shortcutKey, const std::string& shortcutString);
    std::optional<std::string> getCachedShortcutString(const std::string& shortcutKey);
    
    /**
     * @brief Cache system state information
     */
    void cacheSystemState(const std::string& stateKey, const std::string& stateValue);
    std::optional<std::string> getCachedSystemState(const std::string& stateKey);
    
    /**
     * @brief Clear all caches
     */
    void clearAll();
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        ShortcutLRUCache<std::string, ShortcutCheckResult>::Stats detectionResults;
        ShortcutLRUCache<std::string, bool>::Stats hookStatus;
        ShortcutLRUCache<std::string, std::vector<std::string>>::Stats processLists;
        ShortcutLRUCache<std::string, std::string>::Stats shortcutStrings;
        ShortcutLRUCache<std::string, std::string>::Stats systemStates;
    };
    
    CacheStats getStats() const;
    
    /**
     * @brief Cleanup expired entries in all caches
     */
    size_t cleanup();
    
    /**
     * @brief Invalidate cache entries for a specific shortcut
     */
    void invalidateShortcut(const std::string& shortcutKey);
    
    /**
     * @brief Invalidate system-related caches
     */
    void invalidateSystemCaches();

private:
    ShortcutCacheManager();
    ~ShortcutCacheManager() = default;
    
    ShortcutCacheManager(const ShortcutCacheManager&) = delete;
    ShortcutCacheManager& operator=(const ShortcutCacheManager&) = delete;
    
    std::unique_ptr<ShortcutLRUCache<std::string, ShortcutCheckResult>> detectionCache_;
    std::unique_ptr<ShortcutLRUCache<std::string, bool>> hookCache_;
    std::unique_ptr<ShortcutLRUCache<std::string, std::vector<std::string>>> processCache_;
    std::unique_ptr<ShortcutLRUCache<std::string, std::string>> stringCache_;
    std::unique_ptr<ShortcutLRUCache<std::string, std::string>> systemStateCache_;
    
    std::thread cleanupThread_;
    std::atomic<bool> shutdown_{false};
    
    void cleanupLoop();
};

} // namespace shortcut_detector

#endif // SHORTCUT_CACHE_HPP
