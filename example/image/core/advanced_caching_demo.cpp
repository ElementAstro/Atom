/**
 * @file advanced_caching_demo.cpp
 * @brief Advanced caching strategies demonstration
 *
 * This example demonstrates:
 * - Intelligent caching strategies (LRU, LFU, FIFO)
 * - Prefetching and predictive loading
 * - Cache optimization and performance tuning
 * - Memory-aware caching with size limits
 * - Multi-level caching hierarchies
 * - Cache coherency and invalidation
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <iomanip>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/core/cache_manager.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief LRU (Least Recently Used) Cache implementation
 */
template<typename Key, typename Value>
class LRUCache {
private:
    struct CacheNode {
        Key key;
        Value value;
        steady_clock::time_point lastAccess;
        size_t accessCount;
        
        CacheNode(const Key& k, const Value& v) 
            : key(k), value(v), lastAccess(steady_clock::now()), accessCount(1) {}
    };
    
    size_t maxSize_;
    size_t maxMemory_;
    std::atomic<size_t> currentMemory_{0};
    std::unordered_map<Key, typename std::list<CacheNode>::iterator> keyMap_;
    std::list<CacheNode> cacheList_;
    mutable std::mutex mutex_;
    
    // Statistics
    std::atomic<size_t> hits_{0};
    std::atomic<size_t> misses_{0};
    std::atomic<size_t> evictions_{0};

public:
    LRUCache(size_t maxSize, size_t maxMemory) 
        : maxSize_(maxSize), maxMemory_(maxMemory) {}
    
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = keyMap_.find(key);
        if (it == keyMap_.end()) {
            misses_++;
            return false;
        }
        
        // Move to front (most recently used)
        auto nodeIt = it->second;
        nodeIt->lastAccess = steady_clock::now();
        nodeIt->accessCount++;
        
        cacheList_.splice(cacheList_.begin(), cacheList_, nodeIt);
        keyMap_[key] = cacheList_.begin();
        
        value = nodeIt->value;
        hits_++;
        return true;
    }
    
    void put(const Key& key, const Value& value, size_t valueSize = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if key already exists
        auto it = keyMap_.find(key);
        if (it != keyMap_.end()) {
            // Update existing entry
            auto nodeIt = it->second;
            nodeIt->value = value;
            nodeIt->lastAccess = steady_clock::now();
            nodeIt->accessCount++;
            
            // Move to front
            cacheList_.splice(cacheList_.begin(), cacheList_, nodeIt);
            keyMap_[key] = cacheList_.begin();
            return;
        }
        
        // Add new entry
        cacheList_.emplace_front(key, value);
        keyMap_[key] = cacheList_.begin();
        currentMemory_ += valueSize;
        
        // Evict if necessary
        while ((cacheList_.size() > maxSize_ || currentMemory_ > maxMemory_) && !cacheList_.empty()) {
            evictLRU();
        }
    }
    
    void prefetch(const std::vector<Key>& keys, std::function<Value(const Key&)> loader) {
        for (const auto& key : keys) {
            Value dummy;
            if (!get(key, dummy)) {
                // Not in cache, load and cache it
                try {
                    Value value = loader(key);
                    put(key, value, sizeof(Value)); // Simplified size calculation
                } catch (const std::exception& e) {
                    std::cerr << "Prefetch failed for key: " << key << " - " << e.what() << "\n";
                }
            }
        }
    }
    
    struct Statistics {
        size_t hits;
        size_t misses;
        size_t evictions;
        size_t size;
        size_t memoryUsage;
        double hitRate;
    };
    
    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Statistics stats;
        stats.hits = hits_.load();
        stats.misses = misses_.load();
        stats.evictions = evictions_.load();
        stats.size = cacheList_.size();
        stats.memoryUsage = currentMemory_.load();
        
        size_t total = stats.hits + stats.misses;
        stats.hitRate = total > 0 ? (stats.hits * 100.0 / total) : 0.0;
        
        return stats;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cacheList_.clear();
        keyMap_.clear();
        currentMemory_ = 0;
        hits_ = 0;
        misses_ = 0;
        evictions_ = 0;
    }

