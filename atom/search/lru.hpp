#ifndef ATOM_SEARCH_LRU_HPP
#define ATOM_SEARCH_LRU_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <fstream>

#include <spdlog/spdlog.h>

namespace atom::search {

template <typename Key, typename Value, typename Hash>
class ThreadSafeLRUCache;

/**
 * @brief Custom exceptions for LRU Cache operations.
 */
class LRUCacheException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class LRUCacheIOException : public LRUCacheException {
public:
    using LRUCacheException::LRUCacheException;
};

/**
 * @brief A shard of the LRU cache. This is an internal implementation detail.
 */
template <typename Key, typename Value, typename Hash>
class LRUCacheShard {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using ValuePtr = std::shared_ptr<Value>;

    struct CacheItem {
        ValuePtr value;
        TimePoint expiryTime;
        ListIterator iterator;
    };

private:
    friend class ThreadSafeLRUCache<Key, Value, Hash>;

    LRUCacheShard(size_t max_shard_size, ThreadSafeLRUCache<Key, Value, Hash>* parent)
        : max_size_(max_shard_size), parent_(parent) {}

    ValuePtr getShared(const Key& key) {
        std::unique_lock lock(mutex_);
        auto it = cache_items_map_.find(key);

        if (it == cache_items_map_.end() || isExpired(it->second)) {
            parent_->miss_count_++;
            if (it != cache_items_map_.end()) {
                if (parent_->on_erase_) parent_->on_erase_(key);
                cache_items_list_.erase(it->second.iterator);
                cache_items_map_.erase(it);
            }
            return nullptr;
        }

        parent_->hit_count_++;
        cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_, it->second.iterator);
        return it->second.value;
    }

