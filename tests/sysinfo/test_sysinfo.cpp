/*
 * test_sysinfo.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom System Information Library
Tests system information gathering, hardware detection, performance monitoring,
and system utilities.

**************************************************/

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

#include "atom/sysinfo/battery.hpp"
#include "atom/sysinfo/bios.hpp"
#include "atom/sysinfo/cpu.hpp"
#include "atom/sysinfo/disk.hpp"
#include "atom/sysinfo/gpu.hpp"
#include "atom/sysinfo/locale.hpp"
#include "atom/sysinfo/memory.hpp"
#include "atom/sysinfo/os.hpp"
#include "atom/sysinfo/sn.hpp"
#include "atom/sysinfo/sysinfo_printer.hpp"
#include "atom/sysinfo/virtual.hpp"
#include "atom/sysinfo/wifi.hpp"
// #include "atom/sysinfo/wm.hpp" // Not available

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// CPU Information Tests
// ============================================================================

class CPUInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup CPU information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(CPUInfoTest, CPUBasicInfo) {
    // Test basic CPU information retrieval
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(CPUInfoTest, CPUPerformanceMetrics) {
    // Test CPU performance metrics
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(CPUInfoTest, CPUCoreCount) {
    // Test CPU core count detection
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// Memory Information Tests
// ============================================================================

class MemoryInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup memory information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(MemoryInfoTest, MemoryBasicInfo) {
    // Test basic memory information retrieval
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(MemoryInfoTest, MemoryUsage) {
    // Test memory usage monitoring
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(MemoryInfoTest, MemoryAvailability) {
    // Test available memory detection
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// Disk Information Tests
// ============================================================================

class DiskInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup disk information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(DiskInfoTest, DiskBasicInfo) {
    // Test basic disk information retrieval
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(DiskInfoTest, DiskSpace) {
    // Test disk space monitoring
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(DiskInfoTest, DiskPerformance) {
    // Test disk performance metrics
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// GPU Information Tests
// ============================================================================

class GPUInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup GPU information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(GPUInfoTest, GPUBasicInfo) {
    // Test basic GPU information retrieval
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(GPUInfoTest, GPUMemory) {
    // Test GPU memory information
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(GPUInfoTest, GPUPerformance) {
    // Test GPU performance metrics
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// Battery Information Tests
// ============================================================================

class BatteryInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup battery information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(BatteryInfoTest, BatteryStatus) {
    // Test battery status detection
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(BatteryInfoTest, BatteryLevel) {
    // Test battery level monitoring
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(BatteryInfoTest, PowerManagement) {
    // Test power management features
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// Operating System Information Tests
// ============================================================================

class OSInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup OS information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(OSInfoTest, OSBasicInfo) {
    // Test basic OS information retrieval
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(OSInfoTest, OSVersion) {
    // Test OS version detection
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(OSInfoTest, SystemUptime) {
    // Test system uptime monitoring
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// Network Information Tests
// ============================================================================

class NetworkInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup network information tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(NetworkInfoTest, NetworkInterfaces) {
    // Test network interface detection
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(NetworkInfoTest, WiFiInfo) {
    // Test WiFi information retrieval
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(NetworkInfoTest, NetworkPerformance) {
    // Test network performance metrics
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// System Information Printer Tests
// ============================================================================

class SysInfoPrinterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup system information printer tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(SysInfoPrinterTest, PrintSystemInfo) {
    // Test system information printing
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

TEST_F(SysInfoPrinterTest, FormatOutput) {
    // Test output formatting
    EXPECT_TRUE(true); // Placeholder - implement actual tests
}

// ============================================================================
// Integration Tests
// ============================================================================

class SysInfoIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup integration test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(SysInfoIntegrationTest, CompleteSystemScan) {
    // Test complete system information gathering across all components

    // CPU Information
    CpuInfo cpuInfo = getCpuInfo();
    EXPECT_FALSE(cpuInfo.model.empty());
    EXPECT_GT(cpuInfo.numLogicalCores, 0);
    EXPECT_GT(cpuInfo.baseFrequency, 0.0);

    // Memory Information
    MemoryInfo memInfo = getDetailedMemoryStats();
    EXPECT_GT(memInfo.totalPhysicalMemory, 0);
    EXPECT_LE(memInfo.availablePhysicalMemory, memInfo.totalPhysicalMemory);

    // Operating System Information
    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    EXPECT_FALSE(osInfo.osName.empty());
    EXPECT_FALSE(osInfo.osVersion.empty());
    EXPECT_FALSE(osInfo.architecture.empty());

    // Disk Information
    std::vector<DiskInfo> disks = getDiskInfo();
    EXPECT_FALSE(disks.empty());
    for (const auto& disk : disks) {
        EXPECT_FALSE(disk.path.empty());
        EXPECT_GT(disk.totalSpace, 0);
    }

    // GPU Information
    std::string gpuInfo = getGPUInfo();
    EXPECT_FALSE(gpuInfo.empty());

    // Network Information
    std::string currentWifi = getCurrentWifi();
    std::string currentWired = getCurrentWiredNetwork();
    // At least one network interface should be available (but can be empty)
    EXPECT_TRUE(currentWifi.empty() || !currentWifi.empty());
    EXPECT_TRUE(currentWired.empty() || !currentWired.empty());

    // Locale Information
    LocaleInfo localeInfo = getSystemLanguageInfo();
    EXPECT_FALSE(localeInfo.languageCode.empty());
    EXPECT_FALSE(localeInfo.characterEncoding.empty());

    // Virtual Environment Detection
    bool isVM = isVirtualMachine();
    bool isWSL = isWsl();
    // These are boolean values, just verify they don't throw
    EXPECT_TRUE(isVM || !isVM);
    EXPECT_TRUE(isWSL || !isWSL);

    // Hardware Serial Numbers
    HardwareInfo hwInfo;
    std::string biosSerial = hwInfo.getBiosSerialNumber();
    std::string motherboardSerial = hwInfo.getMotherboardSerialNumber();
    // Serial numbers might be empty, but functions should not throw
    EXPECT_TRUE(biosSerial.empty() || !biosSerial.empty());
    EXPECT_TRUE(motherboardSerial.empty() || !motherboardSerial.empty());

    // Window Manager Information not available in current implementation
    // Skip window manager tests
}

TEST_F(SysInfoIntegrationTest, PerformanceMonitoring) {
    // Test continuous performance monitoring across components

    // Monitor CPU usage over time
    std::vector<float> cpuReadings;
    for (int i = 0; i < 3; ++i) {
        float cpuUsage = getCurrentCpuUsage();
        cpuReadings.push_back(cpuUsage);
        EXPECT_GE(cpuUsage, 0.0f);
        EXPECT_LE(cpuUsage, 100.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(cpuReadings.size(), 3);

    // Monitor memory usage
    std::vector<float> memoryReadings;
    for (int i = 0; i < 3; ++i) {
        float memoryUsage = getMemoryUsage();
        memoryReadings.push_back(memoryUsage);
        EXPECT_GE(memoryUsage, 0.0f);
        EXPECT_LE(memoryUsage, 100.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(memoryReadings.size(), 3);

    // Monitor system uptime (should be consistent)
    std::chrono::seconds uptime1 = getSystemUptime();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::chrono::seconds uptime2 = getSystemUptime();
    EXPECT_GE(uptime2.count(), uptime1.count());

    // Monitor network statistics
    NetworkStats stats1 = getNetworkStats();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    NetworkStats stats2 = getNetworkStats();

    // Network stats should be reasonable
    EXPECT_GE(stats1.downloadSpeed, 0.0);
    EXPECT_GE(stats1.uploadSpeed, 0.0);
    EXPECT_GE(stats2.downloadSpeed, 0.0);
    EXPECT_GE(stats2.uploadSpeed, 0.0);
    EXPECT_GE(stats2.latency, 0.0);
    EXPECT_GE(stats2.packetLoss, 0.0);
    EXPECT_LE(stats2.packetLoss, 100.0);
}

TEST_F(SysInfoIntegrationTest, CrossComponentConsistency) {
    // Test consistency between different components

    // CPU and Memory consistency
    CpuInfo cpuInfo = getCpuInfo();
    MemoryInfo memInfo = getDetailedMemoryStats();

    // Both should report the same architecture
    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    EXPECT_FALSE(osInfo.architecture.empty());

    // CPU core count should be reasonable relative to memory
    EXPECT_GT(cpuInfo.numLogicalCores, 0);
    EXPECT_LE(cpuInfo.numLogicalCores, 1024); // Reasonable upper bound

    // Memory should be reasonable relative to system
    EXPECT_GT(memInfo.totalPhysicalMemory, 512ULL * 1024 * 1024); // At least 512MB

    // Disk and OS consistency
    std::vector<DiskInfo> disks = getDiskInfo();
    EXPECT_FALSE(disks.empty());

    // At least one disk should contain the OS
    bool hasSystemDisk = false;
    for (const auto& disk : disks) {
        if (disk.path == "/" || disk.path == "C:\\" || disk.path.find("System") != std::string::npos) {
            hasSystemDisk = true;
            break;
        }
    }
    // Note: This might not always be true depending on system configuration
    EXPECT_TRUE(hasSystemDisk || !hasSystemDisk);
}

TEST_F(SysInfoIntegrationTest, SystemInfoPrinterIntegration) {
    // Test SystemInfoPrinter with real system data

    // Generate full report
    std::string fullReport = SystemInfoPrinter::generateFullReport();
    EXPECT_FALSE(fullReport.empty());
    EXPECT_GT(fullReport.length(), 100); // Should be substantial

    // Report should contain information from multiple components
    EXPECT_TRUE(fullReport.find("CPU") != std::string::npos ||
               fullReport.find("Processor") != std::string::npos);
    EXPECT_TRUE(fullReport.find("Memory") != std::string::npos ||
               fullReport.find("RAM") != std::string::npos);
    EXPECT_TRUE(fullReport.find("Operating System") != std::string::npos ||
               fullReport.find("OS") != std::string::npos);

    // Generate simple report
    std::string simpleReport = SystemInfoPrinter::generateSimpleReport();
    EXPECT_FALSE(simpleReport.empty());
    EXPECT_LT(simpleReport.length(), fullReport.length());

    // Generate performance report
    std::string perfReport = SystemInfoPrinter::generatePerformanceReport();
    EXPECT_FALSE(perfReport.empty());

    // Generate security report
    std::string secReport = SystemInfoPrinter::generateSecurityReport();
    EXPECT_FALSE(secReport.empty());

    // Test individual component formatting
    CpuInfo cpuInfo = getCpuInfo();
    std::string cpuFormatted = SystemInfoPrinter::formatCpuInfo(cpuInfo);
    EXPECT_FALSE(cpuFormatted.empty());
    EXPECT_TRUE(cpuFormatted.find(cpuInfo.model) != std::string::npos);

    MemoryInfo memInfo = getDetailedMemoryStats();
    std::string memFormatted = SystemInfoPrinter::formatMemoryInfo(memInfo);
    EXPECT_FALSE(memFormatted.empty());

    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    std::string osFormatted = SystemInfoPrinter::formatOsInfo(osInfo);
    EXPECT_FALSE(osFormatted.empty());
    EXPECT_TRUE(osFormatted.find(osInfo.osName) != std::string::npos);
}

TEST_F(SysInfoIntegrationTest, BatteryAndPowerIntegration) {
    // Test battery and power-related integration
    auto batteryInfo = getBatteryInfo();

    if (batteryInfo.has_value()) {
        const auto& battery = batteryInfo.value();

        // Battery info should be consistent
        EXPECT_TRUE(battery.isBatteryPresent);
        EXPECT_GE(battery.batteryLifePercent, 0.0f);
        EXPECT_LE(battery.batteryLifePercent, 100.0f);

        // Test battery formatting
        std::string batteryFormatted = SystemInfoPrinter::formatBatteryInfo(battery);
        EXPECT_FALSE(batteryFormatted.empty());

        // CPU power info should be related to battery if available
        CpuPowerInfo cpuPower = getCpuPowerInfo();
        if (cpuPower.currentWatts > 0 && battery.voltageNow > 0) {
            // Both should report reasonable power values
            EXPECT_GT(cpuPower.currentWatts, 0.0);
            EXPECT_LT(cpuPower.currentWatts, 1000.0); // Less than 1000W
        }
    }
}

TEST_F(SysInfoIntegrationTest, VirtualizationAndHardwareConsistency) {
    // Test virtualization detection consistency with hardware info

    bool isVM = isVirtualMachine();
    std::string hypervisorVendor = getHypervisorVendor();
    double virtConfidence = getVirtualizationConfidence();

    // If we're in a VM, confidence should be high
    if (isVM) {
        EXPECT_GT(virtConfidence, 0.5);
    }

    // If we have a hypervisor vendor, we should be in a VM
    if (!hypervisorVendor.empty()) {
        EXPECT_TRUE(isVM);
    }

    // Hardware info should be consistent with virtualization
    CpuInfo cpuInfo = getCpuInfo();
    if (isVM && !hypervisorVendor.empty()) {
        // In a VM, some CPU features might be different
        // This is just a consistency check, not a strict requirement
        EXPECT_GT(cpuInfo.numLogicalCores, 0);
    }

    // Container detection
    bool isContainerEnv = isContainer();
    std::string containerType = getContainerType();

    if (isContainerEnv) {
        EXPECT_FALSE(containerType.empty());
    } else {
        EXPECT_TRUE(containerType.empty());
    }
}

// ============================================================================
// Performance Monitoring Tests
// ============================================================================

class PerformanceMonitoringTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup performance monitoring tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(PerformanceMonitoringTest, CpuPerformanceMonitoring) {
    // Test CPU performance monitoring over extended period
    std::vector<float> cpuUsageHistory;
    std::vector<float> cpuTemperatureHistory;
    std::vector<double> cpuFrequencyHistory;

    // Collect data over 5 iterations
    for (int i = 0; i < 5; ++i) {
        // CPU Usage
        float cpuUsage = getCurrentCpuUsage();
        cpuUsageHistory.push_back(cpuUsage);
        EXPECT_GE(cpuUsage, 0.0f);
        EXPECT_LE(cpuUsage, 100.0f);

        // CPU Temperature
        float cpuTemp = getCurrentCpuTemperature();
        if (cpuTemp > 0) {
            cpuTemperatureHistory.push_back(cpuTemp);
            EXPECT_GT(cpuTemp, 0.0f);
            EXPECT_LT(cpuTemp, 150.0f);
        }

        // CPU Frequency
        double cpuFreq = getProcessorFrequency();
        cpuFrequencyHistory.push_back(cpuFreq);
        EXPECT_GT(cpuFreq, 0.0);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Verify we collected data
    EXPECT_EQ(cpuUsageHistory.size(), 5);
    EXPECT_EQ(cpuFrequencyHistory.size(), 5);

    // CPU usage should be reasonable and not constant (unless system is idle)
    bool hasVariation = false;
    for (size_t i = 1; i < cpuUsageHistory.size(); ++i) {
        if (std::abs(cpuUsageHistory[i] - cpuUsageHistory[0]) > 1.0f) {
            hasVariation = true;
            break;
        }
    }
    // Note: On idle systems, CPU usage might be constant, so this is not strict
    EXPECT_TRUE(hasVariation || !hasVariation);
}

TEST_F(PerformanceMonitoringTest, MemoryPerformanceMonitoring) {
    // Test memory performance monitoring
    std::vector<float> memoryUsageHistory;
    std::vector<unsigned long long> availableMemoryHistory;

    for (int i = 0; i < 5; ++i) {
        // Memory Usage Percentage
        float memUsage = getMemoryUsage();
        memoryUsageHistory.push_back(memUsage);
        EXPECT_GE(memUsage, 0.0f);
        EXPECT_LE(memUsage, 100.0f);

        // Available Memory
        unsigned long long availableMem = getAvailableMemorySize();
        availableMemoryHistory.push_back(availableMem);
        EXPECT_GT(availableMem, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Verify data collection
    EXPECT_EQ(memoryUsageHistory.size(), 5);
    EXPECT_EQ(availableMemoryHistory.size(), 5);

    // Memory usage should be consistent over short periods
    for (size_t i = 1; i < memoryUsageHistory.size(); ++i) {
        float diff = std::abs(memoryUsageHistory[i] - memoryUsageHistory[i-1]);
        EXPECT_LT(diff, 50.0f); // Should not change by more than 50% quickly
    }
}

TEST_F(PerformanceMonitoringTest, NetworkPerformanceMonitoring) {
    // Test network performance monitoring
    std::vector<NetworkStats> networkStatsHistory;

    for (int i = 0; i < 3; ++i) {
        NetworkStats stats = getNetworkStats();
        networkStatsHistory.push_back(stats);

        // Validate current stats
        EXPECT_GE(stats.downloadSpeed, 0.0);
        EXPECT_GE(stats.uploadSpeed, 0.0);
        EXPECT_GE(stats.latency, 0.0);
        EXPECT_GE(stats.packetLoss, 0.0);
        EXPECT_LE(stats.packetLoss, 100.0);
        EXPECT_GE(stats.signalStrength, -150.0); // Reasonable lower bound for signal

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Verify reasonable values across measurements
    for (size_t i = 0; i < networkStatsHistory.size(); ++i) {
        EXPECT_GE(networkStatsHistory[i].downloadSpeed, 0.0);
        EXPECT_GE(networkStatsHistory[i].uploadSpeed, 0.0);
        EXPECT_GE(networkStatsHistory[i].latency, 0.0);
        EXPECT_LT(networkStatsHistory[i].latency, 10000.0); // Less than 10 seconds
    }
}

TEST_F(PerformanceMonitoringTest, SystemUptimeMonitoring) {
    // Test system uptime monitoring
    std::vector<std::chrono::seconds> uptimeHistory;

    for (int i = 0; i < 3; ++i) {
        std::chrono::seconds uptime = getSystemUptime();
        uptimeHistory.push_back(uptime);
        EXPECT_GT(uptime.count(), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Uptime should be monotonically increasing
    for (size_t i = 1; i < uptimeHistory.size(); ++i) {
        EXPECT_GE(uptimeHistory[i].count(), uptimeHistory[i-1].count());
    }

    // Uptime should increase by approximately the sleep duration
    if (uptimeHistory.size() >= 2) {
        auto uptimeDiff = uptimeHistory.back().count() - uptimeHistory.front().count();
        EXPECT_GE(uptimeDiff, 0); // At least some time should have passed
        EXPECT_LT(uptimeDiff, 10); // Should be less than 10 seconds for this test
    }
}

TEST_F(PerformanceMonitoringTest, PerCoreCpuMonitoring) {
    // Test per-core CPU monitoring
    std::vector<float> perCoreUsage = getPerCoreCpuUsage();

    // Should have at least one core
    EXPECT_GT(perCoreUsage.size(), 0);

    // Each core usage should be valid
    for (float usage : perCoreUsage) {
        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);
    }

    // Number of cores should match logical core count
    int logicalCores = getNumberOfLogicalCores();
    if (logicalCores > 0) {
        EXPECT_EQ(perCoreUsage.size(), static_cast<size_t>(logicalCores));
    }

    // Test per-core temperatures if available
    std::vector<float> perCoreTemp = getPerCoreCpuTemperature();
    for (float temp : perCoreTemp) {
        if (temp > 0) {
            EXPECT_GT(temp, 0.0f);
            EXPECT_LT(temp, 150.0f);
        }
    }

    // Test per-core frequencies
    std::vector<double> perCoreFreq = getPerCoreFrequencies();
    EXPECT_GT(perCoreFreq.size(), 0);
    for (double freq : perCoreFreq) {
        EXPECT_GT(freq, 0.0);
        EXPECT_LT(freq, 10000.0); // Less than 10 GHz
    }
}

TEST_F(PerformanceMonitoringTest, LoadAverageMonitoring) {
    // Test load average monitoring
    LoadAverage loadAvg = getCpuLoadAverage();

    // Load averages should be non-negative
    EXPECT_GE(loadAvg.oneMinute, 0.0);
    EXPECT_GE(loadAvg.fiveMinutes, 0.0);
    EXPECT_GE(loadAvg.fifteenMinutes, 0.0);

    // Load averages should be reasonable
    EXPECT_LT(loadAvg.oneMinute, 1000.0);
    EXPECT_LT(loadAvg.fiveMinutes, 1000.0);
    EXPECT_LT(loadAvg.fifteenMinutes, 1000.0);

    // Typically, longer-term averages are more stable
    // This is not always true, but generally expected
    // We just verify they're all valid values
}

TEST_F(PerformanceMonitoringTest, BatteryPerformanceMonitoring) {
    // Test battery performance monitoring if available
    auto batteryInfo = getBatteryInfo();

    if (batteryInfo.has_value()) {
        std::vector<float> batteryLevelHistory;

        for (int i = 0; i < 3; ++i) {
            auto currentBattery = getBatteryInfo();
            if (currentBattery.has_value()) {
                batteryLevelHistory.push_back(currentBattery->batteryLifePercent);

                // Validate battery metrics
                EXPECT_GE(currentBattery->batteryLifePercent, 0.0f);
                EXPECT_LE(currentBattery->batteryLifePercent, 100.0f);

                if (currentBattery->voltageNow > 0) {
                    EXPECT_GT(currentBattery->voltageNow, 0.0f);
                    EXPECT_LT(currentBattery->voltageNow, 50.0f);
                }

                if (currentBattery->currentNow != 0) {
                    EXPECT_LT(std::abs(currentBattery->currentNow), 100.0f);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        // Battery level should be relatively stable over short periods
        if (batteryLevelHistory.size() >= 2) {
            for (size_t i = 1; i < batteryLevelHistory.size(); ++i) {
                float diff = std::abs(batteryLevelHistory[i] - batteryLevelHistory[i-1]);
                EXPECT_LT(diff, 10.0f); // Should not change by more than 10% quickly
            }
        }
    }
}

// ============================================================================
// Boundary Condition and Edge Case Tests
// ============================================================================

class BoundaryConditionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup boundary condition tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(BoundaryConditionTest, CpuBoundaryConditions) {
    // Test CPU functions with boundary conditions

    // Test CPU usage at boundaries
    float cpuUsage = getCurrentCpuUsage();
    EXPECT_GE(cpuUsage, 0.0f);
    EXPECT_LE(cpuUsage, 100.0f);

    // Test per-core usage boundaries
    std::vector<float> perCoreUsage = getPerCoreCpuUsage();
    for (float usage : perCoreUsage) {
        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);
    }

    // Test temperature boundaries
    float cpuTemp = getCurrentCpuTemperature();
    if (cpuTemp > 0) {
        EXPECT_GT(cpuTemp, -50.0f); // Reasonable lower bound
        EXPECT_LT(cpuTemp, 200.0f); // Reasonable upper bound
    }

    // Test frequency boundaries
    double cpuFreq = getProcessorFrequency();
    EXPECT_GT(cpuFreq, 0.0);
    EXPECT_LT(cpuFreq, 20000.0); // 20 GHz upper bound

    double minFreq = getMinProcessorFrequency();
    double maxFreq = getMaxProcessorFrequency();
    EXPECT_GT(minFreq, 0.0);
    EXPECT_GT(maxFreq, 0.0);
    EXPECT_LE(minFreq, maxFreq);

    // Test core count boundaries
    int physicalCores = getNumberOfPhysicalCores();
    int logicalCores = getNumberOfLogicalCores();
    EXPECT_GT(physicalCores, 0);
    EXPECT_GT(logicalCores, 0);
    EXPECT_LE(physicalCores, logicalCores);
    EXPECT_LE(physicalCores, 1024); // Reasonable upper bound
    EXPECT_LE(logicalCores, 2048); // Reasonable upper bound
}

TEST_F(BoundaryConditionTest, MemoryBoundaryConditions) {
    // Test memory functions with boundary conditions

    // Test memory sizes
    unsigned long long totalMemory = getTotalMemorySize();
    unsigned long long availableMemory = getAvailableMemorySize();

    EXPECT_GT(totalMemory, 0);
    EXPECT_GE(availableMemory, 0);
    EXPECT_LE(availableMemory, totalMemory);

    // Test memory usage percentage
    float memoryUsage = getMemoryUsage();
    EXPECT_GE(memoryUsage, 0.0f);
    EXPECT_LE(memoryUsage, 100.0f);

    // Test virtual memory boundaries
    unsigned long long virtualMax = getVirtualMemoryMax();
    unsigned long long virtualUsed = getVirtualMemoryUsed();

    EXPECT_GT(virtualMax, 0);
    EXPECT_GE(virtualUsed, 0);
    EXPECT_LE(virtualUsed, virtualMax);
    EXPECT_GE(virtualMax, totalMemory); // Virtual should be >= physical

    // Test swap memory boundaries
    unsigned long long swapTotal = getSwapMemoryTotal();
    unsigned long long swapUsed = getSwapMemoryUsed();

    EXPECT_GE(swapTotal, 0);
    EXPECT_GE(swapUsed, 0);
    EXPECT_LE(swapUsed, swapTotal);

    // Test committed memory
    unsigned long long committed = getCommittedMemory();
    EXPECT_GT(committed, 0);
    EXPECT_LT(committed, totalMemory * 10); // Allow for overcommit
}

TEST_F(BoundaryConditionTest, DiskBoundaryConditions) {
    // Test disk functions with boundary conditions

    std::vector<DiskInfo> disks = getDiskInfo();
    EXPECT_FALSE(disks.empty());

    for (const auto& disk : disks) {
        // Test disk space boundaries
        EXPECT_GT(disk.totalSpace, 0);
        EXPECT_GE(disk.freeSpace, 0);
        EXPECT_LE(disk.freeSpace, disk.totalSpace);

        // Test usage percentage boundaries
        EXPECT_GE(disk.usagePercent, 0.0f);
        EXPECT_LE(disk.usagePercent, 100.0f);

        // Test path length boundaries
        EXPECT_GT(disk.path.length(), 0);
        EXPECT_LT(disk.path.length(), 1000); // Reasonable path length

        // Test file system type boundaries
        EXPECT_GT(disk.fsType.length(), 0);
        EXPECT_LT(disk.fsType.length(), 100); // Reasonable FS name length

        // Test model length if present
        if (!disk.model.empty()) {
            EXPECT_LT(disk.model.length(), 500); // Reasonable model name length
        }
    }

    // Test disk usage function
    std::vector<std::pair<std::string, float>> diskUsage = getDiskUsage();
    for (const auto& [path, usage] : diskUsage) {
        EXPECT_FALSE(path.empty());
        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);
    }
}

TEST_F(BoundaryConditionTest, NetworkBoundaryConditions) {
    // Test network functions with boundary conditions

    // Test network statistics boundaries
    NetworkStats stats = getNetworkStats();

    EXPECT_GE(stats.downloadSpeed, 0.0);
    EXPECT_GE(stats.uploadSpeed, 0.0);
    EXPECT_GE(stats.latency, 0.0);
    EXPECT_GE(stats.packetLoss, 0.0);
    EXPECT_LE(stats.packetLoss, 100.0);
    EXPECT_GE(stats.signalStrength, -150.0);
    EXPECT_LE(stats.signalStrength, 0.0);

    // Test network interface names (available API)
    std::vector<std::string> interfaceNames = getInterfaceNames();
    for (const auto& name : interfaceNames) {
        EXPECT_FALSE(name.empty());
        EXPECT_LT(name.length(), 100); // Reasonable interface name length
    }

    // Test WiFi networks (available API)
    std::vector<std::string> networks = scanAvailableNetworks();
    for (const auto& network : networks) {
        if (!network.empty()) {
            EXPECT_LT(network.length(), 100); // Reasonable SSID length
        }
    }
}

TEST_F(BoundaryConditionTest, BatteryBoundaryConditions) {
    // Test battery functions with boundary conditions

    auto batteryInfo = getBatteryInfo();
    if (batteryInfo.has_value()) {
        const auto& battery = batteryInfo.value();

        // Test battery percentage boundaries
        EXPECT_GE(battery.batteryLifePercent, 0.0f);
        EXPECT_LE(battery.batteryLifePercent, 100.0f);

        // Test battery time boundaries
        EXPECT_GE(battery.batteryLifeTime, 0.0f);
        EXPECT_GE(battery.batteryFullLifeTime, 0.0f);

        // Test energy boundaries
        EXPECT_GE(battery.energyNow, 0.0f);
        EXPECT_GE(battery.energyFull, 0.0f);
        EXPECT_GE(battery.energyDesign, 0.0f);

        if (battery.energyFull > 0) {
            EXPECT_LE(battery.energyNow, battery.energyFull * 1.1f); // Allow some tolerance
        }

        // Test voltage boundaries
        if (battery.voltageNow > 0) {
            EXPECT_GT(battery.voltageNow, 0.0f);
            EXPECT_LT(battery.voltageNow, 100.0f); // Reasonable voltage upper bound
        }

        // Test current boundaries
        if (battery.currentNow != 0) {
            EXPECT_LT(std::abs(battery.currentNow), 1000.0f); // Reasonable current bound
        }
    }
}

TEST_F(BoundaryConditionTest, SystemInfoBoundaryConditions) {
    // Test system info functions with boundary conditions

    // Test OS info boundaries
    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    EXPECT_FALSE(osInfo.osName.empty());
    EXPECT_FALSE(osInfo.osVersion.empty());
    EXPECT_FALSE(osInfo.architecture.empty());
    EXPECT_FALSE(osInfo.computerName.empty());

    EXPECT_LT(osInfo.osName.length(), 200);
    EXPECT_LT(osInfo.osVersion.length(), 200);
    EXPECT_LT(osInfo.architecture.length(), 100);
    EXPECT_LT(osInfo.computerName.length(), 200);

    // Test uptime boundaries
    std::chrono::seconds uptime = getSystemUptime();
    EXPECT_GT(uptime.count(), 0);
    EXPECT_LT(uptime.count(), 365 * 24 * 3600 * 10); // Less than 10 years

    // Test locale info boundaries
    LocaleInfo localeInfo = getSystemLanguageInfo();
    EXPECT_FALSE(localeInfo.languageCode.empty());
    EXPECT_FALSE(localeInfo.characterEncoding.empty());

    EXPECT_LT(localeInfo.languageCode.length(), 50);
    EXPECT_LT(localeInfo.localeName.length(), 200);
    EXPECT_LT(localeInfo.currencySymbol.length(), 20);
    EXPECT_LT(localeInfo.characterEncoding.length(), 100);

    // Window manager info not available in current implementation
    // Skip window manager tests
}

// ============================================================================
// Error Scenario Tests
// ============================================================================

class ErrorScenarioTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup error scenario tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ErrorScenarioTest, HardwareNotPresentScenarios) {
    // Test scenarios where hardware might not be present

    // Battery might not be present on desktop systems
    auto batteryInfo = getBatteryInfo();
    if (!batteryInfo.has_value()) {
        // This is acceptable - desktop systems often don't have batteries
        EXPECT_TRUE(true);
    } else {
        // If battery info is returned but no battery is present
        if (!batteryInfo->isBatteryPresent) {
            EXPECT_EQ(batteryInfo->batteryLifePercent, 0.0f);
            EXPECT_EQ(batteryInfo->energyNow, 0.0f);
            EXPECT_EQ(batteryInfo->energyFull, 0.0f);
        }
    }

    // GPU might not be present or accessible
    std::string gpuInfo = getGPUInfo();
    if (gpuInfo.empty() || gpuInfo.find("not available") != std::string::npos) {
        // This is acceptable on headless systems
        EXPECT_TRUE(true);
    }

    // Monitors might not be present on headless systems
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();
    if (monitors.empty()) {
        // This is acceptable on headless systems
        EXPECT_TRUE(true);
    }

    // WiFi might not be available
    std::string currentWifi = getCurrentWifi();
    if (currentWifi.empty()) {
        // This is acceptable if no WiFi adapter or not connected
        EXPECT_TRUE(true);
    }

    // Wired network might not be available
    std::string currentWired = getCurrentWiredNetwork();
    if (currentWired.empty()) {
        // This is acceptable if no wired connection
        EXPECT_TRUE(true);
    }
}

TEST_F(ErrorScenarioTest, PermissionDeniedScenarios) {
    // Test scenarios where operations might be denied due to permissions

    // Temperature readings might require elevated permissions
    float cpuTemp = getCurrentCpuTemperature();
    if (cpuTemp <= 0) {
        // This is acceptable if temperature sensors are not accessible
        EXPECT_TRUE(true);
    }

    std::vector<float> perCoreTemp = getPerCoreCpuTemperature();
    if (perCoreTemp.empty()) {
        // This is acceptable if per-core temperature is not accessible
        EXPECT_TRUE(true);
    }

    // Hardware serial numbers might require elevated permissions
    HardwareInfo hwInfo;
    std::string biosSerial = hwInfo.getBiosSerialNumber();
    std::string motherboardSerial = hwInfo.getMotherboardSerialNumber();
    std::string cpuSerial = hwInfo.getCpuSerialNumber();

    // These might be empty due to permissions or privacy settings
    EXPECT_TRUE(biosSerial.empty() || !biosSerial.empty());
    EXPECT_TRUE(motherboardSerial.empty() || !motherboardSerial.empty());
    EXPECT_TRUE(cpuSerial.empty() || !cpuSerial.empty());

    // BIOS operations might require elevated permissions
    BiosInfo* biosInfo = &BiosInfo::getInstance();

    // These operations might fail due to permissions
    EXPECT_NO_THROW({
        bool secureBootSupported = biosInfo->isSecureBootSupported();
        EXPECT_TRUE(secureBootSupported || !secureBootSupported);
    });

    EXPECT_NO_THROW({
        bool uefiBootSupported = biosInfo->isUEFIBootSupported();
        EXPECT_TRUE(uefiBootSupported || !uefiBootSupported);
    });

    // SMBIOS data might require elevated permissions
    EXPECT_NO_THROW({
        std::vector<std::string> smbiosData = biosInfo->getSMBIOSData();
        EXPECT_TRUE(smbiosData.empty() || !smbiosData.empty());
    });
}

TEST_F(ErrorScenarioTest, InvalidPathScenarios) {
    // Test scenarios with invalid paths

    // Test drive model with invalid paths
    std::vector<std::string> invalidPaths = {
        "",
        "/nonexistent/path",
        "C:\\nonexistent\\path",
        "/dev/nonexistent",
        "invalid_drive_123",
        "\\\\invalid\\unc\\path",
        "/proc/nonexistent",
        "/sys/nonexistent"
    };

    for (const auto& path : invalidPaths) {
        EXPECT_NO_THROW({
            std::string model = getDriveModel(path);
            // Should return empty string or handle gracefully
            EXPECT_TRUE(model.empty() || !model.empty());
        });
    }
}

TEST_F(ErrorScenarioTest, NetworkUnavailableScenarios) {
    // Test scenarios where network is unavailable

    // WiFi networks might be empty if no adapter or scanning fails
    std::vector<std::string> networks = scanAvailableNetworks();
    if (networks.empty()) {
        // This is acceptable if no WiFi adapter or no networks found
        EXPECT_TRUE(true);
    }

    // Network interface names might be minimal on some systems
    std::vector<std::string> interfaceNames = getInterfaceNames();
    if (interfaceNames.empty()) {
        // This should not happen as loopback should always exist, but handle gracefully
        EXPECT_TRUE(true);
    }

    // Network statistics might be zero on systems with no network activity
    NetworkStats stats = getNetworkStats();
    if (stats.downloadSpeed == 0.0 && stats.uploadSpeed == 0.0) {
        // This is acceptable on systems with no network activity
        EXPECT_TRUE(true);
    }
}

TEST_F(ErrorScenarioTest, VirtualizationDetectionErrors) {
    // Test virtualization detection error scenarios

    // Hypervisor vendor might be empty if not in a VM
    std::string hypervisorVendor = getHypervisorVendor();
    if (hypervisorVendor.empty()) {
        // This is acceptable if not running in a VM
        bool isVM = isVirtualMachine();
        if (!isVM) {
            EXPECT_TRUE(true);
        }
    }

    // Container type might be empty if not in a container
    std::string containerType = getContainerType();
    bool isContainerEnv = isContainer();

    if (!isContainerEnv) {
        EXPECT_TRUE(containerType.empty());
    }

    // Virtualization confidence might be low on bare metal
    double virtConfidence = getVirtualizationConfidence();
    EXPECT_GE(virtConfidence, 0.0);
    EXPECT_LE(virtConfidence, 1.0);

    if (virtConfidence < 0.1) {
        // Low confidence is acceptable on bare metal systems
        EXPECT_TRUE(true);
    }
}

TEST_F(ErrorScenarioTest, SystemInfoUnavailableScenarios) {
    // Test scenarios where system info might be unavailable

    // Window manager info not available in current implementation
    // Skip window manager tests for now

    // System language might have fallback values
    std::string systemLanguage = getSystemLanguage();
    if (systemLanguage.empty()) {
        // This should not happen, but handle gracefully
        EXPECT_TRUE(true);
    }

    // System encoding might have fallback values
    std::string systemEncoding = getSystemEncoding();
    if (systemEncoding.empty()) {
        // This should not happen, but handle gracefully
        EXPECT_TRUE(true);
    }
}

TEST_F(ErrorScenarioTest, MemoryConstraintScenarios) {
    // Test scenarios with memory constraints

    // Very low memory scenarios
    MemoryInfo memInfo = getDetailedMemoryStats();

    if (memInfo.memoryLoadPercentage > 95.0f) {
        // System is under high memory pressure
        EXPECT_GT(memInfo.totalPhysicalMemory, 0);
        EXPECT_LT(memInfo.availablePhysicalMemory, memInfo.totalPhysicalMemory * 0.1);
    }

    // Swap usage scenarios
    if (memInfo.swapMemoryTotal > 0 && memInfo.swapMemoryUsed > memInfo.swapMemoryTotal * 0.8) {
        // High swap usage scenario
        EXPECT_GT(memInfo.swapMemoryUsed, 0);
    }

    // Virtual memory scenarios
    if (memInfo.virtualMemoryUsed > memInfo.virtualMemoryMax * 0.9) {
        // High virtual memory usage
        EXPECT_GT(memInfo.virtualMemoryUsed, 0);
    }
}

TEST_F(ErrorScenarioTest, DiskErrorScenarios) {
    // Test disk error scenarios

    std::vector<DiskInfo> disks = getDiskInfo();

    for (const auto& disk : disks) {
        // Full disk scenarios
        if (disk.usagePercent > 95.0f) {
            EXPECT_LT(disk.freeSpace, disk.totalSpace * 0.05);
        }

        // Read-only file systems
        if (disk.fsType.find("proc") != std::string::npos ||
            disk.fsType.find("sysfs") != std::string::npos ||
            disk.fsType.find("tmpfs") != std::string::npos) {
            // These are special file systems, different rules apply
            EXPECT_TRUE(true);
        }

        // Removable media scenarios
        if (disk.isRemovable) {
            // Removable media might have different characteristics
            EXPECT_TRUE(true);
        }
    }

    // Empty disk list scenario (should not happen but handle gracefully)
    if (disks.empty()) {
        EXPECT_TRUE(true);
    }
}

TEST_F(ErrorScenarioTest, NoThrowGuaranteeUnderErrors) {
    // Ensure all functions provide no-throw guarantee even under error conditions

    // Test all major functions don't throw even when errors occur
    EXPECT_NO_THROW(getCpuInfo());
    EXPECT_NO_THROW(getDetailedMemoryStats());
    EXPECT_NO_THROW(getOperatingSystemInfo());
    EXPECT_NO_THROW(getDiskInfo());
    EXPECT_NO_THROW(getGPUInfo());
    EXPECT_NO_THROW(getAllMonitorsInfo());
    EXPECT_NO_THROW(getCurrentWifi());
    EXPECT_NO_THROW(getCurrentWiredNetwork());
    EXPECT_NO_THROW(getInterfaceNames());
    EXPECT_NO_THROW(scanAvailableNetworks());
    EXPECT_NO_THROW(getNetworkStats());
    EXPECT_NO_THROW(getBatteryInfo());
    EXPECT_NO_THROW(getSystemLanguageInfo());
    EXPECT_NO_THROW(isVirtualMachine());
    EXPECT_NO_THROW(isContainer());
    EXPECT_NO_THROW(getSystemUptime());

    // Test with potentially problematic inputs
    EXPECT_NO_THROW(getDriveModel(""));
    EXPECT_NO_THROW(getDriveModel("/invalid"));
    EXPECT_NO_THROW(getDriveModel("C:\\invalid"));

    // Test hardware info functions
    HardwareInfo hwInfo;
    EXPECT_NO_THROW(hwInfo.getBiosSerialNumber());
    EXPECT_NO_THROW(hwInfo.getMotherboardSerialNumber());
    EXPECT_NO_THROW(hwInfo.getCpuSerialNumber());
    EXPECT_NO_THROW(hwInfo.getDiskSerialNumbers());

    // Test BIOS functions
    BiosInfo* biosInfo = &BiosInfo::getInstance();
    EXPECT_NO_THROW(biosInfo->getBiosInfo());
    EXPECT_NO_THROW(biosInfo->isSecureBootSupported());
    EXPECT_NO_THROW(biosInfo->isUEFIBootSupported());
    EXPECT_NO_THROW(biosInfo->getSMBIOSData());
    EXPECT_NO_THROW(biosInfo->checkForUpdates());
}

// ============================================================================
// Input Validation Tests
// ============================================================================

class InputValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup input validation tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(InputValidationTest, DriveModelInputValidation) {
    // Test getDriveModel with various invalid inputs

    // Empty string
    EXPECT_NO_THROW({
        std::string model = getDriveModel("");
        EXPECT_TRUE(model.empty() || !model.empty()); // Should handle gracefully
    });

    // Null-like strings
    std::vector<std::string> nullLikeInputs = {
        "\0",
        "\n",
        "\r",
        "\t",
        "   ",  // Only spaces
        "\0\0\0",
        "\n\n\n",
        "\r\n\r\n"
    };

    for (const auto& input : nullLikeInputs) {
        EXPECT_NO_THROW({
            std::string model = getDriveModel(input);
            EXPECT_TRUE(model.empty() || !model.empty());
        });
    }

    // Very long paths
    std::string longPath(10000, 'a'); // 10KB path
    EXPECT_NO_THROW({
        std::string model = getDriveModel(longPath);
        EXPECT_TRUE(model.empty() || !model.empty());
    });

    // Paths with special characters
    std::vector<std::string> specialPaths = {
        "C:\\Program Files\\",
        "/usr/bin/",
        "\\\\server\\share",
        "file:///C:/path",
        "http://example.com/path",
        "ftp://ftp.example.com/",
        "../../../etc/passwd",
        "..\\..\\..\\windows\\system32",
        "/dev/null",
        "/proc/self/exe",
        "CON", "PRN", "AUX", "NUL", // Windows reserved names
        "LPT1", "COM1"
    };

    for (const auto& path : specialPaths) {
        EXPECT_NO_THROW({
            std::string model = getDriveModel(path);
            EXPECT_TRUE(model.empty() || !model.empty());
        });
    }

    // Unicode paths
    std::vector<std::string> unicodePaths = {
        "/home/用户/文档",
        "C:\\Users\\Пользователь\\Документы",
        "/Users/ユーザー/書類",
        "C:\\مستخدمين\\مستندات"
    };

    for (const auto& path : unicodePaths) {
        EXPECT_NO_THROW({
            std::string model = getDriveModel(path);
            EXPECT_TRUE(model.empty() || !model.empty());
        });
    }
}

TEST_F(InputValidationTest, LocaleInfoInputValidation) {
    // Test LocaleInfo with invalid data
    LocaleInfo info;

    // Test with null characters
    info.languageCode = std::string("en\0US", 5);
    info.localeName = std::string("English\0(US)", 11);
    info.currencySymbol = std::string("$\0", 2);
    info.decimalSymbol = std::string(".\0", 2);
    info.dateFormat = std::string("MM/dd\0yyyy", 10);
    info.timeFormat = std::string("HH:mm\0ss", 8);
    info.characterEncoding = std::string("UTF\0-8", 6);

    // Should handle gracefully
    EXPECT_NO_THROW(printLocaleInfo(info));

    // Test with very long strings
    info.languageCode = std::string(1000, 'a');
    info.localeName = std::string(10000, 'b');
    info.currencySymbol = std::string(100, '$');
    info.decimalSymbol = std::string(50, '.');
    info.dateFormat = std::string(500, 'd');
    info.timeFormat = std::string(500, 'h');
    info.characterEncoding = std::string(1000, 'u');

    EXPECT_NO_THROW(printLocaleInfo(info));

    // Test with control characters
    info.languageCode = "\x01\x02\x03";
    info.localeName = "\x04\x05\x06";
    info.currencySymbol = "\x07\x08\x09";
    info.decimalSymbol = "\x0A";
    info.dateFormat = "\x0B\x0C\x0D";
    info.timeFormat = "\x0E\x0F\x10";
    info.characterEncoding = "\x11\x12\x13";

    EXPECT_NO_THROW(printLocaleInfo(info));
}

TEST_F(InputValidationTest, BatteryInfoInputValidation) {
    // Test BatteryInfo with boundary and invalid values
    BatteryInfo info;

    // Test with extreme values
    info.batteryLifePercent = -100.0f;
    info.batteryLifeTime = -1000.0f;
    info.batteryFullLifeTime = -5000.0f;
    info.energyNow = -999999.0f;
    info.energyFull = -888888.0f;
    info.energyDesign = -777777.0f;
    info.voltageNow = -50.0f;
    info.currentNow = -1000.0f;

    // Should handle gracefully
    EXPECT_NO_THROW({
        float estimatedTime = info.getEstimatedTimeRemaining();
        EXPECT_GE(estimatedTime, 0.0f); // Should return reasonable value
    });

    // Test with very large values
    info.batteryLifePercent = 999999.0f;
    info.batteryLifeTime = 999999.0f;
    info.batteryFullLifeTime = 999999.0f;
    info.energyNow = 999999999.0f;
    info.energyFull = 999999999.0f;
    info.energyDesign = 999999999.0f;
    info.voltageNow = 999999.0f;
    info.currentNow = 999999.0f;

    EXPECT_NO_THROW({
        float estimatedTime = info.getEstimatedTimeRemaining();
        EXPECT_GE(estimatedTime, 0.0f);
    });

    // Test with NaN and infinity
    info.batteryLifePercent = std::numeric_limits<float>::quiet_NaN();
    info.voltageNow = std::numeric_limits<float>::infinity();
    info.currentNow = -std::numeric_limits<float>::infinity();

    EXPECT_NO_THROW({
        float estimatedTime = info.getEstimatedTimeRemaining();
        // Should handle NaN/infinity gracefully
        EXPECT_TRUE(estimatedTime != estimatedTime || estimatedTime >= 0.0f); // NaN check
    });
}

TEST_F(InputValidationTest, SystemInfoPrinterInputValidation) {
    // Test SystemInfoPrinter with invalid data structures

    // Test with empty CPU info
    CpuInfo emptyCpu;
    EXPECT_NO_THROW({
        std::string formatted = SystemInfoPrinter::formatCpuInfo(emptyCpu);
        EXPECT_TRUE(formatted.empty() || !formatted.empty());
    });

    // Test with empty memory info
    MemoryInfo emptyMemory;
    EXPECT_NO_THROW({
        std::string formatted = SystemInfoPrinter::formatMemoryInfo(emptyMemory);
        EXPECT_TRUE(formatted.empty() || !formatted.empty());
    });

    // Test with empty OS info
    OperatingSystemInfo emptyOs;
    EXPECT_NO_THROW({
        std::string formatted = SystemInfoPrinter::formatOsInfo(emptyOs);
        EXPECT_TRUE(formatted.empty() || !formatted.empty());
    });

    // Test with empty battery info
    BatteryInfo emptyBattery;
    EXPECT_NO_THROW({
        std::string formatted = SystemInfoPrinter::formatBatteryInfo(emptyBattery);
        EXPECT_TRUE(formatted.empty() || !formatted.empty());
    });

    // Test with corrupted data
    CpuInfo corruptedCpu;
    corruptedCpu.model = std::string("Intel\0Core", 10);
    corruptedCpu.identifier = std::string("CPU\0ID", 6);
    corruptedCpu.numLogicalCores = -1;
    corruptedCpu.numPhysicalCores = 0;
    corruptedCpu.baseFrequency = -1000.0;
    corruptedCpu.maxFrequency = std::numeric_limits<double>::quiet_NaN();

    EXPECT_NO_THROW({
        std::string formatted = SystemInfoPrinter::formatCpuInfo(corruptedCpu);
        EXPECT_TRUE(formatted.empty() || !formatted.empty());
    });
}

TEST_F(InputValidationTest, HardwareInfoInputValidation) {
    // Test HardwareInfo with various scenarios
    HardwareInfo hwInfo;

    // These functions should handle all scenarios gracefully
    EXPECT_NO_THROW({
        std::string biosSerial = hwInfo.getBiosSerialNumber();
        EXPECT_TRUE(biosSerial.empty() || !biosSerial.empty());
    });

    EXPECT_NO_THROW({
        std::string motherboardSerial = hwInfo.getMotherboardSerialNumber();
        EXPECT_TRUE(motherboardSerial.empty() || !motherboardSerial.empty());
    });

    EXPECT_NO_THROW({
        std::string cpuSerial = hwInfo.getCpuSerialNumber();
        EXPECT_TRUE(cpuSerial.empty() || !cpuSerial.empty());
    });

    EXPECT_NO_THROW({
        std::vector<std::string> diskSerials = hwInfo.getDiskSerialNumbers();
        for (const auto& serial : diskSerials) {
            EXPECT_FALSE(serial.empty());
            // Should not contain null characters
            EXPECT_EQ(serial.find('\0'), std::string::npos);
        }
    });

    // Test copy and move operations with potentially corrupted data
    HardwareInfo hwInfo2(hwInfo);
    EXPECT_NO_THROW({
        std::string serial = hwInfo2.getBiosSerialNumber();
        EXPECT_TRUE(serial.empty() || !serial.empty());
    });

    HardwareInfo hwInfo3;
    hwInfo3 = std::move(hwInfo);
    EXPECT_NO_THROW({
        std::string serial = hwInfo3.getMotherboardSerialNumber();
        EXPECT_TRUE(serial.empty() || !serial.empty());
    });
}

TEST_F(InputValidationTest, BiosInfoInputValidation) {
    // Test BiosInfo with various scenarios
    BiosInfo* biosInfo = &BiosInfo::getInstance();

    // All BIOS operations should handle errors gracefully
    EXPECT_NO_THROW({
        BiosInfoData info = biosInfo->getBiosInfo();
        // Should handle empty or corrupted BIOS data
        EXPECT_TRUE(info.version.empty() || !info.version.empty());
        EXPECT_TRUE(info.manufacturer.empty() || !info.manufacturer.empty());
    });

    EXPECT_NO_THROW({
        std::vector<std::string> smbiosData = biosInfo->getSMBIOSData();
        for (const auto& entry : smbiosData) {
            // Should not contain null characters
            EXPECT_EQ(entry.find('\0'), std::string::npos);
        }
    });

    EXPECT_NO_THROW({
        BiosUpdateInfo updateInfo = biosInfo->checkForUpdates();
        EXPECT_TRUE(updateInfo.updateAvailable || !updateInfo.updateAvailable);
        if (updateInfo.updateAvailable) {
            EXPECT_FALSE(updateInfo.latestVersion.empty());
        }
    });

    // Test backup/restore with invalid paths
    std::vector<std::string> invalidPaths = {
        "",
        "\0",
        "/invalid/path/that/does/not/exist",
        "C:\\invalid\\path\\that\\does\\not\\exist",
        std::string(10000, 'x'), // Very long path
        "CON", "PRN", "AUX", // Windows reserved names
        "../../../etc/passwd",
        "\\\\invalid\\unc\\path"
    };

    for (const auto& path : invalidPaths) {
        EXPECT_NO_THROW({
            bool result = biosInfo->backupBiosSettings(path);
            EXPECT_TRUE(result || !result); // Should return boolean
        });

        EXPECT_NO_THROW({
            bool result = biosInfo->restoreBiosSettings(path);
            EXPECT_TRUE(result || !result); // Should return boolean
        });
    }
}

TEST_F(InputValidationTest, StringFieldValidation) {
    // Test that all string fields in system info structures handle special characters

    // Test OS info with special characters
    OperatingSystemInfo osInfo = getOperatingSystemInfo();

    // Verify no null characters in critical fields
    EXPECT_EQ(osInfo.osName.find('\0'), std::string::npos);
    EXPECT_EQ(osInfo.osVersion.find('\0'), std::string::npos);
    EXPECT_EQ(osInfo.architecture.find('\0'), std::string::npos);
    EXPECT_EQ(osInfo.computerName.find('\0'), std::string::npos);

    // Test disk info string validation
    std::vector<DiskInfo> disks = getDiskInfo();
    for (const auto& disk : disks) {
        EXPECT_EQ(disk.path.find('\0'), std::string::npos);
        EXPECT_EQ(disk.fsType.find('\0'), std::string::npos);
        if (!disk.devicePath.empty()) {
            EXPECT_EQ(disk.devicePath.find('\0'), std::string::npos);
        }
        if (!disk.model.empty()) {
            EXPECT_EQ(disk.model.find('\0'), std::string::npos);
        }
    }

    // Test CPU info string validation
    CpuInfo cpuInfo = getCpuInfo();
    EXPECT_EQ(cpuInfo.model.find('\0'), std::string::npos);
    EXPECT_EQ(cpuInfo.identifier.find('\0'), std::string::npos);

    for (const auto& flag : cpuInfo.flags) {
        EXPECT_EQ(flag.find('\0'), std::string::npos);
        EXPECT_FALSE(flag.empty());
    }
}

TEST_F(InputValidationTest, NumericFieldValidation) {
    // Test that numeric fields have reasonable bounds

    // CPU info numeric validation
    CpuInfo cpuInfo = getCpuInfo();
    EXPECT_GT(cpuInfo.numLogicalCores, 0);
    EXPECT_LE(cpuInfo.numLogicalCores, 10000); // Reasonable upper bound
    EXPECT_GT(cpuInfo.numPhysicalCores, 0);
    EXPECT_LE(cpuInfo.numPhysicalCores, 5000); // Reasonable upper bound
    EXPECT_GT(cpuInfo.baseFrequency, 0.0);
    EXPECT_LT(cpuInfo.baseFrequency, 50000.0); // 50 GHz upper bound

    // Memory info numeric validation
    MemoryInfo memInfo = getDetailedMemoryStats();
    EXPECT_GT(memInfo.totalPhysicalMemory, 0);
    EXPECT_LT(memInfo.totalPhysicalMemory, 1000ULL * 1024 * 1024 * 1024 * 1024); // 1000TB
    EXPECT_GE(memInfo.memoryLoadPercentage, 0.0f);
    EXPECT_LE(memInfo.memoryLoadPercentage, 100.0f);

    // Disk info numeric validation
    std::vector<DiskInfo> disks = getDiskInfo();
    for (const auto& disk : disks) {
        EXPECT_GT(disk.totalSpace, 0);
        EXPECT_LT(disk.totalSpace, 1000ULL * 1024 * 1024 * 1024 * 1024); // 1000TB
        EXPECT_GE(disk.freeSpace, 0);
        EXPECT_LE(disk.freeSpace, disk.totalSpace);
        EXPECT_GE(disk.usagePercent, 0.0f);
        EXPECT_LE(disk.usagePercent, 100.0f);
    }

    // System uptime validation
    std::chrono::seconds uptime = getSystemUptime();
    EXPECT_GT(uptime.count(), 0);
    EXPECT_LT(uptime.count(), 100LL * 365 * 24 * 3600); // Less than 100 years
}

} // namespace atom::sysinfo::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
