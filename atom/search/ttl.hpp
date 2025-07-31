#ifndef ATOM_SEARCH_TTL_CACHE_HPP
#define ATOM_SEARCH_TTL_CACHE_HPP

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace atom::search {

/**
 * @brief Custom exception class for TTL Cache errors.
 */
class TTLCacheException : public std::runtime_error {
public:
    explicit TTLCacheException(const std::string& message);
};

/**
 * @brief Cache statistics for monitoring performance and usage.
 */
struct CacheStatistics {
    std::atomic<size_t> hits{0};
    std::atomic<size_t> misses{0};
    std::atomic<size_t> evictions{0};
    std::atomic<size_t> expirations{0};
    size_t current_size{0};
    size_t max_capacity{0};
    double hit_rate{0.0};

    // Enhanced statistics
    std::atomic<size_t> memory_usage_bytes{0};
    std::atomic<size_t> lazy_expirations{0};
    std::atomic<size_t> cleanup_operations{0};
    std::atomic<size_t> batch_operations{0};
    std::atomic<uint64_t> total_access_time_ns{0};
    std::atomic<uint64_t> total_cleanup_time_ns{0};
    std::atomic<size_t> compression_saves_bytes{0};

    CacheStatistics() = default;
    CacheStatistics(const CacheStatistics& other) noexcept;
    CacheStatistics(CacheStatistics&& other) noexcept;
    CacheStatistics& operator=(const CacheStatistics& other) noexcept;
    CacheStatistics& operator=(CacheStatistics&& other) noexcept;

    // Utility methods
    double get_hit_ratio() const noexcept {
        size_t total = hits.load() + misses.load();
        return total > 0 ? static_cast<double>(hits.load()) / total : 0.0;
    }

    double get_memory_usage_mb() const noexcept {
        return static_cast<double>(memory_usage_bytes.load()) / (1024.0 * 1024.0);
    }

    double get_average_access_time_ns() const noexcept {
        size_t total_accesses = hits.load() + misses.load();
        return total_accesses > 0 ?
            static_cast<double>(total_access_time_ns.load()) / total_accesses : 0.0;
    }
};

/**
 * @brief Configuration options for TTL Cache behavior.
 */
struct TTLCacheConfig {
    bool enable_automatic_cleanup{true};
    bool enable_statistics{true};
    bool thread_safe{true};
    size_t cleanup_batch_size{100};

    // Enhanced configuration options
    bool enable_lazy_expiration{true};      ///< Remove expired items on access
    bool enable_memory_tracking{false};     ///< Track memory usage
    size_t max_memory_mb{0};               ///< Memory limit (0 = unlimited)
    bool enable_compression{false};         ///< Enable value compression
    size_t compression_threshold{1024};     ///< Compress values larger than this

    // Performance tuning
    bool enable_expiration_index{true};     ///< Use expiration time index for faster cleanup
    size_t expiration_index_granularity{1000}; ///< Time granularity for expiration index (ms)
    bool enable_batch_optimization{true};   ///< Optimize batch operations

    // Monitoring and health
    bool enable_health_monitoring{true};    ///< Enable health monitoring
    double memory_pressure_threshold{0.8};  ///< Memory pressure threshold
    size_t max_cleanup_time_ms{10};        ///< Max time to spend in cleanup per iteration
};

/**
 * @brief A high-performance, thread-safe Time-to-Live (TTL) Cache with an
 * LRU eviction policy.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class TTLCache {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;
    using ValuePtr = std::shared_ptr<Value>;
    using EvictionCallback =
        std::function<void(const Key&, const Value&, bool)>;
    using KeyContainer = std::vector<Key>;
    using ValueContainer = std::vector<std::optional<Value>>;

    /**
     * @brief Constructs a TTLCache with the specified parameters.
     */
    explicit TTLCache(Duration ttl, size_t max_capacity,
                      std::optional<Duration> cleanup_interval = std::nullopt,
                      TTLCacheConfig config = TTLCacheConfig{},
                      EvictionCallback eviction_callback = nullptr);

    /**
     * @brief Destructor that properly shuts down the cache.
     */
    ~TTLCache() noexcept;

    TTLCache(const TTLCache&) = delete;
    TTLCache& operator=(const TTLCache&) = delete;
    TTLCache(TTLCache&&) = delete;
    TTLCache& operator=(TTLCache&&) = delete;

    // **Basic Operations**
    void put(const Key& key, const Value& value,
             std::optional<Duration> custom_ttl = std::nullopt);

    void put(const Key& key, Value&& value,
             std::optional<Duration> custom_ttl = std::nullopt);

    template <typename... Args>
    void emplace(const Key& key, std::optional<Duration> custom_ttl,
                 Args&&... args);

    void batch_put(const std::vector<std::pair<Key, Value>>& items,
                   std::optional<Duration> custom_ttl = std::nullopt);

    [[nodiscard]] std::optional<Value> get(const Key& key,
                                           bool update_access_time = true);

    [[nodiscard]] ValuePtr get_shared(const Key& key,
                                      bool update_access_time = true);

    [[nodiscard]] ValueContainer batch_get(const KeyContainer& keys,
                                           bool update_access_time = true);

    template <typename Factory>
    Value get_or_compute(const Key& key, Factory&& factory,
                         std::optional<Duration> custom_ttl = std::nullopt);

    bool remove(const Key& key) noexcept;
    size_t batch_remove(const KeyContainer& keys) noexcept;

    // **Query Operations**
    [[nodiscard]] bool contains(const Key& key) const noexcept;
    [[nodiscard]] std::optional<Duration> get_remaining_ttl(
        const Key& key) const noexcept;
    bool update_ttl(const Key& key, Duration new_ttl) noexcept;

    // **Management Operations**
    void force_cleanup() noexcept;
    void clear() noexcept;
    void resize(size_t new_capacity);

    // **Statistics and Information**
    [[nodiscard]] CacheStatistics get_statistics() const noexcept;
    void reset_statistics() noexcept;
    [[nodiscard]] double hit_rate() const noexcept;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return max_capacity_;
    }
    [[nodiscard]] constexpr Duration ttl() const noexcept { return ttl_; }
    [[nodiscard]] KeyContainer get_keys() const;

    // **Configuration**
    void set_eviction_callback(EvictionCallback callback) noexcept;
    void update_config(const TTLCacheConfig& new_config) noexcept;
    [[nodiscard]] TTLCacheConfig get_config() const noexcept;

    // **Enhanced Operations**
    [[nodiscard]] size_t get_memory_usage() const noexcept;
    [[nodiscard]] bool is_healthy() const noexcept;
    void optimize() noexcept;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_health_report() const;
    [[nodiscard]] std::unordered_map<std::string, double> get_efficiency_metrics() const;

    // **Async Operations**
    [[nodiscard]] std::future<std::optional<Value>> async_get(const Key& key);
    [[nodiscard]] std::future<void> async_put(const Key& key, const Value& value,
                                              std::optional<Duration> custom_ttl = std::nullopt);

    // **Advanced Features**
    void warm_cache(const std::function<std::vector<std::pair<Key, Value>>()>& loader,
                    std::optional<Duration> custom_ttl = std::nullopt);
    [[nodiscard]] std::vector<std::unordered_map<std::string, size_t>> get_shard_stats() const;