    void put(const Key& key, Value value, std::optional<std::chrono::seconds> ttl) {
        std::unique_lock lock(mutex_);
        auto effective_ttl = ttl.has_value() ? ttl : parent_->default_ttl_;
        auto expiry_time = effective_ttl.has_value() ? (Clock::now() + *effective_ttl) : TimePoint::max();
        auto value_ptr = std::make_shared<Value>(std::move(value));

        auto it = cache_items_map_.find(key);
        if (it != cache_items_map_.end()) {
            cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_, it->second.iterator);
            it->second.value = value_ptr;
            it->second.expiryTime = expiry_time;
        } else {
            cache_items_list_.emplace_front(key, *value_ptr);
            cache_items_map_[key] = {value_ptr, expiry_time, cache_items_list_.begin()};
            trim();
        }
        if (parent_->on_insert_) parent_->on_insert_(key, *value_ptr);
    }

    void putBatch(const std::vector<KeyValuePair>& items, std::optional<std::chrono::seconds> ttl) {
        std::unique_lock lock(mutex_);
        auto effective_ttl = ttl.has_value() ? ttl : parent_->default_ttl_;
        auto expiry_time = effective_ttl.has_value() ? (Clock::now() + *effective_ttl) : TimePoint::max();

        for (const auto& [key, value] : items) {
            auto value_ptr = std::make_shared<Value>(value);
            auto it = cache_items_map_.find(key);
            if (it != cache_items_map_.end()) {
                cache_items_list_.splice(cache_items_list_.begin(), cache_items_list_, it->second.iterator);
                it->second.value = value_ptr;
                it->second.expiryTime = expiry_time;
            } else {
                cache_items_list_.emplace_front(key, value);
                cache_items_map_[key] = {value_ptr, expiry_time, cache_items_list_.begin()};
            }
            if (parent_->on_insert_) parent_->on_insert_(key, value);
        }
        trim();
    }

    bool erase(const Key& key) {
        std::unique_lock lock(mutex_);
        auto it = cache_items_map_.find(key);
        if (it == cache_items_map_.end()) {
            return false;
        }
        if (parent_->on_erase_) parent_->on_erase_(key);
        cache_items_list_.erase(it->second.iterator);
        cache_items_map_.erase(it);
        return true;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        cache_items_map_.clear();
        cache_items_list_.clear();
    }

    size_t size() const {
        std::shared_lock lock(mutex_);
        return cache_items_map_.size();
    }

    size_t maxSize() const {
        return max_size_;
    }

    bool contains(const Key& key) const {
        std::shared_lock lock(mutex_);
        auto it = cache_items_map_.find(key);
        return it != cache_items_map_.end() && !isExpired(it->second);
    }

    size_t pruneExpired() {
        std::unique_lock lock(mutex_);
        size_t pruned_count = 0;
        auto it = cache_items_list_.begin();
        while (it != cache_items_list_.end()) {
            auto map_it = cache_items_map_.find(it->first);
            if (map_it != cache_items_map_.end() && isExpired(map_it->second)) {
                if (parent_->on_erase_) parent_->on_erase_(it->first);
                cache_items_map_.erase(map_it);
                it = cache_items_list_.erase(it);
                pruned_count++;
            } else {
                ++it;
            }
        }
        return pruned_count;
    }

    void resize(size_t new_max_size) {
        std::unique_lock lock(mutex_);
        max_size_ = new_max_size;
        trim();
    }

    std::vector<Key> keys() const {
        std::shared_lock lock(mutex_);
        std::vector<Key> all_keys;
        all_keys.reserve(cache_items_list_.size());
        for(const auto& pair : cache_items_list_) {
            all_keys.push_back(pair.first);
        }
        return all_keys;
    }

    std::vector<Value> values() const {
        std::shared_lock lock(mutex_);
        std::vector<Value> all_values;
        all_values.reserve(cache_items_list_.size());
        for(const auto& pair : cache_items_list_) {
            all_values.push_back(pair.second);
        }
        return all_values;
    }

    void saveToStream(std::ofstream& ofs) const {
        std::shared_lock lock(mutex_);
        for (const auto& pair : cache_items_list_) {
            auto it = cache_items_map_.find(pair.first);
            if (it == cache_items_map_.end() || isExpired(it->second)) continue;

            auto now = Clock::now();
            int64_t remainingTtl = -1;
            if (it->second.expiryTime != TimePoint::max()) {
                auto ttlDuration = std::chrono::duration_cast<std::chrono::seconds>(it->second.expiryTime - now);
                remainingTtl = ttlDuration.count();
                if (remainingTtl <= 0) continue;
            }

            ofs.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
            ofs.write(reinterpret_cast<const char*>(&remainingTtl), sizeof(remainingTtl));

            if constexpr (std::is_trivially_copyable_v<Value>) {
                ofs.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
            } else if constexpr (std::is_same_v<Value, std::string>) {
                size_t valueSize = pair.second.size();
                ofs.write(reinterpret_cast<const char*>(&valueSize), sizeof(valueSize));
                ofs.write(pair.second.c_str(), valueSize);
            } else {
                // For non-trivial types, a proper serialization would be needed.
                // This is a placeholder and might not compile for complex types.
                static_assert(std::is_trivially_copyable_v<Value> || std::is_same_v<Value, std::string>,
                              "Value type must be trivially copyable or std::string for file operations.");
            }
        }
    }

    bool isExpired(const CacheItem& item) const {
        return item.expiryTime != TimePoint::max() && Clock::now() > item.expiryTime;
    }

    void trim() {
        while (cache_items_map_.size() > max_size_) {
            if (cache_items_list_.empty()) return;
            const auto& key = cache_items_list_.back().first;
            if (parent_->on_erase_) parent_->on_erase_(key);
            cache_items_map_.erase(key);
            cache_items_list_.pop_back();
        }
    }

    mutable std::shared_mutex mutex_;
    std::list<KeyValuePair> cache_items_list_;
    std::unordered_map<Key, CacheItem, Hash> cache_items_map_;
    size_t max_size_;
    ThreadSafeLRUCache<Key, Value, Hash>* parent_;
};