private:
    void evictLRU() {
        if (cacheList_.empty()) return;
        
        auto lastIt = std::prev(cacheList_.end());
        keyMap_.erase(lastIt->key);
        currentMemory_ -= sizeof(Value); // Simplified
        cacheList_.erase(lastIt);
        evictions_++;
    }
};

/**
 * @brief LFU (Least Frequently Used) Cache implementation
 */
template<typename Key, typename Value>
class LFUCache {
private:
    struct CacheNode {
        Key key;
        Value value;
        size_t frequency;
        steady_clock::time_point lastAccess;
        
        CacheNode(const Key& k, const Value& v) 
            : key(k), value(v), frequency(1), lastAccess(steady_clock::now()) {}
    };
    
    size_t maxSize_;
    std::unordered_map<Key, CacheNode> cache_;
    mutable std::mutex mutex_;
    
    // Statistics
    std::atomic<size_t> hits_{0};
    std::atomic<size_t> misses_{0};
    std::atomic<size_t> evictions_{0};

public:
    explicit LFUCache(size_t maxSize) : maxSize_(maxSize) {}
    
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            misses_++;
            return false;
        }
        
        it->second.frequency++;
        it->second.lastAccess = steady_clock::now();
        value = it->second.value;
        hits_++;
        return true;
    }
    
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Update existing entry
            it->second.value = value;
            it->second.frequency++;
            it->second.lastAccess = steady_clock::now();
            return;
        }
        
        // Evict if necessary
        if (cache_.size() >= maxSize_) {
            evictLFU();
        }
        
        // Add new entry
        cache_.emplace(key, CacheNode(key, value));
    }
    
    struct Statistics {
        size_t hits;
        size_t misses;
        size_t evictions;
        size_t size;
        double hitRate;
        double avgFrequency;
    };
    
    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Statistics stats;
        stats.hits = hits_.load();
        stats.misses = misses_.load();
        stats.evictions = evictions_.load();
        stats.size = cache_.size();
        
        size_t total = stats.hits + stats.misses;
        stats.hitRate = total > 0 ? (stats.hits * 100.0 / total) : 0.0;
        
        // Calculate average frequency
        size_t totalFreq = 0;
        for (const auto& [key, node] : cache_) {
            totalFreq += node.frequency;
        }
        stats.avgFrequency = cache_.empty() ? 0.0 : (totalFreq / static_cast<double>(cache_.size()));
        
        return stats;
    }

private:
    void evictLFU() {
        if (cache_.empty()) return;
        
        auto minIt = std::min_element(cache_.begin(), cache_.end(),
            [](const auto& a, const auto& b) {
                if (a.second.frequency != b.second.frequency) {
                    return a.second.frequency < b.second.frequency;
                }
                return a.second.lastAccess < b.second.lastAccess; // Tie-breaker: older access
            });
        
        cache_.erase(minIt);
        evictions_++;
    }
};

/**
 * @brief Multi-level cache hierarchy
 */
class MultiLevelCache {
private:
    LRUCache<std::string, blob> l1Cache_; // Fast, small cache
    LRUCache<std::string, blob> l2Cache_; // Slower, larger cache
    
    std::atomic<size_t> l1Hits_{0};
    std::atomic<size_t> l2Hits_{0};
    std::atomic<size_t> misses_{0};

public:
    MultiLevelCache() 
        : l1Cache_(50, 10 * 1024 * 1024),    // 50 items, 10MB
          l2Cache_(200, 50 * 1024 * 1024)    // 200 items, 50MB
    {}
    
    bool get(const std::string& key, blob& value) {
        // Try L1 cache first
        if (l1Cache_.get(key, value)) {
            l1Hits_++;
            return true;
        }
        
        // Try L2 cache
        if (l2Cache_.get(key, value)) {
            l2Hits_++;
            // Promote to L1 cache
            l1Cache_.put(key, value, value.size());
            return true;
        }
        
        misses_++;
        return false;
    }
    
