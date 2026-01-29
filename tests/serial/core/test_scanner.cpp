/*
 * test_scanner.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-16

Description: Comprehensive Unit Tests for SerialPortScanner
Tests serial port scanning, CH340 detection, caching, and monitoring.

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/serial/scanner.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using atom::serial::CacheEntry;
using atom::serial::ScannerConfig;
using atom::serial::ScannerError;
using atom::serial::ScannerStats;
using atom::serial::SerialPortScanner;

// =============================================================================
// ScannerStats Tests
// =============================================================================

class ScannerStatsTest : public ::testing::Test {
protected:
    void SetUp() override { stats = std::make_unique<ScannerStats>(); }

    std::unique_ptr<ScannerStats> stats;
};

TEST_F(ScannerStatsTest, DefaultConstruction) {
    EXPECT_EQ(stats->total_scans.load(), 0);
    EXPECT_EQ(stats->successful_scans.load(), 0);
    EXPECT_EQ(stats->failed_scans.load(), 0);
    EXPECT_EQ(stats->ports_found.load(), 0);
    EXPECT_EQ(stats->ch340_devices_found.load(), 0);
    EXPECT_EQ(stats->scan_errors.load(), 0);
    EXPECT_EQ(stats->cache_hits.load(), 0);
    EXPECT_EQ(stats->cache_misses.load(), 0);
}

TEST_F(ScannerStatsTest, CopyConstruction) {
    stats->total_scans = 10;
    stats->successful_scans = 8;
    stats->failed_scans = 2;
    stats->ports_found = 5;

    ScannerStats copied(*stats);

    EXPECT_EQ(copied.total_scans.load(), 10);
    EXPECT_EQ(copied.successful_scans.load(), 8);
    EXPECT_EQ(copied.failed_scans.load(), 2);
    EXPECT_EQ(copied.ports_found.load(), 5);
}

TEST_F(ScannerStatsTest, Reset) {
    stats->total_scans = 100;
    stats->successful_scans = 90;
    stats->cache_hits = 50;

    stats->reset();

    EXPECT_EQ(stats->total_scans.load(), 0);
    EXPECT_EQ(stats->successful_scans.load(), 0);
    EXPECT_EQ(stats->cache_hits.load(), 0);
}

TEST_F(ScannerStatsTest, GetAverageScanTime) {
    // No scans - should return 0
    EXPECT_DOUBLE_EQ(stats->get_average_scan_time(), 0.0);

    // With scans
    stats->total_scans = 10;
    stats->total_scan_time = 1000;  // 1000 microseconds total

    EXPECT_DOUBLE_EQ(stats->get_average_scan_time(), 100.0);
}

TEST_F(ScannerStatsTest, GetSuccessRate) {
    // No scans - should return 0
    EXPECT_DOUBLE_EQ(stats->get_success_rate(), 0.0);

    // With scans
    stats->total_scans = 10;
    stats->successful_scans = 8;

    EXPECT_DOUBLE_EQ(stats->get_success_rate(), 80.0);
}

TEST_F(ScannerStatsTest, GetCacheHitRate) {
    // No cache operations - should return 0
    EXPECT_DOUBLE_EQ(stats->get_cache_hit_rate(), 0.0);

    // With cache operations
    stats->cache_hits = 7;
    stats->cache_misses = 3;

    EXPECT_DOUBLE_EQ(stats->get_cache_hit_rate(), 70.0);
}

// =============================================================================
// CacheEntry Tests
// =============================================================================

class CacheEntryTest : public ::testing::Test {
protected:
    void SetUp() override {
        entry.timestamp = std::chrono::steady_clock::now();
        entry.port_path = "/dev/ttyUSB0";
        entry.is_available = true;
        entry.access_count = 0;
    }

    CacheEntry entry;
};

TEST_F(CacheEntryTest, IsExpiredFresh) {
    // Just created - should not be expired
    EXPECT_FALSE(entry.is_expired(1000ms));
}

TEST_F(CacheEntryTest, IsExpiredOld) {
    // Set timestamp to the past
    entry.timestamp =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(2000);

    EXPECT_TRUE(entry.is_expired(1000ms));
}

TEST_F(CacheEntryTest, IsExpiredBoundary) {
    // Set timestamp exactly at TTL boundary
    entry.timestamp =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1000);

    // Should be expired at exactly TTL
    EXPECT_TRUE(entry.is_expired(1000ms));
}

// =============================================================================
// ScannerConfig Tests
// =============================================================================

class ScannerConfigTest : public ::testing::Test {
protected:
    ScannerConfig config;
};

TEST_F(ScannerConfigTest, DefaultValues) {
    EXPECT_TRUE(config.detect_ch340);
    EXPECT_TRUE(config.include_virtual_ports);
    EXPECT_FALSE(config.enable_bluetooth_scan);
    EXPECT_TRUE(config.enable_usb_scan);
    EXPECT_EQ(config.scan_timeout, 5000ms);
    EXPECT_EQ(config.retry_interval, 1000ms);
    EXPECT_EQ(config.cache_ttl, 30000ms);
    EXPECT_EQ(config.max_retry_count, 3);
    EXPECT_EQ(config.max_cache_size, 1000);
    EXPECT_EQ(config.max_concurrent_scans, 4);
}

TEST_F(ScannerConfigTest, IsValidDefault) { EXPECT_TRUE(config.is_valid()); }

TEST_F(ScannerConfigTest, IsValidInvalidTimeout) {
    config.scan_timeout = 0ms;
    EXPECT_FALSE(config.is_valid());
}

TEST_F(ScannerConfigTest, IsValidInvalidRetryInterval) {
    config.retry_interval = 0ms;
    EXPECT_FALSE(config.is_valid());
}

TEST_F(ScannerConfigTest, IsValidInvalidCacheTtl) {
    config.cache_ttl = 0ms;
    EXPECT_FALSE(config.is_valid());
}

TEST_F(ScannerConfigTest, IsValidInvalidMaxRetryCount) {
    config.max_retry_count = 0;
    EXPECT_FALSE(config.is_valid());
}

TEST_F(ScannerConfigTest, IsValidInvalidMaxCacheSize) {
    config.max_cache_size = 0;
    EXPECT_FALSE(config.is_valid());
}

TEST_F(ScannerConfigTest, IsValidInvalidConcurrentScans) {
    config.max_concurrent_scans = 0;
    EXPECT_FALSE(config.is_valid());
}

// =============================================================================
// ScannerError Tests
// =============================================================================

TEST(ScannerErrorTest, ConstructWithString) {
    ScannerError error("Test error message");
    EXPECT_STREQ(error.what(), "Test error message");
}

TEST(ScannerErrorTest, ConstructWithCString) {
    ScannerError error("C-string error");
    EXPECT_STREQ(error.what(), "C-string error");
}

// =============================================================================
// SerialPortScanner Tests
// =============================================================================

class SerialPortScannerTest : public ::testing::Test {
protected:
    void SetUp() override { scanner = std::make_unique<SerialPortScanner>(); }

    void TearDown() override {
        if (scanner && scanner->is_monitoring()) {
            scanner->stop_monitoring();
        }
    }

    // Helper to extract vector from Result variant
    std::vector<SerialPortScanner::PortInfo> getPortsFromResult(
        const SerialPortScanner::Result<
            std::vector<SerialPortScanner::PortInfo>>& result) {
        if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(
                result)) {
            return std::get<std::vector<SerialPortScanner::PortInfo>>(result);
        }
        return {};
    }

    // Helper to extract optional from Result variant
    std::optional<SerialPortScanner::PortDetails> getDetailsFromResult(
        const SerialPortScanner::Result<
            std::optional<SerialPortScanner::PortDetails>>& result) {
        if (std::holds_alternative<
                std::optional<SerialPortScanner::PortDetails>>(result)) {
            return std::get<std::optional<SerialPortScanner::PortDetails>>(
                result);
        }
        return std::nullopt;
    }

    // Helper to check if result contains error
    template <typename T>
    bool hasError(const SerialPortScanner::Result<T>& result) {
        return std::holds_alternative<SerialPortScanner::ErrorInfo>(result);
    }

    // Helper to get error from result
    template <typename T>
    SerialPortScanner::ErrorInfo getError(
        const SerialPortScanner::Result<T>& result) {
        return std::get<SerialPortScanner::ErrorInfo>(result);
    }

    std::unique_ptr<SerialPortScanner> scanner;
};

// Constructor Tests
TEST_F(SerialPortScannerTest, DefaultConstruction) {
    EXPECT_FALSE(scanner->is_monitoring());
    auto config = scanner->get_config();
    EXPECT_TRUE(config.is_valid());
}

TEST_F(SerialPortScannerTest, ConstructionWithConfig) {
    ScannerConfig customConfig;
    customConfig.scan_timeout = 10000ms;
    customConfig.detect_ch340 = false;

    SerialPortScanner customScanner(customConfig);
    auto retrievedConfig = customScanner.get_config();

    EXPECT_EQ(retrievedConfig.scan_timeout, 10000ms);
    EXPECT_FALSE(retrievedConfig.detect_ch340);
}

// Configuration Tests
TEST_F(SerialPortScannerTest, SetConfig) {
    ScannerConfig newConfig;
    newConfig.scan_timeout = 8000ms;
    newConfig.max_retry_count = 5;

    scanner->set_config(newConfig);
    auto retrievedConfig = scanner->get_config();

    EXPECT_EQ(retrievedConfig.scan_timeout, 8000ms);
    EXPECT_EQ(retrievedConfig.max_retry_count, 5);
}

// CH340 Detection Tests
TEST_F(SerialPortScannerTest, IsCH340DeviceKnownVidPid) {
    // Known CH340 VID/PID: 0x1a86/0x7523
    auto result = scanner->is_ch340_device(0x1a86, 0x7523, "USB Serial");
    EXPECT_TRUE(result.first);
    EXPECT_FALSE(result.second.empty());
}

TEST_F(SerialPortScannerTest, IsCH340DeviceKnownVidPid5523) {
    // Another known CH340 PID
    auto result = scanner->is_ch340_device(0x1a86, 0x5523, "Serial Converter");
    EXPECT_TRUE(result.first);
}

TEST_F(SerialPortScannerTest, IsCH340DeviceByDescription) {
    // Unknown VID/PID but description contains CH340
    auto result = scanner->is_ch340_device(0xFFFF, 0xFFFF, "USB CH340 Device");
    EXPECT_TRUE(result.first);
    EXPECT_THAT(result.second, ::testing::HasSubstr("CH340"));
}

TEST_F(SerialPortScannerTest, IsCH340DeviceCH341) {
    auto result = scanner->is_ch340_device(0xFFFF, 0xFFFF, "CH341 Serial");
    EXPECT_TRUE(result.first);
    EXPECT_THAT(result.second, ::testing::HasSubstr("CH341"));
}

TEST_F(SerialPortScannerTest, IsCH340DeviceNotCH340) {
    // FTDI device - not CH340
    auto result = scanner->is_ch340_device(0x0403, 0x6001, "FTDI USB Serial");
    EXPECT_FALSE(result.first);
    EXPECT_TRUE(result.second.empty());
}

TEST_F(SerialPortScannerTest, IsCH340DeviceEmptyDescription) {
    // Known VID/PID with empty description
    auto result = scanner->is_ch340_device(0x1a86, 0x7523, "");
    EXPECT_TRUE(result.first);  // Should still detect by VID/PID
}

TEST_F(SerialPortScannerTest, IsCH340DeviceZeroVidPid) {
    auto result = scanner->is_ch340_device(0, 0, "");
    EXPECT_FALSE(result.first);
    EXPECT_TRUE(result.second.empty());
}

// Statistics Tests
TEST_F(SerialPortScannerTest, GetStatistics) {
    auto stats = scanner->get_statistics();
    EXPECT_EQ(stats.total_scans.load(), 0);
}

TEST_F(SerialPortScannerTest, ResetStatistics) {
    // Perform a scan to generate some stats
    [[maybe_unused]] auto result = scanner->list_available_ports();

    // Reset
    scanner->reset_statistics();

    auto stats = scanner->get_statistics();
    EXPECT_EQ(stats.total_scans.load(), 0);
}

// Cache Tests
TEST_F(SerialPortScannerTest, RefreshCache) {
    // Should not throw
    EXPECT_NO_THROW(scanner->refresh_cache());
}

TEST_F(SerialPortScannerTest, GetCacheInfo) {
    std::string cacheInfo = scanner->get_cache_info();
    EXPECT_FALSE(cacheInfo.empty());
}

// Retry Strategy Tests
TEST_F(SerialPortScannerTest, SetRetryStrategy) {
    EXPECT_NO_THROW(scanner->set_retry_strategy(5, 2000ms));

    auto config = scanner->get_config();
    EXPECT_EQ(config.max_retry_count, 5);
    EXPECT_EQ(config.retry_interval, 2000ms);
}

// Last Scan Time Tests
TEST_F(SerialPortScannerTest, GetLastScanTime) {
    auto beforeScan = std::chrono::steady_clock::now();

    [[maybe_unused]] auto result = scanner->list_available_ports();

    auto lastScanTime = scanner->get_last_scan_time();

    // Last scan time should be after or equal to beforeScan
    EXPECT_GE(lastScanTime.time_since_epoch().count(),
              beforeScan.time_since_epoch().count());
}

// Last Error Tests
TEST_F(SerialPortScannerTest, GetLastErrorInitially) {
    auto lastError = scanner->get_last_error();
    // Initially should be empty
    EXPECT_FALSE(lastError.has_value());
}

// Monitoring Tests
TEST_F(SerialPortScannerTest, IsMonitoringInitially) {
    EXPECT_FALSE(scanner->is_monitoring());
}

TEST_F(SerialPortScannerTest, StartStopMonitoring) {
    bool eventReceived = false;
    auto callback =
        [&eventReceived](const SerialPortScanner::PortEvent& event) {
            eventReceived = true;
        };

    bool started = scanner->start_monitoring(callback);
    // May fail on some systems without proper permissions
    if (started) {
        EXPECT_TRUE(scanner->is_monitoring());

        scanner->stop_monitoring();
        EXPECT_FALSE(scanner->is_monitoring());
    }
}

// Custom Device Detector Tests
TEST_F(SerialPortScannerTest, RegisterDeviceDetector) {
    auto customDetector =
        [](uint16_t vid, uint16_t pid,
           std::string_view desc) -> std::pair<bool, std::string> {
        if (vid == 0x1234 && pid == 0x5678) {
            return {true, "CustomDevice"};
        }
        return {false, ""};
    };

    bool registered =
        scanner->register_device_detector("custom", customDetector);
    EXPECT_TRUE(registered);

    // Registering with same name should fail
    bool registeredAgain =
        scanner->register_device_detector("custom", customDetector);
    EXPECT_FALSE(registeredAgain);
}

// Port Listing Tests (Platform-dependent)
TEST_F(SerialPortScannerTest, ListAvailablePorts) {
    auto result = scanner->list_available_ports();

    // Should return a valid result (either ports or error)
    if (!hasError(result)) {
        auto ports = getPortsFromResult(result);
        // Result is valid, ports may be empty on systems without serial ports
        SUCCEED();
    } else {
        // Error is also acceptable (e.g., permission issues)
        auto error = getError(result);
        EXPECT_FALSE(error.message.empty());
    }
}

TEST_F(SerialPortScannerTest, ListAvailablePortsWithoutCH340Highlighting) {
    auto result = scanner->list_available_ports(false);

    if (!hasError(result)) {
        auto ports = getPortsFromResult(result);
        // All ports should have is_ch340 = false when highlighting is disabled
        for (const auto& port : ports) {
            EXPECT_FALSE(port.is_ch340);
            EXPECT_TRUE(port.ch340_model.empty());
        }
    }
}

// Port Details Tests
TEST_F(SerialPortScannerTest, GetPortDetailsNonExistent) {
    auto result = scanner->get_port_details("NON_EXISTENT_PORT_12345");

    if (!hasError(result)) {
        auto details = getDetailsFromResult(result);
        EXPECT_FALSE(details.has_value());
    }
}

// Port Availability Tests
TEST_F(SerialPortScannerTest, IsPortAvailableNonExistent) {
    auto result = scanner->is_port_available("NON_EXISTENT_PORT_12345");

    if (!hasError(result)) {
        bool available = std::get<bool>(result);
        EXPECT_FALSE(available);
    }
}

// Port Validation Tests
TEST_F(SerialPortScannerTest, ValidatePortNonExistent) {
    auto result = scanner->validate_port("NON_EXISTENT_PORT_12345", 500ms);

    if (!hasError(result)) {
        bool valid = std::get<bool>(result);
        EXPECT_FALSE(valid);
    }
}

// =============================================================================
// PortInfo Structure Tests
// =============================================================================

TEST(PortInfoTest, DefaultValues) {
    SerialPortScanner::PortInfo info;

    EXPECT_TRUE(info.device.empty());
    EXPECT_TRUE(info.description.empty());
    EXPECT_FALSE(info.is_available);
    EXPECT_FALSE(info.is_ch340);
    EXPECT_FALSE(info.is_virtual);
    EXPECT_FALSE(info.is_bluetooth);
    EXPECT_EQ(info.scan_count, 0);
}

// =============================================================================
// PortDetails Structure Tests
// =============================================================================

TEST(PortDetailsTest, DefaultValues) {
    SerialPortScanner::PortDetails details;

    EXPECT_TRUE(details.device_name.empty());
    EXPECT_TRUE(details.description.empty());
    EXPECT_FALSE(details.is_ch340);
    EXPECT_FALSE(details.is_virtual);
    EXPECT_FALSE(details.is_bluetooth);
    EXPECT_FALSE(details.is_available);
    EXPECT_EQ(details.current_baud_rate, 0);
    EXPECT_EQ(details.max_baud_rate, 0);
    EXPECT_EQ(details.error_count, 0);
}

// =============================================================================
// DeviceId Structure Tests
// =============================================================================

TEST(DeviceIdTest, Equality) {
    SerialPortScanner::DeviceId id1{0x1234, 0x5678};
    SerialPortScanner::DeviceId id2{0x1234, 0x5678};
    SerialPortScanner::DeviceId id3{0x1234, 0x9999};

    EXPECT_TRUE(id1 == id2);
    EXPECT_FALSE(id1 == id3);
}

// =============================================================================
// PortEvent Structure Tests
// =============================================================================

TEST(PortEventTest, EventTypes) {
    using EventType = SerialPortScanner::PortEvent::Type;

    SerialPortScanner::PortEvent addedEvent;
    addedEvent.type = EventType::ADDED;
    EXPECT_EQ(addedEvent.type, EventType::ADDED);

    SerialPortScanner::PortEvent removedEvent;
    removedEvent.type = EventType::REMOVED;
    EXPECT_EQ(removedEvent.type, EventType::REMOVED);

    SerialPortScanner::PortEvent changedEvent;
    changedEvent.type = EventType::CHANGED;
    EXPECT_EQ(changedEvent.type, EventType::CHANGED);

    SerialPortScanner::PortEvent errorEvent;
    errorEvent.type = EventType::ERROR_TYPE;
    EXPECT_EQ(errorEvent.type, EventType::ERROR_TYPE);
}

// =============================================================================
// ErrorInfo Structure Tests
// =============================================================================

TEST(ErrorInfoTest, DefaultConstruction) {
    SerialPortScanner::ErrorInfo error;
    EXPECT_TRUE(error.message.empty());
    EXPECT_EQ(error.error_code, 0);
    EXPECT_TRUE(error.context.empty());
}

TEST(ErrorInfoTest, ConstructionWithMessage) {
    SerialPortScanner::ErrorInfo error("Test error", 42, "TestContext");

    EXPECT_EQ(error.message, "Test error");
    EXPECT_EQ(error.error_code, 42);
    EXPECT_EQ(error.context, "TestContext");
    // Timestamp should be set
    EXPECT_NE(error.timestamp.time_since_epoch().count(), 0);
}

// =============================================================================
// Async Operations Tests
// =============================================================================

class SerialPortScannerAsyncTest : public ::testing::Test {
protected:
    void SetUp() override { scanner = std::make_unique<SerialPortScanner>(); }

    std::unique_ptr<SerialPortScanner> scanner;
};

TEST_F(SerialPortScannerAsyncTest, ListAvailablePortsAsync) {
    std::atomic<bool> callbackCalled{false};
    std::mutex mutex;
    std::condition_variable cv;

    scanner->list_available_ports_async(
        [&callbackCalled, &cv](
            SerialPortScanner::Result<std::vector<SerialPortScanner::PortInfo>>
                result) {
            callbackCalled = true;
            cv.notify_one();
        });

    // Wait for callback with timeout
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 10s,
                    [&callbackCalled] { return callbackCalled.load(); });
    }

    EXPECT_TRUE(callbackCalled.load());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

class SerialPortScannerThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override { scanner = std::make_unique<SerialPortScanner>(); }

    std::unique_ptr<SerialPortScanner> scanner;
};

TEST_F(SerialPortScannerThreadSafetyTest, ConcurrentScans) {
    constexpr int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> completedScans{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &completedScans]() {
            [[maybe_unused]] auto result = scanner->list_available_ports();
            ++completedScans;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedScans.load(), numThreads);
}

TEST_F(SerialPortScannerThreadSafetyTest, ConcurrentCH340Detection) {
    constexpr int numThreads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> completedChecks{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &completedChecks]() {
            auto result = scanner->is_ch340_device(0x1a86, 0x7523, "CH340");
            EXPECT_TRUE(result.first);
            ++completedChecks;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedChecks.load(), numThreads);
}

TEST_F(SerialPortScannerThreadSafetyTest, ConcurrentConfigAccess) {
    constexpr int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> completedOps{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &completedOps]() {
            if (i % 2 == 0) {
                ScannerConfig config;
                config.scan_timeout = std::chrono::milliseconds(1000 + i * 100);
                scanner->set_config(config);
            } else {
                [[maybe_unused]] auto config = scanner->get_config();
            }
            ++completedOps;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedOps.load(), numThreads);
}
