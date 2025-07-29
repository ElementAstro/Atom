/**
 * @file test_enhanced_os.cpp
 * @brief Comprehensive tests for the Enhanced OS Module
 *
 * This file contains unit tests and integration tests for the Enhanced
 * Operating System Information Module to ensure reliability and correctness
 * across all supported platforms.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "atom/sysinfo/bios/os/os.hpp"

#ifdef __linux__
#include "atom/sysinfo/bios/os/linux.hpp"
#elif defined(_WIN32)
#include "atom/sysinfo/bios/os/windows.hpp"
#elif defined(__APPLE__)
#include "atom/sysinfo/bios/os/macos.hpp"
#endif

using namespace atom::system;

class EnhancedOSTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = &EnhancedOSManager::getInstance();
    }

    void TearDown() override {
        // Ensure monitoring is stopped
        if (manager->isMonitoring()) {
            manager->stopMonitoring();
        }
    }

    EnhancedOSManager* manager;
};

// Basic functionality tests
TEST_F(EnhancedOSTest, GetEnhancedOSInfo) {
    auto osInfo = manager->getEnhancedOSInfo();

    // Basic validation
    EXPECT_FALSE(osInfo.osName.empty());
    EXPECT_FALSE(osInfo.osVersion.empty());
    EXPECT_FALSE(osInfo.computerName.empty());
    EXPECT_NE(osInfo.osType, OSType::UNKNOWN);
    EXPECT_NE(osInfo.osArch, OSArchitecture::UNKNOWN);
    EXPECT_GT(osInfo.totalMemoryBytes, 0);

    // Validate OS type matches platform
#ifdef _WIN32
    EXPECT_EQ(osInfo.osType, OSType::WINDOWS);
#elif defined(__linux__)
    EXPECT_EQ(osInfo.osType, OSType::LINUX);
#elif defined(__APPLE__)
    EXPECT_EQ(osInfo.osType, OSType::MACOS);
#endif
}

TEST_F(EnhancedOSTest, GetPerformanceMetrics) {
    auto metrics = manager->getPerformanceMetrics();

    // Validate metrics are within reasonable ranges
    EXPECT_GE(metrics.cpuUsagePercent, 0.0);
    EXPECT_LE(metrics.cpuUsagePercent, 100.0);
    EXPECT_GE(metrics.memoryUsagePercent, 0.0);
    EXPECT_LE(metrics.memoryUsagePercent, 100.0);
    EXPECT_GE(metrics.diskUsagePercent, 0.0);
    EXPECT_LE(metrics.diskUsagePercent, 100.0);
    EXPECT_GE(metrics.networkUsageKbps, 0.0);
    EXPECT_GT(metrics.processCount, 0);
}

TEST_F(EnhancedOSTest, GetSecurityInfo) {
    auto secInfo = manager->getSecurityInfo();

    // Security info should be populated (even if features are disabled)
    // This test mainly ensures the function doesn't crash
    EXPECT_NO_THROW({
        bool hasFirewall = secInfo.firewallEnabled;
        bool hasAntivirus = secInfo.antivirusEnabled;
        (void)hasFirewall; // Suppress unused variable warning
        (void)hasAntivirus;
    });
}

TEST_F(EnhancedOSTest, GetNetworkConfiguration) {
    auto netConfig = manager->getNetworkConfiguration();

    // Network configuration should have at least some information
    // Even if some fields are empty, the function should not crash
    EXPECT_NO_THROW({
        std::string interface = netConfig.primaryInterface;
        std::string ip = netConfig.ipAddress;
        (void)interface; // Suppress unused variable warning
        (void)ip;
    });
}

TEST_F(EnhancedOSTest, PerformHealthCheck) {
    auto healthResults = manager->performHealthCheck();

    // Health check should return a vector (may be empty if system is healthy)
    EXPECT_NO_THROW({
        size_t resultCount = healthResults.size();
        (void)resultCount;
    });
}

TEST_F(EnhancedOSTest, AnalyzeResourceUsage) {
    auto resourceUsage = manager->analyzeResourceUsage();

    // Should have at least some resource information
    EXPECT_FALSE(resourceUsage.empty());
    EXPECT_TRUE(resourceUsage.find("cpu_usage_percent") != resourceUsage.end());
    EXPECT_TRUE(resourceUsage.find("memory_usage_percent") != resourceUsage.end());

    // Validate resource usage values
    auto cpuUsage = resourceUsage["cpu_usage_percent"];
    auto memoryUsage = resourceUsage["memory_usage_percent"];

    EXPECT_GE(cpuUsage, 0.0);
    EXPECT_LE(cpuUsage, 100.0);
    EXPECT_GE(memoryUsage, 0.0);
    EXPECT_LE(memoryUsage, 100.0);
}

// Monitoring tests
TEST_F(EnhancedOSTest, MonitoringStartStop) {
    SystemMonitoringConfig config;
    config.performanceIntervalMs = 1000;
    config.enablePerformanceMonitoring = true;

    // Test starting monitoring
    EXPECT_TRUE(manager->startMonitoring(config));
    EXPECT_TRUE(manager->isMonitoring());

    // Test stopping monitoring
    manager->stopMonitoring();
    EXPECT_FALSE(manager->isMonitoring());
}

TEST_F(EnhancedOSTest, MonitoringCallbacks) {
    SystemMonitoringConfig config;
    config.performanceIntervalMs = 500; // Fast interval for testing
    config.enablePerformanceMonitoring = true;

    bool callbackCalled = false;
    SystemPerformanceMetrics receivedMetrics;

    manager->setPerformanceCallback([&](const SystemPerformanceMetrics& metrics) {
        callbackCalled = true;
        receivedMetrics = metrics;
    });

    EXPECT_TRUE(manager->startMonitoring(config));

    // Wait for at least one callback
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    manager->stopMonitoring();

    EXPECT_TRUE(callbackCalled);
    EXPECT_GE(receivedMetrics.cpuUsagePercent, 0.0);
    EXPECT_LE(receivedMetrics.cpuUsagePercent, 100.0);
}

// Legacy compatibility tests
TEST_F(EnhancedOSTest, LegacyCompatibility) {
    // Test legacy functions still work
    auto osInfo = getOperatingSystemInfo();
    EXPECT_FALSE(osInfo.osName.empty());
    EXPECT_FALSE(osInfo.osVersion.empty());

    auto computerName = getComputerName();
    EXPECT_TRUE(computerName.has_value());
    EXPECT_FALSE(computerName->empty());

    auto uptime = getSystemUptime();
    EXPECT_GT(uptime.count(), 0);

    auto language = getSystemLanguage();
    EXPECT_FALSE(language.empty());

    auto encoding = getSystemEncoding();
    EXPECT_FALSE(encoding.empty());

    // Test convenience functions
    auto osName = getOSName();
    auto osVersion = getOSVersion();
    EXPECT_FALSE(osName.empty());
    EXPECT_FALSE(osVersion.empty());

    auto metrics = getSystemMetrics();
    EXPECT_GE(metrics.cpuUsagePercent, 0.0);
    EXPECT_LE(metrics.cpuUsagePercent, 100.0);
}

// Common utility tests
TEST_F(EnhancedOSTest, OSTypeConversion) {
    EXPECT_EQ(osTypeToString(OSType::WINDOWS), "Windows");
    EXPECT_EQ(osTypeToString(OSType::LINUX), "Linux");
    EXPECT_EQ(osTypeToString(OSType::MACOS), "macOS");
    EXPECT_EQ(osTypeToString(OSType::UNKNOWN), "Unknown");
}

TEST_F(EnhancedOSTest, OSArchitectureConversion) {
    EXPECT_EQ(osArchitectureToString(OSArchitecture::X86), "x86");
    EXPECT_EQ(osArchitectureToString(OSArchitecture::X64), "x64");
    EXPECT_EQ(osArchitectureToString(OSArchitecture::ARM), "ARM");
    EXPECT_EQ(osArchitectureToString(OSArchitecture::ARM64), "ARM64");
    EXPECT_EQ(osArchitectureToString(OSArchitecture::UNKNOWN), "Unknown");
}

TEST_F(EnhancedOSTest, OSTypeDetection) {
    auto detectedType = detectOSType();

#ifdef _WIN32
    EXPECT_EQ(detectedType, OSType::WINDOWS);
#elif defined(__linux__)
    EXPECT_EQ(detectedType, OSType::LINUX);
#elif defined(__APPLE__)
    EXPECT_EQ(detectedType, OSType::MACOS);
#endif
}

TEST_F(EnhancedOSTest, OSArchitectureDetection) {
    auto detectedArch = detectOSArchitecture();
    EXPECT_NE(detectedArch, OSArchitecture::UNKNOWN);
}

// Platform-specific tests
#ifdef __linux__
TEST_F(EnhancedOSTest, LinuxSpecificFeatures) {
    LinuxOSImplementation linuxImpl;

    auto distInfo = linuxImpl.getDistributionInfo();
    EXPECT_FALSE(distInfo.name.empty());

    auto kernelInfo = linuxImpl.getKernelInfo();
    EXPECT_FALSE(kernelInfo.version.empty());

    auto packageInfo = linuxImpl.getPackageInfo();
    EXPECT_FALSE(packageInfo.packageManager.empty());

    auto containerInfo = linuxImpl.getContainerInfo();
    // Container info may be empty if not in container - just ensure no crash
    EXPECT_NO_THROW({
        bool isContainer = containerInfo.isContainer;
        (void)isContainer;
    });
}
#endif

#ifdef _WIN32
TEST_F(EnhancedOSTest, WindowsSpecificFeatures) {
    WindowsOSImplementation windowsImpl;

    auto editionInfo = windowsImpl.getEditionInfo();
    EXPECT_FALSE(editionInfo.productName.empty());
    EXPECT_FALSE(editionInfo.buildNumber.empty());

    auto servicesInfo = windowsImpl.getServicesInfo();
    EXPECT_GT(servicesInfo.runningServices.size(), 0);

    auto updateInfo = windowsImpl.getUpdateInfo();
    // Update info may be empty - just ensure no crash
    EXPECT_NO_THROW({
        size_t updateCount = updateInfo.installedUpdates.size();
        (void)updateCount;
    });
}
#endif

#ifdef __APPLE__
TEST_F(EnhancedOSTest, MacOSSpecificFeatures) {
    MacOSOSImplementation macosImpl;

    auto versionInfo = macosImpl.getVersionInfo();
    EXPECT_FALSE(versionInfo.productName.empty());
    EXPECT_FALSE(versionInfo.productVersion.empty());

    auto secInfo = macosImpl.getSecurityInfo();
    // Security info should be available
    EXPECT_NO_THROW({
        bool sipEnabled = secInfo.sipEnabled;
        (void)sipEnabled;
    });

    auto appInfo = macosImpl.getApplicationInfo();
    EXPECT_GT(appInfo.installedApplications.size(), 0);
}
#endif

// Stress tests
TEST_F(EnhancedOSTest, RepeatedInfoRetrieval) {
    // Test that repeated calls don't cause issues
    for (int i = 0; i < 10; ++i) {
        auto osInfo = manager->getEnhancedOSInfo();
        EXPECT_FALSE(osInfo.osName.empty());

        auto metrics = manager->getPerformanceMetrics();
        EXPECT_GE(metrics.cpuUsagePercent, 0.0);
        EXPECT_LE(metrics.cpuUsagePercent, 100.0);
    }
}

TEST_F(EnhancedOSTest, ConcurrentAccess) {
    // Test concurrent access to the singleton
    std::vector<std::thread> threads;
    std::vector<bool> results(5, false);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto& mgr = EnhancedOSManager::getInstance();
                auto osInfo = mgr.getEnhancedOSInfo();
                results[i] = !osInfo.osName.empty();
            } catch (...) {
                results[i] = false;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should succeed
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Error handling tests
TEST_F(EnhancedOSTest, InvalidMonitoringConfig) {
    SystemMonitoringConfig config;
    config.performanceIntervalMs = 0; // Invalid interval

    // Should handle invalid config gracefully
    EXPECT_NO_THROW({
        bool started = manager->startMonitoring(config);
        if (started) {
            manager->stopMonitoring();
        }
    });
}

TEST_F(EnhancedOSTest, DoubleMonitoringStart) {
    SystemMonitoringConfig config;
    config.performanceIntervalMs = 1000;

    // Start monitoring
    EXPECT_TRUE(manager->startMonitoring(config));

    // Try to start again - should return false
    EXPECT_FALSE(manager->startMonitoring(config));

    // Clean up
    manager->stopMonitoring();
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "Running Enhanced OS Module Tests" << std::endl;
    std::cout << "=================================" << std::endl;

    int result = RUN_ALL_TESTS();

    std::cout << "\nTest execution completed." << std::endl;
    return result;
}
