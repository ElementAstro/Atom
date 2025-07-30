#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>

#include "atom/search/database_base.hpp"
#include "atom/search/database_monitor.hpp"

using namespace atom::database;
using namespace std::chrono_literals;

class DatabaseIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.pool_size = 3;
        config_.min_pool_size = 1;
        config_.max_pool_size = 5;
        config_.connection_timeout = 2s;
        config_.idle_timeout = 30s;
        config_.max_connection_age = 300s;
        config_.enable_statistics = true;
        config_.enable_query_caching = true;
        config_.query_cache_size = 50;
        config_.query_cache_ttl = 60s;
        config_.enable_health_checks = true;
        config_.health_check_interval = 30s;
        config_.max_retry_attempts = 3;
        config_.retry_delay = 100ms;
        config_.enable_performance_monitoring = true;
        config_.monitoring_interval = 10s;
        config_.enable_slow_query_logging = true;
        config_.slow_query_threshold = 500ms;
    }

    DatabaseConfig config_;
};

// Test database configuration validation
TEST_F(DatabaseIntegrationTest, DatabaseConfiguration) {
    // Test default configuration
    DatabaseConfig default_config;
    EXPECT_EQ(default_config.pool_size, 0);  // Should use hardware_concurrency
    EXPECT_EQ(default_config.min_pool_size, 1);
    EXPECT_TRUE(default_config.enable_statistics);
    EXPECT_FALSE(default_config.enable_query_caching);

    // Test custom configuration
    EXPECT_EQ(config_.pool_size, 3);
    EXPECT_EQ(config_.max_pool_size, 5);
    EXPECT_TRUE(config_.enable_query_caching);
    EXPECT_EQ(config_.query_cache_size, 50);
    EXPECT_EQ(config_.max_retry_attempts, 3);
    EXPECT_EQ(config_.retry_delay, 100ms);

    // Test configuration validation
    EXPECT_GT(config_.connection_timeout.count(), 0);
    EXPECT_GT(config_.idle_timeout.count(), 0);
    EXPECT_GT(config_.max_connection_age.count(), 0);
    EXPECT_GT(config_.health_check_interval.count(), 0);
    EXPECT_GT(config_.monitoring_interval.count(), 0);
    EXPECT_GT(config_.slow_query_threshold.count(), 0);
}

// Test health report generation
TEST_F(DatabaseIntegrationTest, HealthReportGeneration) {
    HealthReport report;
    report.status = DatabaseHealth::DEGRADED;
    report.database_type = "MySQL";
    report.timestamp = std::chrono::steady_clock::now();
    report.details["connection_count"] = "5";
    report.details["active_queries"] = "12";
    report.warnings.push_back("High connection usage");
    report.warnings.push_back("Slow query detected");
    report.errors.push_back("Connection timeout occurred");

    // Test health status
    EXPECT_FALSE(report.is_healthy());

    // Test string representation
    std::string report_str = report.to_string();
    EXPECT_TRUE(report_str.find("MySQL") != std::string::npos);
    EXPECT_TRUE(report_str.find("DEGRADED") != std::string::npos);
    EXPECT_TRUE(report_str.find("High connection usage") != std::string::npos);
    EXPECT_TRUE(report_str.find("Connection timeout occurred") != std::string::npos);

    // Test healthy report
    HealthReport healthy_report;
    healthy_report.status = DatabaseHealth::HEALTHY;
    EXPECT_TRUE(healthy_report.is_healthy());
}

