#pragma once

#include "thread_safe_xml.hpp"
#include "lock_free_pool.hpp"
#include "parallel_processor.hpp"
#include "../performance/metrics_collector.hpp"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <future>
#include <chrono>
#include <thread>
#include <shared_mutex>
#include <concepts>
#include <algorithm>
#include <execution>

namespace atom::extra::pugixml::concurrent {

/**
 * @brief Hash function for XPath queries
 */
struct XPathHash {
    [[nodiscard]] size_t operator()(const std::string& xpath) const noexcept {
        return std::hash<std::string>{}(xpath);
    }
};

/**
 * @brief Lock-free LRU cache for query results
 */
template<typename Key, typename Value, typename Hash = std::hash<Key>>
class LockFreeLRUCache {
private:
    struct CacheEntry {
        Key key;
        Value value;
        std::atomic<uint64_t> access_time{0};
        std::atomic<CacheEntry*> next{nullptr};
        std::atomic<CacheEntry*> prev{nullptr};
        std::atomic<bool> valid{true};
        
        CacheEntry(Key k, Value v) : key(std::move(k)), value(std::move(v)) {
            access_time.store(std::chrono::steady_clock::now().time_since_epoch().count(),
                            std::memory_order_relaxed);
        }
    };
    
    static constexpr size_t DEFAULT_CAPACITY = 1024;
    static constexpr size_t HASH_TABLE_SIZE = 2048;
    
    std::array<std::atomic<CacheEntry*>, HASH_TABLE_SIZE> hash_table_{};
    LockFreePool<CacheEntry> entry_pool_;
    std::atomic<size_t> size_{0};
    size_t capacity_;
    std::shared_ptr<spdlog::logger> logger_;
    
    [[nodiscard]] size_t hash_index(const Key& key) const noexcept {
        return Hash{}(key) % HASH_TABLE_SIZE;
    }
    
    void evict_oldest() {
        // Find and remove the oldest entry
        CacheEntry* oldest = nullptr;
        uint64_t oldest_time = UINT64_MAX;
        
        for (auto& bucket : hash_table_) {
            CacheEntry* entry = bucket.load(std::memory_order_acquire);
            while (entry) {
                if (entry->valid.load(std::memory_order_acquire)) {
                    auto access_time = entry->access_time.load(std::memory_order_relaxed);
                    if (access_time < oldest_time) {
                        oldest_time = access_time;
                        oldest = entry;
                    }
                }
                entry = entry->next.load(std::memory_order_acquire);
            }
        }
        
        if (oldest) {
            oldest->valid.store(false, std::memory_order_release);
            size_.fetch_sub(1, std::memory_order_relaxed);
            
            if (logger_) {
                logger_->trace("Evicted cache entry");
            }
        }
    }
    
public:
    explicit LockFreeLRUCache(size_t capacity = DEFAULT_CAPACITY,
                             std::shared_ptr<spdlog::logger> logger = nullptr)
        : entry_pool_(logger), capacity_(capacity), logger_(logger) {
        
        if (logger_) {
            logger_->debug("LockFreeLRUCache created with capacity {}", capacity_);
        }
    }
    
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        size_t index = hash_index(key);
        CacheEntry* entry = hash_table_[index].load(std::memory_order_acquire);
        
        while (entry) {
            if (entry->valid.load(std::memory_order_acquire) && entry->key == key) {
                // Update access time
                entry->access_time.store(
                    std::chrono::steady_clock::now().time_since_epoch().count(),
                    std::memory_order_relaxed);
                
                if (logger_) {
                    logger_->trace("Cache hit for key");
                }
                return entry->value;
            }
            entry = entry->next.load(std::memory_order_acquire);
        }
        
        if (logger_) {
            logger_->trace("Cache miss for key");
        }
        return std::nullopt;
    }
    
    void put(const Key& key, const Value& value) {
        // Check if we need to evict
        if (size_.load(std::memory_order_relaxed) >= capacity_) {
            evict_oldest();
        }
        
        size_t index = hash_index(key);
        auto new_entry = entry_pool_.allocate();
        new (new_entry) CacheEntry(key, value);
        
        // Insert at the beginning of the bucket
        CacheEntry* old_head = hash_table_[index].load(std::memory_order_acquire);
        do {
            new_entry->next.store(old_head, std::memory_order_relaxed);
        } while (!hash_table_[index].compare_exchange_weak(old_head, new_entry,
                                                          std::memory_order_release,
                                                          std::memory_order_acquire));
        
        size_.fetch_add(1, std::memory_order_relaxed);
        
        if (logger_) {
            logger_->trace("Added entry to cache");
        }
    }
    
    void clear() {
        for (auto& bucket : hash_table_) {
            bucket.store(nullptr, std::memory_order_release);
        }
        size_.store(0, std::memory_order_release);
        
        if (logger_) {
            logger_->debug("Cache cleared");
        }
    }
    
    [[nodiscard]] size_t size() const noexcept {
        return size_.load(std::memory_order_acquire);
    }
    
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }
};

/**
 * @brief Query result with metadata
 */
struct QueryResult {
    std::vector<ThreadSafeNode> nodes;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::microseconds execution_time;
    bool from_cache;
    
    QueryResult() : timestamp(std::chrono::steady_clock::now()), from_cache(false) {}
    
