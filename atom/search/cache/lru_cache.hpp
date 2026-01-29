/*
 * lru_cache.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file lru_cache.hpp
 * @brief ThreadSafeLRUCache class - a thread-safe LRU cache implementation.
 * @details Provides a highly-optimized LRU cache with thread safety using a
 *          combination of a doubly-linked list and an unordered map.
 */

#ifndef ATOM_SEARCH_CACHE_LRU_CACHE_HPP
#define ATOM_SEARCH_CACHE_LRU_CACHE_HPP

#include <cassert>
#include <fstream>
#include <functional>
#include <list>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "exceptions.hpp"
#include "types.hpp"

#include <spdlog/spdlog.h>

namespace atom::search::cache {

/**
 * @brief A thread-safe LRU (Least Recently Used) cache implementation.
 *
 * This class implements a highly-optimized LRU cache with thread safety using a
 * combination of a doubly-linked list and an unordered map. It supports adding,
 * retrieving, and removing cache items, as well as persisting cache contents to
 * and loading from a file.
 *
 * @tparam Key Type of the cache keys.
 * @tparam Value Type of the cache values.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class ThreadSafeLRUCache {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;
    using ValuePtr = std::shared_ptr<Value>;
    using BatchKeyType = std::vector<Key>;
    using BatchValueType = std::vector<ValuePtr>;

    struct CacheItem {
        ValuePtr value;
        TimePoint expiryTime;
        ListIterator iterator;
    };

    struct LRUCacheStatistics {
        size_t hitCount{0};
        size_t missCount{0};
        float hitRate{0.0f};
        size_t size{0};
        size_t maxSize{0};
        float loadFactor{0.0f};
    };

    /**
     * @brief Constructs a ThreadSafeLRUCache with a specified maximum size.
     * @param max_size The maximum number of items that the cache can hold.
     * @throws std::invalid_argument if max_size is zero
     */
    explicit ThreadSafeLRUCache(size_t max_size);

    ~ThreadSafeLRUCache() = default;

    ThreadSafeLRUCache(const ThreadSafeLRUCache&) = delete;
    ThreadSafeLRUCache& operator=(const ThreadSafeLRUCache&) = delete;
    ThreadSafeLRUCache(ThreadSafeLRUCache&&) = default;
    ThreadSafeLRUCache& operator=(ThreadSafeLRUCache&&) = default;

    // Core operations
    [[nodiscard]] std::optional<Value> get(const Key& key);
    [[nodiscard]] ValuePtr getShared(const Key& key) noexcept;
    [[nodiscard]] BatchValueType getBatch(const BatchKeyType& keys) noexcept;
    [[nodiscard]] bool contains(const Key& key) const noexcept;
    void put(const Key& key, Value value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);
    void putBatch(const std::vector<KeyValuePair>& items,
                  std::optional<std::chrono::seconds> ttl = std::nullopt);
    bool erase(const Key& key) noexcept;
    void clear() noexcept;

    // Accessors
    [[nodiscard]] std::vector<Key> keys() const;
    [[nodiscard]] std::vector<Value> values() const;
    [[nodiscard]] std::optional<KeyValuePair> popLru() noexcept;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] size_t capacity() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    // Statistics
    [[nodiscard]] LRUCacheStatistics getStatistics() const noexcept;
    void resetStatistics() noexcept;

    // Persistence
    void saveToFile(
        const std::string& filename,
        std::function<std::string(const Key&, const Value&)> serializer) const;
    void loadFromFile(
        const std::string& filename,
        std::function<std::pair<Key, Value>(const std::string&)> deserializer);

    // Async operations
    [[nodiscard]] Future<std::optional<Value>> asyncGet(const Key& key);
    [[nodiscard]] Future<void> asyncPut(
        const Key& key, Value value,
        std::optional<std::chrono::seconds> ttl = std::nullopt);

    // Prefetching
    void prefetch(const BatchKeyType& keys,
                  std::function<Value(const Key&)> loader);

    // Callbacks
    void setInsertCallback(
        std::function<void(const Key&, const Value&)> callback);
    void setEraseCallback(
        std::function<void(const Key&, const Value&)> callback);

private:
    void evictLru();
    void moveToFront(const Key& key);
    void cleanupExpired();

    size_t maxSize_;
    std::list<KeyValuePair> cacheItemsList_;
    std::unordered_map<Key, CacheItem, Hash, KeyEqual> cacheItemsMap_;
    mutable SharedMutex mutex_;

    mutable Atomic<size_t> hitCount_{0};
    mutable Atomic<size_t> missCount_{0};

    std::function<void(const Key&, const Value&)> insertCallback_;
    std::function<void(const Key&, const Value&)> eraseCallback_;
};

