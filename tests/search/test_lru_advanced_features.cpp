#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <future>

#include "atom/search/lru.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

class LRUAdvancedFeaturesTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_size = 50;
        config_.concurrency_level = 4;
        config_.enable_statistics = true;
        config_.enable_health_monitoring = true;
    }

    LRUCacheConfig config_;
};

// Test prefetching functionality
TEST_F(LRUAdvancedFeaturesTest, Prefetching) {
    config_.enable_prefetching = true;
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert some initial data
    cache.put("user_001", "data1");
    cache.put("user_002", "data2");
    cache.put("other_001", "other1");

    // Create predictor and loader functions
    auto predictor = []() -> std::vector<std::string> {
        return {"user_003", "user_004", "user_005"};
    };

    auto loader = [](const std::string& key) -> std::optional<std::string> {
        if (key.find("user_") == 0) {
            return "prefetched_" + key;
        }
        return std::nullopt;
    };

    // Trigger prefetching
    cache.prefetch(predictor, loader);

    // Verify prefetched items are in cache
    auto result1 = cache.get("user_003");
    auto result2 = cache.get("user_004");
    auto result3 = cache.get("user_005");

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());

    EXPECT_EQ(*result1, "prefetched_user_003");
    EXPECT_EQ(*result2, "prefetched_user_004");
    EXPECT_EQ(*result3, "prefetched_user_005");

    // Check prefetch metrics
    auto metrics = cache.getMetrics();
    EXPECT_GT(metrics.prefetch_count.load(), 0);
}

// Test adaptive sizing
TEST_F(LRUAdvancedFeaturesTest, AdaptiveSizing) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Test enabling adaptive sizing
    cache.setAdaptiveSizing(true);
    auto config = cache.getConfig();
    EXPECT_TRUE(config.enable_adaptive_sizing);

    // Test disabling adaptive sizing
    cache.setAdaptiveSizing(false);
    config = cache.getConfig();
    EXPECT_FALSE(config.enable_adaptive_sizing);
}

// Test cache optimization
TEST_F(LRUAdvancedFeaturesTest, CacheOptimization) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert items with short expiration
    for (int i = 0; i < 20; ++i) {
        cache.put("opt_key_" + std::to_string(i), "value", std::optional<std::chrono::milliseconds>(50ms));
    }

    EXPECT_EQ(cache.size(), 20);

    // Wait for expiration
    std::this_thread::sleep_for(100ms);

    // Run optimization
    cache.optimize();

    // Expired items should be cleaned up
    EXPECT_LT(cache.size(), 20);
}

// Test thread safety of new features
TEST_F(LRUAdvancedFeaturesTest, ThreadSafety) {
    ThreadSafeLRUCache<int, std::string> cache(config_);

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
                        cache.put(key, value, std::optional<std::chrono::seconds>(5s));
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
                        cache.getMetrics();
                        break;
                    case 5:
                        cache.getHealthReport();
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
    auto metrics = cache.getMetrics();
    EXPECT_GT(metrics.total_operations.load(), 0);

    // No crashes means thread safety is working
    SUCCEED();
}

// Test async operations
TEST_F(LRUAdvancedFeaturesTest, AsyncOperations) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Insert some test data
    cache.put("async_key1", "async_value1");
    cache.put("async_key2", "async_value2");

    // Test async get
    auto future1 = cache.asyncGet("async_key1");
    auto future2 = cache.asyncGet("nonexistent");

    auto result1 = future1.get();
    auto result2 = future2.get();

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "async_value1");
    EXPECT_FALSE(result2.has_value());

    // Test async put
    auto put_future = cache.asyncPut("async_key3", "async_value3", 10s);
    put_future.wait();

    auto result3 = cache.get("async_key3");
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(*result3, "async_value3");
}

// Test comprehensive health reporting
TEST_F(LRUAdvancedFeaturesTest, ComprehensiveHealthReporting) {
    config_.max_memory_mb = 1;  // 1MB limit
    config_.unhealthy_hit_ratio_threshold = 0.3;
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

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

    auto healthy_report = cache.getHealthReport();
    EXPECT_TRUE(healthy_report.count("status"));
    EXPECT_TRUE(healthy_report["status"] == "HEALTHY" || healthy_report["status"] == "WARNING");

    // Create unhealthy scenario
    ThreadSafeLRUCache<std::string, std::string> unhealthy_cache(config_);

    // Insert some items
    for (int i = 0; i < 5; ++i) {
        unhealthy_cache.put("item_" + std::to_string(i), "value");
    }

    // Generate poor hit ratio
    for (int i = 0; i < 100; ++i) {
        unhealthy_cache.get("nonexistent_" + std::to_string(i));
    }

    auto unhealthy_report = unhealthy_cache.getHealthReport();
    EXPECT_TRUE(unhealthy_report.count("status"));
    EXPECT_TRUE(unhealthy_report.count("hit_ratio_warning") ||
                unhealthy_report["status"] == "UNHEALTHY");

    // Should provide recommendations
    EXPECT_TRUE(unhealthy_report.count("recommendation"));
}

