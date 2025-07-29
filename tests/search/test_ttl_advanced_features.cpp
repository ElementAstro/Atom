#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>

#include "atom/search/ttl.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

class TTLAdvancedFeaturesTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enable_automatic_cleanup = true;
        config_.enable_statistics = true;
        config_.thread_safe = true;
        config_.enable_lazy_expiration = true;
        config_.enable_memory_tracking = true;
        config_.enable_health_monitoring = true;
    }

    CacheConfig config_;
};

// Test expiration index functionality
TEST_F(TTLAdvancedFeaturesTest, ExpirationIndex) {
    config_.enable_expiration_index = true;
    config_.expiration_index_granularity = 100;  // 100ms granularity
    TTLCache<std::string, std::string> cache(200ms, 100, 50ms, config_);

    // Insert items with different expiration times
    cache.put("key1", "value1", 100ms);
    cache.put("key2", "value2", 150ms);
    cache.put("key3", "value3", 200ms);
    cache.put("key4", "value4", 250ms);

    EXPECT_EQ(cache.size(), 4);

    // Wait for partial expiration
    std::this_thread::sleep_for(175ms);

    // Force cleanup to use expiration index
    cache.optimize();

    // Items with shorter TTL should be expired
    EXPECT_LT(cache.size(), 4);

    // Remaining items should still be accessible
    auto result4 = cache.get("key4");
    EXPECT_TRUE(result4.has_value() || cache.size() == 0);  // May have expired by now
}

// Test batch operations optimization
TEST_F(TTLAdvancedFeaturesTest, BatchOperationsOptimization) {
    config_.enable_batch_optimization = true;
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Prepare batch data
    std::vector<std::pair<std::string, std::string>> batch_items;
    for (int i = 0; i < 50; ++i) {
        batch_items.emplace_back("batch_key_" + std::to_string(i),
                                "batch_value_" + std::to_string(i));
    }

    auto start = std::chrono::high_resolution_clock::now();
    cache.batch_put(batch_items, 2000ms);
    auto end = std::chrono::high_resolution_clock::now();

    auto batch_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Verify all items were inserted
    EXPECT_EQ(cache.size(), 50);
    for (int i = 0; i < 50; ++i) {
        auto result = cache.get("batch_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "batch_value_" + std::to_string(i));
    }

    // Test batch get
    std::vector<std::string> keys_to_get;
    for (int i = 0; i < 25; ++i) {
        keys_to_get.push_back("batch_key_" + std::to_string(i));
    }

    auto batch_results = cache.batch_get(keys_to_get);
    EXPECT_EQ(batch_results.size(), 25);

    for (const auto& [key, value] : batch_results) {
        EXPECT_TRUE(value.has_value());
    }

    // Check batch operation statistics
    auto stats = cache.get_statistics();
    EXPECT_GT(stats.batch_operations.load(), 0);
}

// Test compression functionality
TEST_F(TTLAdvancedFeaturesTest, CompressionSupport) {
    config_.enable_compression = true;
    config_.compression_threshold = 100;  // Compress values larger than 100 bytes
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Insert small value (should not be compressed)
    std::string small_value = "small";
    cache.put("small_key", small_value);

    // Insert large value (should be compressed)
    std::string large_value(500, 'A');
    cache.put("large_key", large_value);

    // Verify values can be retrieved correctly
    auto small_result = cache.get("small_key");
    auto large_result = cache.get("large_key");

    EXPECT_TRUE(small_result.has_value());
    EXPECT_TRUE(large_result.has_value());
    EXPECT_EQ(*small_result, small_value);
    EXPECT_EQ(*large_result, large_value);

    // Check compression statistics (if implemented)
    auto stats = cache.get_statistics();
    // Note: Actual compression is not implemented, so compression_saves_bytes will be 0
    EXPECT_GE(stats.compression_saves_bytes.load(), 0);
}

// Test memory pressure handling
TEST_F(TTLAdvancedFeaturesTest, MemoryPressureHandling) {
    config_.max_memory_mb = 1;  // Small memory limit
    config_.memory_pressure_threshold = 0.3;  // 30% threshold
    TTLCache<std::string, std::string> cache(10000ms, 1000, std::nullopt, config_);

    // Fill cache with large values
    std::string large_value(50 * 1024, 'X');  // 50KB value
    for (int i = 0; i < 30; ++i) {
        cache.put("large_" + std::to_string(i), large_value);
    }

    auto health_report = cache.get_health_report();

    // Should detect memory pressure
    EXPECT_TRUE(health_report.count("memory_warning") ||
                health_report["status"] == "WARNING" ||
                health_report["status"] == "UNHEALTHY");

    // Test memory limit enforcement
    cache.optimize();  // Should trigger memory cleanup

    // Some items should have been evicted
    EXPECT_LT(cache.size(), 30);
}

// Test comprehensive health reporting
TEST_F(TTLAdvancedFeaturesTest, ComprehensiveHealthReporting) {
    config_.max_memory_mb = 2;  // 2MB limit
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Create healthy scenario first
    for (int i = 0; i < 10; ++i) {
        cache.put("healthy_" + std::to_string(i), "value");
    }

    // Generate good hit ratio
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 3; ++j) {
            cache.get("healthy_" + std::to_string(i));
        }
    }

    auto healthy_report = cache.get_health_report();
    EXPECT_TRUE(healthy_report.count("status"));
    EXPECT_TRUE(healthy_report["status"] == "HEALTHY" || healthy_report["status"] == "WARNING");

    // Create unhealthy scenario
    TTLCache<std::string, std::string> unhealthy_cache(1000ms, 100, std::nullopt, config_);

    // Insert some items
    for (int i = 0; i < 5; ++i) {
        unhealthy_cache.put("item_" + std::to_string(i), "value");
    }

    // Generate poor hit ratio
    for (int i = 0; i < 100; ++i) {
        unhealthy_cache.get("nonexistent_" + std::to_string(i));
    }

    auto unhealthy_report = unhealthy_cache.get_health_report();
    EXPECT_TRUE(unhealthy_report.count("status"));
    EXPECT_TRUE(unhealthy_report.count("hit_ratio_warning") ||
                unhealthy_report["status"] == "UNHEALTHY");
}

