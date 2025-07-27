#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "atom/search/database_base.hpp"
#include "atom/search/database_monitor.hpp"

using namespace atom::database;
using namespace std::chrono_literals;

// Mock connection for testing
class MockConnection {
public:
    MockConnection(bool healthy = true) : healthy_(healthy) {}
    bool is_connected() const { return healthy_; }
    void set_healthy(bool healthy) { healthy_ = healthy; }
    
private:
    bool healthy_;
};

// Mock database connection wrapper
class MockDatabaseConnection : public DatabaseConnection<MockConnection> {
public:
    MockDatabaseConnection(MockConnection* conn, std::chrono::steady_clock::time_point created)
        : DatabaseConnection<MockConnection>(conn, created) {}
    
    bool is_healthy() const override {
        return connection_->is_connected();
    }
    
    ConnectionHealth check_health() override {
        return connection_->is_connected() ? ConnectionHealth::HEALTHY : ConnectionHealth::UNHEALTHY;
    }
    
    void reset_connection() override {
        connection_->set_healthy(true);
    }
};

// Mock connection pool for testing
class MockConnectionPool : public EnhancedConnectionPool<MockConnection, MockDatabaseConnection> {
public:
    MockConnectionPool(const DatabaseConfig& config) 
        : EnhancedConnectionPool<MockConnection, MockDatabaseConnection>(config) {}

protected:
    ConnectionPtr create_connection() override {
        auto conn = new MockConnection(true);
        statistics_.connections_created++;
        return std::make_unique<MockDatabaseConnection>(conn, std::chrono::steady_clock::now());
    }
    
    bool validate_connection(MockDatabaseConnection* conn) override {
        return conn->is_healthy();
    }
};

class DatabaseOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.pool_size = 5;
        config_.min_pool_size = 2;
        config_.max_pool_size = 10;
        config_.connection_timeout = 5s;
        config_.enable_statistics = true;
        config_.enable_health_checks = true;
    }

    DatabaseConfig config_;
};

// Test enhanced connection pool
TEST_F(DatabaseOptimizationTest, EnhancedConnectionPool) {
    MockConnectionPool pool(config_);
    
    // Test connection acquisition
    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();
    
    EXPECT_TRUE(conn1 != nullptr);
    EXPECT_TRUE(conn2 != nullptr);
    EXPECT_TRUE(conn1->is_healthy());
    EXPECT_TRUE(conn2->is_healthy());
    
    // Test pool statistics
    auto stats = pool.get_pool_stats();
    EXPECT_GE(stats["total_connections"], 2);
    EXPECT_EQ(stats["active_connections"], 2);
    EXPECT_EQ(stats["max_pool_size"], 10);
    
    // Test connection release (automatic via RAII)
    conn1.reset();
    conn2.reset();
    
    // Give some time for connections to be returned to pool
    std::this_thread::sleep_for(10ms);
    
    auto updated_stats = pool.get_pool_stats();
    EXPECT_EQ(updated_stats["active_connections"], 0);
}

// Test database statistics
TEST_F(DatabaseOptimizationTest, DatabaseStatistics) {
    DatabaseStatistics stats;
    
    // Test initial state
    EXPECT_EQ(stats.total_queries.load(), 0);
    EXPECT_EQ(stats.successful_queries.load(), 0);
    EXPECT_EQ(stats.failed_queries.load(), 0);
    EXPECT_DOUBLE_EQ(stats.get_success_rate(), 0.0);
    
    // Simulate some queries
    stats.total_queries = 100;
    stats.successful_queries = 90;
    stats.failed_queries = 10;
    stats.total_query_time_ns = 1000000000;  // 1 second total
    
    EXPECT_DOUBLE_EQ(stats.get_success_rate(), 0.9);
    EXPECT_DOUBLE_EQ(stats.get_average_query_time_ms(), 10.0);  // 10ms average
    
    // Test connection pool metrics
    stats.connection_pool_hits = 80;
    stats.connection_pool_misses = 20;
    EXPECT_DOUBLE_EQ(stats.get_connection_pool_hit_rate(), 0.8);
}

// Test query cache
TEST_F(DatabaseOptimizationTest, QueryCache) {
    QueryCache<std::string> cache(100, 60s);
    
    // Test cache miss
    auto result1 = cache.get("SELECT * FROM users");
    EXPECT_FALSE(result1.has_value());
    
    // Test cache put and hit
    cache.put("SELECT * FROM users", "cached_result");
    auto result2 = cache.get("SELECT * FROM users");
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "cached_result");
    
    // Test cache size
    EXPECT_EQ(cache.size(), 1);
    
    // Test cache clear
    cache.clear();
    EXPECT_EQ(cache.size(), 0);
    
    auto result3 = cache.get("SELECT * FROM users");
    EXPECT_FALSE(result3.has_value());
}

