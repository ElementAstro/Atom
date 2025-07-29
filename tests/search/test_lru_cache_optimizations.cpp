#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <random>

#include "atom/search/lru.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

class LRUCacheOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_size = 100;
        config_.concurrency_level = 4;
        config_.enable_statistics = true;
        config_.cleanup_interval = 1s;
        config_.enable_fast_size_tracking = true;
        config_.enable_background_cleanup = true;
    }

    LRUCacheConfig config_;
};

// Test enhanced constructor with LRUCacheConfig
TEST_F(LRUCacheOptimizationTest, EnhancedConstructor) {
    config_.default_ttl = 5s;
    config_.enable_prefetching = true;
    config_.max_memory_mb = 10;

    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Verify configuration was applied
    auto current_config = cache.getConfig();
    EXPECT_EQ(current_config.max_size, 100);
    EXPECT_EQ(current_config.concurrency_level, 4);
    EXPECT_EQ(current_config.default_ttl, 5s);
    EXPECT_TRUE(current_config.enable_prefetching);
    EXPECT_EQ(current_config.max_memory_mb, 10);
}

// Test optimized size tracking
TEST_F(LRUCacheOptimizationTest, FastSizeTracking) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert items
    for (int i = 0; i < 50; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    // Multiple size() calls should be fast due to caching
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        size_t size = cache.size();
        EXPECT_EQ(size, 50);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be very fast due to cached size
    EXPECT_LT(duration.count(), 10000);  // Less than 10ms for 1000 calls
}

// Test optimized batch operations
TEST_F(LRUCacheOptimizationTest, OptimizedBatchOperations) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Prepare batch data
    std::vector<std::pair<std::string, std::string>> batch_items;
    for (int i = 0; i < 50; ++i) {
        batch_items.emplace_back("batch_key_" + std::to_string(i),
                                "batch_value_" + std::to_string(i));
    }

    auto start = std::chrono::high_resolution_clock::now();
    cache.putBatch(batch_items, 10s);
    auto end = std::chrono::high_resolution_clock::now();

    auto batch_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Compare with individual puts
    ThreadSafeLRUCache<std::string, std::string> individual_cache(config_);
    start = std::chrono::high_resolution_clock::now();
    for (const auto& [key, value] : batch_items) {
        individual_cache.put(key, value, 10s);
    }
    end = std::chrono::high_resolution_clock::now();

    auto individual_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Batch operations should be faster or at least comparable
    EXPECT_LE(batch_time.count(), individual_time.count() * 1.2);  // Allow 20% tolerance

    // Verify all items were inserted
    EXPECT_EQ(cache.size(), 50);
    for (int i = 0; i < 50; ++i) {
        auto result = cache.get("batch_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "batch_value_" + std::to_string(i));
    }
}

// Test enhanced metrics
TEST_F(LRUCacheOptimizationTest, EnhancedMetrics) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Perform various operations
    cache.put("test1", "value1", 10s);
    cache.put("test2", "value2", 10s);
    cache.get("test1");
    cache.get("test1");  // Hit
    cache.get("nonexistent");  // Miss
    cache.remove("test2");

    auto metrics = cache.getMetrics();

    // Verify enhanced metrics
    EXPECT_EQ(metrics.hit_count.load(), 2);
    EXPECT_EQ(metrics.miss_count.load(), 1);
    EXPECT_GT(metrics.total_operations.load(), 0);

    // Test hit ratio calculation
    EXPECT_DOUBLE_EQ(metrics.get_hit_ratio(), 2.0 / 3.0);

    // Test metrics reset
    cache.resetMetrics();
    auto reset_metrics = cache.getMetrics();
    EXPECT_EQ(reset_metrics.hit_count.load(), 0);
    EXPECT_EQ(reset_metrics.miss_count.load(), 0);
}

// Test configuration management
TEST_F(LRUCacheOptimizationTest, ConfigurationManagement) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Test getting current config
    auto current_config = cache.getConfig();
    EXPECT_EQ(current_config.max_size, 100);
    EXPECT_TRUE(current_config.enable_statistics);

    // Test updating configuration
    LRUCacheConfig new_config = config_;
    new_config.max_size = 200;
    new_config.default_ttl = 30s;
    new_config.enable_prefetching = true;

    cache.updateConfig(new_config);

    auto updated_config = cache.getConfig();
    EXPECT_EQ(updated_config.max_size, 200);
    EXPECT_EQ(updated_config.default_ttl, 30s);
    EXPECT_TRUE(updated_config.enable_prefetching);
}

// Test memory usage estimation
TEST_F(LRUCacheOptimizationTest, MemoryUsageEstimation) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert items and verify memory tracking
    std::string large_value(1024, 'A');  // 1KB value
    cache.put("key1", large_value);
    cache.put("key2", large_value);

    size_t memory_usage = cache.getMemoryUsage();
    EXPECT_GT(memory_usage, 2048);  // At least 2KB

    // Test value size estimation
    size_t estimated_size = cache.estimateValueSize(large_value);
    EXPECT_GE(estimated_size, 1024);
}