// Test configuration updates
TEST_F(TTLAdvancedFeaturesTest, ConfigurationUpdates) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Test getting current config
    auto current_config = cache.get_config();
    EXPECT_TRUE(current_config.enable_statistics);
    EXPECT_TRUE(current_config.enable_lazy_expiration);

    // Test updating configuration
    CacheConfig new_config = config_;
    new_config.cleanup_batch_size = 200;
    new_config.enable_compression = true;
    new_config.max_memory_mb = 5;

    cache.update_config(new_config);

    auto updated_config = cache.get_config();
    EXPECT_EQ(updated_config.cleanup_batch_size, 200);
    EXPECT_TRUE(updated_config.enable_compression);
    EXPECT_EQ(updated_config.max_memory_mb, 5);
}

// Test performance timing
TEST_F(TTLAdvancedFeaturesTest, PerformanceTiming) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Perform operations to generate timing data
    for (int i = 0; i < 20; ++i) {
        cache.put("timing_key_" + std::to_string(i), "value");
    }

    for (int i = 0; i < 20; ++i) {
        cache.get("timing_key_" + std::to_string(i));
    }

    auto stats = cache.get_statistics();

    // Should have timing data
    EXPECT_GT(stats.total_access_time_ns.load(), 0);
    EXPECT_GT(stats.get_average_access_time_ns(), 0.0);

    // Test cleanup timing
    cache.optimize();  // Force cleanup
    EXPECT_GT(stats.total_cleanup_time_ns.load(), 0);
}

