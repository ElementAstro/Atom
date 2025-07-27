/*
 * cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_CACHE_HPP
#define ATOM_SYSTEM_COMMAND_CACHE_HPP

#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "atom/macro.hpp"
#include "config.hpp"
#include "utils.hpp"

namespace atom::system {

/**
 * @brief Cache entry with TTL support
 */
template<typename T>
struct CacheEntry {
    T value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::milliseconds ttl;
    
    CacheEntry(T val, std::chrono::milliseconds ttl_ms)
        : value(std::move(val)), 
          timestamp(std::chrono::steady_clock::now()),
          ttl(ttl_ms) {}
    
    bool isExpired() const {
        auto now = std::chrono::steady_clock::now();
        return (now - timestamp) >= ttl;
    }
};

/**
 * @brief Thread-safe LRU cache with TTL support
 */
template<typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t maxSize, std::chrono::milliseconds defaultTTL)
        : maxSize_(maxSize), defaultTTL_(defaultTTL) {}
    
    /**
     * @brief Get value from cache
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).cacheMisses++;
            return std::nullopt;
        }
        
        if (it->second.isExpired()) {
            cache_.erase(it);
            const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).cacheMisses++;
            return std::nullopt;
        }
        
        // Move to front (most recently used)
        moveToFront(key);
        const_cast<CommandSystemMetrics&>(COMMAND_METRICS()).cacheHits++;
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
            it->second = CacheEntry<Value>(value, ttl);
            moveToFront(key);
        } else {
            // Add new entry
            if (cache_.size() >= maxSize_) {
                evictLRU();
            }
            
            cache_.emplace(key, CacheEntry<Value>(value, ttl));
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
            cache_.erase(it);
            accessOrder_.remove(key);
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
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t expired = 0;
        for (const auto& [key, entry] : cache_) {
            if (entry.isExpired()) {
                expired++;
            }
        }
        
        return {cache_.size(), maxSize_, expired};
    }
    
    /**
     * @brief Clean up expired entries
     */
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.begin();
        while (it != cache_.end()) {
            if (it->second.isExpired()) {
                accessOrder_.remove(it->first);
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<Key, CacheEntry<Value>> cache_;
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
};

/**
 * @brief Specialized cache manager for command system
 */
class CommandCacheManager {
public:
    static CommandCacheManager& getInstance();
    
    /**
     * @brief Get cached validation result
     */
    std::optional<ValidationResult> getValidationResult(const std::string& command);
    
    /**
     * @brief Cache validation result
     */
    void cacheValidationResult(const std::string& command, const ValidationResult& result);
    
    /**
     * @brief Get cached command metrics
     */
    std::optional<CommandMetrics> getCommandMetrics(const std::string& command);
    
    /**
     * @brief Cache command metrics
     */
    void cacheCommandMetrics(const std::string& command, const CommandMetrics& metrics);
    
    /**
     * @brief Clear all caches
     */
    void clearAll();
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        LRUCache<std::string, ValidationResult>::Stats validation;
        LRUCache<std::string, CommandMetrics>::Stats metrics;
    };
    
    CacheStats getStats() const;
    
    /**
     * @brief Cleanup expired entries
     */
    void cleanup();

private:
    CommandCacheManager();
    ~CommandCacheManager() = default;
    
    CommandCacheManager(const CommandCacheManager&) = delete;
    CommandCacheManager& operator=(const CommandCacheManager&) = delete;
    
    std::unique_ptr<LRUCache<std::string, ValidationResult>> validationCache_;
    std::unique_ptr<LRUCache<std::string, CommandMetrics>> metricsCache_;
    
    std::thread cleanupThread_;
    std::atomic<bool> shutdown_{false};
    
    void cleanupLoop();
};

} // namespace atom::system

#endif // ATOM_SYSTEM_COMMAND_CACHE_HPP