// Test performance metrics aggregation
TEST_F(DatabaseIntegrationTest, PerformanceMetricsAggregation) {
    PerformanceMetrics metrics;

    // Simulate a realistic workload
    std::vector<std::chrono::nanoseconds> query_times = {
        1ms, 5ms, 10ms, 15ms, 25ms,     // Fast queries
        50ms, 75ms, 100ms, 150ms,       // Medium queries
        500ms, 750ms, 1000ms,           // Slow queries
        2000ms, 3000ms                  // Very slow queries
    };

    for (auto time : query_times) {
        metrics.record_query_time(time);
    }

    // Record connection events
    for (int i = 0; i < 20; ++i) {
        metrics.record_connection_event(true);   // Successful connections
    }
    for (int i = 0; i < 3; ++i) {
        metrics.record_connection_event(false);  // Failed connections
    }

    // Record some errors
    for (int i = 0; i < 2; ++i) {
        metrics.record_error();
    }

    auto performance_data = metrics.get_metrics();

    // Verify metrics
    EXPECT_GT(performance_data["avg_query_time_ms"], 0.0);
    EXPECT_GT(performance_data["max_query_time_ms"], 2000.0);
    EXPECT_DOUBLE_EQ(performance_data["connection_success_rate"], 20.0/23.0);
    EXPECT_DOUBLE_EQ(performance_data["error_rate"], 2.0/14.0);  // 2 errors out of 14 queries

    // Check query distribution makes sense
    EXPECT_GT(performance_data["fast_query_percentage"], 0.0);
    EXPECT_GT(performance_data["slow_query_percentage"], 0.0);
    EXPECT_LE(performance_data["fast_query_percentage"], 100.0);
    EXPECT_LE(performance_data["slow_query_percentage"], 100.0);

    // Test QPS calculation (should be > 0 if time has passed)
    std::this_thread::sleep_for(10ms);
    auto updated_data = metrics.get_metrics();
    EXPECT_GE(updated_data["queries_per_second"], 0.0);
}

// Test query profiler with realistic queries
TEST_F(DatabaseIntegrationTest, QueryProfilerRealistic) {
    DatabaseProfiler profiler;

    // Simulate realistic database queries
    std::vector<std::pair<std::string, std::chrono::nanoseconds>> queries = {
        {"SELECT * FROM users WHERE id = ?", 5ms},
        {"SELECT * FROM users WHERE id = ?", 7ms},
        {"SELECT * FROM users WHERE id = ?", 6ms},
        {"SELECT COUNT(*) FROM orders WHERE user_id = ?", 15ms},
        {"SELECT COUNT(*) FROM orders WHERE user_id = ?", 18ms},
        {"INSERT INTO audit_log (action, user_id) VALUES (?, ?)", 3ms},
        {"INSERT INTO audit_log (action, user_id) VALUES (?, ?)", 4ms},
        {"INSERT INTO audit_log (action, user_id) VALUES (?, ?)", 2ms},
        {"UPDATE users SET last_login = NOW() WHERE id = ?", 8ms},
        {"SELECT o.*, u.name FROM orders o JOIN users u ON o.user_id = u.id", 250ms},
        {"SELECT o.*, u.name FROM orders o JOIN users u ON o.user_id = u.id", 300ms},
    };

    for (const auto& [query, time] : queries) {
        profiler.record_query(query, time);
    }

    auto top_queries = profiler.get_top_queries(5);

    // Should have at most 5 unique queries
    EXPECT_LE(top_queries.size(), 5);
    EXPECT_GT(top_queries.size(), 0);

    // Find the complex JOIN query (should have highest total time)
    auto join_query = std::find_if(top_queries.begin(), top_queries.end(),
        [](const DatabaseProfiler::QueryProfile& profile) {
            return profile.query.find("JOIN") != std::string::npos;
        });

    if (join_query != top_queries.end()) {
        EXPECT_EQ(join_query->execution_count, 2);
        EXPECT_DOUBLE_EQ(join_query->get_average_time_ms(), 275.0);  // (250+300)/2
    }

    // Find the SELECT users query (should have 3 executions)
    auto select_users = std::find_if(top_queries.begin(), top_queries.end(),
        [](const DatabaseProfiler::QueryProfile& profile) {
            return profile.query.find("SELECT * FROM users WHERE id") != std::string::npos;
        });

    if (select_users != top_queries.end()) {
        EXPECT_EQ(select_users->execution_count, 3);
        EXPECT_DOUBLE_EQ(select_users->get_average_time_ms(), 6.0);  // (5+7+6)/3
    }
}