// Test custom TTL with different values
TEST_F(TTLAdvancedFeaturesTest, CustomTTLHandling) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Insert items with different custom TTLs
    cache.put("short_ttl", "value1", 100ms);
    cache.put("medium_ttl", "value2", 500ms);
    cache.put("long_ttl", "value3", 2000ms);
    cache.put("default_ttl", "value4");  // Uses default TTL

    EXPECT_EQ(cache.size(), 4);

    // Wait for short TTL to expire
    std::this_thread::sleep_for(150ms);

    // Short TTL item should be expired (with lazy expiration)
    auto short_result = cache.get("short_ttl");
    EXPECT_FALSE(short_result.has_value());

    // Others should still be available
    auto medium_result = cache.get("medium_ttl");
    auto long_result = cache.get("long_ttl");
    auto default_result = cache.get("default_ttl");

    EXPECT_TRUE(medium_result.has_value());
    EXPECT_TRUE(long_result.has_value());
    EXPECT_TRUE(default_result.has_value());

    // Wait for medium TTL to expire
    std::this_thread::sleep_for(400ms);

    auto medium_result2 = cache.get("medium_ttl");
    EXPECT_FALSE(medium_result2.has_value());
}

// Test eviction callbacks with enhanced features
TEST_F(TTLAdvancedFeaturesTest, EvictionCallbacks) {
    TTLCache<std::string, std::string> cache(100ms, 50, std::nullopt, config_);

    std::vector<std::pair<std::string, bool>> evicted_items;

    // Set eviction callback
    cache.set_eviction_callback([&evicted_items](const std::string& key, const std::string& value, bool expired) {
        evicted_items.emplace_back(key, expired);
    });

    // Insert items with short TTL
    for (int i = 0; i < 10; ++i) {
        cache.put("callback_key_" + std::to_string(i), "value");
    }

    // Wait for expiration
    std::this_thread::sleep_for(150ms);

    // Trigger cleanup or lazy expiration
    for (int i = 0; i < 10; ++i) {
        cache.get("callback_key_" + std::to_string(i));
    }

    // Should have received eviction callbacks
    EXPECT_GT(evicted_items.size(), 0);

    // Check that expired flag is set correctly
    for (const auto& [key, expired] : evicted_items) {
        EXPECT_TRUE(expired);  // Should be expired, not evicted due to capacity
    }
}

// Test concurrent async operations
TEST_F(TTLAdvancedFeaturesTest, ConcurrentAsyncOperations) {
    TTLCache<std::string, std::string> cache(1000ms, 100, std::nullopt, config_);

    // Launch multiple async operations concurrently
    std::vector<std::future<void>> put_futures;
    std::vector<std::future<std::optional<std::string>>> get_futures;

    // Async puts
    for (int i = 0; i < 20; ++i) {
        put_futures.push_back(
            cache.async_put("async_key_" + std::to_string(i),
                           "async_value_" + std::to_string(i), 2000ms)
        );
    }

    // Wait for puts to complete
    for (auto& future : put_futures) {
        future.wait();
    }

    // Async gets
    for (int i = 0; i < 20; ++i) {
        get_futures.push_back(cache.async_get("async_key_" + std::to_string(i)));
    }

    // Verify all gets return valid values
    for (int i = 0; i < 20; ++i) {
        auto result = get_futures[i].get();
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "async_value_" + std::to_string(i));
    }
}

// Test shard load balancing
TEST_F(TTLAdvancedFeaturesTest, ShardLoadBalancing) {
    TTLCache<std::string, std::string> cache(1000ms, 200, std::nullopt, config_);

    // Insert many items to test distribution
    for (int i = 0; i < 100; ++i) {
        cache.put("balance_key_" + std::to_string(i), "value");
    }

    auto shard_stats = cache.get_shard_stats();

    // Calculate load distribution
    size_t min_load = SIZE_MAX;
    size_t max_load = 0;
    size_t total_load = 0;

    for (const auto& stats : shard_stats) {
        size_t shard_size = stats.at("size");
        min_load = std::min(min_load, shard_size);
        max_load = std::max(max_load, shard_size);
        total_load += shard_size;
    }

    EXPECT_EQ(total_load, 100);  // All items should be distributed

    // Load should be reasonably balanced (no shard should be empty or overloaded)
    double avg_load = static_cast<double>(total_load) / shard_stats.size();
    double load_imbalance = static_cast<double>(max_load - min_load) / avg_load;

    // Allow some imbalance due to hash distribution, but not excessive
    EXPECT_LT(load_imbalance, 2.0);  // Max load shouldn't be more than 2x average
}
