/*
 * cron_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_CACHE_HPP
#define CRON_CACHE_HPP

#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cron_config.hpp"
#include "cron_job.hpp"

// Forward declaration
struct CronExecutionResult;

/**
 * @brief Cache entry with TTL and access tracking
 */
template<typename T>
struct CronCacheEntry {
    T value;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point lastAccess;
    std::chrono::milliseconds ttl;
    size_t accessCount;
    
    CronCacheEntry(T val, std::chrono::milliseconds ttl_ms)
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
 * @brief LRU cache with TTL support for cron system
 */
template<typename Key, typename Value>
class CronLRUCache {
public:
    explicit CronLRUCache(size_t maxSize, std::chrono::milliseconds defaultTTL)
        : maxSize_(maxSize), defaultTTL_(defaultTTL) {}
    
    /**
     * @brief Get value from cache
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            misses_++;
            CRON_EMIT_EVENT(CronEventType::CACHE_OPERATION, std::string(key), "", "miss");
            return std::nullopt;
        }
        
        if (it->second.isExpired()) {
            removeEntry(it);
            misses_++;
            CRON_EMIT_EVENT(CronEventType::CACHE_OPERATION, std::string(key), "", "expired");
            return std::nullopt;
        }
        
        // Move to front (most recently used)
        moveToFront(key);
        it->second.updateAccess();
        hits_++;
        
        CRON_EMIT_EVENT(CronEventType::CACHE_OPERATION, std::string(key), "", "hit");
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
            it->second = CronCacheEntry<Value>(value, ttl);
            moveToFront(key);
        } else {
            // Add new entry
            if (cache_.size() >= maxSize_) {
                evictLRU();
            }
            
            cache_.emplace(key, CronCacheEntry<Value>(value, ttl));
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
    std::unordered_map<Key, CronCacheEntry<Value>> cache_;
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
        }
    }
    
    void removeEntry(typename std::unordered_map<Key, CronCacheEntry<Value>>::iterator it) {
        accessOrder_.remove(it->first);
        cache_.erase(it);
    }
};

/**
 * @brief Specialized cache manager for cron system
 */
class CronCacheManager {
public:
    static CronCacheManager& getInstance();
    
    /**
     * @brief Cache job data
     */
    void cacheJob(const std::string& jobId, std::shared_ptr<CronJob> job);
    std::optional<std::shared_ptr<CronJob>> getCachedJob(const std::string& jobId);
    
    /**
     * @brief Cache validation results
     */
    void cacheValidationResult(const std::string& expression, bool isValid);
    std::optional<bool> getCachedValidationResult(const std::string& expression);
    
    /**
     * @brief Cache execution results
     */
    void cacheExecutionResult(const std::string& jobId, const CronExecutionResult& result);
    std::optional<CronExecutionResult> getCachedExecutionResult(const std::string& jobId);
    
    /**
     * @brief Cache next execution times
     */
    void cacheNextExecution(const std::string& jobId, std::chrono::system_clock::time_point nextTime);
    std::optional<std::chrono::system_clock::time_point> getCachedNextExecution(const std::string& jobId);
    
    /**
     * @brief Cache job lists by category
     */
    void cacheJobsByCategory(const std::string& category, const std::vector<std::string>& jobIds);
    std::optional<std::vector<std::string>> getCachedJobsByCategory(const std::string& category);
    
    /**
     * @brief Clear all caches
     */
    void clearAll();
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        CronLRUCache<std::string, std::shared_ptr<CronJob>>::Stats jobs;
        CronLRUCache<std::string, bool>::Stats validations;
        CronLRUCache<std::string, CronExecutionResult>::Stats executions;
        CronLRUCache<std::string, std::chrono::system_clock::time_point>::Stats nextExecutions;
        CronLRUCache<std::string, std::vector<std::string>>::Stats categories;
    };
    
    CacheStats getStats() const;
    
    /**
     * @brief Cleanup expired entries in all caches
     */
    size_t cleanup();
    
    /**
     * @brief Invalidate cache entries for a specific job
     */
    void invalidateJob(const std::string& jobId);
    
    /**
     * @brief Invalidate cache entries for a category
     */
    void invalidateCategory(const std::string& category);

private:
    CronCacheManager();
    ~CronCacheManager() = default;
    
    CronCacheManager(const CronCacheManager&) = delete;
    CronCacheManager& operator=(const CronCacheManager&) = delete;
    
    std::unique_ptr<CronLRUCache<std::string, std::shared_ptr<CronJob>>> jobCache_;
    std::unique_ptr<CronLRUCache<std::string, bool>> validationCache_;
    std::unique_ptr<CronLRUCache<std::string, CronExecutionResult>> executionCache_;
    std::unique_ptr<CronLRUCache<std::string, std::chrono::system_clock::time_point>> nextExecutionCache_;
    std::unique_ptr<CronLRUCache<std::string, std::vector<std::string>>> categoryCache_;
    
    std::thread cleanupThread_;
    std::atomic<bool> shutdown_{false};
    
    void cleanupLoop();
};

#endif // CRON_CACHE_HPP