// Test health monitoring
TEST_F(LRUCacheOptimizationTest, HealthMonitoring) {
    config_.unhealthy_hit_ratio_threshold = 0.5;
    config_.max_memory_mb = 1;  // 1MB limit
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Create scenario with poor hit ratio
    for (int i = 0; i < 10; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    // Generate mostly misses
    for (int i = 100; i < 200; ++i) {
        cache.get("nonexistent_" + std::to_string(i));
    }

    // Generate some hits
    for (int i = 0; i < 5; ++i) {
        cache.get("key_" + std::to_string(i));
    }

    auto health_report = cache.getHealthReport();

    // Should detect unhealthy hit ratio
    EXPECT_TRUE(health_report.count("status"));
    EXPECT_TRUE(health_report.count("hit_ratio_warning") ||
                health_report["status"] == "WARNING" ||
                health_report["status"] == "UNHEALTHY");

    // Test health status
    bool is_healthy = cache.isHealthy();
    EXPECT_FALSE(is_healthy);  // Should be unhealthy due to poor hit ratio
}

// Test efficiency metrics
TEST_F(LRUCacheOptimizationTest, EfficiencyMetrics) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Perform mixed operations
    for (int i = 0; i < 20; ++i) {
        cache.put("eff_key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    // Generate hits and misses
    for (int i = 0; i < 10; ++i) {
        cache.get("eff_key_" + std::to_string(i));  // Hits
    }
    for (int i = 100; i < 110; ++i) {
        cache.get("nonexistent_" + std::to_string(i));  // Misses
    }

    auto efficiency = cache.getEfficiencyMetrics();

    // Verify all expected metrics are present
    EXPECT_TRUE(efficiency.count("hit_ratio"));
    EXPECT_TRUE(efficiency.count("memory_efficiency"));
    EXPECT_TRUE(efficiency.count("load_factor"));
    EXPECT_TRUE(efficiency.count("shard_balance_coefficient"));

    // Verify metric ranges
    EXPECT_GE(efficiency["hit_ratio"], 0.0);
    EXPECT_LE(efficiency["hit_ratio"], 1.0);
    EXPECT_GE(efficiency["load_factor"], 0.0);
    EXPECT_LE(efficiency["load_factor"], 1.0);
    EXPECT_GE(efficiency["shard_balance_coefficient"], 0.0);
    EXPECT_LE(efficiency["shard_balance_coefficient"], 1.0);
}

// Test load balancing metrics
TEST_F(LRUCacheOptimizationTest, LoadBalanceMetrics) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert items to distribute across shards
    for (int i = 0; i < 40; ++i) {
        cache.put("balance_test_" + std::to_string(i), "value");
    }

    auto load_balance = cache.getLoadBalanceMetrics();

    // Should have load balance metrics
    EXPECT_TRUE(load_balance.count("variance"));
    EXPECT_TRUE(load_balance.count("balance_coefficient"));
    EXPECT_TRUE(load_balance.count("expected_per_shard"));
    EXPECT_TRUE(load_balance.count("min_shard_size"));
    EXPECT_TRUE(load_balance.count("max_shard_size"));

    EXPECT_GE(load_balance["balance_coefficient"], 0.0);
    EXPECT_LE(load_balance["balance_coefficient"], 1.0);
    EXPECT_GE(load_balance["variance"], 0.0);
}

// Test cache warming
TEST_F(LRUCacheOptimizationTest, CacheWarming) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Create a loader function
    auto loader = []() -> std::vector<std::pair<std::string, std::string>> {
        std::vector<std::pair<std::string, std::string>> items;
        for (int i = 0; i < 15; ++i) {
            items.emplace_back("warm_key_" + std::to_string(i),
                              "warm_value_" + std::to_string(i));
        }
        return items;
    };

    // Warm the cache
    cache.warmCache(loader, 10s);

    // Verify items were loaded
    EXPECT_EQ(cache.size(), 15);

    for (int i = 0; i < 15; ++i) {
        auto result = cache.get("warm_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "warm_value_" + std::to_string(i));
    }
}

// Test shard statistics
TEST_F(LRUCacheOptimizationTest, ShardStatistics) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert items to distribute across shards
    for (int i = 0; i < 40; ++i) {
        cache.put("shard_test_" + std::to_string(i), "value");
    }

    auto shard_stats = cache.getShardStats();

    // Should have stats for all shards
    EXPECT_EQ(shard_stats.size(), config_.concurrency_level);

    size_t total_entries = 0;
    for (const auto& stats : shard_stats) {
        EXPECT_TRUE(stats.count("shard_id"));
        EXPECT_TRUE(stats.count("size"));
        EXPECT_TRUE(stats.count("max_size"));
        EXPECT_TRUE(stats.count("load_factor"));

        total_entries += stats.at("size");
        EXPECT_GE(stats.at("load_factor"), 0);
        EXPECT_LE(stats.at("load_factor"), 100);
    }

    // Total entries across shards should match cache size
    EXPECT_EQ(total_entries, cache.size());
}

// Test background cleanup thread
TEST_F(LRUCacheOptimizationTest, BackgroundCleanup) {
    config_.cleanup_interval = 100ms;  // Fast cleanup for testing
    config_.enable_background_cleanup = true;
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert items with short TTL
    for (int i = 0; i < 20; ++i) {
        cache.put("expire_key_" + std::to_string(i), "value", 50ms);
    }

    EXPECT_EQ(cache.size(), 20);

    // Wait for expiration and cleanup
    std::this_thread::sleep_for(200ms);

    // Items should be cleaned up by background thread
    EXPECT_LT(cache.size(), 20);

    auto metrics = cache.getMetrics();
    EXPECT_GT(metrics.expiration_count.load(), 0);
}