// Test query cache expiration
TEST_F(DatabaseOptimizationTest, QueryCacheExpiration) {
    QueryCache<std::string> cache(100, 50ms);  // Short TTL for testing
    
    cache.put("SELECT * FROM test", "result");
    
    // Should be available immediately
    auto result1 = cache.get("SELECT * FROM test");
    EXPECT_TRUE(result1.has_value());
    
    // Wait for expiration
    std::this_thread::sleep_for(100ms);
    
    // Should be expired now
    auto result2 = cache.get("SELECT * FROM test");
    EXPECT_FALSE(result2.has_value());
}

// Test retry manager
TEST_F(DatabaseOptimizationTest, RetryManager) {
    RetryManager retry_manager(3, 10ms);
    
    // Test successful operation (no retry needed)
    int call_count = 0;
    auto result1 = retry_manager.execute_with_retry([&call_count]() {
        call_count++;
        return 42;
    });
    
    EXPECT_EQ(result1, 42);
    EXPECT_EQ(call_count, 1);
    
    // Test operation that fails then succeeds
    call_count = 0;
    auto result2 = retry_manager.execute_with_retry([&call_count]() {
        call_count++;
        if (call_count < 3) {
            throw std::runtime_error("Temporary failure");
        }
        return 100;
    });
    
    EXPECT_EQ(result2, 100);
    EXPECT_EQ(call_count, 3);
    
    // Test operation that always fails
    call_count = 0;
    EXPECT_THROW(
        retry_manager.execute_with_retry([&call_count]() {
            call_count++;
            throw std::runtime_error("Permanent failure");
        }),
        std::runtime_error
    );
    
    EXPECT_EQ(call_count, 3);  // Should have tried 3 times
}

// Test performance metrics
TEST_F(DatabaseOptimizationTest, PerformanceMetrics) {
    PerformanceMetrics metrics;
    
    // Record some query times
    metrics.record_query_time(5ms);   // Fast query
    metrics.record_query_time(50ms);  // Medium query
    metrics.record_query_time(500ms); // Slow query
    metrics.record_query_time(2000ms); // Very slow query
    
    // Record connection events
    metrics.record_connection_event(true);   // Success
    metrics.record_connection_event(true);   // Success
    metrics.record_connection_event(false);  // Failure
    
    // Record errors
    metrics.record_error();
    
    auto performance_data = metrics.get_metrics();
    
    // Check metrics are calculated correctly
    EXPECT_GT(performance_data["avg_query_time_ms"], 0.0);
    EXPECT_GT(performance_data["max_query_time_ms"], 1000.0);  // Should be ~2000ms
    EXPECT_DOUBLE_EQ(performance_data["connection_success_rate"], 2.0/3.0);
    EXPECT_DOUBLE_EQ(performance_data["error_rate"], 0.25);  // 1 error out of 4 queries
    
    // Check query distribution
    EXPECT_DOUBLE_EQ(performance_data["fast_query_percentage"], 25.0);  // 1 out of 4
    EXPECT_DOUBLE_EQ(performance_data["slow_query_percentage"], 50.0);  // 2 out of 4
    
    // Test reset
    metrics.reset();
    auto reset_data = metrics.get_metrics();
    EXPECT_DOUBLE_EQ(reset_data["avg_query_time_ms"], 0.0);
    EXPECT_DOUBLE_EQ(reset_data["error_rate"], 0.0);
}

// Test database health monitor
TEST_F(DatabaseOptimizationTest, DatabaseHealthMonitor) {
    DatabaseHealthMonitor monitor(100ms);  // Fast interval for testing
    
    // Add a healthy database
    monitor.add_database("test_db", []() {
        HealthReport report;
        report.status = DatabaseHealth::HEALTHY;
        report.database_type = "test";
        return report;
    });
    
    // Add an unhealthy database
    monitor.add_database("failing_db", []() {
        HealthReport report;
        report.status = DatabaseHealth::CRITICAL;
        report.database_type = "test";
        report.errors.push_back("Connection failed");
        return report;
    });
    
    // Start monitoring
    monitor.start();
    
    // Wait for health checks to run
    std::this_thread::sleep_for(200ms);
    
    // Check health reports
    auto reports = monitor.get_health_reports();
    EXPECT_EQ(reports.size(), 2);
    
    auto healthy_report = monitor.get_health_report("test_db");
    EXPECT_EQ(healthy_report.status, DatabaseHealth::HEALTHY);
    EXPECT_TRUE(healthy_report.is_healthy());
    
    auto unhealthy_report = monitor.get_health_report("failing_db");
    EXPECT_EQ(unhealthy_report.status, DatabaseHealth::CRITICAL);
    EXPECT_FALSE(unhealthy_report.is_healthy());
    
    // Test overall health
    EXPECT_FALSE(monitor.is_all_healthy());
    
    // Stop monitoring
    monitor.stop();
}

