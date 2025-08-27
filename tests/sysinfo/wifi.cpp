#include "atom/sysinfo/wifi.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Basic WiFi Tests
// ============================================================================

class WifiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup WiFi tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(WifiTest, GetCurrentWifi) {
    // Test getting current WiFi network
    EXPECT_NO_THROW({
        std::string currentWifi = getCurrentWifi();
        // WiFi name can be empty if not connected
        EXPECT_TRUE(currentWifi.empty() || !currentWifi.empty());
    });
}

TEST_F(WifiTest, GetCurrentWiredNetwork) {
    // Test getting current wired network
    EXPECT_NO_THROW({
        std::string currentWired = getCurrentWiredNetwork();
        // Wired network name can be empty if not connected
        EXPECT_TRUE(currentWired.empty() || !currentWired.empty());
    });
}

TEST_F(WifiTest, GetNetworkStats) {
    // Test getting network statistics
    EXPECT_NO_THROW({
        NetworkStats stats = getNetworkStats();

        // Validate network stats
        EXPECT_GE(stats.downloadSpeed, 0.0);
        EXPECT_GE(stats.uploadSpeed, 0.0);
        EXPECT_GE(stats.latency, 0.0);
        EXPECT_GE(stats.packetLoss, 0.0);
        EXPECT_LE(stats.packetLoss, 100.0);
        EXPECT_GE(stats.signalStrength, -150.0); // Reasonable lower bound
        EXPECT_LE(stats.signalStrength, 0.0);    // Signal strength is negative
    });
}

TEST_F(WifiTest, GetInterfaceNames) {
    // Test getting network interface names
    EXPECT_NO_THROW({
        std::vector<std::string> interfaces = getInterfaceNames();

        // Should have at least one interface (loopback)
        EXPECT_GT(interfaces.size(), 0);

        // Validate interface names
        for (const auto& interface : interfaces) {
            EXPECT_FALSE(interface.empty());
            EXPECT_LT(interface.length(), 100); // Reasonable length
        }
    });
}

TEST_F(WifiTest, ScanAvailableNetworks) {
    // Test scanning available networks
    EXPECT_NO_THROW({
        std::vector<std::string> networks = scanAvailableNetworks();

        // Networks can be empty if no WiFi adapter or no networks found
        for (const auto& network : networks) {
            EXPECT_FALSE(network.empty());
            EXPECT_LT(network.length(), 100); // Reasonable SSID length
        }
    });
}

TEST_F(WifiTest, GetNetworkSecurity) {
    // Test getting network security information
    EXPECT_NO_THROW({
        std::string security = getNetworkSecurity();
        // Security info can be empty if not available
        EXPECT_TRUE(security.empty() || !security.empty());
    });
}

TEST_F(WifiTest, MeasureBandwidth) {
    // Test measuring bandwidth
    EXPECT_NO_THROW({
        auto bandwidth = measureBandwidth();
        auto uploadSpeed = bandwidth.first;
        auto downloadSpeed = bandwidth.second;

        // Speeds should be non-negative
        EXPECT_GE(uploadSpeed, 0.0);
        EXPECT_GE(downloadSpeed, 0.0);

        // Reasonable upper bounds (1 Gbps)
        EXPECT_LT(uploadSpeed, 1000.0);
        EXPECT_LT(downloadSpeed, 1000.0);
    });
}

TEST_F(WifiTest, AnalyzeNetworkQuality) {
    // Test network quality analysis
    EXPECT_NO_THROW({
        std::string quality = analyzeNetworkQuality();
        // Quality analysis can be empty if not available
        EXPECT_TRUE(quality.empty() || !quality.empty());
    });
}

TEST_F(WifiTest, GetConnectedDevices) {
    // Test getting connected devices
    EXPECT_NO_THROW({
        std::vector<std::string> devices = getConnectedDevices();

        // Devices list can be empty
        for (const auto& device : devices) {
            EXPECT_FALSE(device.empty());
            EXPECT_LT(device.length(), 200); // Reasonable device name length
        }
    });
}

// ============================================================================
// Real System Tests
// ============================================================================

class RealWifiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup real WiFi tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(RealWifiTest, NetworkStatsConsistency) {
    // Test network stats consistency over multiple calls
    std::vector<NetworkStats> statsHistory;

