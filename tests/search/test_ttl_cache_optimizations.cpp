#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <random>

#include "atom/search/ttl.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

class TTLCacheOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enable_automatic_cleanup = true;
        config_.enable_statistics = true;
        config_.thread_safe = true;
        config_.cleanup_batch_size = 50;
        config_.enable_lazy_expiration = true;
        config_.enable_memory_tracking = true;
        config_.enable_expiration_index = true;
        config_.enable_health_monitoring = true;
    }

    TTLCacheConfig config_;
};

// Test enhanced configuration
TEST_F(TTLCacheOptimizationTest, EnhancedConfiguration) {
    config_.max_memory_mb = 10;
    config_.enable_compression = true;
    config_.compression_threshold = 512;
    config_.memory_pressure_threshold = 0.7;

    TTLCache<std::string, std::string> cache(1000ms, 100, 500ms, config_);

    auto current_config = cache.get_config();
    EXPECT_EQ(current_config.max_memory_mb, 10);
    EXPECT_TRUE(current_config.enable_compression);
    EXPECT_EQ(current_config.compression_threshold, 512);
    EXPECT_DOUBLE_EQ(current_config.memory_pressure_threshold, 0.7);
}

// Test lazy expiration
TEST_F(TTLCacheOptimizationTest, LazyExpiration) {
    TTLCache<std::string, std::string> cache(100ms, 50, 1000ms, config_);

    // Insert items with short TTL
    cache.put("key1", "value1");
    cache.put("key2", "value2");
    cache.put("key3", "value3");

    EXPECT_EQ(cache.size(), 3);

    // Wait for expiration
    std::this_thread::sleep_for(150ms);

    // Access expired items - should trigger lazy expiration
    auto result1 = cache.get("key1");
    auto result2 = cache.get("key2");

    EXPECT_FALSE(result1.has_value());
    EXPECT_FALSE(result2.has_value());

    // Check that lazy expiration statistics are updated
    auto stats = cache.get_statistics();
    EXPECT_GT(stats.lazy_expirations.load(), 0);
}

// Test optimized expiration cleanup
TEST_F(TTLCacheOptimizationTest, OptimizedExpirationCleanup) {
    config_.enable_expiration_index = true;
    TTLCache<std::string, std::string> cache(50ms, 100, 25ms, config_);

    // Insert many items with short TTL
    for (int i = 0; i < 50; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    EXPECT_EQ(cache.size(), 50);

    // Wait for expiration and cleanup
    std::this_thread::sleep_for(100ms);

    // Items should be cleaned up by background thread
    EXPECT_LT(cache.size(), 50);

    auto stats = cache.get_statistics();
    EXPECT_GT(stats.expirations, 0);
    EXPECT_GT(stats.cleanup_operations.load(), 0);
}

// Test memory tracking
TEST_F(TTLCacheOptimizationTest, MemoryTracking) {
    config_.enable_memory_tracking = true;
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Insert items and verify memory tracking
    std::string large_value(1024, 'A');  // 1KB value
    cache.put("key1", large_value);
    cache.put("key2", large_value);

    size_t memory_usage = cache.get_memory_usage();
    EXPECT_GT(memory_usage, 2048);  // At least 2KB

    auto stats = cache.get_statistics();
    EXPECT_GT(stats.memory_usage_bytes.load(), 0);
    EXPECT_GT(stats.get_memory_usage_mb(), 0.0);
}

// Test memory limits enforcement
TEST_F(TTLCacheOptimizationTest, MemoryLimitsEnforcement) {
    config_.max_memory_mb = 1;  // 1MB limit
    config_.memory_pressure_threshold = 0.5;  // 50% threshold
    TTLCache<std::string, std::string> cache(10000ms, 1000, std::nullopt, config_);

    // Fill cache beyond memory limit
    std::string large_value(100 * 1024, 'B');  // 100KB value
    for (int i = 0; i < 20; ++i) {
        cache.put("large_key_" + std::to_string(i), large_value);
    }

    // Should have triggered memory-based evictions
    cache.optimize();  // Force memory limit enforcement

    size_t final_size = cache.size();
    EXPECT_LT(final_size, 20);  // Some items should have been evicted
}

// Test enhanced statistics
TEST_F(TTLCacheOptimizationTest, EnhancedStatistics) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Perform various operations
    cache.put("test1", "value1");
    cache.put("test2", "value2");
    cache.get("test1");
    cache.get("test1");  // Hit
    cache.get("nonexistent");  // Miss
    cache.remove("test2");

    auto stats = cache.get_statistics();

    // Verify enhanced statistics
    EXPECT_EQ(stats.hits.load(), 2);
    EXPECT_EQ(stats.misses.load(), 1);
    EXPECT_DOUBLE_EQ(stats.get_hit_ratio(), 2.0 / 3.0);

    // Test performance timing if enabled
    EXPECT_GE(stats.get_average_access_time_ns(), 0.0);

    // Test statistics reset
    cache.reset_statistics();
    auto reset_stats = cache.get_statistics();
    EXPECT_EQ(reset_stats.hits.load(), 0);
    EXPECT_EQ(reset_stats.misses.load(), 0);
}

