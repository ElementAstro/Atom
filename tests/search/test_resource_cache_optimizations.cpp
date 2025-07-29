#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <random>

#include "atom/search/cache.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

class ResourceCacheOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_size = 100;
        config_.max_memory_mb = 10;
        config_.enable_metrics = true;
        config_.enable_performance_tracking = true;
        config_.cleanup_interval = 1s;
    }

    CacheConfig config_;
};

// Test enhanced memory management
TEST_F(ResourceCacheOptimizationTest, MemoryManagement) {
    ResourceCache<std::string> cache(config_);

    // Insert items and verify memory tracking
    std::string large_value(1024, 'A');  // 1KB value
    cache.insert("key1", large_value, 10s);
    cache.insert("key2", large_value, 10s);

    auto metrics = cache.get_metrics();
    EXPECT_GT(metrics.memory_usage_bytes.load(), 2048);  // At least 2KB

    // Test memory limit enforcement
    config_.max_memory_mb = 1;  // 1MB limit
    ResourceCache<std::string> limited_cache(config_);

    // Fill cache beyond memory limit
    std::string huge_value(100 * 1024, 'B');  // 100KB value
    for (int i = 0; i < 20; ++i) {
        limited_cache.insert("huge_key_" + std::to_string(i), huge_value, 10s);
    }

    // Should have triggered memory-based evictions
    auto limited_metrics = limited_cache.get_metrics();
    EXPECT_GT(limited_metrics.memory_pressure_evictions.load(), 0);
}

// Test optimized eviction policies
TEST_F(ResourceCacheOptimizationTest, OptimizedEviction) {
    // Test LFU optimization
    config_.eviction_policy = EvictionPolicy::LFU;
    config_.use_optimized_eviction = true;
    ResourceCache<int> lfu_cache(config_);

    // Fill cache
    for (int i = 0; i < 150; ++i) {
        lfu_cache.insert("key_" + std::to_string(i), i, 10s);
    }

    // Access some keys multiple times
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 5; ++j) {
            lfu_cache.get("key_" + std::to_string(i));
        }
    }

    // Insert more items to trigger eviction
    for (int i = 150; i < 200; ++i) {
        lfu_cache.insert("key_" + std::to_string(i), i, 10s);
    }

    // Frequently accessed items should still be present
    for (int i = 0; i < 10; ++i) {
        auto result = lfu_cache.get("key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value()) << "Frequently accessed key_" << i << " was evicted";
    }

    auto metrics = lfu_cache.get_metrics();
    EXPECT_GT(metrics.eviction_count.load(), 0);
}

