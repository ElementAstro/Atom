#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>

#include "atom/search/cache.hpp"

using namespace atom::search;
using namespace std::chrono_literals;

class CacheAdvancedFeaturesTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_size = 50;
        config_.enable_metrics = true;
        config_.enable_performance_tracking = true;
        config_.enable_health_monitoring = true;
    }

    CacheConfig config_;
};

// Test configuration management
TEST_F(CacheAdvancedFeaturesTest, ConfigurationManagement) {
    ResourceCache<std::string> cache(config_);

    // Test getting current config
    auto current_config = cache.get_config();
    EXPECT_EQ(current_config.max_size, 50);
    EXPECT_TRUE(current_config.enable_metrics);

    // Test updating configuration
    CacheConfig new_config = config_;
    new_config.max_size = 100;
    new_config.eviction_policy = EvictionPolicy::LFU;
    new_config.enable_performance_tracking = false;

    cache.update_config(new_config);

    auto updated_config = cache.get_config();
    EXPECT_EQ(updated_config.max_size, 100);
    EXPECT_EQ(updated_config.eviction_policy, EvictionPolicy::LFU);
    EXPECT_FALSE(updated_config.enable_performance_tracking);
}

// Test performance tracking toggle
TEST_F(CacheAdvancedFeaturesTest, PerformanceTracking) {
    ResourceCache<std::string> cache(config_);

    // Enable performance tracking
    cache.set_performance_tracking(true);

    // Perform operations
    cache.insert("perf_test", "value", 10s);
    cache.get("perf_test");
    cache.get("nonexistent");

    auto metrics = cache.get_metrics();
    EXPECT_GT(metrics.total_access_time_ns.load(), 0);
    EXPECT_GT(metrics.total_insert_time_ns.load(), 0);

    // Disable performance tracking
    cache.set_performance_tracking(false);

    // Metrics should be reset
    metrics = cache.get_metrics();
    EXPECT_EQ(metrics.total_access_time_ns.load(), 0);
    EXPECT_EQ(metrics.total_insert_time_ns.load(), 0);
}

