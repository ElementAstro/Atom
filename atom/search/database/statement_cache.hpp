/*
 * statement_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file statement_cache.hpp
 * @brief LRU cache for prepared statements.
 */

#ifndef ATOM_SEARCH_DATABASE_STATEMENT_CACHE_HPP
#define ATOM_SEARCH_DATABASE_STATEMENT_CACHE_HPP

#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace atom::search::database {

/**
 * @brief Generic LRU cache for prepared statements.
 * @tparam Statement The statement type to cache.
 */
template <typename Statement>
class StatementCache {
public:
    using StatementPtr = Statement*;
    using Finalizer = std::function<void(Statement*)>;

    struct CacheEntry {
        Statement* stmt;
        std::chrono::steady_clock::time_point lastUsed;
        size_t hitCount{0};
    };

    /**
     * @brief Construct a statement cache.
     * @param maxSize Maximum number of statements to cache.
     * @param finalizer Function to clean up statements when evicted.
     */
    explicit StatementCache(size_t maxSize = 50, Finalizer finalizer = nullptr)
        : maxSize_(maxSize), finalizer_(std::move(finalizer)) {}

    ~StatementCache() { clear(); }

    StatementCache(const StatementCache&) = delete;
    StatementCache& operator=(const StatementCache&) = delete;

    StatementCache(StatementCache&& other) noexcept
        : cache_(std::move(other.cache_)),
          lruList_(std::move(other.lruList_)),
          maxSize_(other.maxSize_),
          finalizer_(std::move(other.finalizer_)),
          hits_(other.hits_),
          misses_(other.misses_) {
        other.maxSize_ = 0;
    }

    StatementCache& operator=(StatementCache&& other) noexcept {
        if (this != &other) {
            clear();
            cache_ = std::move(other.cache_);
            lruList_ = std::move(other.lruList_);
            maxSize_ = other.maxSize_;
            finalizer_ = std::move(other.finalizer_);
            hits_ = other.hits_;
            misses_ = other.misses_;
            other.maxSize_ = 0;
        }
        return *this;
    }

    /**
     * @brief Get a cached statement or nullptr if not found.
     * @param key The query string as cache key.
     * @return Pointer to cached statement or nullptr.
     */
    Statement* get(std::string_view key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(std::string(key));
        if (it == cache_.end()) {
            ++misses_;
            return nullptr;
        }

        ++hits_;
        it->second.lastUsed = std::chrono::steady_clock::now();
        ++it->second.hitCount;

        // Move to front of LRU list
        lruList_.remove(it->first);
        lruList_.push_front(it->first);

        return it->second.stmt;
    }

    /**
     * @brief Insert a statement into the cache.
     * @param key The query string as cache key.
     * @param stmt The statement to cache.
     */
    void put(std::string_view key, Statement* stmt) {
        if (!stmt)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        std::string keyStr(key);

        // Check if already exists
        auto it = cache_.find(keyStr);
        if (it != cache_.end()) {
            it->second.stmt = stmt;
            it->second.lastUsed = std::chrono::steady_clock::now();
            return;
        }

        // Evict if at capacity
        while (cache_.size() >= maxSize_ && !lruList_.empty()) {
            evictOldest();
        }

        // Insert new entry
        CacheEntry entry;
        entry.stmt = stmt;
        entry.lastUsed = std::chrono::steady_clock::now();
        entry.hitCount = 0;

        cache_[keyStr] = entry;
        lruList_.push_front(keyStr);
    }

    /**
     * @brief Remove a specific statement from cache.
     * @param key The query string as cache key.
     */
    void remove(std::string_view key) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string keyStr(key);
        auto it = cache_.find(keyStr);
        if (it != cache_.end()) {
            if (finalizer_ && it->second.stmt) {
                finalizer_(it->second.stmt);
            }
            cache_.erase(it);
            lruList_.remove(keyStr);
        }
    }

    /**
     * @brief Clear all cached statements.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (finalizer_) {
            for (auto& pair : cache_) {
                if (pair.second.stmt) {
                    finalizer_(pair.second.stmt);
                }
            }
        }

        cache_.clear();
        lruList_.clear();
        spdlog::debug("Statement cache cleared");
    }

    /**
     * @brief Get cache statistics.
     */
    struct Stats {
        size_t size;
        size_t maxSize;
        size_t hits;
        size_t misses;
        double hitRate;
    };

    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = hits_ + misses_;
        return {cache_.size(), maxSize_, hits_, misses_,
                total > 0 ? static_cast<double>(hits_) / total : 0.0};
    }

    /**
     * @brief Get current cache size.
     */
    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    /**
     * @brief Check if cache is empty.
     */
    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.empty();
    }

private:
    void evictOldest() {
        if (lruList_.empty())
            return;

        const std::string& oldest = lruList_.back();
        auto it = cache_.find(oldest);

        if (it != cache_.end()) {
            if (finalizer_ && it->second.stmt) {
                finalizer_(it->second.stmt);
            }
            cache_.erase(it);
        }

        lruList_.pop_back();
    }

    std::unordered_map<std::string, CacheEntry> cache_;
    std::list<std::string> lruList_;
    size_t maxSize_;
    Finalizer finalizer_;

    mutable std::mutex mutex_;
    size_t hits_{0};
    size_t misses_{0};
};

}  // namespace atom::search::database

#endif  // ATOM_SEARCH_DATABASE_STATEMENT_CACHE_HPP