private:
    struct CacheItem {
        Key key;
        ValuePtr value;
        TimePoint expiry_time;
        TimePoint access_time;

        CacheItem(const Key& k, const Value& v, const TimePoint& expiry,
                  const TimePoint& access);
        CacheItem(const Key& k, Value&& v, const TimePoint& expiry,
                  const TimePoint& access);
        template <typename... Args>
        CacheItem(const Key& k, const TimePoint& expiry,
                  const TimePoint& access, Args&&... args);
    };

    using CacheList = std::list<CacheItem>;
    using CacheMap =
        std::unordered_map<Key, typename CacheList::iterator, Hash, KeyEqual>;

    struct Shard {
        explicit Shard(size_t capacity);
        CacheList list;
        CacheMap map;
        mutable std::shared_mutex mutex;
        size_t max_capacity;

        // Enhanced shard features
        std::atomic<size_t> memory_usage{0};
        std::multimap<TimePoint, typename CacheList::iterator> expiration_index;
        mutable std::atomic<size_t> lazy_cleanup_count{0};

        // Performance optimization
        void update_expiration_index(typename CacheList::iterator item, const TimePoint& old_expiry);
        void remove_from_expiration_index(typename CacheList::iterator item);
        size_t cleanup_expired_optimized(const TimePoint& now, size_t max_items);
    };

    // **Private Methods**
    Shard& get_shard(const Key& key) const;

    template <typename V>
    void put_impl(const Key& key, V&& value,
                  std::optional<Duration> custom_ttl);

    void move_to_front(Shard& shard, typename CacheList::iterator item);
    void evict_items(Shard& shard, size_t count) noexcept;
    void cleanup_expired_items(Shard& shard) noexcept;
    void cleanup_expired_items_optimized(Shard& shard) noexcept;
    void notify_eviction(const Key& key, const Value& value,
                         bool expired) noexcept;
    [[nodiscard]] inline bool is_expired(
        const TimePoint& expiry_time) const noexcept;
    void cleaner_task() noexcept;
    void cleanup() noexcept;

    // Enhanced private methods
    void lazy_cleanup_expired(Shard& shard, const Key& key) noexcept;
    size_t estimate_value_size(const Value& value) const noexcept;
    void update_memory_usage(Shard& shard, const Key& key, const Value& value, bool adding) noexcept;
    Value compress_value(const Value& value) const;
    Value decompress_value(const Value& value) const;
    void enforce_memory_limits() noexcept;

    // **Member Variables**
    Duration ttl_;
    Duration cleanup_interval_;
    std::atomic<size_t> max_capacity_;
    TTLCacheConfig config_;
    EvictionCallback eviction_callback_;

    std::vector<std::unique_ptr<Shard>> shards_;
    const size_t shard_mask_;

    std::atomic<size_t> current_size_{0};
    std::atomic<size_t> hit_count_{0};
    std::atomic<size_t> miss_count_{0};
    std::atomic<size_t> eviction_count_{0};
    std::atomic<size_t> expiration_count_{0};

    // Enhanced statistics
    std::atomic<size_t> lazy_expirations_{0};
    std::atomic<size_t> cleanup_operations_{0};
    std::atomic<size_t> batch_operations_{0};
    std::atomic<uint64_t> total_access_time_ns_{0};
    std::atomic<uint64_t> total_cleanup_time_ns_{0};
    std::atomic<size_t> compression_saves_bytes_{0};
    std::atomic<size_t> memory_usage_bytes_{0};

    std::thread cleaner_thread_;
    std::atomic<bool> stop_flag_{false};
    mutable std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_TTL_CACHE_HPP

#ifndef ATOM_SEARCH_TTL_CACHE_IMPL_HPP
#define ATOM_SEARCH_TTL_CACHE_IMPL_HPP

#include "ttl.hpp"