// Test database profiler
TEST_F(DatabaseOptimizationTest, DatabaseProfiler) {
    DatabaseProfiler profiler;
    
    // Record some queries
    profiler.record_query("SELECT * FROM users", 10ms);
    profiler.record_query("SELECT * FROM users", 15ms);
    profiler.record_query("SELECT * FROM users", 20ms);
    profiler.record_query("INSERT INTO logs VALUES (?)", 5ms);
    profiler.record_query("UPDATE users SET last_login = NOW()", 100ms);
    
    // Get top queries
    auto top_queries = profiler.get_top_queries(3);
    
    EXPECT_LE(top_queries.size(), 3);
    EXPECT_GT(top_queries.size(), 0);
    
    // Find the SELECT query (should have highest total time)
    auto select_query = std::find_if(top_queries.begin(), top_queries.end(),
        [](const DatabaseProfiler::QueryProfile& profile) {
            return profile.query.find("SELECT * FROM users") != std::string::npos;
        });
    
    EXPECT_NE(select_query, top_queries.end());
    EXPECT_EQ(select_query->execution_count, 3);
    EXPECT_DOUBLE_EQ(select_query->get_average_time_ms(), 15.0);  // (10+15+20)/3
    
    // Test clear
    profiler.clear();
    auto empty_queries = profiler.get_top_queries();
    EXPECT_TRUE(empty_queries.empty());
}

// Test connection health checking
TEST_F(DatabaseOptimizationTest, ConnectionHealthChecking) {
    auto conn = std::make_unique<MockConnection>(true);
    MockDatabaseConnection db_conn(conn.get(), std::chrono::steady_clock::now());
    
    // Test healthy connection
    EXPECT_TRUE(db_conn.is_healthy());
    EXPECT_EQ(db_conn.check_health(), ConnectionHealth::HEALTHY);
    
    // Test unhealthy connection
    conn->set_healthy(false);
    EXPECT_FALSE(db_conn.is_healthy());
    EXPECT_EQ(db_conn.check_health(), ConnectionHealth::UNHEALTHY);
    
    // Test connection reset
    db_conn.reset_connection();
    EXPECT_TRUE(db_conn.is_healthy());
    
    // Test connection aging
    EXPECT_FALSE(db_conn.is_expired(1h));  // Should not be expired
    EXPECT_FALSE(db_conn.is_idle(1h));     // Should not be idle
    
    // Test use count
    EXPECT_EQ(db_conn.get_use_count(), 0);
    db_conn.update_last_used();
    EXPECT_EQ(db_conn.get_use_count(), 1);
}

// Test alert callback functionality
TEST_F(DatabaseOptimizationTest, AlertCallback) {
    DatabaseHealthMonitor monitor(50ms);
    
    std::vector<std::pair<std::string, DatabaseHealth>> alerts;
    
    // Set alert callback
    monitor.set_alert_callback([&alerts](const std::string& name, const HealthReport& report) {
        alerts.emplace_back(name, report.status);
    });
    
    // Add a database that will become unhealthy
    bool is_healthy = true;
    monitor.add_database("test_db", [&is_healthy]() {
        HealthReport report;
        report.status = is_healthy ? DatabaseHealth::HEALTHY : DatabaseHealth::CRITICAL;
        report.database_type = "test";
        if (!is_healthy) {
            report.errors.push_back("Simulated failure");
        }
        return report;
    });
    
    monitor.start();
    
    // Wait for initial health check
    std::this_thread::sleep_for(100ms);
    
    // Make database unhealthy
    is_healthy = false;
    
    // Wait for health check to detect the change
    std::this_thread::sleep_for(100ms);
    
    monitor.stop();
    
    // Should have received an alert
    EXPECT_GT(alerts.size(), 0);
    if (!alerts.empty()) {
        EXPECT_EQ(alerts.back().first, "test_db");
        EXPECT_EQ(alerts.back().second, DatabaseHealth::CRITICAL);
    }
}