    void put(const std::string& key, const blob& value) {
        // Always put in L1 first
        l1Cache_.put(key, value, value.size());
        
        // Also put in L2 for larger capacity
        l2Cache_.put(key, value, value.size());
    }
    
    struct Statistics {
        size_t l1Hits;
        size_t l2Hits;
        size_t misses;
        double l1HitRate;
        double l2HitRate;
        double overallHitRate;
        size_t l1Size;
        size_t l2Size;
    };
    
    Statistics getStatistics() const {
        Statistics stats;
        stats.l1Hits = l1Hits_.load();
        stats.l2Hits = l2Hits_.load();
        stats.misses = misses_.load();
        
        size_t totalRequests = stats.l1Hits + stats.l2Hits + stats.misses;
        
        if (totalRequests > 0) {
            stats.l1HitRate = (stats.l1Hits * 100.0) / totalRequests;
            stats.l2HitRate = (stats.l2Hits * 100.0) / totalRequests;
            stats.overallHitRate = ((stats.l1Hits + stats.l2Hits) * 100.0) / totalRequests;
        } else {
            stats.l1HitRate = stats.l2HitRate = stats.overallHitRate = 0.0;
        }
        
        auto l1Stats = l1Cache_.getStatistics();
        auto l2Stats = l2Cache_.getStatistics();
        
        stats.l1Size = l1Stats.size;
        stats.l2Size = l2Stats.size;
        
        return stats;
    }
};

/**
 * @brief Predictive prefetcher
 */
class PredictivePrefetcher {
private:
    std::unordered_map<std::string, std::vector<std::string>> accessPatterns_;
    std::unordered_map<std::string, size_t> sequenceCounters_;
    mutable std::mutex mutex_;

public:
    void recordAccess(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Simple pattern: record last accessed key for each key
        static thread_local std::string lastKey;
        
        if (!lastKey.empty() && lastKey != key) {
            accessPatterns_[lastKey].push_back(key);
            
            // Keep only recent patterns (limit to 10)
            if (accessPatterns_[lastKey].size() > 10) {
                accessPatterns_[lastKey].erase(accessPatterns_[lastKey].begin());
            }
        }
        
        lastKey = key;
    }
    
    std::vector<std::string> predictNext(const std::string& key, size_t maxPredictions = 3) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = accessPatterns_.find(key);
        if (it == accessPatterns_.end()) {
            return {};
        }
        
        // Count frequency of next accesses
        std::unordered_map<std::string, size_t> frequency;
        for (const auto& nextKey : it->second) {
            frequency[nextKey]++;
        }
        
        // Sort by frequency
        std::vector<std::pair<std::string, size_t>> sorted;
        for (const auto& [nextKey, count] : frequency) {
            sorted.emplace_back(nextKey, count);
        }
        
        std::sort(sorted.begin(), sorted.end(), 
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::vector<std::string> predictions;
        for (size_t i = 0; i < std::min(maxPredictions, sorted.size()); ++i) {
            predictions.push_back(sorted[i].first);
        }
        
        return predictions;
    }
};

/**
 * @brief Create test data for caching demonstrations
 */
std::unordered_map<std::string, blob> createTestData(size_t count) {
    std::unordered_map<std::string, blob> testData;
    
    for (size_t i = 0; i < count; ++i) {
        std::string key = "image_" + std::to_string(i);
        
        // Create blob with varying sizes
        size_t size = 1024 + (i % 10) * 512; // 1KB to 6KB
        std::vector<uint8_t> data(size);
        
        // Fill with pattern based on index
        for (size_t j = 0; j < size; ++j) {
            data[j] = static_cast<uint8_t>((i + j) % 256);
        }
        
        testData[key] = blob(data.data(), size);
    }
    
    return testData;
}

/**
 * @brief Demonstrate LRU cache performance
 */