namespace atom::search {

// **TTLCacheException Implementation**
inline TTLCacheException::TTLCacheException(const std::string& message)
    : std::runtime_error(message) {}

// **CacheStatistics Implementation**
inline CacheStatistics::CacheStatistics(const CacheStatistics& other) noexcept {
    hits = other.hits.load();
    misses = other.misses.load();
    evictions = other.evictions.load();
    expirations = other.expirations.load();
    current_size = other.current_size;
    max_capacity = other.max_capacity;
    hit_rate = other.hit_rate;
    memory_usage_bytes = other.memory_usage_bytes.load();
    lazy_expirations = other.lazy_expirations.load();
    cleanup_operations = other.cleanup_operations.load();
    batch_operations = other.batch_operations.load();
    total_access_time_ns = other.total_access_time_ns.load();
    total_cleanup_time_ns = other.total_cleanup_time_ns.load();
    compression_saves_bytes = other.compression_saves_bytes.load();
}

inline CacheStatistics::CacheStatistics(CacheStatistics&& other) noexcept {
    hits = other.hits.load();
    misses = other.misses.load();
    evictions = other.evictions.load();
    expirations = other.expirations.load();
    current_size = other.current_size;
    max_capacity = other.max_capacity;
    hit_rate = other.hit_rate;
    memory_usage_bytes = other.memory_usage_bytes.load();
    lazy_expirations = other.lazy_expirations.load();
    cleanup_operations = other.cleanup_operations.load();
    batch_operations = other.batch_operations.load();
    total_access_time_ns = other.total_access_time_ns.load();
    total_cleanup_time_ns = other.total_cleanup_time_ns.load();
    compression_saves_bytes = other.compression_saves_bytes.load();
}

inline CacheStatistics& CacheStatistics::operator=(const CacheStatistics& other) noexcept {
    if (this != &other) {
        hits = other.hits.load();
        misses = other.misses.load();
        evictions = other.evictions.load();
        expirations = other.expirations.load();
        current_size = other.current_size;
        max_capacity = other.max_capacity;
        hit_rate = other.hit_rate;
        memory_usage_bytes = other.memory_usage_bytes.load();
        lazy_expirations = other.lazy_expirations.load();
        cleanup_operations = other.cleanup_operations.load();
        batch_operations = other.batch_operations.load();
        total_access_time_ns = other.total_access_time_ns.load();
        total_cleanup_time_ns = other.total_cleanup_time_ns.load();
        compression_saves_bytes = other.compression_saves_bytes.load();
    }
    return *this;
}

inline CacheStatistics& CacheStatistics::operator=(CacheStatistics&& other) noexcept {
    if (this != &other) {
        hits = other.hits.load();
        misses = other.misses.load();
        evictions = other.evictions.load();
        expirations = other.expirations.load();
        current_size = other.current_size;
        max_capacity = other.max_capacity;
        hit_rate = other.hit_rate;
        memory_usage_bytes = other.memory_usage_bytes.load();
        lazy_expirations = other.lazy_expirations.load();
        cleanup_operations = other.cleanup_operations.load();
        batch_operations = other.batch_operations.load();
        total_access_time_ns = other.total_access_time_ns.load();
        total_cleanup_time_ns = other.total_cleanup_time_ns.load();
        compression_saves_bytes = other.compression_saves_bytes.load();
    }
    return *this;
}

// **CacheItem Implementations**
template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::CacheItem::CacheItem(
    const Key& k, const Value& v, const TimePoint& expiry,
    const TimePoint& access)
    : key(k),
      value(std::make_shared<Value>(v)),
      expiry_time(expiry),
      access_time(access) {}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::CacheItem::CacheItem(
    const Key& k, Value&& v, const TimePoint& expiry, const TimePoint& access)
    : key(k),
      value(std::make_shared<Value>(std::move(v))),
      expiry_time(expiry),
      access_time(access) {}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename... Args>
TTLCache<Key, Value, Hash, KeyEqual>::CacheItem::CacheItem(
    const Key& k, const TimePoint& expiry, const TimePoint& access,
    Args&&... args)
    : key(k),
      value(std::make_shared<Value>(std::forward<Args>(args)...)),
      expiry_time(expiry),
      access_time(access) {}

// **Shard Implementation**
template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::Shard::Shard(size_t capacity)
    : max_capacity(capacity) {}

// **TTLCache Constructor**
template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::TTLCache(
    Duration ttl, size_t max_capacity, std::optional<Duration> cleanup_interval,
    TTLCacheConfig config, EvictionCallback eviction_callback)
    : ttl_(ttl),
      cleanup_interval_(cleanup_interval.value_or(ttl / 2)),
      max_capacity_(max_capacity),
      config_(std::move(config)),
      eviction_callback_(std::move(eviction_callback)),
      shard_mask_([&] {
          size_t shard_count = 1;
          if (config_.thread_safe) {
              shard_count = std::thread::hardware_concurrency();
              if (shard_count == 0)
                  shard_count = 4;
              size_t power = 1;
              while (power < shard_count)
                  power <<= 1;
              shard_count = power;
          }
          return shard_count - 1;
      }()) {
    if (ttl <= Duration::zero()) {
        throw TTLCacheException("TTL must be greater than zero");
    }
    if (max_capacity == 0) {
        throw TTLCacheException("Maximum capacity must be greater than zero");
    }

    size_t shard_count = shard_mask_ + 1;
    shards_.reserve(shard_count);
    size_t per_shard_capacity = (max_capacity + shard_count - 1) / shard_count;

    for (size_t i = 0; i < shard_count; ++i) {
        shards_.emplace_back(std::make_unique<Shard>(per_shard_capacity));
    }

    if (config_.enable_automatic_cleanup) {
        try {
            cleaner_thread_ = std::thread([this] { cleaner_task(); });
        } catch (const std::exception& e) {
            spdlog::error("Failed to create cleaner thread: {}", e.what());
            throw TTLCacheException("Failed to create cleaner thread: " +
                                    std::string(e.what()));
        }
    }
}

// **TTLCache Destructor**
template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCache<Key, Value, Hash, KeyEqual>::~TTLCache() noexcept {
    stop_flag_ = true;
    cleanup_cv_.notify_all();
    if (cleaner_thread_.joinable()) {
        cleaner_thread_.join();
    }
}

// **Private Helper Methods**
template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::get_shard(const Key& key) const
    -> Shard& {
    return *shards_[std::hash<Key>{}(key)&shard_mask_];
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename V>
void TTLCache<Key, Value, Hash, KeyEqual>::put_impl(
    const Key& key, V&& value, std::optional<Duration> custom_ttl) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto now = Clock::now();
        auto expiry = now + custom_ttl.value_or(ttl_);

        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            it->second->value = std::make_shared<Value>(std::forward<V>(value));
            it->second->expiry_time = expiry;
            it->second->access_time = now;
            move_to_front(shard, it->second);
        } else {
            if (shard.map.size() >= shard.max_capacity) {
                evict_items(shard, 1);
            }
            shard.list.emplace_front(key, std::forward<V>(value), expiry, now);
            shard.map[key] = shard.list.begin();
            current_size_++;
        }
    } catch (const std::bad_alloc&) {
        spdlog::error("Memory allocation failed while putting item in cache.");
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error putting item in cache: {}", e.what());
        throw TTLCacheException(std::string("Error putting item in cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::move_to_front(
    Shard& shard, typename CacheList::iterator item) {
    if (item != shard.list.begin()) {
        shard.list.splice(shard.list.begin(), shard.list, item);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::evict_items(Shard& shard,
                                                       size_t count) noexcept {
    for (size_t i = 0; i < count && !shard.list.empty(); ++i) {
        auto& last = shard.list.back();
        notify_eviction(last.key, *(last.value), false);
        shard.map.erase(last.key);
        shard.list.pop_back();
        current_size_--;
        if (config_.enable_statistics)
            eviction_count_++;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup_expired_items(
    Shard& shard) noexcept {
    size_t batch_count = 0;
    for (auto it = shard.list.begin();
         it != shard.list.end() && batch_count < config_.cleanup_batch_size;) {
        if (is_expired(it->expiry_time)) {
            notify_eviction(it->key, *(it->value), true);
            shard.map.erase(it->key);
            it = shard.list.erase(it);
            current_size_--;
            batch_count++;
            if (config_.enable_statistics)
                expiration_count_++;
        } else {
            ++it;
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::notify_eviction(
    const Key& key, const Value& value, bool expired) noexcept {
    try {
        if (eviction_callback_) {
            eviction_callback_(key, value, expired);
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in eviction callback: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
inline bool TTLCache<Key, Value, Hash, KeyEqual>::is_expired(
    const TimePoint& expiry_time) const noexcept {
    return expiry_time <= Clock::now();
}

// **Public Method Implementations**
template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, const Value& value, std::optional<Duration> custom_ttl) {
    put_impl(key, value, custom_ttl);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::put(
    const Key& key, Value&& value, std::optional<Duration> custom_ttl) {
    put_impl(key, std::move(value), custom_ttl);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename... Args>
void TTLCache<Key, Value, Hash, KeyEqual>::emplace(
    const Key& key, std::optional<Duration> custom_ttl, Args&&... args) {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto now = Clock::now();
        auto expiry = now + custom_ttl.value_or(ttl_);

        if (shard.map.count(key)) {
            auto it = shard.map.find(key);
            notify_eviction(it->second->key, *(it->second->value), false);
            shard.list.erase(it->second);
            shard.map.erase(it);
            current_size_--;
        }

        if (shard.map.size() >= shard.max_capacity) {
            evict_items(shard, 1);
        }

        shard.list.emplace_front(key, expiry, now, std::forward<Args>(args)...);
        shard.map[key] = shard.list.begin();
        current_size_++;
    } catch (const std::bad_alloc&) {
        spdlog::error("Memory allocation failed while emplacing item.");
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error emplacing item in cache: {}", e.what());
        throw TTLCacheException(std::string("Error emplacing item in cache: ") +
                                e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::batch_put(
    const std::vector<std::pair<Key, Value>>& items,
    std::optional<Duration> custom_ttl) {
    if (items.empty())
        return;

    try {
        auto ttl_to_use = custom_ttl.value_or(ttl_);
        std::vector<std::vector<std::pair<Key, Value>>> keys_by_shard(
            shards_.size());

        for (const auto& item : items) {
            keys_by_shard[std::hash<Key>{}(item.first) & shard_mask_].push_back(
                item);
        }

        for (size_t i = 0; i < shards_.size(); ++i) {
            if (keys_by_shard[i].empty())
                continue;

            auto& shard = *shards_[i];
            std::unique_lock lock(shard.mutex);
            auto now = Clock::now();

            for (const auto& item : keys_by_shard[i]) {
                auto expiry = now + ttl_to_use;
                auto it = shard.map.find(item.first);

                if (it != shard.map.end()) {
                    it->second->value = std::make_shared<Value>(item.second);
                    it->second->expiry_time = expiry;
                    it->second->access_time = now;
                    move_to_front(shard, it->second);
                } else {
                    if (shard.map.size() >= shard.max_capacity) {
                        evict_items(shard, 1);
                    }
                    shard.list.emplace_front(item.first, item.second, expiry,
                                             now);
                    shard.map[item.first] = shard.list.begin();
                    current_size_++;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during batch put: {}", e.what());
        throw TTLCacheException(std::string("Error during batch put: ") +
                                e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<Value> TTLCache<Key, Value, Hash, KeyEqual>::get(
    const Key& key, bool update_access_time) {
    auto shared_val = get_shared(key, update_access_time);
    return shared_val ? std::optional<Value>(*shared_val) : std::nullopt;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::get_shared(const Key& key,
                                                      bool update_access_time)
    -> ValuePtr {
    try {
        auto& shard = get_shard(key);
        auto start_time = config_.enable_statistics ? Clock::now() : TimePoint{};

        if (update_access_time) {
            std::unique_lock lock(shard.mutex);
            auto it = shard.map.find(key);
            if (it == shard.map.end()) {
                if (config_.enable_statistics)
                    miss_count_++;
                return nullptr;
            }

            // Check expiration and perform lazy cleanup if enabled
            if (is_expired(it->second->expiry_time)) {
                if (config_.enable_lazy_expiration) {
                    // Remove expired item immediately
                    notify_eviction(it->second->key, *(it->second->value), true);
                    shard.remove_from_expiration_index(it->second);
                    shard.list.erase(it->second);
                    shard.map.erase(it);
                    current_size_--;
                    shard.lazy_cleanup_count++;
                    if (config_.enable_statistics)
                        lazy_expirations_++;
                }
                if (config_.enable_statistics)
                    miss_count_++;
                return nullptr;
            }

            it->second->access_time = Clock::now();
            move_to_front(shard, it->second);
            if (config_.enable_statistics) {
                hit_count_++;
                if (start_time != TimePoint{}) {
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - start_time).count();
                    total_access_time_ns_.fetch_add(duration);
                }
            }
            return it->second->value;
        } else {
            std::shared_lock lock(shard.mutex);
            auto it = shard.map.find(key);
            if (it == shard.map.end() || is_expired(it->second->expiry_time)) {
                if (config_.enable_statistics)
                    miss_count_++;
                return nullptr;
            }
            if (config_.enable_statistics) {
                hit_count_++;
                if (start_time != TimePoint{}) {
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - start_time).count();
                    total_access_time_ns_.fetch_add(duration);
                }
            }
            return it->second->value;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error getting item from cache: {}", e.what());
        if (config_.enable_statistics)
            miss_count_++;
        return nullptr;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::batch_get(const KeyContainer& keys,
                                                     bool update_access_time)
    -> ValueContainer {
    if (keys.empty())
        return {};

    ValueContainer results(keys.size());
    std::unordered_map<const Key*, size_t> key_to_idx;
    for (size_t i = 0; i < keys.size(); ++i) {
        key_to_idx[&keys[i]] = i;
    }

    std::vector<std::vector<const Key*>> keys_by_shard(shards_.size());
    for (const auto& key : keys) {
        keys_by_shard[std::hash<Key>{}(key)&shard_mask_].push_back(&key);
    }

    for (size_t i = 0; i < shards_.size(); ++i) {
        if (keys_by_shard[i].empty())
            continue;

        auto& shard = *shards_[i];
        auto now = Clock::now();

        if (update_access_time) {
            std::unique_lock lock(shard.mutex);
            for (const Key* key_ptr : keys_by_shard[i]) {
                auto it = shard.map.find(*key_ptr);
                if (it != shard.map.end() &&
                    !is_expired(it->second->expiry_time)) {
                    it->second->access_time = now;
                    move_to_front(shard, it->second);
                    results[key_to_idx[key_ptr]] = *(it->second->value);
                    if (config_.enable_statistics)
                        hit_count_++;
                } else {
                    if (config_.enable_statistics)
                        miss_count_++;
                }
            }
        } else {
            std::shared_lock lock(shard.mutex);
            for (const Key* key_ptr : keys_by_shard[i]) {
                auto it = shard.map.find(*key_ptr);
                if (it != shard.map.end() &&
                    !is_expired(it->second->expiry_time)) {
                    results[key_to_idx[key_ptr]] = *(it->second->value);
                    if (config_.enable_statistics)
                        hit_count_++;
                } else {
                    if (config_.enable_statistics)
                        miss_count_++;
                }
            }
        }
    }
    return results;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
template <typename Factory>
Value TTLCache<Key, Value, Hash, KeyEqual>::get_or_compute(
    const Key& key, Factory&& factory, std::optional<Duration> custom_ttl) {
    auto cached_value = get_shared(key);
    if (cached_value) {
        return *cached_value;
    }

    Value computed_value = factory();
    put(key, computed_value, custom_ttl);
    return computed_value;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::remove(const Key& key) noexcept {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            notify_eviction(it->second->key, *(it->second->value), false);
            shard.list.erase(it->second);
            shard.map.erase(it);
            current_size_--;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error removing item from cache: {}", e.what());
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::batch_remove(
    const KeyContainer& keys) noexcept {
    if (keys.empty())
        return 0;

    size_t removed_count = 0;
    std::vector<std::vector<Key>> keys_by_shard(shards_.size());
    for (const auto& key : keys) {
        keys_by_shard[std::hash<Key>{}(key)&shard_mask_].push_back(key);
    }

    for (size_t i = 0; i < shards_.size(); ++i) {
        if (keys_by_shard[i].empty())
            continue;

        auto& shard = *shards_[i];
        std::unique_lock lock(shard.mutex);
        for (const auto& key : keys_by_shard[i]) {
            auto it = shard.map.find(key);
            if (it != shard.map.end()) {
                notify_eviction(it->second->key, *(it->second->value), false);
                shard.list.erase(it->second);
                shard.map.erase(it);
                current_size_--;
                removed_count++;
            }
        }
    }
    return removed_count;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::contains(
    const Key& key) const noexcept {
    try {
        auto& shard = get_shard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        return (it != shard.map.end() && !is_expired(it->second->expiry_time));
    } catch (const std::exception& e) {
        spdlog::error("Error in contains check: {}", e.what());
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::update_ttl(
    const Key& key, Duration new_ttl) noexcept {
    try {
        auto& shard = get_shard(key);
        std::unique_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end() && !is_expired(it->second->expiry_time)) {
            it->second->expiry_time = Clock::now() + new_ttl;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error updating TTL: {}", e.what());
        return false;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::optional<typename TTLCache<Key, Value, Hash, KeyEqual>::Duration>
TTLCache<Key, Value, Hash, KeyEqual>::get_remaining_ttl(
    const Key& key) const noexcept {
    try {
        auto& shard = get_shard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.map.find(key);
        if (it != shard.map.end()) {
            auto now = Clock::now();
            if (it->second->expiry_time > now) {
                return std::chrono::duration_cast<Duration>(
                    it->second->expiry_time - now);
            }
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        spdlog::error("Error getting remaining TTL: {}", e.what());
        return std::nullopt;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::force_cleanup() noexcept {
    cleanup();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
CacheStatistics TTLCache<Key, Value, Hash, KeyEqual>::get_statistics()
    const noexcept {
    CacheStatistics stats;
    stats.hits = hit_count_.load();
    stats.misses = miss_count_.load();
    stats.evictions = eviction_count_.load();
    stats.expirations = expiration_count_.load();
    stats.current_size = current_size_.load();
    stats.max_capacity = max_capacity_.load();

    size_t total = stats.hits + stats.misses;
    stats.hit_rate = total > 0 ? static_cast<double>(stats.hits) / total : 0.0;
    return stats;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::reset_statistics() noexcept {
    if (config_.enable_statistics) {
        hit_count_ = 0;
        miss_count_ = 0;
        eviction_count_ = 0;
        expiration_count_ = 0;
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
double TTLCache<Key, Value, Hash, KeyEqual>::hit_rate() const noexcept {
    if (!config_.enable_statistics)
        return 0.0;

    size_t hits = hit_count_.load();
    size_t misses = miss_count_.load();
    size_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::size() const noexcept {
    return current_size_.load();
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::empty() const noexcept {
    return size() == 0;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
auto TTLCache<Key, Value, Hash, KeyEqual>::get_keys() const -> KeyContainer {
    KeyContainer all_keys;
    all_keys.reserve(size());

    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& item : shard_ptr->list) {
            if (!is_expired(item.expiry_time)) {
                all_keys.push_back(item.key);
            }
        }
    }
    return all_keys;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::clear() noexcept {
    try {
        for (auto& shard_ptr : shards_) {
            std::unique_lock lock(shard_ptr->mutex);
            if (eviction_callback_) {
                for (const auto& item : shard_ptr->list) {
                    notify_eviction(item.key, *(item.value), false);
                }
            }
            shard_ptr->list.clear();
            shard_ptr->map.clear();
        }
        current_size_ = 0;
        reset_statistics();
    } catch (const std::exception& e) {
        spdlog::error("Error clearing cache: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::resize(size_t new_capacity) {
    if (new_capacity == 0) {
        throw TTLCacheException("New capacity must be greater than zero");
    }

    max_capacity_ = new_capacity;
    size_t per_shard_capacity =
        (new_capacity + shards_.size() - 1) / shards_.size();

    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->max_capacity = per_shard_capacity;
        if (shard_ptr->map.size() > per_shard_capacity) {
            evict_items(*shard_ptr, shard_ptr->map.size() - per_shard_capacity);
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::set_eviction_callback(
    EvictionCallback callback) noexcept {
    std::lock_guard<std::mutex> lock(cleanup_mutex_);
    eviction_callback_ = std::move(callback);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::update_config(
    const TTLCacheConfig& new_config) noexcept {
    std::lock_guard<std::mutex> lock(cleanup_mutex_);
    config_ = new_config;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
TTLCacheConfig TTLCache<Key, Value, Hash, KeyEqual>::get_config() const noexcept {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(cleanup_mutex_));
    return config_;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleaner_task() noexcept {
    while (!stop_flag_) {
        try {
            std::unique_lock<std::mutex> lock(cleanup_mutex_);
            cleanup_cv_.wait_for(lock, cleanup_interval_,
                                 [this] { return stop_flag_.load(); });
            if (stop_flag_)
                break;
            lock.unlock();
            cleanup();
        } catch (const std::exception& e) {
            spdlog::error("Exception in cleaner task: {}", e.what());
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup() noexcept {
    auto start_time = config_.enable_statistics ? Clock::now() : TimePoint{};

    for (auto& shard_ptr : shards_) {
        if (stop_flag_)
            return;
        try {
            std::unique_lock lock(shard_ptr->mutex);
            if (config_.enable_expiration_index) {
                cleanup_expired_items_optimized(*shard_ptr);
            } else {
                cleanup_expired_items(*shard_ptr);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error during shard cleanup: {}", e.what());
        }
    }

    if (config_.enable_statistics && start_time != TimePoint{}) {
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start_time).count();
        total_cleanup_time_ns_.fetch_add(duration);
        cleanup_operations_++;
    }
}

// Implementation of enhanced methods
template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::cleanup_expired_items_optimized(
    Shard& shard) noexcept {
    if (!config_.enable_expiration_index) {
        cleanup_expired_items(shard);
        return;
    }

    auto now = Clock::now();
    size_t cleaned = shard.cleanup_expired_optimized(now, config_.cleanup_batch_size);

    if (config_.enable_statistics) {
        expiration_count_.fetch_add(cleaned);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::get_memory_usage() const noexcept {
    if (config_.enable_memory_tracking) {
        return memory_usage_bytes_.load();
    }

    // Estimate memory usage
    size_t total_memory = 0;
    size_t current_size = size();

    // Rough estimation: each entry has key + value + overhead
    total_memory += current_size * (sizeof(Key) + sizeof(Value) + 128); // 128 bytes overhead
    total_memory += shards_.size() * sizeof(Shard);

    return total_memory;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
bool TTLCache<Key, Value, Hash, KeyEqual>::is_healthy() const noexcept {
    if (!config_.enable_health_monitoring) {
        return true;
    }

    // Check memory limits
    if (config_.max_memory_mb > 0) {
        double memory_mb = static_cast<double>(get_memory_usage()) / (1024.0 * 1024.0);
        if (memory_mb > config_.max_memory_mb) {
            return false;
        }
    }

    // Check hit ratio if we have enough operations
    if (config_.enable_statistics) {
        size_t total_ops = hit_count_.load() + miss_count_.load();
        if (total_ops > 100) {  // Only check if we have enough data
            double hit_ratio = static_cast<double>(hit_count_.load()) / total_ops;
            if (hit_ratio < 0.1) {  // Less than 10% hit ratio is concerning
                return false;
            }
        }
    }

    return true;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::optimize() noexcept {
    spdlog::info("Starting TTL cache optimization...");

    // Force cleanup of expired items
    cleanup();

    // Enforce memory limits if configured
    if (config_.max_memory_mb > 0) {
        enforce_memory_limits();
    }

    spdlog::info("TTL cache optimization completed");
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::unordered_map<std::string, std::string>
TTLCache<Key, Value, Hash, KeyEqual>::get_health_report() const {
    std::unordered_map<std::string, std::string> report;

    if (!config_.enable_health_monitoring) {
        report["status"] = "MONITORING_DISABLED";
        return report;
    }

    auto stats = get_statistics();
    double hit_ratio = stats.get_hit_ratio();
    double memory_usage_mb = stats.get_memory_usage_mb();

    // Overall health assessment
    if (hit_ratio < 0.1 && (stats.hits + stats.misses) > 100) {
        report["status"] = "UNHEALTHY";
        report["hit_ratio_warning"] = "Hit ratio (" + std::to_string(hit_ratio) +
                                     ") is very low";
    } else if (hit_ratio < 0.5 && (stats.hits + stats.misses) > 100) {
        report["status"] = "WARNING";
        report["hit_ratio_info"] = "Hit ratio could be improved";
    } else {
        report["status"] = "HEALTHY";
    }

    // Memory health
    if (config_.max_memory_mb > 0) {
        double memory_usage_ratio = memory_usage_mb / config_.max_memory_mb;
        if (memory_usage_ratio > config_.memory_pressure_threshold) {
            report["memory_warning"] = "Memory usage (" + std::to_string(memory_usage_mb) +
                                      "MB) is above threshold (" + std::to_string(config_.memory_pressure_threshold * 100) + "%)";
        }
    }

    // Expiration health
    if (config_.enable_statistics) {
        size_t total_ops = stats.hits + stats.misses;
        if (total_ops > 0 && (static_cast<double>(stats.expirations) / total_ops) > 0.5) {
            report["expiration_warning"] = "High expiration rate detected - consider increasing TTL";
        }
    }

    return report;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::unordered_map<std::string, double>
TTLCache<Key, Value, Hash, KeyEqual>::get_efficiency_metrics() const {
    std::unordered_map<std::string, double> metrics;

    auto stats = get_statistics();

    // Basic efficiency metrics
    metrics["hit_ratio"] = stats.get_hit_ratio();
    metrics["memory_efficiency"] = size() > 0 ?
        static_cast<double>(get_memory_usage()) / size() : 0.0;

    // Load factor
    metrics["load_factor"] = static_cast<double>(size()) / capacity();

    // Expiration efficiency
    size_t total_ops = stats.hits + stats.misses;
    metrics["expiration_rate"] = total_ops > 0 ?
        static_cast<double>(stats.expirations) / total_ops : 0.0;

    // Performance metrics
    if (config_.enable_statistics) {
        metrics["avg_access_time_ns"] = stats.get_average_access_time_ns();
        metrics["cleanup_operations"] = static_cast<double>(cleanup_operations_.load());
        metrics["lazy_expirations"] = static_cast<double>(lazy_expirations_.load());
    }

    return metrics;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::future<std::optional<Value>>
TTLCache<Key, Value, Hash, KeyEqual>::async_get(const Key& key) {
    return std::async(std::launch::async, [this, key]() {
        return get(key);
    });
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::future<void> TTLCache<Key, Value, Hash, KeyEqual>::async_put(
    const Key& key, const Value& value, std::optional<Duration> custom_ttl) {
    return std::async(std::launch::async, [this, key, value, custom_ttl]() {
        put(key, value, custom_ttl);
    });
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::warm_cache(
    const std::function<std::vector<std::pair<Key, Value>>()>& loader,
    std::optional<Duration> custom_ttl) {

    try {
        auto items = loader();
        spdlog::info("Warming TTL cache with {} items", items.size());

        batch_put(items, custom_ttl);

        spdlog::info("TTL cache warming completed successfully");
    } catch (const std::exception& e) {
        spdlog::error("TTL cache warming failed: {}", e.what());
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
std::vector<std::unordered_map<std::string, size_t>>
TTLCache<Key, Value, Hash, KeyEqual>::get_shard_stats() const {
    std::vector<std::unordered_map<std::string, size_t>> stats;
    stats.reserve(shards_.size());

    for (size_t i = 0; i < shards_.size(); ++i) {
        std::unordered_map<std::string, size_t> shard_stats;
        std::shared_lock lock(shards_[i]->mutex);

        shard_stats["shard_id"] = i;
        shard_stats["size"] = shards_[i]->list.size();
        shard_stats["max_capacity"] = shards_[i]->max_capacity;
        shard_stats["memory_usage"] = shards_[i]->memory_usage.load();
        shard_stats["lazy_cleanup_count"] = shards_[i]->lazy_cleanup_count.load();
        shard_stats["load_factor"] = static_cast<size_t>(
            (static_cast<double>(shards_[i]->list.size()) / shards_[i]->max_capacity) * 100);

        stats.push_back(std::move(shard_stats));
    }

    return stats;
}

// Implementation of Shard enhanced methods
template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::Shard::update_expiration_index(
    typename CacheList::iterator item, const TimePoint& old_expiry) {
    // Remove old entry if it exists
    if (old_expiry != TimePoint{}) {
        auto range = expiration_index.equal_range(old_expiry);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == item) {
                expiration_index.erase(it);
                break;
            }
        }
    }

    // Add new entry
    expiration_index.emplace(item->expiry_time, item);
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::Shard::remove_from_expiration_index(
    typename CacheList::iterator item) {
    auto range = expiration_index.equal_range(item->expiry_time);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == item) {
            expiration_index.erase(it);
            break;
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::Shard::cleanup_expired_optimized(
    const TimePoint& now, size_t max_items) {
    size_t cleaned = 0;

    // Use expiration index for efficient cleanup
    auto it = expiration_index.begin();
    while (it != expiration_index.end() && it->first <= now && cleaned < max_items) {
        auto list_it = it->second;

        // Verify the item is still in the list and expired
        if (list_it->expiry_time <= now) {
            // Remove from map and list
            map.erase(list_it->key);
            list.erase(list_it);

            // Remove from expiration index
            it = expiration_index.erase(it);
            cleaned++;
        } else {
            ++it;
        }
    }

    return cleaned;
}

// Implementation of utility methods
template <typename Key, typename Value, typename Hash, typename KeyEqual>
size_t TTLCache<Key, Value, Hash, KeyEqual>::estimate_value_size(const Value& value) const noexcept {
    if constexpr (std::is_arithmetic_v<Value>) {
        return sizeof(Value);
    } else if constexpr (std::is_same_v<Value, std::string>) {
        return sizeof(std::string) + value.capacity();
    } else if constexpr (requires { value.size(); }) {
        // For containers with size() method
        return sizeof(Value) + value.size() * sizeof(typename Value::value_type);
    } else {
        // Default estimation for complex types
        return sizeof(Value) + 64;  // Base size + estimated overhead
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::update_memory_usage(
    Shard& shard, const Key& /* key */, const Value& value, bool adding) noexcept {
    if (!config_.enable_memory_tracking) return;

    size_t key_size = sizeof(Key);
    size_t value_size = estimate_value_size(value);
    size_t total_size = key_size + value_size + sizeof(CacheItem);

    if (adding) {
        shard.memory_usage.fetch_add(total_size);
        memory_usage_bytes_.fetch_add(total_size);
    } else {
        shard.memory_usage.fetch_sub(total_size);
        memory_usage_bytes_.fetch_sub(total_size);
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
void TTLCache<Key, Value, Hash, KeyEqual>::enforce_memory_limits() noexcept {
    if (config_.max_memory_mb == 0) return;

    size_t max_bytes = config_.max_memory_mb * 1024 * 1024;
    size_t current_bytes = get_memory_usage();

    if (current_bytes > max_bytes) {
        spdlog::info("Enforcing memory limits: current {} MB, limit {} MB",
                     current_bytes / (1024 * 1024), config_.max_memory_mb);

        // Force cleanup and eviction until under limit

        for (auto& shard_ptr : shards_) {
            if (get_memory_usage() <= max_bytes) break;

            std::unique_lock lock(shard_ptr->mutex);
            size_t items_to_evict = shard_ptr->list.size() / 5;  // Evict 20% from this shard
            evict_items(*shard_ptr, items_to_evict);
        }
    }
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
Value TTLCache<Key, Value, Hash, KeyEqual>::compress_value(const Value& value) const {
    // Placeholder for compression - would need actual compression library
    if (config_.enable_compression) {
        // TODO: Implement actual compression
        spdlog::debug("Compression not yet implemented");
    }
    return value;
}

template <typename Key, typename Value, typename Hash, typename KeyEqual>
Value TTLCache<Key, Value, Hash, KeyEqual>::decompress_value(const Value& value) const {
    // Placeholder for decompression - would need actual compression library
    if (config_.enable_compression) {
        // TODO: Implement actual decompression
        spdlog::debug("Decompression not yet implemented");
    }
    return value;
}

}  // namespace atom::search

#endif  // ATOM_SEARCH_TTL_CACHE_IMPL_HPP