/**
 * @brief A thread-safe, sharded LRU (Least Recently Used) cache for high-concurrency scenarios.
 *
 * This class implements a highly-optimized LRU cache by sharding the data across multiple
 * independent caches, each with its own lock. This design minimizes lock contention and
 * improves scalability on multi-core systems.
 *
 * @tparam Key Type of the cache keys.
 * @tparam Value Type of the cache values.
 * @tparam Hash Hash function for keys. Defaults to std::hash<Key>.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class ThreadSafeLRUCache {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ValuePtr = std::shared_ptr<Value>;
    using BatchKeyType = std::vector<Key>;
    using BatchValueType = std::vector<ValuePtr>;

    struct CacheStatistics {
        size_t hitCount;
        size_t missCount;
        float hitRate;
        size_t size;
        size_t maxSize;
        float loadFactor;
    };

    /**
     * @brief Constructs a ThreadSafeLRUCache.
     * @param max_size The maximum number of items the cache can hold.
     * @param concurrency_level The number of shards to distribute data across. Defaults to hardware concurrency.
     * @throws std::invalid_argument if max_size is zero.
     */
    explicit ThreadSafeLRUCache(size_t max_size, size_t concurrency_level = 0)
        : max_size_(max_size),
          concurrency_level_(concurrency_level > 0 ? concurrency_level : std::thread::hardware_concurrency()) {
        if (max_size == 0) {
            throw std::invalid_argument("Cache max size must be greater than zero.");
        }
        if (concurrency_level_ == 0) {
            const_cast<size_t&>(concurrency_level_) = 1;
        }

        shards_.reserve(concurrency_level_);
        size_t max_shard_size = (max_size + concurrency_level_ - 1) / concurrency_level_;
        for (size_t i = 0; i < concurrency_level_; ++i) {
            shards_.emplace_back(std::make_unique<LRUCacheShard<Key, Value, Hash>>(max_shard_size, this));
        }
    }

    ~ThreadSafeLRUCache() = default;

    /**
     * @brief Retrieves a value from the cache.
     * @param key The key of the item to retrieve.
     * @return An optional containing the value if found and not expired, otherwise std::nullopt.
     */
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        auto sharedPtr = getShared(key);
        if (sharedPtr) {
            return *sharedPtr;
        }
        return std::nullopt;
    }

    /**
     * @brief Retrieves a value as a shared pointer from the cache.
     * @param key The key of the item to retrieve.
     * @return A shared pointer to the value if found and not expired, otherwise nullptr.
     */
    [[nodiscard]] ValuePtr getShared(const Key& key) {
        return get_shard(key).getShared(key);
    }

    /**
     * @brief Batch retrieval of multiple values from the cache.
     * @param keys Vector of keys to retrieve.
     * @return Vector of shared pointers to values (nullptr for missing items).
     */
    [[nodiscard]] BatchValueType getBatch(const BatchKeyType& keys) {
        BatchValueType results;
        results.reserve(keys.size());
        for (const auto& key : keys) {
            results.push_back(getShared(key));
        }
        return results;
    }

    /**
     * @brief Checks if a key exists in the cache.
     * @param key The key to check.
     * @return True if the key exists and is not expired, false otherwise.
     */
    [[nodiscard]] bool contains(const Key& key) const {
        return get_shard(key).contains(key);
    }

    /**
     * @brief Inserts or updates a value in the cache.
     * @param key The key of the item to insert or update.
     * @param value The value to associate with the key.
     * @param ttl Optional time-to-live duration for the cache item.
     */
    void put(const Key& key, Value value, std::optional<std::chrono::seconds> ttl = std::nullopt) {
        get_shard(key).put(key, std::move(value), ttl);
    }

    /**
     * @brief Inserts or updates a batch of values in the cache.
     * @param items Vector of key-value pairs to insert.
     * @param ttl Optional time-to-live duration for all cache items.
     */
    void putBatch(const std::vector<KeyValuePair>& items, std::optional<std::chrono::seconds> ttl = std::nullopt) {
        for (const auto& item : items) {
            put(item.first, item.second, ttl);
        }
    }

    /**
     * @brief Erases an item from the cache.
     * @param key The key of the item to remove.
     * @return True if the item was found and removed, false otherwise.
     */
    bool erase(const Key& key) {
        return get_shard(key).erase(key);
    }

    /**
     * @brief Clears all items from the cache.
     */
    void clear() {
        for (auto& shard : shards_) {
            shard->clear();
        }
        if (on_clear_) on_clear_();
    }

    /**
     * @brief Retrieves all keys in the cache.
     * @return A vector containing all keys currently in the cache.
     */
    [[nodiscard]] std::vector<Key> keys() const {
        std::vector<Key> all_keys;
        for (const auto& shard : shards_) {
            auto shard_keys = shard->keys();
            all_keys.insert(all_keys.end(), shard_keys.begin(), shard_keys.end());
        }
        return all_keys;
    }

    /**
     * @brief Retrieves all values in the cache.
     * @return A vector containing all values currently in the cache.
     */
    [[nodiscard]] std::vector<Value> values() const {
        std::vector<Value> all_values;
        for (const auto& shard : shards_) {
            auto shard_values = shard->values();
            all_values.insert(all_values.end(), shard_values.begin(), shard_values.end());
        }
        return all_values;
    }

    /**
     * @brief Resizes the cache to a new maximum size.
     * @param new_max_size The new maximum size of the cache.
     * @throws std::invalid_argument if new_max_size is zero.
     */
    void resize(size_t new_max_size) {
        if (new_max_size == 0) {
            throw std::invalid_argument("Cache max size must be greater than zero.");
        }
        max_size_ = new_max_size;
        size_t new_max_shard_size = (new_max_size + concurrency_level_ - 1) / concurrency_level_;
        for (auto& shard : shards_) {
            shard->resize(new_max_shard_size);
        }
    }

    /**
     * @brief Gets the current size of the cache.
     * @return The number of items currently in the cache.
     */
    [[nodiscard]] size_t size() const {
        size_t total_size = 0;
        for (const auto& shard : shards_) {
            total_size += shard->size();
        }
        return total_size;
    }

    /**
     * @brief Gets the maximum size of the cache.
     * @return The maximum number of items the cache can hold.
     */
    [[nodiscard]] size_t maxSize() const noexcept {
        return max_size_;
    }

    /**
     * @brief Gets the current load factor of the cache.
     * @return The load factor of the cache.
     */
    [[nodiscard]] float loadFactor() const {
        return static_cast<float>(size()) / static_cast<float>(max_size_);
    }

    /**
     * @brief Checks if the cache is empty.
     * @return True if the cache is empty, false otherwise.
     */
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    /**
     * @brief Sets a callback function to be called on item insertion.
     * @param callback The callback function.
     */
    void setInsertCallback(std::function<void(const Key&, const Value&)> callback) {
        on_insert_ = std::move(callback);
    }

    /**
     * @brief Sets a callback function to be called on item erasure.
     * @param callback The callback function.
     */
    void setEraseCallback(std::function<void(const Key&)> callback) {
        on_erase_ = std::move(callback);
    }

    /**
     * @brief Sets a callback function to be called when the cache is cleared.
     * @param callback The callback function.
     */
    void setClearCallback(std::function<void()> callback) {
        on_clear_ = std::move(callback);
    }

    /**
     * @brief Gets the hit rate of the cache.
     * @return The hit rate as a float between 0.0 and 1.0.
     */
    [[nodiscard]] float hitRate() const noexcept {
        size_t hits = hit_count_.load(std::memory_order_relaxed);
        size_t misses = miss_count_.load(std::memory_order_relaxed);
        size_t total = hits + misses;
        return total == 0 ? 0.0f : static_cast<float>(hits) / static_cast<float>(total);
    }

    /**
     * @brief Gets comprehensive statistics about the cache.
     * @return A CacheStatistics struct.
     */
    [[nodiscard]] CacheStatistics getStatistics() const {
        size_t current_size = size();
        return {hit_count_.load(std::memory_order_relaxed),
                miss_count_.load(std::memory_order_relaxed),
                hitRate(),
                current_size,
                max_size_,
                static_cast<float>(current_size) / static_cast<float>(max_size_)};
    }

    /**
     * @brief Resets cache statistics (hits and misses).
     */
    void resetStatistics() noexcept {
        hit_count_.store(0, std::memory_order_relaxed);
        miss_count_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Saves the cache contents to a file.
     * @param filename The name of the file to save to.
     * @throws LRUCacheIOException If file operations fail.
     */
    void saveToFile(const std::string& filename) const {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            throw LRUCacheIOException("Failed to open file for writing: " + filename);
        }
        size_t current_size = size();
        ofs.write(reinterpret_cast<const char*>(&current_size), sizeof(current_size));
        ofs.write(reinterpret_cast<const char*>(&max_size_), sizeof(max_size_));

        for (const auto& shard : shards_) {
            shard->saveToStream(ofs);
        }
        if (!ofs) {
            throw LRUCacheIOException("Failed writing to file: " + filename);
        }
    }

    /**
     * @brief Loads cache contents from a file.
     * @param filename The name of the file to load from.
     * @throws LRUCacheIOException If file operations fail.
     */
    void loadFromFile(const std::string& filename) {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) {
            throw LRUCacheIOException("Failed to open file for reading: " + filename);
        }
        clear();

        size_t stored_size, stored_max_size;
        ifs.read(reinterpret_cast<char*>(&stored_size), sizeof(stored_size));
        ifs.read(reinterpret_cast<char*>(&stored_max_size), sizeof(stored_max_size));
        if (!ifs) throw LRUCacheIOException("Failed to read cache metadata from file");

        resize(stored_max_size);

        for (size_t i = 0; i < stored_size && ifs; ++i) {
            Key key;
            int64_t ttlSeconds;
            Value value;

            ifs.read(reinterpret_cast<char*>(&key), sizeof(key));
            ifs.read(reinterpret_cast<char*>(&ttlSeconds), sizeof(ttlSeconds));

            if constexpr (std::is_trivially_copyable_v<Value>) {
                ifs.read(reinterpret_cast<char*>(&value), sizeof(value));
            } else if constexpr (std::is_same_v<Value, std::string>) {
                size_t valueSize;
                ifs.read(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
                value.resize(valueSize);
                ifs.read(&value[0], static_cast<std::streamsize>(valueSize));
            } else {
                 static_assert(std::is_trivially_copyable_v<Value> || std::is_same_v<Value, std::string>,
                              "Value type must be trivially copyable or std::string for file operations.");
            }

            if (!ifs) break;

            std::optional<std::chrono::seconds> ttl = (ttlSeconds >= 0)
                ? std::optional<std::chrono::seconds>(std::chrono::seconds(ttlSeconds))
                : std::nullopt;

            put(key, std::move(value), ttl);
        }
    }

    /**
     * @brief Prunes expired items from the cache.
     * @return Number of items pruned.
     */
    size_t pruneExpired() {
        size_t total_pruned = 0;
        for (auto& shard : shards_) {
            total_pruned += shard->pruneExpired();
        }
        return total_pruned;
    }

    /**
     * @brief Asynchronously retrieves a value from the cache.
     * @param key The key of the item to retrieve.
     * @return A future containing an optional with the value if found, otherwise std::nullopt.
     */
    [[nodiscard]] std::future<std::optional<Value>> asyncGet(const Key& key) {
        return std::async(std::launch::async, [this, key]() { return get(key); });
    }

    /**
     * @brief Asynchronously inserts or updates a value in the cache.
     * @param key The key of the item to insert or update.
     * @param value The value to associate with the key.
     * @param ttl Optional time-to-live duration for the cache item.
     * @return A future that completes when the operation is done.
     */
    std::future<void> asyncPut(const Key& key, Value value, std::optional<std::chrono::seconds> ttl = std::nullopt) {
        return std::async(std::launch::async, [this, key, value = std::move(value), ttl]() mutable {
            put(key, std::move(value), ttl);
        });
    }

    /**
     * @brief Sets the default TTL for cache items.
     * @param ttl The default time-to-live duration.
     */
    void setDefaultTTL(std::chrono::seconds ttl) {
        default_ttl_ = ttl;
    }

    /**
     * @brief Gets the default TTL for cache items.
     * @return The default time-to-live duration.
     */
    [[nodiscard]] std::optional<std::chrono::seconds> getDefaultTTL() const noexcept {
        return default_ttl_;
    }

private:
    friend class LRUCacheShard<Key, Value, Hash>;

    LRUCacheShard<Key, Value, Hash>& get_shard(const Key& key) const {
        return *shards_[key_hasher_(key) % concurrency_level_];
    }

    size_t max_size_;
    const size_t concurrency_level_;
    std::vector<std::unique_ptr<LRUCacheShard<Key, Value, Hash>>> shards_;
    Hash key_hasher_;

    std::atomic<size_t> hit_count_{0};
    std::atomic<size_t> miss_count_{0};
    std::function<void(const Key&, const Value&)> on_insert_;
    std::function<void(const Key&)> on_erase_;
    std::function<void()> on_clear_;
    std::optional<std::chrono::seconds> default_ttl_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_LRU_HPP
