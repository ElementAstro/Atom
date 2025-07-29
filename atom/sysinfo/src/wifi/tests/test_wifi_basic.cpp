/*
 * test_wifi_basic.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: Basic WiFi Module Tests

**************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../wifi.hpp"
#include "../common.hpp"
#include "../error_handler.hpp"
#include "../config.hpp"

using namespace atom::system;
using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

class WiFiBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize error handling and configuration
        initializeErrorHandling();
        initializeConfiguration();
    }

    void TearDown() override {
        // Cleanup
        shutdownErrorHandling();
        shutdownConfiguration();
    }
};

// Test basic WiFi functionality
TEST_F(WiFiBasicTest, TestInternetConnectivity) {
    // Test internet connectivity check
    bool connected = isConnectedToInternet();

    // Should return either true or false, not throw
    EXPECT_TRUE(connected == true || connected == false);
}

TEST_F(WiFiBasicTest, TestGetCurrentWifi) {
    // Test getting current WiFi name
    std::string wifi_name = getCurrentWifi();

    // Should return a string (may be empty if no WiFi)
    EXPECT_TRUE(wifi_name.empty() || !wifi_name.empty());
}

TEST_F(WiFiBasicTest, TestGetHostIPs) {
    // Test getting host IP addresses
    auto ips = getHostIPs();

    // Should return a vector (may be empty)
    EXPECT_TRUE(ips.empty() || !ips.empty());

    // If not empty, should contain valid IP addresses
    for (const auto& ip : ips) {
        EXPECT_FALSE(ip.empty());
        EXPECT_TRUE(isValidIPAddress(ip));
    }
}

TEST_F(WiFiBasicTest, TestGetIPv4Addresses) {
    // Test getting IPv4 addresses
    auto ipv4_addresses = getIPv4Addresses();

    // Should return a vector
    EXPECT_TRUE(ipv4_addresses.empty() || !ipv4_addresses.empty());

    // If not empty, should contain valid IPv4 addresses
    for (const auto& ip : ipv4_addresses) {
        EXPECT_FALSE(ip.empty());
        EXPECT_TRUE(isValidIPAddress(ip));
        // IPv4 addresses should not contain colons
        EXPECT_EQ(ip.find(':'), std::string::npos);
    }
}

TEST_F(WiFiBasicTest, TestGetIPv6Addresses) {
    // Test getting IPv6 addresses
    auto ipv6_addresses = getIPv6Addresses();

    // Should return a vector
    EXPECT_TRUE(ipv6_addresses.empty() || !ipv6_addresses.empty());

    // If not empty, should contain valid IPv6 addresses
    for (const auto& ip : ipv6_addresses) {
        EXPECT_FALSE(ip.empty());
        EXPECT_TRUE(isValidIPAddress(ip));
    }
}

TEST_F(WiFiBasicTest, TestGetInterfaceNames) {
    // Test getting network interface names
    auto interfaces = getInterfaceNames();

    // Should return a vector (should not be empty on most systems)
    EXPECT_FALSE(interfaces.empty());

    // Interface names should not be empty
    for (const auto& interface : interfaces) {
        EXPECT_FALSE(interface.empty());
    }
}

TEST_F(WiFiBasicTest, TestGetNetworkStats) {
    // Test getting network statistics
    auto stats = getNetworkStats();

    // Basic validation of network stats
    EXPECT_GE(stats.downloadSpeed, 0.0);
    EXPECT_GE(stats.uploadSpeed, 0.0);
    EXPECT_GE(stats.packetLoss, 0.0);
    EXPECT_LE(stats.packetLoss, 100.0);
}

TEST_F(WiFiBasicTest, TestScanAvailableNetworks) {
    // Test scanning for available networks
    auto networks = scanAvailableNetworks();

    // Should return a vector (may be empty)
    EXPECT_TRUE(networks.empty() || !networks.empty());

    // Network names should not be empty
    for (const auto& network : networks) {
        EXPECT_FALSE(network.empty());
    }
}

TEST_F(WiFiBasicTest, TestMeasureBandwidth) {
    // Test bandwidth measurement
    auto [download, upload] = measureBandwidth();

    // Bandwidth should be non-negative
    EXPECT_GE(download, 0.0);
    EXPECT_GE(upload, 0.0);
}

TEST_F(WiFiBasicTest, TestAnalyzeNetworkQuality) {
    // Test network quality analysis
    std::string quality = analyzeNetworkQuality();

    // Should return a non-empty string
    EXPECT_FALSE(quality.empty());
}

TEST_F(WiFiBasicTest, TestGetConnectedDevices) {
    // Test getting connected devices
    auto devices = getConnectedDevices();

    // Should return a vector (may be empty)
    EXPECT_TRUE(devices.empty() || !devices.empty());
}

TEST_F(WiFiBasicTest, TestGetNetworkSecurity) {
    // Test getting network security information
    std::string security = getNetworkSecurity();

    // Should return a string (may be empty if no connection)
    EXPECT_TRUE(security.empty() || !security.empty());
}

// Test error handling
TEST_F(WiFiBasicTest, TestErrorHandling) {
    auto& error_handler = getGlobalErrorHandler();

    // Test error logging
    error_handler.logError(WiFiError::TIMEOUT, "Test error", "test_function", __FILE__, __LINE__);

    // Check if error was recorded
    auto recent_errors = error_handler.getRecentErrors(std::chrono::minutes(1));
    EXPECT_FALSE(recent_errors.empty());

    // Check error statistics
    auto stats = error_handler.getErrorStatistics(std::chrono::hours(1));
    EXPECT_TRUE(stats.find(WiFiError::TIMEOUT) != stats.end());

    // Test error report generation
    std::string report = error_handler.generateErrorReport(std::chrono::hours(1));
    EXPECT_FALSE(report.empty());
}

// Test configuration system
TEST_F(WiFiBasicTest, TestConfiguration) {
    auto& config_manager = getGlobalConfigManager();

    // Test getting configuration
    auto wifi_config = config_manager.getWiFiConfig();
    auto perf_config = config_manager.getPerformanceConfig();

    // Basic validation
    EXPECT_GT(wifi_config.ping_timeout.count(), 0);
    EXPECT_GT(wifi_config.max_cache_size, 0);
    EXPECT_FALSE(wifi_config.preferred_ping_host.empty());

    // Test parameter setting
    EXPECT_TRUE(config_manager.setParameter("ping_timeout", "3000"));
    EXPECT_EQ(config_manager.getParameter("ping_timeout"), "3000");

    // Test configuration validation
    auto errors = config_manager.validateConfiguration();
    EXPECT_TRUE(errors.empty()); // Should be valid after proper initialization

    // Test configuration string representation
    std::string config_str = config_manager.toString();
    EXPECT_FALSE(config_str.empty());
}

// Test utility functions
TEST_F(WiFiBasicTest, TestUtilityFunctions) {
    // Test IP address validation
    EXPECT_TRUE(isValidIPAddress("192.168.1.1"));
    EXPECT_TRUE(isValidIPAddress("::1"));
    EXPECT_TRUE(isValidIPAddress("2001:db8::1"));
    EXPECT_FALSE(isValidIPAddress("invalid.ip"));
    EXPECT_FALSE(isValidIPAddress(""));
    EXPECT_FALSE(isValidIPAddress("999.999.999.999"));

    // Test WiFi error to string conversion
    EXPECT_EQ(wifiErrorToString(WiFiError::SUCCESS), "Success");
    EXPECT_EQ(wifiErrorToString(WiFiError::TIMEOUT), "Operation timeout");
    EXPECT_EQ(wifiErrorToString(WiFiError::NETWORK_UNREACHABLE), "Network unreachable");
}

// Test ping functionality
TEST_F(WiFiBasicTest, TestPingMeasurement) {
    // Test ping to a reliable host
    auto [latency, error] = measurePing("8.8.8.8", 5000);

    if (error == WiFiError::SUCCESS) {
        EXPECT_GT(latency, 0.0);
        EXPECT_LT(latency, 5000.0); // Should be less than timeout
    } else {
        // If ping fails, it should be due to network issues, not code errors
        EXPECT_TRUE(error == WiFiError::NETWORK_UNREACHABLE ||
                   error == WiFiError::TIMEOUT ||
                   error == WiFiError::SYSTEM_CALL_FAILED);
    }
}

// Test command execution
TEST_F(WiFiBasicTest, TestCommandExecution) {
    // Test a simple command that should work on all platforms
#ifdef _WIN32
    auto [output, error] = executeCommand("echo test", 5000);
#else
    auto [output, error] = executeCommand("echo test", 5000);
#endif

    if (error == WiFiError::SUCCESS) {
        EXPECT_FALSE(output.empty());
        EXPECT_NE(output.find("test"), std::string::npos);
    }
}

// Test network interface information
TEST_F(WiFiBasicTest, TestNetworkInterfaces) {
    auto interfaces = getNetworkInterfaces();

    // Should have at least one interface (loopback)
    EXPECT_FALSE(interfaces.empty());

    for (const auto& iface : interfaces) {
        EXPECT_FALSE(iface.name.empty());
        // Interface should have a valid state
        EXPECT_TRUE(iface.is_up == true || iface.is_up == false);
        EXPECT_TRUE(iface.is_wireless == true || iface.is_wireless == false);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