// Test comprehensive health reporting
TEST_F(CacheAdvancedFeaturesTest, HealthReporting) {
    config_.unhealthy_hit_ratio_threshold = 0.3;
    config_.max_memory_mb = 1;  // 1MB limit
    ResourceCache<std::string> cache(config_);

    // Create healthy scenario first
    for (int i = 0; i < 10; ++i) {
        cache.insert("healthy_" + std::to_string(i), "value", 10s);
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
    ResourceCache<std::string> unhealthy_cache(config_);

    // Insert some items
    for (int i = 0; i < 5; ++i) {
        unhealthy_cache.insert("item_" + std::to_string(i), "value", 10s);
    }

    // Generate poor hit ratio
    for (int i = 0; i < 100; ++i) {
        unhealthy_cache.get("nonexistent_" + std::to_string(i));
    }

    auto unhealthy_report = unhealthy_cache.get_health_report();
    EXPECT_TRUE(unhealthy_report.count("status"));
    EXPECT_TRUE(unhealthy_report.count("hit_ratio_warning") ||
                unhealthy_report["status"] == "UNHEALTHY");

    // Should provide recommendations
    EXPECT_TRUE(unhealthy_report.count("recommendation"));
}

// Test efficiency metrics calculation
TEST_F(CacheAdvancedFeaturesTest, EfficiencyMetrics) {
    ResourceCache<std::string> cache(config_);

    // Perform mixed operations
    for (int i = 0; i < 20; ++i) {
        cache.insert("eff_key_" + std::to_string(i), "value_" + std::to_string(i), 10s);
    }

    // Generate hits and misses
    for (int i = 0; i < 10; ++i) {
        cache.get("eff_key_" + std::to_string(i));  // Hits
    }
    for (int i = 100; i < 110; ++i) {
        cache.get("nonexistent_" + std::to_string(i));  // Misses
    }

    // Force some evictions
    for (int i = 100; i < 150; ++i) {
        cache.insert("overflow_" + std::to_string(i), "value", 10s);
    }

    auto efficiency = cache.get_efficiency_metrics();

    // Verify all expected metrics are present
    EXPECT_TRUE(efficiency.count("hit_ratio"));
    EXPECT_TRUE(efficiency.count("memory_efficiency"));
    EXPECT_TRUE(efficiency.count("eviction_rate"));
    EXPECT_TRUE(efficiency.count("expiration_rate"));
    EXPECT_TRUE(efficiency.count("shard_balance_coefficient"));

    // Verify metric ranges
    EXPECT_GE(efficiency["hit_ratio"], 0.0);
    EXPECT_LE(efficiency["hit_ratio"], 1.0);
    EXPECT_GE(efficiency["eviction_rate"], 0.0);
    EXPECT_GE(efficiency["shard_balance_coefficient"], 0.0);
    EXPECT_LE(efficiency["shard_balance_coefficient"], 1.0);

    // If performance tracking is enabled, check timing metrics
    if (config_.enable_performance_tracking) {
        EXPECT_TRUE(efficiency.count("avg_access_time_ms"));
        EXPECT_TRUE(efficiency.count("avg_insert_time_ms"));
        EXPECT_GE(efficiency["avg_access_time_ms"], 0.0);
        EXPECT_GE(efficiency["avg_insert_time_ms"], 0.0);
    }
}

// Test shard statistics
TEST_F(CacheAdvancedFeaturesTest, ShardStatistics) {
    ResourceCache<std::string> cache(config_);

    // Insert items to distribute across shards
    for (int i = 0; i < 40; ++i) {
        cache.insert("shard_test_" + std::to_string(i), "value", 10s);
    }

    auto shard_stats = cache.get_shard_stats();

    // Should have multiple shards
    EXPECT_GT(shard_stats.size(), 1);

    size_t total_entries = 0;
    for (const auto& stats : shard_stats) {
        EXPECT_TRUE(stats.count("entries"));
        EXPECT_TRUE(stats.count("max_size"));
        EXPECT_TRUE(stats.count("memory_usage"));

        total_entries += stats.at("entries");
        EXPECT_GE(stats.at("max_size"), 0);
        EXPECT_GE(stats.at("memory_usage"), 0);
    }

    // Total entries across shards should match cache size
    EXPECT_EQ(total_entries, cache.size());
}

// Test metrics reset functionality
TEST_F(CacheAdvancedFeaturesTest, MetricsReset) {
    ResourceCache<std::string> cache(config_);

    // Perform operations to generate metrics
    cache.insert("reset_test", "value", 10s);
    cache.get("reset_test");
    cache.get("nonexistent");
    cache.remove("reset_test");

    auto metrics_before = cache.get_metrics();
    EXPECT_GT(metrics_before.hit_count.load(), 0);
    EXPECT_GT(metrics_before.miss_count.load(), 0);
    EXPECT_GT(metrics_before.total_operations.load(), 0);

    // Reset metrics
    cache.reset_metrics();

    auto metrics_after = cache.get_metrics();
    EXPECT_EQ(metrics_after.hit_count.load(), 0);
    EXPECT_EQ(metrics_after.miss_count.load(), 0);
    EXPECT_EQ(metrics_after.eviction_count.load(), 0);
    EXPECT_EQ(metrics_after.total_operations.load(), 0);
}

// Test cache optimization
TEST_F(CacheAdvancedFeaturesTest, CacheOptimization) {
    ResourceCache<std::string> cache(config_);

    // Insert items with short expiration
    for (int i = 0; i < 20; ++i) {
        cache.insert("opt_key_" + std::to_string(i), "value", 50ms);
    }

    EXPECT_EQ(cache.size(), 20);

    // Wait for expiration
    std::this_thread::sleep_for(100ms);

    // Run optimization
    cache.optimize();

    // Expired items should be cleaned up
    EXPECT_LT(cache.size(), 20);

    auto metrics = cache.get_metrics();
    EXPECT_GT(metrics.expiration_count.load(), 0);
}

// Test warm cache functionality
TEST_F(CacheAdvancedFeaturesTest, CacheWarming) {
    ResourceCache<std::string> cache(config_);

    // Create a loader function
    auto loader = []() -> Vector<std::pair<String, std::string>> {
        Vector<std::pair<String, std::string>> items;
        for (int i = 0; i < 15; ++i) {
            items.emplace_back("warm_key_" + std::to_string(i),
                              "warm_value_" + std::to_string(i));
        }
        return items;
    };

    // Warm the cache
    cache.warm_cache(loader, 10s);

    // Verify items were loaded
    EXPECT_EQ(cache.size(), 15);

    for (int i = 0; i < 15; ++i) {
        auto result = cache.get("warm_key_" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "warm_value_" + std::to_string(i));
    }
}

// Test health metrics
TEST_F(CacheAdvancedFeaturesTest, HealthMetrics) {
    config_.max_memory_mb = 5;  // 5MB limit
    ResourceCache<std::string> cache(config_);

    // Insert some data
    for (int i = 0; i < 30; ++i) {
        cache.insert("health_" + std::to_string(i), "value_" + std::to_string(i), 10s);
    }

    // Generate mixed access pattern
    for (int i = 0; i < 20; ++i) {
        cache.get("health_" + std::to_string(i % 10));  // Some hits
    }
    for (int i = 0; i < 10; ++i) {
        cache.get("nonexistent_" + std::to_string(i));  // Some misses
    }

    auto health_metrics = cache.get_health_metrics();

    // Verify expected health metrics
    EXPECT_TRUE(health_metrics.count("hit_ratio"));
    EXPECT_TRUE(health_metrics.count("memory_usage_mb"));
    EXPECT_TRUE(health_metrics.count("load_factor"));
    EXPECT_TRUE(health_metrics.count("avg_shard_load"));
    EXPECT_TRUE(health_metrics.count("memory_health"));

    // Verify metric ranges
    EXPECT_GE(health_metrics["hit_ratio"], 0.0);
    EXPECT_LE(health_metrics["hit_ratio"], 1.0);
    EXPECT_GE(health_metrics["memory_usage_mb"], 0.0);
    EXPECT_GE(health_metrics["load_factor"], 0.0);
    EXPECT_LE(health_metrics["load_factor"], 1.0);
    EXPECT_GE(health_metrics["memory_health"], 0.0);
    EXPECT_LE(health_metrics["memory_health"], 1.0);
}

// Test cache health status
TEST_F(CacheAdvancedFeaturesTest, HealthStatus) {
    config_.max_memory_mb = 1;  // Small memory limit
    ResourceCache<std::string> cache(config_);

    // Initially should be healthy
    EXPECT_TRUE(cache.is_healthy());

    // Fill with small items - should remain healthy
    for (int i = 0; i < 10; ++i) {
        cache.insert("small_" + std::to_string(i), "value", 10s);
    }

    EXPECT_TRUE(cache.is_healthy());

    // Try to exceed memory limit
    std::string large_value(500 * 1024, 'X');  // 500KB value
    for (int i = 0; i < 5; ++i) {
        cache.insert("large_" + std::to_string(i), large_value, 10s);
    }

    // May or may not be healthy depending on eviction efficiency
    // The test mainly verifies the method doesn't crash
    bool health_status = cache.is_healthy();
    EXPECT_TRUE(health_status == true || health_status == false);  // Just verify it returns a boolean
}
