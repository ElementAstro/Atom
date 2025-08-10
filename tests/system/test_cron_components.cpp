/*
 * test_cron_components.cpp
 *
 * Comprehensive tests for the enhanced crontab system components
 * Tests cron_cache, cron_monitor, cron_security, and other modular components
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <memory>
#include <string>
#include <vector>

#include "atom/system/crontab/cron_cache.hpp"
#include "atom/system/crontab/cron_monitor.hpp"
#include "atom/system/crontab/cron_security.hpp"
#include "atom/system/crontab/cron_config.hpp"
#include "atom/system/crontab/cron_job.hpp"
#include "atom/system/crontab/cron_manager.hpp"

using namespace testing;
using namespace std::chrono_literals;

class CronComponentsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
        testJobId = "test_job_001";
        testCommand = "echo 'test output'";
        testSchedule = "0 0 * * *";  // Daily at midnight
        
        // Create test job
        testJob = std::make_shared<CronJob>(testSchedule, testCommand, true, "test", "Test job");
        testJob->id_ = testJobId;
    }

    void TearDown() override {
        // Cleanup
    }

    std::string testJobId;
    std::string testCommand;
    std::string testSchedule;
    std::shared_ptr<CronJob> testJob;
};

// Test CronCache functionality
class CronCacheTest : public CronComponentsTest {
protected:
    void SetUp() override {
        CronComponentsTest::SetUp();
        cache = std::make_unique<CronCache>();
    }

    std::unique_ptr<CronCache> cache;
};

TEST_F(CronCacheTest, BasicCacheOperations) {
    std::string key = "test_key";
    std::string value = "test_value";
    
    // Test put and get
    cache->put(key, value, 1000ms);
    
    auto retrieved = cache->get<std::string>(key);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value(), value);
}

TEST_F(CronCacheTest, CacheExpiration) {
    std::string key = "expiring_key";
    std::string value = "expiring_value";
    
    // Put with short TTL
    cache->put(key, value, 100ms);
    
    // Should be available immediately
    auto retrieved = cache->get<std::string>(key);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value(), value);
    
    // Wait for expiration
    std::this_thread::sleep_for(150ms);
    
    // Should be expired
    auto expired = cache->get<std::string>(key);
    EXPECT_FALSE(expired.has_value());
}

TEST_F(CronCacheTest, CacheStatistics) {
    // Add some entries
    cache->put("key1", "value1", 1000ms);
    cache->put("key2", "value2", 1000ms);
    cache->put("key3", "value3", 1000ms);
    
    auto stats = cache->getStatistics();
    EXPECT_EQ(stats.total_entries, 3);
    EXPECT_EQ(stats.hits, 0);  // No gets yet
    
    // Access some entries
    cache->get<std::string>("key1");
    cache->get<std::string>("key2");
    cache->get<std::string>("nonexistent");
    
    stats = cache->getStatistics();
    EXPECT_EQ(stats.hits, 2);
    EXPECT_EQ(stats.misses, 1);
}

TEST_F(CronCacheTest, CacheClearAndCleanup) {
    // Add entries
    cache->put("key1", "value1", 100ms);
    cache->put("key2", "value2", 1000ms);
    
    EXPECT_EQ(cache->size(), 2);
    
    // Clear all
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
    
    // Add entries with different TTLs
    cache->put("short", "value", 50ms);
    cache->put("long", "value", 1000ms);
    
    // Wait for short TTL to expire
    std::this_thread::sleep_for(100ms);
    
    // Cleanup expired entries
    cache->cleanup();
    EXPECT_EQ(cache->size(), 1);  // Only long-lived entry should remain
}

// Test CronMonitor functionality
class CronMonitorTest : public CronComponentsTest {
protected:
    void SetUp() override {
        CronComponentsTest::SetUp();
        monitor = std::make_unique<CronMonitor>();
    }

    std::unique_ptr<CronMonitor> monitor;
};

TEST_F(CronMonitorTest, EventLogging) {
    std::vector<MonitorEvent> capturedEvents;
    
    // Set up event handler
    monitor->setEventHandler([&capturedEvents](const MonitorEvent& event) {
        capturedEvents.push_back(event);
    });
    
    // Log some events
    monitor->logJobStarted(testJobId, "Test job started");
    monitor->logJobCompleted(testJobId, "Test job completed", 1000ms);
    monitor->logJobFailed(testJobId, "Test job failed", "Error message");
    
    EXPECT_EQ(capturedEvents.size(), 3);
    EXPECT_EQ(capturedEvents[0].type, MonitorEventType::JOB_STARTED);
    EXPECT_EQ(capturedEvents[1].type, MonitorEventType::JOB_COMPLETED);
    EXPECT_EQ(capturedEvents[2].type, MonitorEventType::JOB_FAILED);
}

TEST_F(CronMonitorTest, PerformanceMetrics) {
    // Start monitoring
    monitor->startJobExecution(testJobId);
    
    // Simulate some work
    std::this_thread::sleep_for(100ms);
    
    // End monitoring
    monitor->endJobExecution(testJobId, true);
    
    auto metrics = monitor->getJobMetrics(testJobId);
    ASSERT_TRUE(metrics.has_value());
    EXPECT_GT(metrics->execution_time.count(), 90);  // Should be around 100ms
    EXPECT_EQ(metrics->success_count, 1);
    EXPECT_EQ(metrics->failure_count, 0);
}

TEST_F(CronMonitorTest, AlertThresholds) {
    std::vector<MonitorEvent> alerts;
    
    monitor->setEventHandler([&alerts](const MonitorEvent& event) {
        if (event.severity >= EventSeverity::WARNING) {
            alerts.push_back(event);
        }
    });
    
    // Configure thresholds
    monitor->setFailureThreshold(testJobId, 2);
    monitor->setExecutionTimeThreshold(testJobId, 50ms);
    
    // Trigger failure threshold
    monitor->logJobFailed(testJobId, "Failure 1", "Error");
    monitor->logJobFailed(testJobId, "Failure 2", "Error");
    monitor->logJobFailed(testJobId, "Failure 3", "Error");  // Should trigger alert
    
    // Trigger execution time threshold
    monitor->startJobExecution(testJobId);
    std::this_thread::sleep_for(100ms);  // Longer than threshold
    monitor->endJobExecution(testJobId, true);
    
    EXPECT_GE(alerts.size(), 1);  // Should have at least one alert
}

// Test CronSecurity functionality
class CronSecurityTest : public CronComponentsTest {
protected:
    void SetUp() override {
        CronComponentsTest::SetUp();
        security = std::make_unique<CronSecurity>();
        
        // Create test security context
        testContext = std::make_shared<SecurityContext>("test_user", "test_session");
        testContext->roles.push_back("cron_user");
    }

    std::unique_ptr<CronSecurity> security;
    std::shared_ptr<SecurityContext> testContext;
};

TEST_F(CronSecurityTest, PermissionChecking) {
    // Grant permissions
    security->grantPermission("test_user", "cron_jobs", PermissionLevel::WRITE);
    
    // Test permission checking
    EXPECT_TRUE(security->hasPermission(*testContext, "cron_jobs", PermissionLevel::READ));
    EXPECT_TRUE(security->hasPermission(*testContext, "cron_jobs", PermissionLevel::WRITE));
    EXPECT_FALSE(security->hasPermission(*testContext, "cron_jobs", PermissionLevel::ADMIN));
}

TEST_F(CronSecurityTest, CommandValidation) {
    // Configure allowed commands
    std::vector<std::string> allowedCommands = {"echo", "ls", "date"};
    security->setAllowedCommands(allowedCommands);
    
    // Test command validation
    EXPECT_TRUE(security->validateCommand("echo 'hello'", *testContext));
    EXPECT_TRUE(security->validateCommand("ls -la", *testContext));
    EXPECT_FALSE(security->validateCommand("rm -rf /", *testContext));
    EXPECT_FALSE(security->validateCommand("sudo something", *testContext));
}

TEST_F(CronSecurityTest, ResourceLimits) {
    ResourceLimits limits;
    limits.max_memory_mb = 512;
    limits.max_cpu_time = 60s;
    limits.max_wall_time = 120s;
    
    security->setResourceLimits("test_user", limits);
    
    auto userLimits = security->getResourceLimits("test_user");
    ASSERT_TRUE(userLimits.has_value());
    EXPECT_EQ(userLimits->max_memory_mb, 512);
    EXPECT_EQ(userLimits->max_cpu_time, 60s);
}

TEST_F(CronSecurityTest, SecurityAuditLog) {
    std::vector<SecurityAuditEntry> auditLog;
    
    security->setAuditHandler([&auditLog](const SecurityAuditEntry& entry) {
        auditLog.push_back(entry);
    });
    
    // Perform some operations that should be audited
    security->validateCommand("echo test", *testContext);
    security->hasPermission(*testContext, "cron_jobs", PermissionLevel::WRITE);
    
    EXPECT_GE(auditLog.size(), 2);
}

// Integration tests for multiple components
class CronIntegrationTest : public CronComponentsTest {
protected:
    void SetUp() override {
        CronComponentsTest::SetUp();
        
        // Create integrated system
        cache = std::make_unique<CronCache>();
        monitor = std::make_unique<CronMonitor>();
        security = std::make_unique<CronSecurity>();
        
        // Setup security
        testContext = std::make_shared<SecurityContext>("test_user", "test_session");
        security->grantPermission("test_user", "cron_jobs", PermissionLevel::WRITE);
    }

    std::unique_ptr<CronCache> cache;
    std::unique_ptr<CronMonitor> monitor;
    std::unique_ptr<CronSecurity> security;
    std::shared_ptr<SecurityContext> testContext;
};

TEST_F(CronIntegrationTest, EndToEndJobExecution) {
    // Cache job configuration
    cache->put("job_config_" + testJobId, *testJob, 3600s);
    
    // Validate security
    EXPECT_TRUE(security->validateCommand(testCommand, *testContext));
    
    // Start monitoring
    monitor->startJobExecution(testJobId);
    
    // Simulate job execution
    std::this_thread::sleep_for(50ms);
    
    // End monitoring
    monitor->endJobExecution(testJobId, true);
    
    // Verify cached data
    auto cachedJob = cache->get<CronJob>("job_config_" + testJobId);
    ASSERT_TRUE(cachedJob.has_value());
    EXPECT_EQ(cachedJob->command_, testCommand);
    
    // Verify monitoring data
    auto metrics = monitor->getJobMetrics(testJobId);
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->success_count, 1);
}

TEST_F(CronIntegrationTest, SecurityViolationHandling) {
    std::vector<MonitorEvent> securityEvents;
    
    monitor->setEventHandler([&securityEvents](const MonitorEvent& event) {
        if (event.type == MonitorEventType::SYSTEM_ERROR) {
            securityEvents.push_back(event);
        }
    });
    
    // Try to execute unauthorized command
    std::string maliciousCommand = "rm -rf /";
    bool isValid = security->validateCommand(maliciousCommand, *testContext);
    
    if (!isValid) {
        monitor->logSystemError("Security violation: unauthorized command", 
                               EventSeverity::CRITICAL);
    }
    
    EXPECT_FALSE(isValid);
    EXPECT_GE(securityEvents.size(), 1);
}