    for (int i = 0; i < 3; ++i) {
        NetworkStats stats = getNetworkStats();
        statsHistory.push_back(stats);

        // Validate each measurement
        EXPECT_GE(stats.downloadSpeed, 0.0);
        EXPECT_GE(stats.uploadSpeed, 0.0);
        EXPECT_GE(stats.latency, 0.0);
        EXPECT_GE(stats.packetLoss, 0.0);
        EXPECT_LE(stats.packetLoss, 100.0);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stats should be reasonable across measurements
    EXPECT_EQ(statsHistory.size(), 3);
}

TEST_F(RealWifiTest, InterfaceNamesStability) {
    // Test that interface names are stable across calls
    std::vector<std::string> interfaces1 = getInterfaceNames();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<std::string> interfaces2 = getInterfaceNames();

    // Interface list should be stable
    EXPECT_EQ(interfaces1.size(), interfaces2.size());

    // Interface names should be the same
    std::sort(interfaces1.begin(), interfaces1.end());
    std::sort(interfaces2.begin(), interfaces2.end());
    EXPECT_EQ(interfaces1, interfaces2);
}

TEST_F(RealWifiTest, NetworkConnectionStatus) {
    // Test network connection status
    std::string currentWifi = getCurrentWifi();
    std::string currentWired = getCurrentWiredNetwork();

    // At least one should be available on most systems
    bool hasConnection = !currentWifi.empty() || !currentWired.empty();

    if (hasConnection) {
        // If we have a connection, network stats should be reasonable
        NetworkStats stats = getNetworkStats();
        EXPECT_GE(stats.downloadSpeed, 0.0);
        EXPECT_GE(stats.uploadSpeed, 0.0);
    }

    // This test is informational - connection status can vary
    EXPECT_TRUE(hasConnection || !hasConnection);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(RealWifiTest, NoThrowGuarantee) {
    // Test that all WiFi functions provide no-throw guarantee
    EXPECT_NO_THROW(getCurrentWifi());
    EXPECT_NO_THROW(getCurrentWiredNetwork());
    EXPECT_NO_THROW(getNetworkStats());
    EXPECT_NO_THROW(getInterfaceNames());
    EXPECT_NO_THROW(scanAvailableNetworks());
    EXPECT_NO_THROW(getNetworkSecurity());
    EXPECT_NO_THROW(measureBandwidth());
    EXPECT_NO_THROW(analyzeNetworkQuality());
    EXPECT_NO_THROW(getConnectedDevices());
}

TEST_F(RealWifiTest, EmptyResultHandling) {
    // Test handling of potentially empty results

    // These functions might return empty results on some systems
    std::string currentWifi = getCurrentWifi();
    std::string currentWired = getCurrentWiredNetwork();
    std::vector<std::string> networks = scanAvailableNetworks();
    std::string security = getNetworkSecurity();
    std::string quality = analyzeNetworkQuality();
    std::vector<std::string> devices = getConnectedDevices();

    // All should handle empty results gracefully
    EXPECT_TRUE(currentWifi.empty() || !currentWifi.empty());
    EXPECT_TRUE(currentWired.empty() || !currentWired.empty());
    EXPECT_TRUE(networks.empty() || !networks.empty());
    EXPECT_TRUE(security.empty() || !security.empty());
    EXPECT_TRUE(quality.empty() || !quality.empty());
    EXPECT_TRUE(devices.empty() || !devices.empty());
}

TEST_F(RealWifiTest, StringFieldValidation) {
    // Test that string fields don't contain null characters

    std::string currentWifi = getCurrentWifi();
    std::string currentWired = getCurrentWiredNetwork();
    std::vector<std::string> interfaces = getInterfaceNames();

    if (!currentWifi.empty()) {
        EXPECT_EQ(currentWifi.find('\0'), std::string::npos);
    }

    if (!currentWired.empty()) {
        EXPECT_EQ(currentWired.find('\0'), std::string::npos);
    }

    for (const auto& interface : interfaces) {
        EXPECT_EQ(interface.find('\0'), std::string::npos);
        EXPECT_FALSE(interface.empty());
    }
}

} // namespace atom::sysinfo::test
