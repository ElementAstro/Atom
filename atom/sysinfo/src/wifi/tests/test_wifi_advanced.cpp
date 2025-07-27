/*
 * test_wifi_advanced.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: Advanced WiFi Module Tests - Monitoring, Quality Analysis, Performance

**************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>

#include "../monitor.hpp"
#include "../quality.hpp"
#include "../config.hpp"
#include "../error_handler.hpp"

using namespace atom::system;
using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

class WiFiAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize all systems
        initializeErrorHandling();
        initializeConfiguration();
        initializeNetworkMonitoring();
    }

    void TearDown() override {
        // Cleanup
        shutdownNetworkMonitoring();
        shutdownConfiguration();
        shutdownErrorHandling();
    }
};

// Test Network Monitoring
TEST_F(WiFiAdvancedTest, TestNetworkMonitor) {
    auto& monitor = getGlobalNetworkMonitor();

    // Test monitor start/stop
    EXPECT_TRUE(monitor.start());
    EXPECT_TRUE(monitor.isRunning());

    // Wait a bit for monitoring to collect data
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test getting current stats
    auto current_stats = monitor.getCurrentStats();
    EXPECT_GE(current_stats.downloadSpeed, 0.0);
    EXPECT_GE(current_stats.uploadSpeed, 0.0);

    // Test getting stats history
    auto history = monitor.getStatsHistory(std::chrono::minutes(1));
    EXPECT_FALSE(history.empty());

    // Test average stats
    auto avg_stats = monitor.getAverageStats(std::chrono::minutes(1));
    EXPECT_GE(avg_stats.downloadSpeed, 0.0);

    // Test peak stats
    auto peak_stats = monitor.getPeakStats(std::chrono::minutes(1));
    EXPECT_GE(peak_stats.downloadSpeed, 0.0);

    // Test event callback registration
    bool callback_called = false;
    monitor.registerEventCallback([&callback_called](const NetworkEventData& event) {
        callback_called = true;
    });

    // Stop monitoring
    monitor.stop();
    EXPECT_FALSE(monitor.isRunning());
}

// Test Network Quality Analysis
TEST_F(WiFiAdvancedTest, TestNetworkQualityAnalyzer) {
    NetworkQualityAnalyzer analyzer;

    // Test jitter measurement
    JitterConfig jitter_config;
    jitter_config.packet_count = 5; // Reduced for faster testing
    jitter_config.target_host = "8.8.8.8";

    double jitter = analyzer.measureJitter(jitter_config);
    if (jitter >= 0) {
        EXPECT_GE(jitter, 0.0);
        EXPECT_LT(jitter, 1000.0); // Should be reasonable
    }

    // Test packet loss measurement
    PacketLossConfig packet_loss_config;
    packet_loss_config.packet_count = 10; // Reduced for faster testing
    packet_loss_config.target_host = "8.8.8.8";

    double packet_loss = analyzer.measurePacketLoss(packet_loss_config);
    if (packet_loss >= 0) {
        EXPECT_GE(packet_loss, 0.0);
        EXPECT_LE(packet_loss, 100.0);
    }

    // Test signal quality calculation
    double signal_quality_excellent = NetworkQualityAnalyzer::calculateSignalQuality(-30.0);
    double signal_quality_poor = NetworkQualityAnalyzer::calculateSignalQuality(-80.0);

    EXPECT_GT(signal_quality_excellent, signal_quality_poor);
    EXPECT_GE(signal_quality_excellent, 0.0);
    EXPECT_LE(signal_quality_excellent, 1.0);
    EXPECT_GE(signal_quality_poor, 0.0);
    EXPECT_LE(signal_quality_poor, 1.0);

    // Test comprehensive analysis
    QualityMetrics metrics = analyzer.performComprehensiveAnalysis();

    EXPECT_GE(metrics.overall_quality, 0.0);
    EXPECT_LE(metrics.overall_quality, 1.0);

    // Test quality level determination
    auto quality_level = NetworkQualityAnalyzer::getQualityLevel(metrics);
    EXPECT_TRUE(quality_level == QualityLevel::EXCELLENT ||
                quality_level == QualityLevel::GOOD ||
                quality_level == QualityLevel::FAIR ||
                quality_level == QualityLevel::POOR ||
                quality_level == QualityLevel::CRITICAL);

    // Test quality report generation
    std::string report = NetworkQualityAnalyzer::generateQualityReport(metrics);
    EXPECT_FALSE(report.empty());

    // Test quality recommendations
    auto recommendations = NetworkQualityAnalyzer::getQualityRecommendations(metrics);
    EXPECT_FALSE(recommendations.empty());
}

// Test Network Quality Monitor
TEST_F(WiFiAdvancedTest, TestNetworkQualityMonitor) {
    NetworkQualityMonitor quality_monitor(std::chrono::seconds(2)); // Fast interval for testing

    // Test monitor start/stop
    EXPECT_TRUE(quality_monitor.start());
    EXPECT_TRUE(quality_monitor.isRunning());

    // Wait for some quality assessments
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Test getting latest metrics
    auto latest_metrics = quality_monitor.getLatestMetrics();
    if (latest_metrics.has_value()) {
        EXPECT_GE(latest_metrics->overall_quality, 0.0);
        EXPECT_LE(latest_metrics->overall_quality, 1.0);
    }

    // Test getting quality history
    auto history = quality_monitor.getQualityHistory(std::chrono::hours(1));
    EXPECT_FALSE(history.empty());

    // Test alert callback
    bool alert_triggered = false;
    quality_monitor.setQualityAlertCallback([&alert_triggered](const QualityMetrics& metrics, QualityLevel level) {
        alert_triggered = true;
    });
    quality_monitor.setAlertThreshold(QualityLevel::CRITICAL);

    // Stop monitoring
    quality_monitor.stop();
    EXPECT_FALSE(quality_monitor.isRunning());
}

// Test Performance Monitoring
TEST_F(WiFiAdvancedTest, TestPerformanceMonitor) {
    auto& perf_monitor = getGlobalPerformanceMonitor();

    // Test timing recording
    perf_monitor.recordTiming("test_operation", std::chrono::microseconds(1000));
    perf_monitor.recordTiming("test_operation", std::chrono::microseconds(1500));
    perf_monitor.recordTiming("test_operation", std::chrono::microseconds(800));

    // Test memory usage recording
    perf_monitor.recordMemoryUsage("test_operation", 1024);
    perf_monitor.recordMemoryUsage("test_operation", 2048);

    // Test cache access recording
    perf_monitor.recordCacheAccess("test_cache", true);
    perf_monitor.recordCacheAccess("test_cache", true);
    perf_monitor.recordCacheAccess("test_cache", false);

    // Test getting performance statistics
    std::string stats = perf_monitor.getPerformanceStats(std::chrono::hours(1));
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("test_operation"), std::string::npos);

    // Test cache hit ratio
    double hit_ratio = perf_monitor.getCacheHitRatio("test_cache", std::chrono::hours(1));
    EXPECT_GT(hit_ratio, 0.0);
    EXPECT_LE(hit_ratio, 1.0);
    EXPECT_DOUBLE_EQ(hit_ratio, 2.0/3.0); // 2 hits out of 3 accesses

    // Test average operation time
    auto avg_time = perf_monitor.getAverageOperationTime("test_operation", std::chrono::hours(1));
    EXPECT_GT(avg_time.count(), 0);
    EXPECT_DOUBLE_EQ(avg_time.count(), (1000 + 1500 + 800) / 3.0);
}

// Test Performance Timer
TEST_F(WiFiAdvancedTest, TestPerformanceTimer) {
    auto& perf_monitor = getGlobalPerformanceMonitor();

    // Test automatic timing with RAII
    {
        PerformanceTimer timer("timed_operation", perf_monitor);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // Timer destructor should record the timing

    // Verify timing was recorded
    auto avg_time = perf_monitor.getAverageOperationTime("timed_operation", std::chrono::hours(1));
    EXPECT_GT(avg_time.count(), 9000); // Should be at least 9ms (allowing for some variance)
}

// Test Configuration Advanced Features
TEST_F(WiFiAdvancedTest, TestAdvancedConfiguration) {
    auto& config_manager = getGlobalConfigManager();

    // Test configuration change callbacks
    bool callback_called = false;
    config_manager.registerChangeCallback([&callback_called](const WiFiConfig& wifi_config, const PerformanceConfig& perf_config) {
        callback_called = true;
    });

    // Trigger configuration change
    WiFiConfig new_config = config_manager.getWiFiConfig();
    new_config.ping_timeout = std::chrono::milliseconds(6000);
    config_manager.updateWiFiConfig(new_config);

    EXPECT_TRUE(callback_called);

    // Test auto-tuning
    config_manager.autoTune();
    auto tuned_config = config_manager.getWiFiConfig();
    EXPECT_GT(tuned_config.max_concurrent_operations, 0);

    // Test configuration validation
    WiFiConfig invalid_config;
    invalid_config.ping_timeout = std::chrono::milliseconds(-1000); // Invalid
    invalid_config.max_cache_size = 0; // Invalid
    config_manager.updateWiFiConfig(invalid_config);

    auto validation_errors = config_manager.validateConfiguration();
    EXPECT_FALSE(validation_errors.empty());
}

// Test Error Handler Advanced Features
TEST_F(WiFiAdvancedTest, TestAdvancedErrorHandling) {
    auto& error_handler = getGlobalErrorHandler();

    // Test error callback registration
    bool callback_called = false;
    std::string callback_message;
    error_handler.registerErrorCallback([&callback_called, &callback_message](const ErrorEvent& event) {
        callback_called = true;
        callback_message = event.error_message;
    });

    // Log an error
    error_handler.logError(WiFiError::NETWORK_INTERFACE_ERROR, "Test callback error", "test_function", __FILE__, __LINE__);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_message, "Test callback error");

    // Test error rate calculation
    for (int i = 0; i < 5; ++i) {
        error_handler.logError(WiFiError::TIMEOUT, "Rate test error", "test_function", __FILE__, __LINE__);
    }

    double error_rate = error_handler.getErrorRate(WiFiError::TIMEOUT, std::chrono::hours(1));
    EXPECT_GT(error_rate, 0.0);

    // Test recent error checking
    EXPECT_TRUE(error_handler.hasRecentError(WiFiError::TIMEOUT, std::chrono::minutes(1)));
    EXPECT_FALSE(error_handler.hasRecentError(WiFiError::MEMORY_ALLOCATION_FAILED, std::chrono::minutes(1)));
}

// Test WiFi Result wrapper
TEST_F(WiFiAdvancedTest, TestWiFiResult) {
    // Test successful result
    WiFiResult<std::string> success_result("test_value");
    EXPECT_TRUE(success_result.isSuccess());
    EXPECT_FALSE(success_result.isError());
    EXPECT_EQ(success_result.getValue(), "test_value");
    EXPECT_EQ(success_result.getValueOr("default"), "test_value");

    // Test error result
    WiFiResult<std::string> error_result(WiFiError::TIMEOUT, "Timeout occurred");
    EXPECT_FALSE(error_result.isSuccess());
    EXPECT_TRUE(error_result.isError());
    EXPECT_EQ(error_result.getErrorCode(), WiFiError::TIMEOUT);
    EXPECT_EQ(error_result.getErrorMessage(), "Timeout occurred");
    EXPECT_EQ(error_result.getValueOr("default"), "default");

    // Test exception throwing
    EXPECT_THROW(error_result.getValue(), WiFiException);

    // Test result transformation
    auto transformed = success_result.map<int>([](const std::string& s) { return static_cast<int>(s.length()); });
    EXPECT_TRUE(transformed.isSuccess());
    EXPECT_EQ(transformed.getValue(), 10); // "test_value" has 10 characters

    auto error_transformed = error_result.map<int>([](const std::string& s) { return static_cast<int>(s.length()); });
    EXPECT_TRUE(error_transformed.isError());
    EXPECT_EQ(error_transformed.getErrorCode(), WiFiError::TIMEOUT);
}

// Test quality level string conversion
TEST_F(WiFiAdvancedTest, TestQualityLevelConversion) {
    EXPECT_EQ(qualityLevelToString(QualityLevel::EXCELLENT), "Excellent");
    EXPECT_EQ(qualityLevelToString(QualityLevel::GOOD), "Good");
    EXPECT_EQ(qualityLevelToString(QualityLevel::FAIR), "Fair");
    EXPECT_EQ(qualityLevelToString(QualityLevel::POOR), "Poor");
    EXPECT_EQ(qualityLevelToString(QualityLevel::CRITICAL), "Critical");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