void demonstrateLRUCache() {
    std::cout << "\n=== LRU Cache Demonstration ===\n";
    
    try {
        LRUCache<std::string, blob> cache(20, 50 * 1024); // 20 items, 50KB
        auto testData = createTestData(50);
        
        std::cout << "Testing LRU cache with " << testData.size() << " test items\n";
        
        auto start = steady_clock::now();
        
        // Phase 1: Sequential access (should cause many evictions)
        std::cout << "Phase 1: Sequential access\n";
        for (const auto& [key, value] : testData) {
            cache.put(key, value, value.size());
        }
        
        auto phase1Time = duration_cast<microseconds>(steady_clock::now() - start);
        auto stats = cache.getStatistics();
        
        std::cout << "  After sequential loading:\n";
        std::cout << "    Cache size: " << stats.size << "\n";
        std::cout << "    Memory usage: " << stats.memoryUsage << " bytes\n";
        std::cout << "    Evictions: " << stats.evictions << "\n";
        std::cout << "    Load time: " << phase1Time.count() << " μs\n";
        
        // Phase 2: Random access pattern
        std::cout << "Phase 2: Random access pattern\n";
        start = steady_clock::now();
        
        std::vector<std::string> keys;
        for (const auto& [key, value] : testData) {
            keys.push_back(key);
        }
        
        // Shuffle for random access
        std::random_shuffle(keys.begin(), keys.end());
        
        size_t accessCount = 100;
        for (size_t i = 0; i < accessCount; ++i) {
            std::string key = keys[i % keys.size()];
            blob value;
            cache.get(key, value);
        }
        
        auto phase2Time = duration_cast<microseconds>(steady_clock::now() - start);
        stats = cache.getStatistics();
        
        std::cout << "  After random access (" << accessCount << " requests):\n";
        std::cout << "    Hit rate: " << std::fixed << std::setprecision(1) << stats.hitRate << "%\n";
        std::cout << "    Hits: " << stats.hits << ", Misses: " << stats.misses << "\n";
        std::cout << "    Access time: " << phase2Time.count() << " μs\n";
        std::cout << "    Avg time per access: " << (phase2Time.count() / accessCount) << " μs\n";
        
        // Phase 3: Locality-based access (should have better hit rate)
        std::cout << "Phase 3: Locality-based access\n";
        cache.clear();
        start = steady_clock::now();
        
        // Access first 15 items repeatedly (should fit in cache)
        for (size_t round = 0; round < 10; ++round) {
            for (size_t i = 0; i < 15; ++i) {
                std::string key = "image_" + std::to_string(i);
                blob value;
                if (!cache.get(key, value)) {
                    cache.put(key, testData[key], testData[key].size());
                }
            }
        }
        
        auto phase3Time = duration_cast<microseconds>(steady_clock::now() - start);
        stats = cache.getStatistics();
        
        std::cout << "  After locality-based access:\n";
        std::cout << "    Hit rate: " << std::fixed << std::setprecision(1) << stats.hitRate << "%\n";
        std::cout << "    Hits: " << stats.hits << ", Misses: " << stats.misses << "\n";
        std::cout << "    Access time: " << phase3Time.count() << " μs\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in LRU cache demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate LFU cache performance
 */
void demonstrateLFUCache() {
    std::cout << "\n=== LFU Cache Demonstration ===\n";
    
    try {
        LFUCache<std::string, blob> cache(20);
        auto testData = createTestData(30);
        
        std::cout << "Testing LFU cache with " << testData.size() << " test items\n";
        
        // Load all data
        for (const auto& [key, value] : testData) {
            cache.put(key, value);
        }
        
        auto start = steady_clock::now();
        
        // Create frequency-based access pattern
        // Access first 10 items frequently, next 10 moderately, rest rarely
        for (size_t round = 0; round < 20; ++round) {
            // Frequent access (10 times per round)
            for (size_t rep = 0; rep < 10; ++rep) {
                for (size_t i = 0; i < 10; ++i) {
                    std::string key = "image_" + std::to_string(i);
                    blob value;
                    cache.get(key, value);
                }
            }
            
            // Moderate access (3 times per round)
            for (size_t rep = 0; rep < 3; ++rep) {
                for (size_t i = 10; i < 20; ++i) {
                    std::string key = "image_" + std::to_string(i);
                    blob value;
                    cache.get(key, value);
                }
            }
            
            // Rare access (1 time per round)
            for (size_t i = 20; i < 30; ++i) {
                std::string key = "image_" + std::to_string(i);
                blob value;
                cache.get(key, value);
            }
        }
        
        auto accessTime = duration_cast<microseconds>(steady_clock::now() - start);
        auto stats = cache.getStatistics();
        
        std::cout << "After frequency-based access pattern:\n";
        std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) << stats.hitRate << "%\n";
        std::cout << "  Hits: " << stats.hits << ", Misses: " << stats.misses << "\n";
        std::cout << "  Evictions: " << stats.evictions << "\n";
        std::cout << "  Average frequency: " << std::setprecision(2) << stats.avgFrequency << "\n";
        std::cout << "  Access time: " << accessTime.count() << " μs\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in LFU cache demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate multi-level cache
 */
void demonstrateMultiLevelCache() {
    std::cout << "\n=== Multi-Level Cache Demonstration ===\n";
    
    try {
        MultiLevelCache cache;
        auto testData = createTestData(100);
        
        std::cout << "Testing multi-level cache with " << testData.size() << " test items\n";
        
        auto start = steady_clock::now();
        
        // Phase 1: Load data
        for (const auto& [key, value] : testData) {
            cache.put(key, value);
        }
        
        auto loadTime = duration_cast<microseconds>(steady_clock::now() - start);
        
        // Phase 2: Mixed access pattern
        start = steady_clock::now();
        
        std::vector<std::string> keys;
        for (const auto& [key, value] : testData) {
            keys.push_back(key);
        }
        
        // Hot data (frequently accessed)
        std::vector<std::string> hotKeys(keys.begin(), keys.begin() + 20);
        // Warm data (moderately accessed)
        std::vector<std::string> warmKeys(keys.begin() + 20, keys.begin() + 60);
        // Cold data (rarely accessed)
        std::vector<std::string> coldKeys(keys.begin() + 60, keys.end());
        
        size_t totalAccesses = 0;
        
        for (size_t round = 0; round < 50; ++round) {
            // Access hot data frequently
            for (size_t i = 0; i < 5; ++i) {
                for (const auto& key : hotKeys) {
                    blob value;
                    cache.get(key, value);
                    totalAccesses++;
                }
            }
            
            // Access warm data moderately
            for (size_t i = 0; i < 2; ++i) {
                for (const auto& key : warmKeys) {
                    blob value;
                    cache.get(key, value);
                    totalAccesses++;
                }
            }
            
            // Access cold data rarely
            if (round % 10 == 0) {
                for (const auto& key : coldKeys) {
                    blob value;
                    cache.get(key, value);
                    totalAccesses++;
                }
            }
        }
        
        auto accessTime = duration_cast<microseconds>(steady_clock::now() - start);
        auto stats = cache.getStatistics();
        
        std::cout << "Multi-level cache performance:\n";
        std::cout << "  Load time: " << loadTime.count() << " μs\n";
        std::cout << "  Total accesses: " << totalAccesses << "\n";
        std::cout << "  Access time: " << accessTime.count() << " μs\n";
        std::cout << "  Avg time per access: " << (accessTime.count() / totalAccesses) << " μs\n";
        std::cout << "\nCache hierarchy statistics:\n";
        std::cout << "  L1 hit rate: " << std::fixed << std::setprecision(1) << stats.l1HitRate << "%\n";
        std::cout << "  L2 hit rate: " << stats.l2HitRate << "%\n";
        std::cout << "  Overall hit rate: " << stats.overallHitRate << "%\n";
        std::cout << "  L1 cache size: " << stats.l1Size << " items\n";
        std::cout << "  L2 cache size: " << stats.l2Size << " items\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in multi-level cache demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate predictive prefetching
 */
void demonstratePredictivePrefetching() {
    std::cout << "\n=== Predictive Prefetching Demonstration ===\n";
    
    try {
        LRUCache<std::string, blob> cache(30, 100 * 1024);
        PredictivePrefetcher prefetcher;
        auto testData = createTestData(50);
        
        std::cout << "Testing predictive prefetching\n";
        
        // Define access sequences that create patterns
        std::vector<std::vector<std::string>> sequences = {
            {"image_0", "image_1", "image_2", "image_3"},
            {"image_10", "image_11", "image_12"},
            {"image_20", "image_21", "image_22", "image_23", "image_24"},
            {"image_5", "image_6", "image_7"}
        };
        
        auto start = steady_clock::now();
        
        // Phase 1: Train the prefetcher with patterns
        std::cout << "Phase 1: Training prefetcher with access patterns\n";
        
        for (size_t round = 0; round < 10; ++round) {
            for (const auto& sequence : sequences) {
                for (const auto& key : sequence) {
                    prefetcher.recordAccess(key);
                    
                    blob value;
                    if (!cache.get(key, value)) {
                        cache.put(key, testData[key], testData[key].size());
                    }
                }
            }
        }
        
        auto trainingTime = duration_cast<microseconds>(steady_clock::now() - start);
        auto trainingStats = cache.getStatistics();
        
        std::cout << "  Training completed in " << trainingTime.count() << " μs\n";
        std::cout << "  Training hit rate: " << std::fixed << std::setprecision(1) 
                 << trainingStats.hitRate << "%\n";
        
        // Phase 2: Test with prefetching
        std::cout << "Phase 2: Testing with predictive prefetching\n";
        
        cache.clear();
        start = steady_clock::now();
        
        size_t prefetchHits = 0;
        size_t totalPrefetches = 0;
        
        for (const auto& sequence : sequences) {
            for (size_t i = 0; i < sequence.size(); ++i) {
                const auto& key = sequence[i];
                
                // Get current item
                blob value;
                bool hit = cache.get(key, value);
                if (!hit) {
                    cache.put(key, testData[key], testData[key].size());
                }
                
                // Predict and prefetch next items
                auto predictions = prefetcher.predictNext(key, 2);
                
                for (const auto& predictedKey : predictions) {
                    blob prefetchValue;
                    if (!cache.get(predictedKey, prefetchValue)) {
                        // Prefetch the predicted item
                        auto it = testData.find(predictedKey);
                        if (it != testData.end()) {
                            cache.put(predictedKey, it->second, it->second.size());
                            totalPrefetches++;
                        }
                    } else {
                        prefetchHits++;
                    }
                }
                
                prefetcher.recordAccess(key);
            }
        }
        
        auto testTime = duration_cast<microseconds>(steady_clock::now() - start);
        auto testStats = cache.getStatistics();
        
        std::cout << "Predictive prefetching results:\n";
        std::cout << "  Test time: " << testTime.count() << " μs\n";
        std::cout << "  Test hit rate: " << std::fixed << std::setprecision(1) 
                 << testStats.hitRate << "%\n";
        std::cout << "  Total prefetches: " << totalPrefetches << "\n";
        std::cout << "  Prefetch hits: " << prefetchHits << "\n";
        std::cout << "  Prefetch accuracy: " << (totalPrefetches > 0 ? 
                 (prefetchHits * 100.0 / totalPrefetches) : 0.0) << "%\n";
        
        // Show some predictions
        std::cout << "\nSample predictions:\n";
        for (const auto& sequence : sequences) {
            if (!sequence.empty()) {
                auto predictions = prefetcher.predictNext(sequence[0], 3);
                std::cout << "  After '" << sequence[0] << "' -> ";
                for (const auto& pred : predictions) {
                    std::cout << pred << " ";
                }
                std::cout << "\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in predictive prefetching demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Compare different caching strategies
 */
void compareCachingStrategies() {
    std::cout << "\n=== Caching Strategy Comparison ===\n";
    
    try {
        auto testData = createTestData(100);
        
        // Test parameters
        const size_t cacheSize = 25;
        const size_t accessRounds = 20;
        
        std::cout << "Comparing caching strategies with " << cacheSize << " cache slots\n";
        
        // Test LRU
        {
            LRUCache<std::string, blob> lruCache(cacheSize, 1024 * 1024);
            
            auto start = steady_clock::now();
            
            // Mixed access pattern
            for (size_t round = 0; round < accessRounds; ++round) {
                // Hot data (20% of keys, 80% of accesses)
                for (size_t i = 0; i < 16; ++i) {
                    std::string key = "image_" + std::to_string(i % 20);
                    blob value;
                    if (!lruCache.get(key, value)) {
                        lruCache.put(key, testData[key], testData[key].size());
                    }
                }
                
                // Cold data (80% of keys, 20% of accesses)
                for (size_t i = 0; i < 4; ++i) {
                    std::string key = "image_" + std::to_string(20 + (i % 80));
                    blob value;
                    if (!lruCache.get(key, value)) {
                        lruCache.put(key, testData[key], testData[key].size());
                    }
                }
            }
            
            auto lruTime = duration_cast<microseconds>(steady_clock::now() - start);
            auto lruStats = lruCache.getStatistics();
            
            std::cout << "LRU Cache:\n";
            std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) << lruStats.hitRate << "%\n";
            std::cout << "  Evictions: " << lruStats.evictions << "\n";
            std::cout << "  Time: " << lruTime.count() << " μs\n";
        }
        
        // Test LFU
        {
            LFUCache<std::string, blob> lfuCache(cacheSize);
            
            auto start = steady_clock::now();
            
            // Same access pattern
            for (size_t round = 0; round < accessRounds; ++round) {
                for (size_t i = 0; i < 16; ++i) {
                    std::string key = "image_" + std::to_string(i % 20);
                    blob value;
                    if (!lfuCache.get(key, value)) {
                        lfuCache.put(key, testData[key]);
                    }
                }
                
                for (size_t i = 0; i < 4; ++i) {
                    std::string key = "image_" + std::to_string(20 + (i % 80));
                    blob value;
                    if (!lfuCache.get(key, value)) {
                        lfuCache.put(key, testData[key]);
                    }
                }
            }
            
            auto lfuTime = duration_cast<microseconds>(steady_clock::now() - start);
            auto lfuStats = lfuCache.getStatistics();
            
            std::cout << "LFU Cache:\n";
            std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) << lfuStats.hitRate << "%\n";
            std::cout << "  Evictions: " << lfuStats.evictions << "\n";
            std::cout << "  Avg frequency: " << std::setprecision(2) << lfuStats.avgFrequency << "\n";
            std::cout << "  Time: " << lfuTime.count() << " μs\n";
        }
        
        std::cout << "\nConclusion: LFU typically performs better with frequency-based access patterns,\n";
        std::cout << "while LRU performs better with temporal locality patterns.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in caching strategy comparison: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Advanced Caching Demo ===\n";
    std::cout << "This example demonstrates advanced caching strategies and optimizations\n";

    // Run all demonstrations
    demonstrateLRUCache();
    demonstrateLFUCache();
    demonstrateMultiLevelCache();
    demonstratePredictivePrefetching();
    compareCachingStrategies();

    std::cout << "\n=== Advanced caching demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- LRU (Least Recently Used) caching with memory limits\n";
    std::cout << "- LFU (Least Frequently Used) caching with frequency tracking\n";
    std::cout << "- Multi-level cache hierarchies (L1/L2)\n";
    std::cout << "- Predictive prefetching based on access patterns\n";
    std::cout << "- Performance comparison of different strategies\n";
    std::cout << "- Thread-safe cache implementations\n";
    std::cout << "- Comprehensive cache statistics and monitoring\n";
    
    return 0;
}