// Test memory pressure handling
TEST_F(LRUAdvancedFeaturesTest, MemoryPressureHandling) {
    config_.max_memory_mb = 1;  // Small memory limit
    config_.memory_pressure_threshold = 0.5;  // 50% threshold
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Fill cache with large values
    std::string large_value(100 * 1024, 'X');  // 100KB value
    for (int i = 0; i < 20; ++i) {
        cache.put("large_" + std::to_string(i), large_value);
    }

    auto health_report = cache.getHealthReport();

    // Should detect memory pressure
    EXPECT_TRUE(health_report.count("memory_warning") ||
                health_report["status"] == "WARNING" ||
                health_report["status"] == "UNHEALTHY");
}

// Test configuration validation
TEST_F(LRUAdvancedFeaturesTest, ConfigurationValidation) {
    // Test invalid configurations
    LRUCacheConfig invalid_config;
    invalid_config.max_size = 0;  // Invalid

    // Test invalid configuration throws exception
    bool threw_exception = false;
    try {
        ThreadSafeLRUCache<std::string, std::string> cache(invalid_config);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }
    EXPECT_TRUE(threw_exception);

    // Test valid configuration updates
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    LRUCacheConfig new_config = config_;
    new_config.max_size = 200;
    new_config.cleanup_interval = 30s;

    EXPECT_NO_THROW(cache.updateConfig(new_config));

    auto updated_config = cache.getConfig();
    EXPECT_EQ(updated_config.max_size, 200);
    EXPECT_EQ(updated_config.cleanup_interval, 30s);
}

// Test value size estimation for different types
TEST_F(LRUAdvancedFeaturesTest, ValueSizeEstimation) {
    ThreadSafeLRUCache<std::string, std::string> string_cache(config_);
    ThreadSafeLRUCache<std::string, int> int_cache(config_);
    ThreadSafeLRUCache<std::string, std::vector<int>> vector_cache(config_);

    // Test string size estimation
    std::string test_string = "Hello, World!";
    size_t string_size = string_cache.estimateValueSize(test_string);
    EXPECT_GE(string_size, test_string.length());

    // Test int size estimation
    int test_int = 42;
    size_t int_size = int_cache.estimateValueSize(test_int);
    EXPECT_EQ(int_size, sizeof(int));

    // Test vector size estimation
    std::vector<int> test_vector = {1, 2, 3, 4, 5};
    size_t vector_size = vector_cache.estimateValueSize(test_vector);
    EXPECT_GE(vector_size, sizeof(std::vector<int>) + test_vector.size() * sizeof(int));
}

// Test cleanup thread lifecycle
TEST_F(LRUAdvancedFeaturesTest, CleanupThreadLifecycle) {
    config_.cleanup_interval = 100ms;
    config_.enable_background_cleanup = true;

    // Create cache with cleanup thread
    auto cache = std::make_unique<ThreadSafeLRUCache<std::string, std::string>>(config_);

    // Insert items with short TTL
    for (int i = 0; i < 10; ++i) {
        cache->put("cleanup_key_" + std::to_string(i), "value", std::optional<std::chrono::milliseconds>(50ms));
    }

    EXPECT_EQ(cache->size(), 10);

    // Wait for cleanup
    std::this_thread::sleep_for(200ms);

    // Items should be cleaned up
    EXPECT_LT(cache->size(), 10);

    // Destroy cache (should stop cleanup thread gracefully)
    cache.reset();

    // No crashes means cleanup thread was stopped properly
    SUCCEED();
}

// Test batch operations with TTL
TEST_F(LRUAdvancedFeaturesTest, BatchOperationsWithTTL) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Prepare batch data
    std::vector<std::pair<std::string, std::string>> batch_items;
    for (int i = 0; i < 20; ++i) {
        batch_items.emplace_back("ttl_key_" + std::to_string(i),
                                "ttl_value_" + std::to_string(i));
    }

    // Insert batch with TTL
    cache.putBatch(batch_items, std::optional<std::chrono::milliseconds>(100ms));

    // Verify all items are present
    EXPECT_EQ(cache.size(), 20);
    for (int i = 0; i < 20; ++i) {
        auto result = cache.get("ttl_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
    }

    // Wait for expiration
    std::this_thread::sleep_for(150ms);

    // Items should expire
    cache.pruneExpired();  // Manual prune to ensure expiration
    EXPECT_LT(cache.size(), 20);
}

// Test resize functionality
TEST_F(LRUAdvancedFeaturesTest, ResizeFunctionality) {
    ThreadSafeLRUCache<std::string, std::string> cache(config_);

    // Fill cache to capacity
    for (int i = 0; i < 50; ++i) {
        cache.put("resize_key_" + std::to_string(i), "value");
    }

    EXPECT_EQ(cache.size(), 50);

    // Resize to smaller capacity
    cache.resize(30);

    // Should evict items to fit new size
    EXPECT_LE(cache.size(), 30);

    // Resize to larger capacity
    cache.resize(100);

    // Should allow more items
    for (int i = 50; i < 80; ++i) {
        cache.put("resize_key_" + std::to_string(i), "value");
    }

    EXPECT_GT(cache.size(), 30);
}