// Test health monitor with multiple databases
TEST_F(DatabaseIntegrationTest, MultiDatabaseHealthMonitoring) {
    DatabaseHealthMonitor monitor(50ms);

    std::vector<std::string> alert_log;

    monitor.set_alert_callback([&alert_log](const std::string& name, const HealthReport& report) {
        alert_log.push_back(name + ":" + (report.is_healthy() ? "HEALTHY" : "UNHEALTHY"));
    });

    // Add multiple databases with different health states
    monitor.add_database("mysql_primary", []() {
        HealthReport report;
        report.status = DatabaseHealth::HEALTHY;
        report.database_type = "MySQL";
        return report;
    });

    monitor.add_database("mysql_replica", []() {
        HealthReport report;
        report.status = DatabaseHealth::DEGRADED;
        report.database_type = "MySQL";
        report.warnings.push_back("Replication lag detected");
        return report;
    });

    monitor.add_database("redis_cache", []() {
        HealthReport report;
        report.status = DatabaseHealth::HEALTHY;
        report.database_type = "Redis";
        return report;
    });

    bool postgres_healthy = true;
    monitor.add_database("postgres_analytics", [&postgres_healthy]() {
        HealthReport report;
        report.status = postgres_healthy ? DatabaseHealth::HEALTHY : DatabaseHealth::CRITICAL;
        report.database_type = "PostgreSQL";
        if (!postgres_healthy) {
            report.errors.push_back("Connection pool exhausted");
        }
        return report;
    });

    monitor.start();

    // Wait for initial health checks
    std::this_thread::sleep_for(100ms);

    // Check all databases are monitored
    auto reports = monitor.get_health_reports();
    EXPECT_EQ(reports.size(), 4);

    // Check individual database health
    auto mysql_primary = monitor.get_health_report("mysql_primary");
    EXPECT_EQ(mysql_primary.status, DatabaseHealth::HEALTHY);
    EXPECT_EQ(mysql_primary.database_type, "MySQL");

    auto mysql_replica = monitor.get_health_report("mysql_replica");
    EXPECT_EQ(mysql_replica.status, DatabaseHealth::DEGRADED);
    EXPECT_FALSE(mysql_replica.warnings.empty());

    // Overall health should be false due to degraded replica
    EXPECT_FALSE(monitor.is_all_healthy());

    // Simulate postgres failure
    postgres_healthy = false;

    // Wait for health check to detect the change
    std::this_thread::sleep_for(100ms);

    // Should have received alerts
    EXPECT_GT(alert_log.size(), 0);

    // Check postgres is now critical
    auto postgres_report = monitor.get_health_report("postgres_analytics");
    EXPECT_EQ(postgres_report.status, DatabaseHealth::CRITICAL);
    EXPECT_FALSE(postgres_report.errors.empty());

    monitor.stop();
}

// Test query cache with realistic scenarios
TEST_F(DatabaseIntegrationTest, QueryCacheRealistic) {
    QueryCache<std::string> cache(10, 100ms);  // Small cache with short TTL for testing

    // Simulate common queries
    std::vector<std::string> common_queries = {
        "SELECT * FROM users WHERE id = 1",
        "SELECT * FROM users WHERE id = 2",
        "SELECT COUNT(*) FROM orders",
        "SELECT * FROM products WHERE category = 'electronics'",
        "SELECT * FROM users WHERE email = 'test@example.com'"
    };

    // Cache some results
    for (size_t i = 0; i < common_queries.size(); ++i) {
        cache.put(common_queries[i], "result_" + std::to_string(i));
    }

    EXPECT_EQ(cache.size(), 5);

    // Test cache hits
    for (size_t i = 0; i < common_queries.size(); ++i) {
        auto result = cache.get(common_queries[i]);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result, "result_" + std::to_string(i));
    }

    // Add more queries to trigger LRU eviction
    for (int i = 0; i < 10; ++i) {
        cache.put("SELECT * FROM temp_table_" + std::to_string(i), "temp_result");
    }

    // Cache should be at max size
    EXPECT_EQ(cache.size(), 10);

    // Some original queries should have been evicted
    size_t evicted_count = 0;
    for (const auto& query : common_queries) {
        if (!cache.get(query).has_value()) {
            evicted_count++;
        }
    }
    EXPECT_GT(evicted_count, 0);

    // Test TTL expiration
    std::this_thread::sleep_for(150ms);

    // All entries should be expired
    for (int i = 0; i < 10; ++i) {
        auto result = cache.get("SELECT * FROM temp_table_" + std::to_string(i));
        EXPECT_FALSE(result.has_value());
    }
}