    explicit QueryResult(std::vector<ThreadSafeNode> result_nodes, 
                        std::chrono::microseconds exec_time = {},
                        bool cached = false)
        : nodes(std::move(result_nodes)), 
          timestamp(std::chrono::steady_clock::now()),
          execution_time(exec_time),
          from_cache(cached) {}
};

/**
 * @brief High-performance parallel XPath query engine
 */
class ParallelQueryEngine {
private:
    ParallelXmlProcessor processor_;
    LockFreeLRUCache<std::string, QueryResult, XPathHash> result_cache_;
    performance::MetricsCollector metrics_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<bool> cache_enabled_{true};
    
    /**
     * @brief Execute XPath query without caching
     */
    QueryResult execute_xpath_internal(const ThreadSafeNode& root, const std::string& xpath) {
        auto timer = performance::HighResolutionTimer{};
        
        try {
            // Convert to native pugi node for XPath execution
            auto native_node = root.native();
            auto xpath_result = native_node.select_nodes(xpath.c_str());
            
            std::vector<ThreadSafeNode> result_nodes;
            result_nodes.reserve(xpath_result.size());
            
            for (const auto& selected : xpath_result) {
                result_nodes.emplace_back(selected.node(), logger_);
            }
            
            auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                timer.elapsed());
            
            metrics_.record_timing("xpath_execution", timer.elapsed_microseconds());
            
            if (logger_) {
                logger_->debug("XPath query '{}' returned {} nodes in {:.3f}μs", 
                              xpath, result_nodes.size(), timer.elapsed_microseconds());
            }
            
            return QueryResult{std::move(result_nodes), execution_time, false};
            
        } catch (const std::exception& e) {
            metrics_.record_error("xpath_execution");
            if (logger_) {
                logger_->error("XPath query '{}' failed: {}", xpath, e.what());
            }
            throw;
        }
    }
    
public:
    explicit ParallelQueryEngine(size_t num_threads = std::thread::hardware_concurrency(),
                                size_t cache_capacity = 1024,
                                std::shared_ptr<spdlog::logger> logger = nullptr)
        : processor_(num_threads, logger),
          result_cache_(cache_capacity, logger),
          metrics_(logger),
          logger_(logger) {
        
        if (logger_) {
            logger_->info("ParallelQueryEngine created with {} threads, cache capacity {}",
                         num_threads, cache_capacity);
        }
    }
    
    /**
     * @brief Execute XPath query with caching support
     */
    [[nodiscard]] QueryResult query(const ThreadSafeNode& root, const std::string& xpath) {
        auto timer = performance::HighResolutionTimer{};
        
        // Try cache first if enabled
        if (cache_enabled_.load(std::memory_order_relaxed)) {
            if (auto cached_result = result_cache_.get(xpath)) {
                metrics_.record_timing("cache_hit", timer.elapsed_microseconds());
                if (logger_) {
                    logger_->trace("Cache hit for XPath: {}", xpath);
                }
                cached_result->from_cache = true;
                return *cached_result;
            }
        }
        
        // Execute query
        auto result = execute_xpath_internal(root, xpath);
        
        // Cache result if enabled
        if (cache_enabled_.load(std::memory_order_relaxed)) {
            result_cache_.put(xpath, result);
        }
        
        return result;
    }
    
    /**
     * @brief Execute multiple XPath queries in parallel
     */
    [[nodiscard]] std::vector<std::future<QueryResult>> 
    query_parallel(const ThreadSafeNode& root, const std::vector<std::string>& xpaths) {
        
        std::vector<std::future<QueryResult>> futures;
        futures.reserve(xpaths.size());
        
        for (const auto& xpath : xpaths) {
            futures.push_back(
                processor_.submit_async([this, &root, xpath]() {
                    return query(root, xpath);
                })
            );
        }
        
        if (logger_) {
            logger_->debug("Submitted {} parallel XPath queries", xpaths.size());
        }
        
        return futures;
    }
    
    /**
     * @brief Execute XPath query with custom predicate filtering
     */
    template<typename Predicate>
    [[nodiscard]] QueryResult query_filtered(const ThreadSafeNode& root, 
                                            const std::string& xpath,
                                            Predicate&& predicate) {
        auto base_result = query(root, xpath);
        
        std::vector<ThreadSafeNode> filtered_nodes;
        std::copy_if(base_result.nodes.begin(), base_result.nodes.end(),
                    std::back_inserter(filtered_nodes),
                    std::forward<Predicate>(predicate));
        
        return QueryResult{std::move(filtered_nodes), base_result.execution_time, false};
    }
    
    /**
     * @brief Clear query result cache
     */
    void clear_cache() {
        result_cache_.clear();
        if (logger_) {
            logger_->info("Query result cache cleared");
        }
    }
    
    /**
     * @brief Enable/disable result caching
     */
    void set_cache_enabled(bool enabled) noexcept {
        cache_enabled_.store(enabled, std::memory_order_relaxed);
        if (logger_) {
            logger_->info("Query result caching {}", enabled ? "enabled" : "disabled");
        }
    }
    
    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] size_t cache_size() const noexcept {
        return result_cache_.size();
    }
    
    /**
     * @brief Get performance metrics
     */
    [[nodiscard]] auto get_metrics() const {
        return metrics_.get_all_stats();
    }
    
    /**
     * @brief Generate performance report
     */
    void generate_report() const {
        metrics_.generate_report();
    }
};

}  // namespace atom::extra::pugixml::concurrent