// Test health monitoring
TEST_F(TTLCacheOptimizationTest, HealthMonitoring) {
    config_.max_memory_mb = 1;  // 1MB limit
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

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

    auto health_report = cache.get_health_report();

    // Should detect unhealthy hit ratio
    EXPECT_TRUE(health_report.count("status"));
    EXPECT_TRUE(health_report.count("hit_ratio_warning") ||
                health_report["status"] == "WARNING" ||
                health_report["status"] == "UNHEALTHY");

    // Test health status
    bool is_healthy = cache.is_healthy();
    EXPECT_FALSE(is_healthy);  // Should be unhealthy due to poor hit ratio
}

// Test efficiency metrics
TEST_F(TTLCacheOptimizationTest, EfficiencyMetrics) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

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

    auto efficiency = cache.get_efficiency_metrics();

    // Verify all expected metrics are present
    EXPECT_TRUE(efficiency.count("hit_ratio"));
    EXPECT_TRUE(efficiency.count("memory_efficiency"));
    EXPECT_TRUE(efficiency.count("load_factor"));
    EXPECT_TRUE(efficiency.count("expiration_rate"));

    // Verify metric ranges
    EXPECT_GE(efficiency["hit_ratio"], 0.0);
    EXPECT_LE(efficiency["hit_ratio"], 1.0);
    EXPECT_GE(efficiency["load_factor"], 0.0);
    EXPECT_LE(efficiency["load_factor"], 1.0);
    EXPECT_GE(efficiency["expiration_rate"], 0.0);

    // Performance metrics should be available
    EXPECT_TRUE(efficiency.count("avg_access_time_ns"));
    EXPECT_GE(efficiency["avg_access_time_ns"], 0.0);
}

// Test async operations
TEST_F(TTLCacheOptimizationTest, AsyncOperations) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Insert some test data
    cache.put("async_key1", "async_value1");
    cache.put("async_key2", "async_value2");

    // Test async get
    auto future1 = cache.async_get("async_key1");
    auto future2 = cache.async_get("nonexistent");

    auto result1 = future1.get();
    auto result2 = future2.get();

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "async_value1");
    EXPECT_FALSE(result2.has_value());

    // Test async put
    auto put_future = cache.async_put("async_key3", "async_value3", 2000ms);
    put_future.wait();

    auto result3 = cache.get("async_key3");
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(*result3, "async_value3");
}

// Test cache warming
TEST_F(TTLCacheOptimizationTest, CacheWarming) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

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
    cache.warm_cache(loader, 2000ms);

    // Verify items were loaded
    EXPECT_EQ(cache.size(), 15);

    for (int i = 0; i < 15; ++i) {
        auto result = cache.get("warm_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "warm_value_" + std::to_string(i));
    }
}

// Test shard statistics
TEST_F(TTLCacheOptimizationTest, ShardStatistics) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Insert items to distribute across shards
    for (int i = 0; i < 40; ++i) {
        cache.put("shard_test_" + std::to_string(i), "value");
    }

    auto shard_stats = cache.get_shard_stats();

    // Should have multiple shards
    EXPECT_GT(shard_stats.size(), 1);

    size_t total_entries = 0;
    for (const auto& stats : shard_stats) {
        EXPECT_TRUE(stats.count("shard_id"));
        EXPECT_TRUE(stats.count("size"));
        EXPECT_TRUE(stats.count("max_capacity"));
        EXPECT_TRUE(stats.count("memory_usage"));
        EXPECT_TRUE(stats.count("load_factor"));

        total_entries += stats.at("size");
        EXPECT_GE(stats.at("load_factor"), 0);
        EXPECT_LE(stats.at("load_factor"), 100);
    }

    // Total entries across shards should match cache size
    EXPECT_EQ(total_entries, cache.size());
}

// Test cache optimization
TEST_F(TTLCacheOptimizationTest, CacheOptimization) {
    TTLCache<std::string, std::string> cache(50ms, 100, std::nullopt, config_);

    // Insert items with short expiration
    for (int i = 0; i < 20; ++i) {
        cache.put("opt_key_" + std::to_string(i), "value");
    }

    EXPECT_EQ(cache.size(), 20);

    // Wait for expiration
    std::this_thread::sleep_for(100ms);

    // Run optimization
    cache.optimize();

    // Expired items should be cleaned up
    EXPECT_LT(cache.size(), 20);
}

// Test thread safety of optimizations
TEST_F(TTLCacheOptimizationTest, ThreadSafety) {
    TTLCache<int, std::string> cache(1000ms, 200, std::nullopt, config_);

    const int num_threads = 4;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;

    // Launch multiple threads performing concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, t, operations_per_thread]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> dist(0, operations_per_thread * 2);

            for (int i = 0; i < operations_per_thread; ++i) {
                int key = dist(rng);
                std::string value = "thread_" + std::to_string(t) + "_value_" + std::to_string(key);

                // Mix of operations
                switch (i % 6) {
                    case 0:
                        cache.put(key, value, 2000ms);
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
                    case 4:
                        cache.get_statistics();
                        break;
                    case 5:
                        cache.get_health_report();
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
    auto stats = cache.get_statistics();
    EXPECT_GT(stats.hits + stats.misses, 0);

    // No crashes means thread safety is working
    SUCCEED();
}