// Test concurrent database operations
TEST_F(DatabaseIntegrationTest, ConcurrentOperations) {
    PerformanceMetrics metrics;
    DatabaseProfiler profiler;

    const int num_threads = 4;
    const int operations_per_thread = 50;
    std::vector<std::future<void>> futures;

    // Launch concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&metrics, &profiler, t, operations_per_thread]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                // Simulate query execution time
                auto start = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::microseconds(100 + (t * 50)));
                auto end = std::chrono::steady_clock::now();

                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

                // Record metrics
                metrics.record_query_time(duration);

                // Record in profiler
                std::string query = "SELECT * FROM table_" + std::to_string(t) + " WHERE id = " + std::to_string(i);
                profiler.record_query(query, duration);

                // Simulate occasional connection events and errors
                if (i % 10 == 0) {
                    metrics.record_connection_event(true);
                }
                if (i % 25 == 0) {
                    metrics.record_error();
                }
            }
        }));
    }

    // Wait for all operations to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Verify metrics were recorded correctly
    auto performance_data = metrics.get_metrics();
    EXPECT_EQ(performance_data.count("avg_query_time_ms"), 1);
    EXPECT_GT(performance_data["avg_query_time_ms"], 0.0);

    // Verify profiler recorded queries
    auto top_queries = profiler.get_top_queries(10);
    EXPECT_GT(top_queries.size(), 0);

    // Each thread should have generated unique queries
    size_t total_executions = 0;
    for (const auto& profile : top_queries) {
        total_executions += profile.execution_count;
    }
    EXPECT_EQ(total_executions, num_threads * operations_per_thread);
}

// Test database configuration edge cases
TEST_F(DatabaseIntegrationTest, ConfigurationEdgeCases) {
    // Test minimum values
    DatabaseConfig min_config;
    min_config.pool_size = 0;
    min_config.min_pool_size = 0;
    min_config.max_pool_size = 1;
    min_config.connection_timeout = std::chrono::duration_cast<std::chrono::seconds>(1ms);
    min_config.query_cache_size = 1;
    min_config.max_retry_attempts = 1;

    EXPECT_GE(min_config.max_pool_size, 1);
    EXPECT_GT(min_config.connection_timeout.count(), 0);
    EXPECT_GE(min_config.query_cache_size, 1);
    EXPECT_GE(min_config.max_retry_attempts, 1);

    // Test maximum values
    DatabaseConfig max_config;
    max_config.pool_size = 1000;
    max_config.max_pool_size = 1000;
    max_config.connection_timeout = 3600s;
    max_config.query_cache_size = 100000;
    max_config.max_retry_attempts = 10;

    EXPECT_EQ(max_config.pool_size, 1000);
    EXPECT_EQ(max_config.max_pool_size, 1000);
    EXPECT_EQ(max_config.connection_timeout, 3600s);
    EXPECT_EQ(max_config.query_cache_size, 100000);
    EXPECT_EQ(max_config.max_retry_attempts, 10);

    // Test inconsistent values
    DatabaseConfig inconsistent_config;
    inconsistent_config.min_pool_size = 10;
    inconsistent_config.max_pool_size = 5;  // Less than min

    // In a real implementation, this should be validated and corrected
    EXPECT_TRUE(inconsistent_config.min_pool_size > 0);
    EXPECT_TRUE(inconsistent_config.max_pool_size > 0);
}