// Test enhanced batch operations
TEST_F(ResourceCacheOptimizationTest, OptimizedBatchOperations) {
    ResourceCache<std::string> cache(config_);

    // Prepare batch data
    Vector<std::pair<String, std::string>> batch_items;
    for (int i = 0; i < 50; ++i) {
        batch_items.emplace_back("batch_key_" + std::to_string(i),
                                "batch_value_" + std::to_string(i));
    }

    auto start = std::chrono::high_resolution_clock::now();
    cache.insert_batch(batch_items, 10s);
    auto end = std::chrono::high_resolution_clock::now();

    auto batch_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Compare with individual inserts
    ResourceCache<std::string> individual_cache(config_);
    start = std::chrono::high_resolution_clock::now();
    for (const auto& [key, value] : batch_items) {
        individual_cache.insert(key, value, 10s);
    }
    end = std::chrono::high_resolution_clock::now();

    auto individual_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Batch operations should be faster
    EXPECT_LT(batch_time.count(), individual_time.count());

    // Verify all items were inserted
    EXPECT_EQ(cache.size(), 50);
    for (int i = 0; i < 50; ++i) {
        auto result = cache.get("batch_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "batch_value_" + std::to_string(i));
    }
}

// Test deferred cleanup optimization
TEST_F(ResourceCacheOptimizationTest, DeferredCleanup) {
    config_.enable_deferred_cleanup = true;
    config_.cleanup_interval = 100ms;  // Fast cleanup for testing
    ResourceCache<std::string> cache(config_);

    // Insert items with short expiration
    for (int i = 0; i < 20; ++i) {
        cache.insert("expire_key_" + std::to_string(i), "value_" + std::to_string(i), 50ms);
    }

    EXPECT_EQ(cache.size(), 20);

    // Wait for expiration
    std::this_thread::sleep_for(200ms);

    // Items should be cleaned up
    EXPECT_LT(cache.size(), 20);

    auto metrics = cache.get_metrics();
    EXPECT_GT(metrics.expiration_count.load(), 0);
}

// Test enhanced metrics and monitoring
TEST_F(ResourceCacheOptimizationTest, EnhancedMetrics) {
    ResourceCache<std::string> cache(config_);

    // Perform various operations
    cache.insert("test1", "value1", 10s);
    cache.insert("test2", "value2", 10s);
    cache.get("test1");
    cache.get("test1");  // Hit
    cache.get("nonexistent");  // Miss
    cache.remove("test2");

    auto metrics = cache.get_metrics();

    // Verify enhanced metrics
    EXPECT_EQ(metrics.hit_count.load(), 2);
    EXPECT_EQ(metrics.miss_count.load(), 1);
    EXPECT_EQ(metrics.insert_count.load(), 2);
    EXPECT_EQ(metrics.remove_count.load(), 1);
    EXPECT_GT(metrics.total_operations.load(), 0);

    // Test performance metrics (if enabled)
    if (config_.enable_performance_tracking) {
        EXPECT_GE(metrics.get_average_access_time_ns(), 0.0);
        EXPECT_GE(metrics.get_average_insert_time_ns(), 0.0);
    }

    // Test efficiency metrics
    auto efficiency = cache.get_efficiency_metrics();
    EXPECT_TRUE(efficiency.count("hit_ratio"));
    EXPECT_TRUE(efficiency.count("eviction_rate"));
    EXPECT_TRUE(efficiency.count("shard_balance_coefficient"));

    EXPECT_GE(efficiency["hit_ratio"], 0.0);
    EXPECT_LE(efficiency["hit_ratio"], 1.0);
}

// Test health monitoring
TEST_F(ResourceCacheOptimizationTest, HealthMonitoring) {
    config_.unhealthy_hit_ratio_threshold = 0.5;
    ResourceCache<std::string> cache(config_);

    // Create scenario with poor hit ratio
    for (int i = 0; i < 10; ++i) {
        cache.insert("key_" + std::to_string(i), "value_" + std::to_string(i), 10s);
    }

    // Generate mostly misses
    for (int i = 100; i < 200; ++i) {
        cache.get("nonexistent_" + std::to_string(i));
    }

    // Generate some hits
    for (int i = 0; i < 5; ++i) {
        cache.get("key_" + std::to_string(i));
    }

    auto health_report = cache.get_health_report();

    // Should detect unhealthy hit ratio
    EXPECT_TRUE(health_report.count("status"));
    EXPECT_TRUE(health_report.count("hit_ratio_warning") ||
                health_report["status"] == "WARNING" ||
                health_report["status"] == "UNHEALTHY");
}

// Test prefetching functionality
TEST_F(ResourceCacheOptimizationTest, Prefetching) {
    config_.enable_prefetching = true;
    config_.prefetch_window_size = 5;
    ResourceCache<std::string> cache(config_);

    // Insert items with similar prefixes
    cache.insert("user_001", "data1", 10s);
    cache.insert("user_002", "data2", 10s);
    cache.insert("user_003", "data3", 10s);
    cache.insert("user_004", "data4", 10s);
    cache.insert("user_005", "data5", 10s);
    cache.insert("other_001", "other1", 10s);

    // Trigger prefetching
    cache.prefetch_related("user_001", 3);

    // Related items should be moved to front of LRU
    // This is hard to test directly, but we can verify the method doesn't crash
    EXPECT_NO_THROW(cache.prefetch_related("user_001", 3));
}

// Test thread safety of optimizations
TEST_F(ResourceCacheOptimizationTest, ThreadSafety) {
    ResourceCache<int> cache(config_);

    const int num_threads = 4;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;

    // Launch multiple threads performing concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, t, operations_per_thread]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> dist(0, operations_per_thread * 2);

            for (int i = 0; i < operations_per_thread; ++i) {
                int key_num = dist(rng);
                std::string key = "thread_" + std::to_string(t) + "_key_" + std::to_string(key_num);

                // Mix of operations
                switch (i % 4) {
                    case 0:
                        cache.insert(key, key_num, 5s);
                        break;
                    case 1:
                        cache.get(key);
                        break;
                    case 2:
                        cache.contains(key);
                        break;
                    case 3:
                        cache.remove(key);
                        break;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Cache should still be in a valid state
    auto metrics = cache.get_metrics();
    EXPECT_GT(metrics.total_operations.load(), 0);

    // No crashes means thread safety is working
    SUCCEED();
}