// =============================================================================
// Implementation
// =============================================================================

template <typename Key, typename Value, typename Hash, typename KeyEqual>
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::ThreadSafeLRUCache(
    size_t max_size)
    : maxSize_(max_size) {
    if (max_size == 0) {
        throw std::invalid_argument("Cache max_size must be greater than zero");
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<Value> ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::get(
    const Key& key) {
    UniqueLock<SharedMutex> lock(mutex_);

    auto it = cacheItemsMap_.find(key);
    if (it == cacheItemsMap_.end()) {
        missCount_++;
        return std::nullopt;
    }

    // Check expiration
    if (it->second.expiryTime != TimePoint{} &&
        Clock::now() > it->second.expiryTime) {
        cacheItemsList_.erase(it->second.iterator);
        cacheItemsMap_.erase(it);
        missCount_++;
        return std::nullopt;
    }

    // Move to front (most recently used)
    cacheItemsList_.splice(cacheItemsList_.begin(), cacheItemsList_,
                           it->second.iterator);
    hitCount_++;

    return *(it->second.value);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::ValuePtr
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::getShared(
    const Key& key) noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);

        auto it = cacheItemsMap_.find(key);
        if (it == cacheItemsMap_.end()) {
            missCount_++;
            return nullptr;
        }

        if (it->second.expiryTime != TimePoint{} &&
            Clock::now() > it->second.expiryTime) {
            cacheItemsList_.erase(it->second.iterator);
            cacheItemsMap_.erase(it);
            missCount_++;
            return nullptr;
        }

        cacheItemsList_.splice(cacheItemsList_.begin(), cacheItemsList_,
                               it->second.iterator);
        hitCount_++;

        return it->second.value;
    } catch (...) {
        return nullptr;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::BatchValueType
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::getBatch(
    const BatchKeyType& keys) noexcept {
    BatchValueType results;
    results.reserve(keys.size());

    for (const auto& key : keys) {
        results.push_back(getShared(key));
    }

    return results;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::contains(
    const Key& key) const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    auto it = cacheItemsMap_.find(key);
    if (it == cacheItemsMap_.end()) {
        return false;
    }
    if (it->second.expiryTime != TimePoint{} &&
        Clock::now() > it->second.expiryTime) {
        return false;
    }
    return true;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
    UniqueLock<SharedMutex> lock(mutex_);

    auto it = cacheItemsMap_.find(key);
    if (it != cacheItemsMap_.end()) {
        // Update existing
        it->second.value = std::make_shared<Value>(std::move(value));
        if (ttl) {
            it->second.expiryTime = Clock::now() + *ttl;
        }
        cacheItemsList_.splice(cacheItemsList_.begin(), cacheItemsList_,
                               it->second.iterator);
    } else {
        // Insert new
        if (cacheItemsMap_.size() >= maxSize_) {
            evictLru();
        }

        cacheItemsList_.push_front({key, value});
        CacheItem item;
        item.value = std::make_shared<Value>(std::move(value));
        item.iterator = cacheItemsList_.begin();
        if (ttl) {
            item.expiryTime = Clock::now() + *ttl;
        }
        cacheItemsMap_[key] = std::move(item);

        if (insertCallback_) {
            try {
                insertCallback_(key, *cacheItemsMap_[key].value);
            } catch (const std::exception& e) {
                spdlog::warn("Exception in insert callback: {}", e.what());
            }
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::putBatch(
    const std::vector<KeyValuePair>& items,
    std::optional<std::chrono::seconds> ttl) {
    for (const auto& [key, value] : items) {
        put(key, value, ttl);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::erase(
    const Key& key) noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);

        auto it = cacheItemsMap_.find(key);
        if (it == cacheItemsMap_.end()) {
            return false;
        }

        if (eraseCallback_) {
            try {
                eraseCallback_(key, *it->second.value);
            } catch (const std::exception& e) {
                spdlog::warn("Exception in erase callback: {}", e.what());
            }
        }

        cacheItemsList_.erase(it->second.iterator);
        cacheItemsMap_.erase(it);
        return true;
    } catch (...) {
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::clear() noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);
        cacheItemsList_.clear();
        cacheItemsMap_.clear();
    } catch (...) {
        // Suppress exceptions
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::vector<Key> ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::keys() const {
    SharedLock<SharedMutex> lock(mutex_);
    std::vector<Key> result;
    result.reserve(cacheItemsMap_.size());
    for (const auto& [key, item] : cacheItemsMap_) {
        result.push_back(key);
    }
    return result;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::vector<Value> ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::values()
    const {
    SharedLock<SharedMutex> lock(mutex_);
    std::vector<Value> result;
    result.reserve(cacheItemsMap_.size());
    for (const auto& [key, item] : cacheItemsMap_) {
        if (item.value) {
            result.push_back(*item.value);
        }
    }
    return result;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<
    typename ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::KeyValuePair>
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::popLru() noexcept {
    try {
        UniqueLock<SharedMutex> lock(mutex_);

        if (cacheItemsList_.empty()) {
            return std::nullopt;
        }

        auto& back = cacheItemsList_.back();
        KeyValuePair result = back;

        cacheItemsMap_.erase(back.first);
        cacheItemsList_.pop_back();

        return result;
    } catch (...) {
        return std::nullopt;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::size() const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    return cacheItemsMap_.size();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::capacity()
    const noexcept {
    return maxSize_;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::empty() const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    return cacheItemsMap_.empty();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
typename ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::LRUCacheStatistics
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::getStatistics() const noexcept {
    SharedLock<SharedMutex> lock(mutex_);
    LRUCacheStatistics stats;
    stats.hitCount = hitCount_.load();
    stats.missCount = missCount_.load();
    size_t total = stats.hitCount + stats.missCount;
    stats.hitRate =
        total > 0 ? static_cast<float>(stats.hitCount) / total : 0.0f;
    stats.size = cacheItemsMap_.size();
    stats.maxSize = maxSize_;
    stats.loadFactor =
        maxSize_ > 0 ? static_cast<float>(stats.size) / maxSize_ : 0.0f;
    return stats;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash,
                        KeyEqual>::resetStatistics() noexcept {
    hitCount_.store(0);
    missCount_.store(0);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::saveToFile(
    const std::string& filename,
    std::function<std::string(const Key&, const Value&)> serializer) const {
    SharedLock<SharedMutex> lock(mutex_);

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw LRUCacheIOException("Failed to open file for writing: " +
                                  filename);
    }

    for (const auto& [key, item] : cacheItemsMap_) {
        if (item.value) {
            file << serializer(key, *item.value) << "\n";
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::loadFromFile(
    const std::string& filename,
    std::function<std::pair<Key, Value>(const std::string&)> deserializer) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw LRUCacheIOException("Failed to open file for reading: " +
                                  filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            auto [key, value] = deserializer(line);
            put(key, std::move(value));
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
Future<std::optional<Value>>
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::asyncGet(const Key& key) {
    return std::async(std::launch::async, [this, key]() { return get(key); });
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
Future<void> ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::asyncPut(
    const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
    return std::async(std::launch::async,
                      [this, key, value = std::move(value), ttl]() mutable {
                          put(key, std::move(value), ttl);
                      });
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::prefetch(
    const BatchKeyType& keys, std::function<Value(const Key&)> loader) {
    for (const auto& key : keys) {
        if (!contains(key)) {
            try {
                Value value = loader(key);
                put(key, std::move(value));
            } catch (const std::exception& e) {
                spdlog::warn("Prefetch failed for key: {}", e.what());
            }
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::setInsertCallback(
    std::function<void(const Key&, const Value&)> callback) {
    UniqueLock<SharedMutex> lock(mutex_);
    insertCallback_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::setEraseCallback(
    std::function<void(const Key&, const Value&)> callback) {
    UniqueLock<SharedMutex> lock(mutex_);
    eraseCallback_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::evictLru() {
    if (cacheItemsList_.empty()) {
        return;
    }

    auto& back = cacheItemsList_.back();
    if (eraseCallback_ && cacheItemsMap_.count(back.first)) {
        try {
            eraseCallback_(back.first, *cacheItemsMap_[back.first].value);
        } catch (const std::exception& e) {
            spdlog::warn("Exception in erase callback during eviction: {}",
                         e.what());
        }
    }

    cacheItemsMap_.erase(back.first);
    cacheItemsList_.pop_back();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::moveToFront(
    const Key& key) {
    auto it = cacheItemsMap_.find(key);
    if (it != cacheItemsMap_.end()) {
        cacheItemsList_.splice(cacheItemsList_.begin(), cacheItemsList_,
                               it->second.iterator);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::cleanupExpired() {
    UniqueLock<SharedMutex> lock(mutex_);
    auto now = Clock::now();

    for (auto it = cacheItemsMap_.begin(); it != cacheItemsMap_.end();) {
        if (it->second.expiryTime != TimePoint{} &&
            now > it->second.expiryTime) {
            cacheItemsList_.erase(it->second.iterator);
            it = cacheItemsMap_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace atom::search::cache

// Expose in atom::search namespace for backward compatibility
namespace atom::search {
using cache::ThreadSafeLRUCache;
}  // namespace atom::search

#endif  // ATOM_SEARCH_CACHE_LRU_CACHE_HPP
